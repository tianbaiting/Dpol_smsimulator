# NN 重建（QMD 窄窗口模型）在修正靶点后的执行说明

> **2026-04-22 更新 —— 清洁模型重跑已完成**
>
> 旧 qmd-window 模型（`formal_B115T3deg_qmdwindow/20260227_223007/`）因"靶物质污染"已作废：原训练使用的 Geant4 模拟依赖 `DeutDetectorConstruction::fSetTarget=true`，而输入是已穿靶的 IMQMD 次级粒子 —— Sn 靶被 Geant4 二次散射，训练数据被污染。详见 `docs/audit/2026-04-20-nn_target_contamination_audit.md`。清洁版重训结果见 `data/nn_target_momentum/formal_B115T3deg/20260420_184345/`，采样不变（R=100、pz [560,680]）。
>
> | Val RMSE (MeV/c) | 污染旧版 | 清洁新版 | 改善 |
> |------|---------|---------|------|
> | px   | 13.60   | **6.70**| −51% |
> | py   | 12.44   | **2.40**| −81% |
> | pz   | 5.89    | 5.80    | −2%  |
> | \|p\| | 5.88    | 6.08    | +3% (pz 主导) |
>
> 横向动量 px/py 在验证集上大幅改善，符合物理直觉（Sn 靶多次散射主要扰动横向分量）。
>
> **本次重跑**：用清洁模型对 `data/simulation/g4output/eval_20260227_235751/` 下原有 156 个 sim 文件重跑 reconstruct_target_momentum（不重跑 Geant4），输出到新目录 `data/reconstruction/eval_20260422_cleanmodel/`，再跑 `evaluate_reconstruct_sn_nn` 全量重算 RMSE。详细结果见第 4 节。

## 1. 本次采用的模型与几何
- **最新可用清洁模型（2026-04-20 重训）：**
  - `data/nn_target_momentum/formal_B115T3deg/20260420_184345/model/model.pt`
  - `data/nn_target_momentum/formal_B115T3deg/20260420_184345/model/model_meta.json`
  - ~~`data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/`（已删除：污染）~~
