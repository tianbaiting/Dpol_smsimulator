# dbreak elastic g4 pipeline (ypol+zpol Sn112/Sn124) design

Date: 2026-04-30

## Goal

把本地 `data/qmdrawdata/qmdrawdata/` 里的 dbreak 弹性散射数据(ypol + zpol、Sn112/Sn124、γ ∈ {0.50, 0.60, 0.70, 0.80}),经过 virtual-breakup cut,过 GenInputRoot_qmdrawdata 转成 g4input ROOT,再用 sim_deuteron 跑出 g4output ROOT。本次只覆盖弹性 (`--source elastic`) 阶段;allevent 后续再做。

**核心前置:** 主仓库刚合并 `feature/qmd-g4-io-branches` (commits `de18e38`/`2c45f4b`/`ce2f265`),GenInputRoot 现在写 `b_phi`/`phi_np_truth` 进 input ROOT、`EventTruthConverter` 在 g4 output 写 `truth_bimp/truth_b_phi/truth_phi_np` per-event scalar。zpol 用 `--randomize-zpol on` 每事件 uniform[0, 2π) 随机旋转(不再"对齐"); ypol 用 `--randomize-ypol off`,因为 phi_random/20260413ypol 数据已带 rpphi 列,GenInputRoot 自动嗅探。**reco 验证依赖这两端 branch 同事件对应**: input ROOT 第 0 号粒子 `fUserDouble[1] == b_phi`,output ROOT `truth_b_phi` 来自该输入,事件序号一一对应。

数据在远端 spana03 (`/home/tbt/workspace/Dpol_smsimulator`) 跑,大文件用 rsync 推上去。**脚本执行架构 (revised 2026-04-30):** Python driver 设计为**在远端运行**(本地 SSH 进入 spana03 后直接 `python3 scripts/batch/...`),所有 stage 直接 subprocess 调用,不绕 inner ssh wrapping。本地→远端的数据推送由独立的小 bash helper `scripts/batch/sync_dbreak_to_spana.sh` 完成,从本地手动触发。代码通过 `git remote spana03` push 到远端 repo。

## Non-goals

- 不动远端 C++ 代码或重编译 GenInputRoot_qmdrawdata / sim_deuteron。
- 不引入新的 G4 macro 模板,直接复用 `scripts/simulation/run_g4input_batch_parallel.sh` 的宏生成逻辑。
- 不处理 allevent / GEMINI 通道。
- 不改 `data/qmdrawdata/qmdrawdata/z_pol/b_discrete/` 里已经在远端的原始数据。

## Inputs

| 数据集 | 来源 | 备注 |
|---|---|---|
| ypol Sn112/Sn124 elastic dbreak | 本地 `data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn{112,124}E190/d+Sn{112,124}E190g{050,060,070,080}{ynp,ypn}-RP360/dbreak.dat` | 单文件,含 `b` 与 `rpphi` 列(已经按 phi 随机化) |
| zpol Sn112/Sn124 elastic dbreak | 远端已存在 `data/qmdrawdata/qmdrawdata/z_pol/b_discrete/d+Sn{112,124}E190g{050,060,070,080}{znp,zpn}/dbreakb{01..10}.dat` | 按 b 切片,无 rpphi,需要 `--randomize-zpol on` |
| GenInputRoot 二进制 | 远端 `build/bin/GenInputRoot_qmdrawdata` | 现成,不重编译 |
| sim_deuteron 二进制 | 远端 `build/bin/sim_deuteron` | 现成 |
| 几何宏 | 远端 `configs/simulation/geometry/3deg_1.15T.mac` | 与 ips_position_scan / polarization_validation 一致 |

## 关键约束 / 实现技巧

GenInputRoot_qmdrawdata 的 elastic 分支只在 `<input_base>/y_pol/phi_random/` 和 `<input_base>/z_pol/b_discrete/` 两个**硬编码子树**下找 `dbreak*.dat`,且 polarization 检测靠路径里的 `y_pol` / `-ypol` / `z_pol` / `-zpol` 子串。20260413ypol 路径都不匹配。**绕开方式**:把 20260413ypol 数据保留在原结构,在远端建立**文件级软链接**指进 `y_pol/phi_random/<dir>/dbreak.dat`。`recursive_directory_iterator` 默认不进目录软链接但会把文件软链接当 regular file 收集,正好满足。

## Architecture

**两个工件:**

