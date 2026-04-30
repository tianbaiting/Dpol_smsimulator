# dbreak elastic g4 pipeline (ypol+zpol Sn112/Sn124) design

Date: 2026-04-30

## Goal

把本地 `data/qmdrawdata/qmdrawdata/` 里的 dbreak 弹性散射数据(ypol + zpol、Sn112/Sn124、γ ∈ {0.50, 0.60, 0.70, 0.80}),经过 virtual-breakup cut,过 GenInputRoot_qmdrawdata 转成 g4input ROOT,再用 sim_deuteron 跑出 g4output ROOT。本次只覆盖弹性 (`--source elastic`) 阶段;allevent 后续再做。

数据在远端 spana03 (`/home/tbt/workspace/Dpol_smsimulator`) 跑,大文件用 rsync 推上去;脚本执行入口在本地 (`/home/tian/workspace/dpol/smsimulator5.5`)。

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

一个 Python 驱动脚本 `scripts/batch/run_dbreak_elastic_pipeline.py`,在本地启动,通过 `ssh spana03` 调远端二进制。四个子命令 + `all` + `status`:

```
sync       本地 rsync 推 20260413ypol/Sn{112,124} 到 spana03 同路径
prepare    SSH 远端建 file symlink y_pol/phi_random/<dir>/dbreak.dat → 20260413ypol/...
gen-input  SSH 远端跑 GenInputRoot_qmdrawdata 4 次 (ypol-Sn112/124、zpol-Sn112/124)
gen-output SSH 远端跑 run_g4input_batch_parallel.sh,JOBS=8,只针对 Sn112/Sn124 + 4 个 γ
all        以上四步串行
status     不联网,读远端状态文件 + g4output 文件数,打印进度
```

driver 顶部唯一 `CONFIG` 字典集中所有路径/物理参数/JOBS,审计参数只看一处。

## CONFIG (driver 顶部)

```python
CONFIG = {
    "local_smsim_dir":  "/home/tian/workspace/dpol/smsimulator5.5",
    "remote_host":      "spana03",
    "remote_smsim_dir": "/home/tbt/workspace/Dpol_smsimulator",

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

## 数据流 / 目录布局

```
本地
└── data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn{112,124}E190/.../dbreak.dat
       │  STAGE 1: sync (rsync -avz, 只同步 dbreak.dat)
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

```
python3 scripts/batch/run_dbreak_elastic_pipeline.py \
    {sync|prepare|gen-input|gen-output|all|status} \
    [--dry-run] [-v] [--force] [--only ypol|zpol] [--isotope Sn112|Sn124]
```

### `sync`
本地 `rsync -avz --info=progress2` 把 `20260413ypol/d+Sn{112,124}E190/` 推到远端同路径。`--include='*/' --include='dbreak.dat' --exclude='*'` 只同步 dbreak.dat (不要 geminiout/reactiontype)。结束写 `state_dir/sync.done`。

### `prepare`
SSH 远端,**只为 CONFIG.gammas 列出的 γ** 遍历 `20260413ypol/d+Sn{112,124}E190/d+Sn{112,124}E190g{050,060,070,080}{ynp,ypn}-RP360/dbreak.dat` (16 条),对每个建立 file symlink:
```
y_pol/phi_random/d+Sn{ISO}E190g{XXX}{ynp,ypn}/dbreak.dat
  → ../../20260413ypol/d+Sn{ISO}E190/d+Sn{ISO}E190g{XXX}{ynp,ypn}-RP360/dbreak.dat
```
存在且指向正确则跳过。完成写 `state_dir/prepare.done` + 软链清单 JSON。

### `gen-input`
SSH 远端,分 4 次调用 GenInputRoot_qmdrawdata:
```bash
source setup_spana.sh && \
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
为了让 `run_g4input_batch_parallel.sh` 只跑目标 isotope/gamma,driver 在调用前在 `state_dir/g4input_filtered_<ypol|zpol>/` 下用文件级软链接搭一个**只含目标 root** 的子树,把它作为 `INPUT_ROOT` 传给 batch 脚本。这样不动现有 batch 脚本。

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
依次跑 `sync → prepare → gen-input → gen-output`,任一失败终止并打印断点。

### `status`
不联网,读 `state_dir/*.done` 和 g4output 下文件数,打印 4 阶段进度。

## 错误处理 / 重跑

| 触发 | 行为 |
|---|---|
| SSH 连不上 spana03 | 立刻报错,提示检查 ~/.ssh/config |
| `setup_spana.sh` 后找不到二进制 | 退出,打印 stderr 末 30 行 |
| GenInputRoot 跑完但 g4input ROOT 数 = 0 | 报错,不进 gen-output |
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
├── sync.done                           # JSON {timestamp, files_count, bytes}
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

1. `sync --dry-run` 看 rsync 文件列表,确认只匹配 dbreak.dat、目标路径正确
2. `prepare --dry-run` 看打印的 16 条 symlink 计划
3. **smoke (1 file)**:CONFIG 临时改成 `isotopes=["Sn112"]`、`gammas=["g050"]`、`ypol_directions=["ynp"]` → 跑 `all`,期望 1 个 ypol input + 1 个 ypol output;ROOT 打开看 FragSimData / NEBULAPla 分支非空
4. **zpol smoke**:`Sn112`/`g050`/`zpol`/`znp` → 期望 10 个输入 + 10 个输出,b 越大输出 entries 越多 (验证 b/b_max 缩减)
5. **cut 对比**:`--cut-unphysical on` vs `off` 两次,on 的 g4input root entries 应明显少
6. `status` 子命令确认 4 阶段都标 done、g4output 文件数 = 176
7. 回归对比(可选):任一 zpol 输出对比现有 `polarization_validation_sn124_zpol_g060` 的 root 分支结构

## Open questions / risks

- ypol 20260413 数据 γ 上限只到 g085;如果后续要加 g090/g100,需要补数据再跑。
- zpol gen-input 阶段 GenInputRoot 没有 γ 过滤参数,会扫整个 z_pol/b_discrete/ 把所有 γ (g050..g100) 都转成 g4input ROOT。多出来的 γ 文件不进 gen-output (被 filtered 子树过滤),只是在 g4input 下多占少量磁盘。需要严格限制再加 `--target-filter Sn112E190g050` 形式的复合 filter,但要求多次调用 (8 次而非 4 次),目前不做。
- zpol 的 `randomize-zpol on` 每次跑会因 rotation seed=0 (系统时间派生) 拿到不同 phi 旋转。如果需要 reproducibility,把 `rotation_seed` 改成正整数。
- `recursive_directory_iterator` 跨平台对 file symlink 的 `is_regular_file()` 判定遵循 stat 语义,Linux 下确认成立;若以后迁移到 macOS 验证一次。
