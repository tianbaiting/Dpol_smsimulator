# QMD raw → g4 input → g4 output: b_phi 元数据传递设计

**Date:** 2026-04-28
**Branch:** `feature/qmd-g4-io-branches`
**Author:** Baiting Tian (with brainstorming via Claude)

## 1. 背景与目标

### 1.1 当前现状

DPOL 模拟链路的两端:

- **QMD raw** (`data/qmdrawdata/...` 或外部 SSD `dpol_data/QMDdata/inputtree/`): ImQMD + GEMINI 文本输出。`dbreak*.dat` 为弹性破裂事件 (deuteron breakup) 的逐事件 (px, py, pz) 列表。
- **g4 input ROOT**: `GenInputRoot_qmdrawdata` 读 `dbreak*.dat` / `geminiout*.dat` 写出 `TBeamSimData` 数组,作为 `bin/sim_deuteron` 的 `BeamTypeTree` 输入。
- **g4 output ROOT**: `bin/sim_deuteron` 输出 PDC/NEBULA 命中,经 `FragSimDataConverter_Basic` 写出 per-event 标量 (PDC1U/X/V, target_x/y/theta/phi, ok_target...)。

### 1.2 当前问题

1. **z_pol 旋转是"对齐"而非"随机化"**: `GenInputRoot_qmdrawdata.cc` 在 `rotate_zpol=true` 默认下对每事件做 `angle = -atan2(sum_py, sum_px)` 旋转,使所有事件 np 总动量对齐到 +x。这是 commit `3a91522` ("add px scan test on target pdc-reconstruction") 引入的临时简化,**不是物理事实**:
   - 阻止任何反应平面重建验证(truth phi 全为 0,reco 总返回 0 也对)
   - 实验里 z_pol 反应平面方位本就该均匀,acceptance 不真实
   - 与 y_pol/phi_random(已经均匀)语义不一致
2. **y_pol/phi_random 数据有 9 列 `b, rpphi`**(QMD 已应用的随机反应平面角度),当前 parser 只读前 7 列,**rpphi 信息被丢弃**。
3. **truth 反应平面角度在 g4 输入/输出 ROOT 都没有存**: 下游分析无法用 truth 验证反应平面 reco 精度。

### 1.3 目标

1. 用 **b_phi**(碰撞参数 b 在 lab 系的方位角,单位 rad)作为统一的"反应平面方位"标签,记录到输入和输出 ROOT。
2. z_pol: 替换"对齐"为每事件 uniform[0, 2π) 随机旋转,记录 b_phi。
3. y_pol/phi_random: 直接读取 9 列里的 rpphi,不再额外旋转。
4. y_pol/phi_fixed 与所有 7 列旧数据: 视为 b_phi=0,语义一致。
5. 输出 ROOT 透传 truth (b_phi, bimp, phi_np_truth),供 reco 验证。

### 1.4 约定

- **单位**: 内部统一 **rad**(代码、ROOT branch、metadata 元素全用 rad)。CLI / log / raw QMD 文件以 ° 输入时在 parser 边界转换。
- **b_phi**: 碰撞参数 b 在 lab 系的方位角(rad,范围 [0, 2π))。raw QMD 默认坐标 b 沿 +x → b_phi_raw = 0;y_pol/phi_random 给出 `rpphi`(°)→ b_phi_raw = rpphi · π/180。叠加任何旋转 Δ 后 b_phi = (b_phi_raw + Δ) mod 2π。
- **phi_np_truth**: `atan2(sum_py, sum_px)`,**旋转后**坐标系下计算,范围 (-π, π](TMath::ATan2 默认值域)。
- **吸引散射**(默认主体): np 总动量与 b 反向 → `phi_np_truth ≈ b_phi + π (mod 2π)`;少数库仑排斥事件 phi_np ≈ b_phi(已通过 5000 事件 input analysis 验证,见 §5)。

## 2. 架构与数据流

```
QMD raw (.dat, 7 或 9 列)
        │
        ▼  GenInputRoot_qmdrawdata
        │   • 自动嗅探列数: 9 列读 b/rpphi,7 列填 0
        │   • z_pol: Δ ~ U(0, 2π); 旋转 (px,py); b_phi = b_phi_raw + Δ
        │   • y_pol: Δ = 0; b_phi = b_phi_raw
        │   • phi_np_truth = atan2(sum_py_after, sum_px_after)
        │   • stamp fUserDouble[bimp, b_phi, phi_np_truth] per-particle (值 per-event 一致)
        ▼
g4 input ROOT (TBeamSimData TClonesArray)
        │
        ▼  PrimaryGeneratorActionBasic::BeamTypeTree
        │   • 不旋转(已离线 baked)
        │   • 把首个 primary 的 fUserDouble 元数据塞给 SimDataManager
        ▼
Geant4 simulation
        │
        ▼  EventTruthConverter (新)
        │   • DefineBranch: truth_b_phi, truth_bimp, truth_phi_np (per-event Double)
        │   • ConvertSimData: 从 SimDataManager 读当前事件元数据写 branch
        ▼
g4 output ROOT (新 truth_* branches + 现有 PDC/NEBULA branches)
        │
        ▼  下游 reconstruction (run_reconstruction / extract_phys_observables)
            • 从 PDC + NEBULA 重建 k_in / k_out / 反应平面 phi_RP_reco
            • 对照 truth_b_phi(吸引散射期望 phi_RP_reco ≈ truth_b_phi + π)
```