1. `scripts/batch/sync_dbreak_to_spana.sh` —— **本地** bash helper,只做 rsync 推数据。架构最干净,跨机器边界明确。
2. `scripts/batch/run_dbreak_elastic_pipeline.py` —— **远端** Python driver。在 spana03 上 `python3` 直接跑;每个 stage `subprocess.run(...)` 调本地命令(因为已经在 spana03)。四个子命令 + `all` + `status`:

```
prebuild   git pull + bash build.sh, 验证 libsmdata.so 含 EventTruthConverter
prepare    建 file symlink y_pol/phi_random/<dir>/dbreak.dat → 20260413ypol/...
gen-input  跑 GenInputRoot_qmdrawdata 4 次 (ypol-Sn112/124、zpol-Sn112/124)
gen-output 跑 run_g4input_batch_parallel.sh,JOBS=8,只针对 Sn112/Sn124 + 4 个 γ
all        以上四步串行
status     读状态文件 + g4output 文件数,打印进度
```

**用户工作流:**

```bash
# (local) 推数据
bash scripts/batch/sync_dbreak_to_spana.sh

# (local) 推代码 (代码已在 feature 分支)
git push spana03 feature/dbreak-elastic-pipeline

# (ssh spana03) 切分支,跑 pipeline
ssh spana03
cd /home/tbt/workspace/Dpol_smsimulator
git checkout feature/dbreak-elastic-pipeline
python3 scripts/batch/run_dbreak_elastic_pipeline.py all
```

**为什么需要 prebuild:** 远端 git HEAD 已在 `42f9130`,但 `build/` 还是合并之前的产物 (`build/lib/libsmdata.so` 没有 `EventTruthConverter` 符号);若不重 build,跑出来的 g4output 不会有 truth_b_phi 等 branch,reco 验证就废了。

driver 顶部唯一 `CONFIG` 字典集中所有路径/物理参数/JOBS,审计参数只看一处。

## CONFIG (driver 顶部)

driver 在远端运行,`remote_smsim_dir` 是当前工作目录(本机视角)。`local_smsim_dir` 不是 driver 直接用的,但由 `sync_dbreak_to_spana.sh` 引用。

```python
CONFIG = {
    "local_smsim_dir":  "/home/tian/workspace/dpol/smsimulator5.5",
    "remote_host":      "spana03",
    "remote_smsim_dir": "/home/tbt/workspace/Dpol_smsimulator",
    "git_remote":       "origin",
    "git_branch":       "feature/dbreak-elastic-pipeline",  # 本 feature
    "build_cmd":        "bash build.sh",   # 或者 cmake --build build -j8

    "isotopes": ["Sn112", "Sn124"],
    "gammas":   ["g050", "g060", "g070", "g080"],
    "energy":   "E190",
    "ypol_directions": ["ynp", "ypn"],
    "zpol_directions": ["znp", "zpn"],

    "geninput_bin":     "build/bin/GenInputRoot_qmdrawdata",
    "cut_unphysical":   "on",
    "cut_axis_limit":   150.0,
    "randomize_ypol":   "off",
    "randomize_zpol":   "on",
    "rotation_seed":    0,

    "geom_mac":         "configs/simulation/geometry/3deg_1.15T.mac",
    "g4_jobs":          8,
    "beam_on":          0,
    "tag":              "elastic",

    "ypol_source_dir":  "data/qmdrawdata/qmdrawdata/20260413ypol",
    "ypol_link_target": "data/qmdrawdata/qmdrawdata/y_pol/phi_random",
    "zpol_input_dir":   "data/qmdrawdata/qmdrawdata/z_pol/b_discrete",
    "g4input_base":     "data/simulation/g4input",
    "g4output_base":    "data/simulation/g4output",

    "state_dir":        "logs/dbreak_elastic_pipeline",
    "macro_dir":        "scripts/simulation/_macros",
}
```

## Branch 契约 (input ROOT ↔ output ROOT)

| 信息 | input ROOT (g4input) | output ROOT (g4output) | 单位 |
|---|---|---|---|
| 碰撞参数模 | `TBeamSimData.fUserDouble[0]` (per-particle, 同事件值一致) | `truth_bimp/D` (per-event scalar) | fm |
| 反应平面方位 b_phi | `TBeamSimData.fUserDouble[1]` | `truth_b_phi/D` | rad ∈ [0, 2π) |
| np 总动量 truth phi | `TBeamSimData.fUserDouble[2]` | `truth_phi_np/D` | rad ∈ (-π, π] |
| EventTruthConverter 行为 | — | 每事件从 `gBeamSimDataArray->at(0).fUserDouble[0..2]` 读 → 写 truth_* | — |
| 事件序号对应 | g4input tree entry i | g4output tree entry i | i 一一对应,sim_deuteron 顺序读 input tree |