- 几何（已写入正确靶点位置和角度）：
  - `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
  - 靶点参数：
    - `/samurai/geometry/Target/Position -1.2488 0.0009 -106.9458 cm`
    - `/samurai/geometry/Target/Angle 3.0 deg`
  - **`SetTarget false`** —— 关键修复，在清洁数据流下靶物质必须关闭。

## 2. 数据流程（Data Flow）
1. 复用已有的 Geant4 输入（QMD 转换结果）：
   - `data/simulation/g4input/y_pol/phi_random/...`
   - `data/simulation/g4input/z_pol/b_discrete/...`
2. 运行脚本：
   - `scripts/analysis/run_sn124_nn_reco_pipeline.sh`
3. 脚本为每个输入 root 生成 Geant4 宏：
   - `data/simulation/g4output/eval_20260227_235751/_macros/*.mac`
4. 每个宏执行目标几何：
   - `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac`
5. 输出 Geant4 模拟与重建结果，并执行评估。

## 3. 写入位置（写的东西分别在哪里）
- Geant4 输出（复用 2026-02 的那批）：
  - `data/simulation/g4output/eval_20260227_235751/`
- 重建输出：
  - 旧污染模型 → `data/reconstruction/eval_20260227_235751/`（保留以作对照，**数字作废**）
  - 清洁模型（2026-04-22 重跑）→ `data/reconstruction/eval_20260422_cleanmodel/`
- 生成的 Geant4 运行宏：
  - `data/simulation/g4output/eval_20260227_235751/_macros/`
- 文档 beamer：
  - `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg.tex`
  - `docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg.pdf`

## 4. 评估结果（清洁模型，全文件 + 全事件，beamOn=0）

评估命令：
- `build/bin/evaluate_reconstruct_sn_nn --input-dir data/reconstruction/eval_20260422_cleanmodel`
- `build/bin/evaluate_reconstruct_sn_nn --input-dir data/reconstruction/eval_20260422_cleanmodel/y_pol`
- `build/bin/evaluate_reconstruct_sn_nn --input-dir data/reconstruction/eval_20260422_cleanmodel/z_pol`

模型：`data/nn_target_momentum/formal_B115T3deg/20260420_184345/model/model_cpp.json`（清洁版，2026-04-22 导出）。

### Overall（全部 156 文件）
- events_total = 5,413,747；matched_events = 3,632,185
- reco_efficiency_vs_truth = **67.092%**

| 分量 | MAE | RMSE | Bias | P95 \|err\| |
|---|---:|---:|---:|---:|
| px  | 11.670 | **18.328** | −2.016 | 29.310 |
| py  |  8.553 | **11.170** |  0.091 | 21.507 |
| pz  |  9.719 | **16.072** | −6.336 | 18.234 |
| \|p\| |  9.210 | **13.021** | −6.649 | 17.677 |

### Y-pol（16 文件，`y_pol/phi_random/d+Pb208*`）
- events_total = 5,178,328；matched_events = 3,463,201
- reco_efficiency_vs_truth = **66.879%**

| 分量 | MAE | RMSE | Bias | P95 \|err\| |
|---|---:|---:|---:|---:|
| px  | 11.236 | **17.403** | −1.425 | 27.710 |
| py  |  8.522 | **11.141** |  0.050 | 21.423 |
| pz  |  9.308 | **15.073** | −6.928 | 17.446 |
| \|p\| |  8.916 | **12.296** | −7.119 | 17.033 |

### Z-pol（140 文件，`z_pol/b_discrete/d+Sn124*`）
- events_total = 235,419；matched_events = 168,984
- reco_efficiency_vs_truth = **71.780%**

| 分量 | MAE | RMSE | Bias | P95 \|err\| |
|---|---:|---:|---:|---:|
| px  | 20.545 | **31.828** | −14.125 | 75.460 |
| py  |  9.194 | **11.746** |   0.932 | 23.047 |
| pz  | 18.152 | **29.937** |   5.786 | 72.763 |
| \|p\| | 15.241 | **23.356** |   2.997 | 56.147 |

### 清洁 vs 污染（RMSE, MeV/c）

| 集合 | 量 | 污染旧 | 清洁新 | Δ |
|---|---|---:|---:|---:|
| Overall | px | 38.73 | **18.33** | **−53%** |
| Overall | py | 11.29 | 11.17 | −1% |
| Overall | pz | 12.00 | 16.07 | +34% |
| Overall | \|p\| |  8.43 | 13.02 | +54% |
| Y-pol   | px | 37.88 | **17.40** | **−54%** |
| Y-pol   | py | 11.26 | 11.14 | −1% |
| Y-pol   | pz | 11.11 | 15.07 | +36% |
| Y-pol   | \|p\| |  7.90 | 12.30 | +56% |
| Z-pol   | px | 53.15 | **31.83** | **−40%** |
| Z-pol   | py | 11.87 | 11.75 | −1% |
| Z-pol   | pz | 23.81 | 29.94 | +26% |
| Z-pol   | \|p\| | 15.73 | 23.36 | +49% |

## 5. 重建效果结论

- **匹配效率无变化**（67.09% overall / 66.88% y-pol / 71.78% z-pol），与污染模型 bit-for-bit 一致 —— 匹配逻辑只看 `reco_proton_*` 是否存在，不看数值，故模型替换不影响效率。
- **px RMSE 全面下降 40–54%**：y-pol 37.9 → 17.4、z-pol 53.2 → 31.8、overall 38.7 → 18.3 MeV/c。与验证集上的 px −51% 趋势一致。py 几乎没变（本身很小，~11 MeV/c），说明污染主要扭曲 px。
- **pz 和 \|p\| RMSE 上升 26–56%**：overall pz 12.0 → 16.1、\|p\| 8.4 → 13.0 MeV/c。与验证集趋势（pz −2%、\|p\| +3%）相反。原因应该是全量 eval 中有大量 **pz 落在训练窗口 [560, 680] MeV/c 之外**的事件（truth 输出可见 pz 低至 82 MeV/c 或高至 370 MeV/c 的离群点）：污染模型在这些域外样本上"偶然好"，清洁模型严格贴合训练分布，反而显得 RMSE 更大。
- **Bias 读到了一个明显的 pz 系统偏**：overall pz bias = −6.34 MeV/c、\|p\| bias = −6.65 MeV/c，y-pol 更极端（pz bias = −6.93、\|p\| bias = −7.12）。z-pol 的 px bias = −14.1 MeV/c 也很大。这些偏量和 pz 的域外分布耦合，需要下一步用 truth pz 分 bin 看残差谱确认。
- **py 基本干净**：bias < 1 MeV/c，RMSE 也最小，三种情形都在 11–12 MeV/c —— NN 对 py 学得最好。
- **下一步**：(a) 按 truth pz 分 bin 重做 RMSE，分离"训练分布内"与"OOD"两段结论；(b) 检查是否应把训练 pz 窗口拓宽到 [400, 720] 涵盖更多 IMQMD 动量谱；(c) 调 `run_target_momentum_reco_pipeline.sh` 里 `MODEL_PT/MODEL_META/MODEL_JSON` 默认值指向清洁模型。