## 3. 决策记录

| 决策 | 选项 | 选定 | 理由 |
|---|---|---|---|
| Q1 z_pol 旋转语义 | (A) 随机替换对齐 / (B) 对齐+随机 / (C) 直接随机不对齐 | **C** | 保留 raw QMD 物理(吸引/排斥比例),只把方位均匀化 |
| Q2 旋转位置 | (A) GenInputRoot 离线 / (B) g4 PGA 运行时 | **A** | 输入 ROOT 一旦生成稳定可复现;不污染 PGA 逻辑;与 y_pol 不对称在 GenInputRoot 一行处理 |
| Q3 新增 branch 范围 | (a) b_phi (b) bimp (c) phi_np_truth (d-j 其他) | **a + b + c** | 最小集合即可支撑 reco 验证目标;helicity/gamma 已在路径里 |
| Q4(i) RNG seed CLI | (α) 显式 `--rotation-seed` / (β) hash 派生 / (γ) 不暴露 | **α** | 显式可复现 |
| Q4(ii) 旧 flag 处理 | (α) 删 / (β) 偷换语义 / (γ) 重命名 | **γ** | `--randomize-zpol/-ypol`,语义清楚不破坏可读性 |
| Q4(iii) 9 列嗅探 | (α) 自动嗅探 / (β) CLI 标记 | **α** | 老数据自动兼容,无副作用 |

## 4. 文件改动清单

### 4.1 元数据契约

`libs/smg4lib/include/QMDInputMetadata.hh`:
- 新增 `kBPhiIndex = 1` 在 fUserDouble[]
- 新增 `kPhiNpTruthIndex = 2` 在 fUserDouble[]
- 既有 `kBimpIndex = 0` 不变

### 4.2 输入侧 (GenInputRoot_qmdrawdata)

`scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`:

1. **9 列嗅探**: `ConvertElasticFile` 中 `iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn` 之后,尝试再读两个 token (b_perevt, rpphi);失败则用 0。
2. **删除对齐逻辑**: 移除 line 528-549 的 `angle = -phi` 块。
3. **新随机旋转逻辑**(在 `ConvertElasticFile` 内部,使用 `main()` 创建并按 `--rotation-seed` 初始化的单一 `TRandom3` 实例,通过函数参数传入):
   ```cpp
   const double rpphi_deg = has_rpphi_column ? rpphi_token : 0.0;
   const double b_phi_raw = rpphi_deg * (TMath::Pi() / 180.0);  // ° → rad
   double delta = 0.0;
   if (pol == kZ && opts.randomize_zpol) {
       delta = TMath::TwoPi() * rng.Uniform();
   } else if (pol == kY && opts.randomize_ypol) {
       delta = TMath::TwoPi() * rng.Uniform();
   }
   if (delta != 0.0) { /* rotate (px,py) by +delta — 标准 2D 旋转 */ }
   const double b_phi = std::fmod(b_phi_raw + delta, TMath::TwoPi());
   const double phi_np_truth = std::atan2(pyp + pyn, pxp + pxn);  // 旋转后
   ```
4. **CLI**:
   - 重命名 `--rotate-ypol/-zpol` → `--randomize-ypol/-zpol`(默认: ypol=off, zpol=on)
   - 新增 `--rotation-seed N`(默认 0 → 用 system time 派生)
5. **元数据 stamp 扩展**: `StampMetadata` 多写两个 `fUserDouble[1] = b_phi`、`fUserDouble[2] = phi_np_truth`。

### 4.3 g4 端 truth 透传

**新文件** `libs/smg4lib/src/data/include/TEventTruthData.hh` + `src/TEventTruthData.cc`:
- ROOT-aware POD: `Double_t fBPhi, fBimp, fPhiNpTruth;`
- 由 `SimDataManager` 持有一份 `TEventTruthData* fEventTruth`(per-event)

**新文件** `libs/smg4lib/src/data/include/EventTruthConverter.hh` + `src/EventTruthConverter.cc`(继承 `SimDataConverter`):
- `DefineBranch(tree)`: 写 `truth_b_phi/D`、`truth_bimp/D`、`truth_phi_np/D`(per-event scalar)
- `ConvertSimData()`: 从 `SimDataManager` 读 `fEventTruth` 拷给本地变量(让 TBranch 写出)
- `ClearBuffer()`: 标记 -999 / NaN 表示未填