### GenInputRoot b_phi 计算约定 (要点)

```
b_phi = wrap_two_pi(b_phi_raw + Δ)
```

| pol | b_phi_raw 来源 | Δ (旋转角) | 净 b_phi |
|---|---|---|---|
| **ypol (phi_random / 20260413ypol)** | raw QMD 第 9 列 `rpphi` (°) → rad | 0 (`--randomize-ypol off`) | 等于 raw rpphi 的弧度值 |
| **zpol (b_discrete)** | 0 (无 rpphi 列) | uniform[0, 2π) (`--randomize-zpol on`) | **= Δ,即施加的随机旋转角本身** |
| ypol 旧 7 列数据 (phi_fixed) | 0 | 0 | 0 (语义一致) |

**关键不变量:** 每个 zpol 事件被旋转的角度 Δ 同时是它被 stamp 进 input ROOT 的 `b_phi`,也是 output ROOT 的 `truth_b_phi`。下游 reco 重建出的 `phi_RP_reco` 直接对照 `truth_b_phi`(吸引散射 expected `phi_RP_reco ≈ truth_b_phi + π mod 2π`)。

reco 验证时,用 g4output 自己的 `truth_b_phi` 即可;如要 cross-check,对同一文件名的 input/output 各取 entry i 比对 `fUserDouble[1] == truth_b_phi`。

## 数据流 / 目录布局

```
本地
└── data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn{112,124}E190/.../dbreak.dat
       │  STAGE 1 (本地 bash helper): sync_dbreak_to_spana.sh
       │      rsync -avz, 只同步 dbreak.dat
       ▼
远端
├── data/qmdrawdata/qmdrawdata/20260413ypol/...                                (real, rsync 落点)
├── data/qmdrawdata/qmdrawdata/y_pol/phi_random/d+Sn{112,124}E190g{XXX}{ynp,ypn}/dbreak.dat
│      → ../../20260413ypol/d+Sn{112,124}E190/d+Sn{112,124}E190g{XXX}{ynp,ypn}-RP360/dbreak.dat   (STAGE 2 file symlink)
└── data/qmdrawdata/qmdrawdata/z_pol/b_discrete/...                            (已存在)

       │  STAGE 3: gen-input (4 次 GenInputRoot,分 ypol/zpol × Sn112/Sn124)
       ▼
data/simulation/g4input/
├── y_pol/phi_random/d+Sn{112,124}E190g{050,060,070,080}{ynp,ypn}/dbreak.root  (16 文件)
└── z_pol/b_discrete/d+Sn{112,124}E190g{050,060,070,080}{znp,zpn}/dbreakb{01..10}.root  (160 文件)

       │  STAGE 4: gen-output (run_g4input_batch_parallel.sh, JOBS=8)
       ▼
data/simulation/g4output/
└── 镜像 g4input 子树, 每个 .root 旁伴一个 .log
```

总量 176 个 G4 模拟任务。zpol 每个 dbreakbXX 经 b/b_max 比例缩减事件数;ypol 每个文件 ~10⁶ events 是上界,被 virtual-breakup cut (|Δp_y| < 150 MeV/c) 后会少一部分。

## 子命令接口

driver 在远端运行,无 sync 子命令(sync 由独立 bash helper 处理)。

```
# 远端
python3 scripts/batch/run_dbreak_elastic_pipeline.py \
    {prebuild|prepare|gen-input|gen-output|all|status} \
    [--dry-run] [-v] [--force] [--only ypol|zpol] [--isotope Sn112|Sn124]

# 本地
bash scripts/batch/sync_dbreak_to_spana.sh [--dry-run]
```

### `sync_dbreak_to_spana.sh` (本地 bash helper)
本地直接 `rsync -avz --info=progress2 --include='*/' --include='dbreak.dat' --exclude='*'` 推 `data/qmdrawdata/qmdrawdata/20260413ypol/` 到远端同路径。10 行左右,无状态文件,人工触发。

### `prebuild`
在远端:
```bash
git fetch origin && git checkout <feature-branch> && git pull --ff-only && \
  source setup.sh && bash build.sh 2>&1 | tail -50
```
完成后校验 `nm -D build/lib/libsmdata.so | grep EventTruthConverter` 必须非空,否则报错并打印 build 末尾 50 行。写 `state_dir/prebuild.done` JSON `{git_head, build_timestamp, has_event_truth}`。`--force` 时先 `git reset --hard <remote>/<branch>` 再 build (慎用)。