**改动** `libs/smg4lib/src/action/src/PrimaryGeneratorActionBasic.cc::BeamTypeTree`:
- 在读完 `fInputTree->GetEntry(...)` 之后,从首个 primary 的 `fUserDouble` 拷出 (bimp, b_phi, phi_np_truth) → `SimDataManager::SetEventTruth(...)`
- 缺失(`fUserDouble.size() < 3`)时填 NaN 并 log warn 一次

**改动** `libs/smg4lib/src/action/src/RunActionBasic.cc`:
- 在 `BeginOfRunAction` 注册 `EventTruthConverter` 到 `SimDataManager`(与 `FragSimDataConverter_Basic` 并列)。

**`apps/sim_deuteron/main.cc`**: 不需要改动 — converter 注册由 `RunActionBasic` 在 BeginOfRunAction 内完成,顺序由 SimDataManager 保证(input PGA 在 EventAction 之前)。

### 4.4 测试

`tests/smg4lib/test_event_truth_metadata.cc` *(新, GoogleTest unit)*:
- 给定 `pxp,pyp,pxn,pyn` 已知 → 计算期望 b_phi、phi_np_truth → 与 GenInputRoot 实现对比
- 固定 seed 验证可复现性
- 9 列 vs 7 列两种输入

`tests/smg4lib/test_event_truth_branch.cc` *(新, integration)*:
- 跑迷你 GenInputRoot → 跑迷你 g4(macro 100 事件)→ 验证输出 ROOT 的 `truth_b_phi` 等值与输入元数据一致

### 4.5 下游兼容性 (informative)

- `scripts/analysis/ypol_phys/extract_phys_observables.cc` (用户当前 working tree): 新 truth_b_phi/truth_phi_np 可作为附加 CSV 列;但**本设计不强制改它**,只保证 g4 output 有这些 branch
- `libs/analysis/src/EventDataReader.cc`: 加 `SetBranchAddress` 检查时容忍缺失(老 ROOT 无新 branch)

## 5. 验证依据(已完成)

5000 个 y_pol/phi_random (Sn124, E190, g050, ynp 与 ypn) 事件分析:

| 数据集 | (phi_np − rpphi) mod 360° 峰位 | 集中度 |
|---|---|---|
| ynp_RP360 | 170-180° | 18.3% / 10° bin (均匀基线 2.78%) |
| ypn_RP360 | 180-190° | 19.0% |
| ypol/phi_fixed/ynp_b05 (隐含 rpphi=0) | 170-180° | 10.3% |
| zpol/znp_b05 (隐含 rpphi=0) | 170-180° | 11.2% |

**结论**:
- 所有四种数据集 np 总动量主峰一致在 b 反向 ~180°(吸引散射),少量在 0°(库仑排斥)
- y_pol 与 z_pol 共用同一坐标约定,**不需要负号**
- rpphi 在新格式中 = b 方向 (lab 系),不是 np 方向

## 6. 破坏性变更与迁移

1. **CLI 重命名**: `--rotate-zpol/-ypol` → `--randomize-zpol/-ypol`。`scripts/simulation/run_g4input_batch.sh` 同步更新。
2. **z_pol input ROOT 内容变化**: 旋转语义从"对齐到 phi=0"变成"随机方位",已生成的 g4 input ROOT 需重跑(用户已知,旧文件主要在 `build/test_output/...` 临时位置)。
3. **g4 input ROOT schema 扩展**: `fUserDouble.size()` 从 1 增至 3。g4 端读取做向后兼容(缺失填 NaN + warn)。
4. **g4 output ROOT 新增 3 个 branch**: 老 reco / 分析脚本若用 `SetBranchAddress` 不会看到新 branch,无破坏。

## 7. 测试验收标准

- [ ] 单元测试: 给定固定输入,b_phi、phi_np_truth 与解析期望相等(容差 1e-6 rad)
- [ ] Reproducibility: 同 seed 两次跑出来 ROOT 文件 byte-for-byte 一致(或 b_phi 列完全一致)
- [ ] z_pol b_phi 直方图: 均匀分布(KS 检验 p>0.01,500 事件)
- [ ] y_pol/phi_random b_phi 直方图: 与输入 rpphi 严格相等(逐事件)
- [ ] g4 端到端: `truth_b_phi` 在输出 ROOT 中等于输入 ROOT 中 `fUserDouble[1]`(逐事件)
- [ ] 物理一致性: phi_np_truth − b_phi (mod 2π) 直方图主峰在 π,与 §5 表一致

## 8. Out of scope / 推迟

- helicity / gamma_label / source_kind 透传到输出 branch(可后续补)
- `extract_phys_observables.cc` 真正使用新 truth 列(用户 working tree 中,后续单独处理)
- 输入 ROOT TBeamSimData 升级为 per-event branch(per-particle 复制是冗余但向后兼容,移除是更大改动)
- allevent (geminiout*.dat) 路径的 b_phi 处理: 当前 allevent 的 b 来自 GEMINI 行 BIMP,且这些事件本身是 QMD 内部已经平均过的 fragment 输出,反应平面 phi 概念不直接适用。**本设计仅针对 elastic dbreak 路径**;allevent 写入时 b_phi/phi_np_truth 填 NaN。