### `prepare`
在远端,**只为 CONFIG.gammas 列出的 γ** 遍历 `20260413ypol/d+Sn{112,124}E190/d+Sn{112,124}E190g{050,060,070,080}{ynp,ypn}-RP360/dbreak.dat` (16 条),对每个建立 file symlink:
```
y_pol/phi_random/d+Sn{ISO}E190g{XXX}{ynp,ypn}/dbreak.dat
  → ../../20260413ypol/d+Sn{ISO}E190/d+Sn{ISO}E190g{XXX}{ynp,ypn}-RP360/dbreak.dat
```
存在且指向正确则跳过。完成写 `state_dir/prepare.done` + 软链清单 JSON。

### `gen-input`
在远端,分 4 次调用 GenInputRoot_qmdrawdata:
```bash
source setup.sh && \
  build/bin/GenInputRoot_qmdrawdata \
    --mode {ypol|zpol} --source elastic \
    --target-filter {Sn112|Sn124} \
    --cut-unphysical on \
    --cut-ypol-axis-limit 150 --cut-zpol-axis-limit 150 \
    --randomize-ypol off --randomize-zpol on \
    --rotation-seed 0
```
每次 stdout/err 落到 `state_dir/geninput_{ypol|zpol}_Sn{112|124}.log`。`--isotope` / `--only` 时只跑对应组合。

### `gen-output`
在远端。为了让 `run_g4input_batch_parallel.sh` 只跑目标 isotope/gamma,driver 在调用前在 `state_dir/g4input_filtered_<ypol|zpol>/` 下用文件级软链接搭一个**只含目标 root** 的子树,把它作为 `INPUT_ROOT` 传给 batch 脚本。这样不动现有 batch 脚本。

```bash
INPUT_ROOT=<state_dir>/g4input_filtered_ypol \
OUTPUT_BASE=data/simulation/g4output/y_pol/phi_random \
GEOM_MAC=configs/simulation/geometry/3deg_1.15T.mac \
MACRO_DIR=scripts/simulation/_macros \
JOBS=8 PATTERN='dbreak.root' TAG=ypol_Sn_elastic \
bash scripts/simulation/run_g4input_batch_parallel.sh
```
zpol 同理,`PATTERN='dbreakb*.root'`、INPUT_ROOT 指向 zpol filtered 子树。

### `all`
依次跑 `prebuild → prepare → gen-input → gen-output` (sync 不在 driver 里;由 `sync_dbreak_to_spana.sh` 在本地预先完成),任一失败终止并打印断点。

### `status`
不联网,读 `state_dir/*.done` 和 g4output 下文件数,打印 4 阶段进度。

## 错误处理 / 重跑

driver 在远端跑,所以下表"出错"指 driver 内部或它调起的子进程。

| 触发 | 行为 |
|---|---|
| 本地 sync_dbreak_to_spana.sh: ssh 连不上 / rsync 失败 | rsync 自身报错;用户手动重试 |
| driver 在远端找不到 setup.sh / 不可执行的二进制 | 退出,打印 stderr 末 30 行 |
| 远端 git 不在 feature 分支 / 有未提交改动 | prebuild 拒绝跑,提示用户先理顺;`--force` 时 `git reset --hard <remote>/<branch>` |
| 远端 build.sh 失败 | 打印末 50 行 stderr,prebuild.done 不写 |
| build 完后 libsmdata.so 仍无 EventTruthConverter | 报错,提示远端代码版本不对 |
| GenInputRoot 跑完但 g4input ROOT 数 = 0 | 报错,不进 gen-output |
| g4input ROOT 的 TBeamSimData[0] `fUserDouble.size() < 3` | 报错(input 元数据没 stamp),停止;说明 GenInputRoot 是老二进制 |
| 单个 sim_deuteron 失败 | batch 脚本留 macro/log;driver 收尾扫 [FAIL] 汇总 |
| Ctrl-C | 已写 `*.done` 阶段不重做,未完成阶段下次接着 |

幂等性:
- **sync** rsync 自然幂等 (mtime+size);`--force` 加 `-c` 校验 checksum
- **prepare** 用 `ln -snf` 覆盖 symlink;指向正确则跳过
- **gen-input** GenInputRoot 是 `RECREATE`;默认只在目标无 .root 时跑;`--force` 强制
- **gen-output** batch 脚本已有 skip-if-exists (输出 > 100KB);`--force` 时先 `rm -f` 目标子树

## 状态文件与日志

```
远端 logs/dbreak_elastic_pipeline/
├── prebuild.done                       # JSON {git_head, build_timestamp, has_event_truth: true}
├── prebuild.log                        # build.sh stdout/err 末 200 行
├── prepare.done                        # JSON 软链清单
├── geninput_{ypol,zpol}_Sn{112,124}.log
├── geninput.done                       # JSON {ypol_files, zpol_files, total_events}
├── gen_output_{ypol,zpol}.log
├── gen_output.done                     # JSON {planned, ok, fail, skipped}
├── g4input_filtered_ypol/              # symlink 子树,batch 脚本的 INPUT_ROOT
├── g4input_filtered_zpol/
└── pipeline.summary                    # status 子命令的人类可读汇总
```

driver 本地 stdout 只输出单行进度 + `[INFO]/[WARN]/[ERROR]` 决策点;详细日志全在远端。

## 验证策略

按从小到大独立验证:

1. **prebuild 自检** —— `nm -D build/lib/libsmdata.so | grep EventTruthConverter` 必须非空;远端 `build/bin/sim_deuteron` mtime 在 git pull 之后
2. `sync_dbreak_to_spana.sh --dry-run` (本地) 看 rsync 文件列表,确认只匹配 dbreak.dat、目标路径正确
3. `prepare --dry-run` (远端) 看打印的 16 条 symlink 计划
4. **smoke (1 file ypol)**:CONFIG 临时改成 `isotopes=["Sn112"]`、`gammas=["g050"]`、`ypol_directions=["ynp"]` → 跑 `all`,期望 1 个 ypol input + 1 个 ypol output;ROOT 打开:
   - g4input ROOT 的 `tree` 含 `TBeamSimData` 分支,第 0 号粒子 `fUserDouble.size() >= 3`、`fUserDouble[1]` ∈ [0, 2π)
   - g4output ROOT 含 **`truth_b_phi`、`truth_bimp`、`truth_phi_np`** 三个 per-event scalar 分支(D 类型),全部非 NaN
   - 同一事件 input `fUserDouble[1]` 与 output `truth_b_phi` 数值相等(浮点容差 1e-9)
5. **zpol smoke**:`Sn112`/`g050`/`zpol`/`znp` → 期望 10 个输入 + 10 个输出,b 越大输出 entries 越多 (验证 b/b_max 缩减);`truth_b_phi` 在 [0, 2π) 均匀(每文件 ≥100 事件画 hist 检查)
5b. **zpol Δ ↔ b_phi 等价校验**:对一个 zpol input ROOT 取首事件,用 `(pxp, pyp)` 反推 Δ_recovered 的方法是 — 由于原始 QMD znp 数据 b 沿 +x、Δ 把 (pxp, pyp) 旋转角度 Δ;但 raw 是几何旋转,无法从单一动量分量唯一反推。**实际等价校验**改为: 跑 GenInputRoot 时加 `--rotation-seed 12345` 复现一次,对比第 i 事件 `fUserDouble[1]` 与同 seed 重跑结果一致(可复现性证明 Δ 即 b_phi);并独立画一个 (pxp+pxn, pyp+pyn) 的 phi 分布与 `truth_b_phi+π` 模 2π 的相关图,吸引散射应在对角线附近 ±0.5 rad 散布
6. **ypol b_phi 透传**:对一个 ypol smoke 文件,确认 `truth_b_phi` 与 raw QMD 第 9 列 rpphi (°→rad) 一致(因 `randomize_ypol off`,无额外旋转)
7. **cut 对比**:`--cut-unphysical on` vs `off` 两次,on 的 g4input root entries 应明显少
8. `status` 子命令确认 4 阶段都标 done、g4output 文件数 = 176
9. 回归对比(可选):任一 zpol 输出对比现有 `polarization_validation_sn124_zpol_g060` 的 root 分支结构

## Open questions / risks

- ypol 20260413 数据 γ 上限只到 g085;如果后续要加 g090/g100,需要补数据再跑。
- zpol gen-input 阶段 GenInputRoot 没有 γ 过滤参数,会扫整个 z_pol/b_discrete/ 把所有 γ (g050..g100) 都转成 g4input ROOT。多出来的 γ 文件不进 gen-output (被 filtered 子树过滤),只是在 g4input 下多占少量磁盘。需要严格限制再加 `--target-filter Sn112E190g050` 形式的复合 filter,但要求多次调用 (8 次而非 4 次),目前不做。
- zpol 的 `randomize-zpol on` 每次跑会因 rotation seed=0 (系统时间派生) 拿到不同 phi 旋转。如果需要 reproducibility,把 `rotation_seed` 改成正整数。
- `recursive_directory_iterator` 跨平台对 file symlink 的 `is_regular_file()` 判定遵循 stat 语义,Linux 下确认成立;若以后迁移到 macOS 验证一次。
