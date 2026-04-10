# 190 MeV/u 极化氘核散射实验的计数率、事件率与靶设计说明

本文件给出当前可复算的 planning 版本，用于回答四个问题：

1. 束流、反应、触发与探测器 rate 应该分别怎么定义。
2. 现阶段应该以 `ImQMD`、`Geant4` 还是外部数据库为主。
3. 现有草稿中 rate 公式的数值不一致来自哪里。
4. 在每种同位素各 `1.2 g` 的库存约束下，`Sn112` / `Sn124` 靶应该做多厚、多大。

本说明的计算脚本与结果文件在：

- [calc_rate_target_design_190MeVu.py](/home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/calc_rate_target_design_190MeVu.py)
- [run_polarization_validation_sn124_zpol_g060.sh](/home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/run_polarization_validation_sn124_zpol_g060.sh)
- [validate_polarization_tex_from_data.py](/home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/validate_polarization_tex_from_data.py)
- [inputs.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/inputs.json)
- [rates.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/rates.csv)
- [rates.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/rates.json)
- [target_design.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/target_design.csv)
- [inventory_backsolve.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/inventory_backsolve.csv)
- [rate_validation_comparison.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/rate_validation_comparison.csv)
- [rate_validation_comparison.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/rate_validation_comparison.json)
- [external_database_cross_section_comparison.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/external_database_cross_section_comparison.csv)
- [external_database_cross_section_comparison.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/external_database_cross_section_comparison.json)
- [validation_summary.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/validation_summary.json)
- [generator_selection_audit.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/generator_selection_audit.csv)
- [g4_validation_counts.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/g4_validation_counts.csv)
- [fresh_vs_archive_compare.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/fresh_vs_archive_compare.csv)
- [imqmd_cross_section_estimates.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/imqmd_cross_section_estimates.csv)
- [imqmd_pb_profile.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/imqmd_pb_profile.csv)
- [imqmd_breakup_probability_and_cumulative_sigma.png](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/imqmd_breakup_probability_and_cumulative_sigma.png)
- [run_manifest.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/run_manifest.json)
- [README_sources.md](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/README_sources.md)

## 1. 先给结论

对于这个实验，三类数据源的职责应当明确分开：

| 任务 | 主数据源 | 本文处理方式 |
| --- | --- | --- |
| 反应产额、末态相空间、目标核依赖 | `ImQMD` | 作为物理输入主线 |
| 探测器响应、阈值、几何、触发与 coincidence 效率 | `Geant4` | 作为探测链主线 |
| 文献校核、能损和公共实验数据 | `EXFOR`、`ATIMA`、Geant4 官方文档 | 只做交叉检查，不替代主结果 |

因此，这份 planning 文档的主结论是：

- 物理产额不能只用 `Geant4` 算，因为 `Geant4` 不应替代你的反应模型。
- 探测器 rate 也不能只用 `ImQMD` 算，因为 `ImQMD` 不给你真实探测效率、阈值、死区和触发。
- 当前最合理的链路是：`ImQMD -> Geant4 -> rate / trigger / DAQ`。

在当前仓库里，最完整且可直接落数的 planning anchor 仍然是 `Sn124` 这一组本地结果：

- 束流强度：`I = 1.0e7 pps`
- planning 反应截面：`sigma = 550 mb`
- planning coincidence 效率：`eta_coin = 4.65%`
- SRC 束团结构：`h = 6`，`R_ext = 5.36 m`，`T = 190 MeV/u`

在这组输入下：

- `3 mm` 参考厚度时，反应率约 `58.6 kHz`
- `3 mm` 参考厚度时，full-chain detected coincidence rate 约 `2.72 kHz`
- `16 h` 机时下，detected coincidence 总数约 `1.57e8`
- 束团间隔约 `33.62 ns`
- 每个束团平均入射氘核数约 `0.3362`
- `3 mm` 参考厚度下，每束团平均 detected coincidence 数约 `9.16e-5`

这些数字和旧草稿中的 `1.6e8` 总事件量级一致，但公式写法需要重写。

本次新增的本地复核还给出三条直接结论：

- 按当前仓库里实际可复算的链路，`Sn124 zpol g060` 在 `b<=9 fm`、`b/bmax` 前缀抽样、再经过 fresh Geant4 后，得到的是 `8408 / 22174 = 37.92%` 的 selected-breakup coincidence efficiency，而不是 `4.65%`。
- 这一 fresh 结果和归档 `eval_20260227_235751` 的同口径结果 `8414 / 22174 = 37.95%` 一致，差值只有 `-2.71e-4`。
- 因此，仓库内现有数据可以稳定复核的是“当前 selected-breakup 样本的探测 acceptance”，不能直接独立证明 `polarization.tex` 里那条 `eta_det = 4.65%`。
- 公开数据库与文献能找到的是同体系低能区 `d + 112Sn / 124Sn / 208Pb` 的总反应截面 `sigma_R`，以及若干 `Sn/Pb` 重靶的 breakup 文献锚点；但当前没有找到可以直接替代 `190 MeV/u` breakup 积分截面的同口径公共条目。

## 2. 数据从哪里来

### 2.1 本地仓库中的直接输入

| 数据 | 数值 | 来源 |
| --- | --- | --- |
| 束流强度 | `1.0e7 pps` | [polarization.tex](/home/tian/workspace/dpol/smsimulator5.5/docs/reports/qmdrawdataAna/article/arxiv/polarization.tex#L340) |
| 监测器 30 min 计数规模 | `1.0e5` at `theta_1` | [polarization.tex](/home/tian/workspace/dpol/smsimulator5.5/docs/reports/qmdrawdataAna/article/arxiv/polarization.tex#L368) |
| planning breakup 截面 | `550 mb` | [polarization.tex](/home/tian/workspace/dpol/smsimulator5.5/docs/reports/qmdrawdataAna/article/arxiv/polarization.tex#L458) |
| overall detected efficiency | `4.65%` | [polarization.tex](/home/tian/workspace/dpol/smsimulator5.5/docs/reports/qmdrawdataAna/article/arxiv/polarization.tex#L462) |
| 原稿中的 number density | `3.685e22 cm^-3` | [polarization.tex](/home/tian/workspace/dpol/smsimulator5.5/docs/reports/qmdrawdataAna/article/arxiv/polarization.tex#L464) |
| 原稿中的 compact coefficient | `3.4 (d/mm)(I/pps)(t/h)` | [polarization.tex](/home/tian/workspace/dpol/smsimulator5.5/docs/reports/qmdrawdataAna/article/arxiv/polarization.tex#L467) |
| 当前主几何 | `3deg_1.15T` | [3deg_1.15T.mac](/home/tian/workspace/dpol/smsimulator5.5/configs/simulation/geometry/3deg_1.15T.mac#L10) |
| 3deg 几何内的 target placeholder | `80 x 80 x 30 mm`, `SetTarget false` | [3deg_1.15T.mac](/home/tian/workspace/dpol/smsimulator5.5/configs/simulation/geometry/3deg_1.15T.mac#L15) |
| legacy acceptance split | `PDC 30.09%`, `NEBULA 85.50%`, `Coincidence 30.00%` at `1.2T, 5deg` | [acceptance_report.txt](/home/tian/workspace/dpol/smsimulator5.5/results/geo_acceptence_results/results/1.2T/acceptance_report.txt#L15) |
| QMD 原始数据靶种覆盖 | `zpol: Pb208 Sn112 Sn124 Xe140` | [readme.txt](/home/tian/workspace/dpol/smsimulator5.5/data/qmdrawdata/qmdrawdata/readme.txt#L24) |

### 2.2 来自用户输入的直接参数

| 数据 | 数值 | 角色 |
| --- | --- | --- |
| beam energy | `190 MeV/u` | 用于 relativistic `beta`、beam velocity 和 `f_rev` 推导 |
| SRC harmonic number | `6` | 用于 `f_bucket = h f_rev` |
| SRC extraction radius | `5.36 m` | 用于 `C = 2 pi R_ext` 与 `f_rev = v / C` |
| Sn 库存 | `Sn112 = 1.2 g`, `Sn124 = 1.2 g` | 用于靶厚度与尺寸约束 |

### 2.3 外部官方来源的定位

这些官方来源被保存在 [README_sources.md](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/README_sources.md) 中，目前不直接替代本地主结果，只作为后续校核入口：

- [IAEA EXFOR](https://nds.iaea.org/exfor/)
- [GSI ATIMA](https://web-docs.gsi.de/~weick/atima/)
- [Geant4 Physics Reference Manual, hadronic cross sections](https://geant4.web.cern.ch/documentation/dev/prm_html/PhysicsReferenceManual/hadronic/Cross_Section/index.html)
- [Auce et al., Phys. Rev. C 53 (1996) 2919 searchable PDF mirror](https://lib.ysu.am/articles_art/5bd57cbcd47a121344e92133a487946e.pdf)
- [Auce et al. bibliographic record at OSTI](https://www.osti.gov/biblio/284777)

### 2.4 `polarization.tex` 的本地复核结果

这次复核固定使用：

- 原始输入：`data/qmdrawdata/qmdrawdata/z_pol/b_discrete/d+Sn124E190g060znp` 与 `d+Sn124E190g060zpn`
- 生成规则：对每个 `b` 档按 `floor(N_header * b / bmax)` 设定前缀选择上限，其中 `bmax = 10 fm`
- 事件清洗：沿用 `GenInputRoot_qmdrawdata` 当前实现的 `zpol` unphysical breakup cut
- Geant4 几何：`geometry_B115T_pdcOptimized_20260227_target3deg.mac`
- headline 汇总：只合并 `b01` 到 `b09`

这里有一个必须单独说明的细节：

- `dbreakbXX.dat` 头里的 `10000events` 是 ImQMD 头信息，不等于文件中实际存下来的 breakup 条目数
- 因此 low-`b` 档会出现“理论 `b/bmax` 预选上限远大于文件里实际 breakup 条目数”的情况
- 在当前仓库实现里，真正进入 Geant4 的 event 数由 `generator_selected_events_total` 和 `generator_written_events_total` 给出，而不是简单等于 `floor(10000 * b / 10)`

`b<=9 fm` 的主汇总结果如下：

| 量 | 数值 | 说明 |
| --- | --- | --- |
| header events total | `180000` | 来自 18 个 raw 文件头里的 `10000events` |
| theoretical preselect total | `90000` | 按 `b/bmax` 规则得到的选择上限 |
| stored breakup rows before cut | `28611` | 当前 raw `dbreak` 文件里实际可供选择的 breakup 条目数 |
| written Geant4 input events | `22174` | 经过 unphysical cut 后真正写入 `g4input` 的事件数 |
| fresh coincidence hits | `8408` | fresh Geant4 rerun 的 coincidence 计数 |
| fresh coincidence efficiency | `37.92%` | `8408 / 22174` |
| archive coincidence hits | `8414` | 归档 `eval_20260227_235751` 的同口径计数 |
| archive coincidence efficiency | `37.95%` | `8414 / 22174` |
| fresh - archive | `-0.0271` percentage points | `-2.71e-4` 的绝对效率差 |

直接对应的输出文件是：

- [generator_selection_audit.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/generator_selection_audit.csv)
- [g4_validation_counts.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/g4_validation_counts.csv)
- [fresh_vs_archive_compare.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/fresh_vs_archive_compare.csv)
- [validation_summary.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/validation_summary.json)

这组复核说明两件事：

- 当前仓库的 fresh rerun 能稳定复现归档 full-chain Geant4 结果
- 但当前仓库能复现的是约 `38%` 的 selected-breakup acceptance，而不是 `polarization.tex` 中那条 `4.65%` 的 overall detected efficiency

因此，本地数据对 `polarization.tex` 的判断应写成：

- `eta_det = 4.65%` 目前没有被仓库内现有数据独立复现
- `sigma = 550 mb` 也仍然不能仅凭当前仓库反推出绝对归一化，所以仍应保留为 manuscript anchor only
- 现有仓库数据真正支持的是：`Sn124 zpol g060` 的当前 selected-breakup sample 在这条 Geant4 链上的 coincidence acceptance 大约为 `37.9%`

如果仍然临时沿用 manuscript 的 `sigma = 550 mb`，则 [rate_validation_comparison.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/rate_validation_comparison.csv) 给出的 `3 mm`, `1e7 pps`, `16 h` 对照是：

| 口径 | coincidence efficiency | coincidence rate | 16 h total |
| --- | --- | --- | --- |
| manuscript anchor | `4.65%` | `2.724 kHz` | `1.569e8` |
| manuscript sigma + local eta | `37.92%` | `22.21 kHz` | `1.279e9` |

这说明一旦效率口径不一致，后面的 rate 估算会整体偏掉一个大因子，所以 proposal 正文里必须把“哪一个 `eta_det` 对应哪一种事件样本”写清楚。

### 2.6 `ImQMD` 截面归一化算法的修正

这里还要单独修正一件事：把 `sigma = pi b_max^2 (N/N0)` 直接套到 `b_discrete` 数据上并不安全。

只有在下面这个前提成立时，这条公式才成立：

- 入射样本确实是在 `0 <= b <= b_max` 上按面积均匀抽样
- `N0` 对应的就是这个整块圆盘内的总入射数

但当前仓库这套 `z_pol/b_discrete` 数据不是这种组织方式。每个 `dbreakbXX.dat` 文件头都独立写着 `10000events`，而文件中的 event number 又是跳号的，这说明更合理的解释是：

- 每个固定 `b` 点各自跑了 `10000` 个 incident deuterons
- 文件里只保存了其中发生 elastic breakup 的事件

在这种数据结构下，更稳妥的截面算法应写成逐个 `b` 环积分：

```text
P_i = N_breakup(b_i) / N_incident(b_i)
sigma_breakup ~= sum_i 2*pi*b_i*Delta b*P_i
```

若 `Delta b = 1 fm`，这就等价于按每个 `b` shell 的面积权重来积累 breakup probability，而不是把所有 `b` 档先混成一个总比例。

用当前本地 `Sn124 zpol g060` raw 数据做直接示例，[imqmd_cross_section_estimates.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/imqmd_cross_section_estimates.csv) 给出的结果是：

| 口径 | incident count | breakup rows | 截面算法 | `sigma_breakup` |
| --- | --- | --- | --- | --- |
| `b = 1..9` centers | `180000` | `28611` | shell midpoint `sum 2*pi*b_i*Delta b*P_i` | `678.2 mb` |
| `b = 1..9` centers | `180000` | `28611` | naive `pi*9^2*(N/N0)` | `404.5 mb` |
| `b = 5..9` centers | `100000` | `28447` | shell midpoint `sum 2*pi*b_i*Delta b*P_i` | `676.8 mb` |
| `b = 5..9` centers | `100000` | `28447` | naive `pi*9^2*(N/N0)` | `723.9 mb` |

这张表说明：

- 只靠 aggregate `N_breakup / N_incident` 和一句 “`b <= 9 fm`” 是不能唯一确定截面的。
- 你必须先知道 ImQMD 原始样本是“面积均匀抽样”还是“固定 `b` 点等统计抽样”。
- 对当前仓库的 `b_discrete` 数据，推荐算法是逐 shell 积分，不推荐直接把 `9 fm` 当成总面积半径塞进 `pi b_max^2`。

为了把这个判断看得更直观，当前文档还加入了 `P(b)` 与累计截面的图：

![ImQMD breakup probability and cumulative cross section](../../../results/rate_target_design_190MeVu/polarization_validation/imqmd_breakup_probability_and_cumulative_sigma.png)

若当前 Markdown 预览器不显示嵌入图，可直接打开：

- [imqmd_breakup_probability_and_cumulative_sigma.png](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/imqmd_breakup_probability_and_cumulative_sigma.png)

这张图是直接由 [imqmd_pb_profile.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/imqmd_pb_profile.csv) 画出来的。图中的定义是：

- 上图：`P(b) = N_breakup(b) / N_incident(b)`
- 下图橙线：每个 shell 的 `d sigma ~= 2 pi b Delta b P(b)`
- 下图红线：从最小 `b` 开始累加的 `sigma(B) = sum_{b_i <= B} 2 pi b_i Delta b P(b_i)`
- 下图绿线：只从 `b >= 5 fm` 开始累加的同类积分，用来对应原稿“`5 - 10 fm`”的物理区间

从当前 `Sn124 zpol g060` 本地样本可直接读出三条现象：

- `P(b)` 在 `b < 5 fm` 时极小，`b = 7 - 8 fm` 达到峰值，说明 breakup 主要来自外周碰撞。
- `b = 10 fm` 时 `P(b)` 仍然不小，在这批数据里大约还是 `0.376`，所以积分到 `9 fm` 不能简单认为已经完全收敛。
- 对应地，`sigma_breakup` 从 `b = 5..9` 的 `676.8 mb` 增长到 `b = 5..10` 的 `912.9 mb`，说明尾部 shell 的贡献在当前样本里仍然显著。

因此，对 `polarization.tex` 里那句

```text
2,162,519 among 10 M deuterons with b <= 9 fm -> sigma = 550 mb
```

更严格的表述应该是：

- 如果那 `10 M` 真的是在 `0..9 fm` 或某个等效圆盘区域内按面积均匀抽样得到的总 incident 数，那么 `550 mb` 这一换算是可以成立的。
- 如果那 `10 M` 实际上来自固定 `b` 点等统计、或来自 `5..10 fm` 之类的环带样本，那么就必须回到每个 `b` 档分别计数，再做 shell 积分；单条 aggregate 公式不够。

### 2.5 外部数据库与文献对照

这一步要把两类公共数据严格分开：

- 第一类是同体系的总反应截面 `sigma_R`。这类数据对 `Sn112`、`Sn124`、`Pb208` 是能找到的，而且能给出一个很好的 sanity envelope。
- 第二类是 breakup 专门文献。它们说明 `Sn/Pb` 重靶上的 breakup 确实是公开研究过的，但目前我没有找到能直接拿来替换 `190 MeV/u` 积分 breakup 截面的同口径公共数值表。

当前最直接可量化的公共表格来自 Auce 等人的 `d + target` 总反应截面测量。按 [external_database_cross_section_comparison.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/external_database_cross_section_comparison.csv) 整理后，`112Sn`、`124Sn`、`208Pb` 的 `sigma_R` 是：

| Target | `sigma_R(37.9 MeV)` | `sigma_R(65.5 MeV)` | `sigma_R(97.4 MeV)` | `550 mb / sigma_R` |
| --- | --- | --- | --- | --- |
| `112Sn` | `2130 ± 76 mb` | `2156 ± 47 mb` | `2212 ± 59 mb` | `0.249 - 0.258` |
| `124Sn` | `2282 ± 90 mb` | `2332 ± 57 mb` | `2343 ± 59 mb` | `0.235 - 0.241` |
| `208Pb` | `2844 ± 142 mb` | `3049 ± 71 mb` | `3250 ± 82 mb` | `0.169 - 0.193` |

这张表只能这样使用：

- 它告诉你 `polarization.tex` 里的 `550 mb` 若被理解为 breakup planning 截面，那么它大约是同体系公开低能 `sigma_R` 的 `17% - 26%`。
- 这个比例从物理上看并不离谱，因为 breakup 截面必须小于总反应截面。
- 但它不能证明 `550 mb` 就是对的，因为你的实验是 `190 MeV/u = 380 MeV` 总氘核能量，而这组公共 `sigma_R` 只有 `37.9 - 97.4 MeV` 总能量。

breakup 专门文献这边，当前能明确定位到的公共锚点是：

| 来源 | 体系 | 能量 | 公开信息 | 对本文的作用 |
| --- | --- | --- | --- | --- |
| `Yahiro et al. (1984)` | `118Sn(d,pn)` | `56 MeV` | 摘要明确写到 proton-neutron coincidence cross sections 和 proton spectrum 被 CDCC 成功分析 | 说明 Sn 同位素上的 breakup coincidence 量在公开文献里是存在的 |
| `Iseri et al. (1988)` | `118Sn`, `208Pb` | `56 MeV`, `21.5 MeV` | 摘要明确写到 virtual breakup 对 cross sections 和 analyzing powers 不可忽略 | 说明重靶 `Sn/Pb` 的 breakup 耦合不能忽略 |
| `Okamura et al. (2010)` | `208Pb` | `140 MeV` | 摘要明确写到对 `208Pb` 做了 kinematically complete elastic breakup measurement | 说明更高能区的 `Pb` breakup 也有直接实验锚点 |
| `Kalbach (2017)` | 全局系统学 | 文献数据库汇编 | 文中指出 absorptive breakup 可占总反应截面的 `50% - 60%`，且一般随靶质量增大而上升 | 给 `550 mb` 相对 `sigma_R` 的比例一个数据库系统学上限背景 |

因此，外部公共数据对 `polarization.tex` 里 `550 mb` 的判断应写成：

- 公开库能支持的最强定量判断，是 `550 mb` 明显低于同体系低能总反应截面 `sigma_R`，所以它作为 breakup planning 截面的量级并不明显反常。
- 公开库目前还不能直接给出“`190 MeV/u`、`d + 124Sn/Pb`、与你当前事件定义严格同口径”的 breakup 积分截面。
- 所以 proposal 正文里最稳妥的写法仍然是：`550 mb` 来自内部 `ImQMD`-based planning anchor，外部数据库只作为 sanity check，而不是主输入。

## 3. 为什么原稿公式不一致

### 3.1 正确的公式层次

这里必须把三个量分开：

- 反应率：`R_reaction = n * d * I * sigma`
- detected coincidence rate：`R_coin = R_reaction * eta_coin`
- 积分计数：`N_coin = R_coin * t`

其中：

- `n` 的单位是 `cm^-3`
- `d` 要先从 `mm` 换成 `cm`
- `sigma` 要先从 `mb` 换成 `cm^2`
- `t` 若以小时输入，要再乘 `3600`

对 `Sn124` planning anchor，本说明采用：

- `rho_Sn = 7.31 g/cm^3`
- `M_Sn124 = 124 g/mol`
- `n = N_A * rho / M = 3.5501e22 cm^-3`

于是：

```text
R_reaction = 1.9526e4 * (d / mm) * (I / 1e7 pps)  Hz
R_coin     = 9.0795e2 * (d / mm) * (I / 1e7 pps)  Hz
```

对 `d = 3 mm`、`I = 1e7 pps`：

```text
R_reaction = 5.8577e4 Hz
R_coin     = 2.7239e3 Hz
N_coin(16h)= 1.5689e8
```

更适合 proposal 直接引用的写法是：

```text
N_coin = 1.5689e8
       * (d / 3 mm)
       * (I / 1e7 pps)
       * (t / 16 h)
```

若坚持使用完全带单位的紧凑写法，则应写成：

```text
N_coin = 0.3271 * (d / mm) * (I / pps) * (t / h)
```

这里的 `0.3271` 才是和当前脚本输入一致的 dimensionally explicit coefficient。

### 3.2 原稿里到底错在什么地方

原稿写成了：

```text
N = 3.4 * (d/mm) * (I/pps) * (t/h)
```

这一写法至少缺了一层归一化说明。因为如果直接把 `I = 1e7` 代进去，会得到不合理的大数。原稿后面给出的 `d = 3 mm`、`I = 1e7 pps`、`t = 16 h` 结果 `1.6e8` 反而是对的，说明：

- 公式文字写法有问题
- 但作者在代数或心算时实际上用了别的归一化

### 3.3 number density 为什么也有偏差

脚本重算得到：

- `n(Sn124, recomputed) = 3.5501e22 cm^-3`

而原稿写的是：

- `n(Sn124, manuscript) = 3.685e22 cm^-3`

把原稿的 `n` 反解回去，在 `rho = 7.31 g/cm^3` 下对应的有效摩尔质量大约是：

- `M_eff = 119.46 g/mol`

这说明原稿那一项更像是按“接近天然锡平均摩尔质量”的口径在算，而不是按同位素富集 `Sn124` 在算。对于 enriched target 的 planning，应该改成按 `Sn124` 本身来算。

## 4. 束团结构与 pile-up

这里必须纠正原先那条 `11.91 kHz -> 13.99 us` 的算法。对当前 SRC 条件，更合理的推导应直接使用：

- deuteron beam kinetic energy: `T_u = 190 MeV/u`
- SRC harmonic number: `h = 6`
- SRC 导出轨道半径: `R_ext = 5.36 m`

### 4.1 推导过程

先由 `190 MeV/u` 求 relativistic 因子。取 `m_u c^2 = 931.494 MeV`，则

```text
gamma = 1 + T_u / (m_u c^2)
      = 1 + 190 / 931.494
      = 1.20397

beta  = sqrt(1 - gamma^-2)
      = 0.55689

v     = beta c
      = 0.55689 * 2.9979e8 m/s
      = 1.6695e8 m/s
```

然后用导出半径 `R_ext = 5.36 m` 计算导出轨道圆周：

```text
C = 2 pi R_ext
  = 2 pi * 5.36 m
  = 33.678 m
```

于是 SRC 导出点的周回频率、bucket 频率和束团间隔分别为：

```text
f_rev    = v / C
         = 1.6695e8 / 33.678
         = 4.9573 MHz

f_bucket = h * f_rev
         = 6 * 4.9573 MHz
         = 29.7441 MHz

Delta t  = 1 / f_bucket
         = 33.62 ns
```

这也是为什么这里必须把旧的 `14 us` 改掉。对 `R_ext = 5.36 m`、`190 MeV/u` 的束流，`14 us` 在量级上就不可能对应 SRC 导出点的 micro-bunch spacing。

同一条推导链已经写进 [calc_rate_target_design_190MeVu.py](/home/tian/workspace/dpol/smsimulator5.5/scripts/analysis/calc_rate_target_design_190MeVu.py) 和 [inputs.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/inputs.json) 中：

- `gamma = 1 + T_u / (m_u c^2)`
- `beta = sqrt(1 - gamma^-2)`
- `v = beta c`
- `f_rev = v / (2 pi R_ext)`
- `f_bucket = h f_rev`
- `Delta t = 1 / f_bucket`

### 4.2 修正后的束团占有率

在 `I = 1e7 pps` 下：

- 每束团平均入射氘核数约 `0.3362`
- 也就是说，平均每三个 bucket 才有大约一个入射氘核

对 `3 mm` 参考厚度：

- 每束团平均选定 breakup 反应数约 `1.969e-3`
- 每束团至少一个选定 breakup 反应的概率约 `1.967e-3`
- 每束团至少两个选定 breakup 反应的概率约 `1.94e-6`
- 每束团平均 detected coincidence 数约 `9.158e-5`
- 每束团至少一个 detected coincidence 的概率约 `9.157e-5`
- 每束团至少两个 detected coincidence 的概率约 `4.19e-9`

这说明：

- 修正后的 SRC micro-bunch spacing 是 `33.62 ns`，不是 `13.99 us`
- 按当前 planning anchor，reaction 与 coincidence 在单个 bucket 上都非常稀疏
- 因而从 beam micro-structure 本身看，pile-up 比之前估得更轻
- 真正需要额外检查的是探测器电子学积分窗是否会跨越多个 bucket，而不是单 bucket occupancy 本身

## 5. 探测器 rate 的现阶段口径

当前仓库中，已经有一条可直接复核的 `Sn124 zpol g060` full-chain 验证链，但它对应的是按 `b/bmax` 处理后的 selected-breakup 样本，而不是 `polarization.tex` 那段 planning paragraph 的整体口径。因此本文把 detector rate 分成三个层次：

| 层次 | 是否可直接用于主结论 | 数据来源 |
| --- | --- | --- |
| manuscript planning coincidence | 是 | `sigma = 550 mb`, `eta_coin = 4.65%` |
| selected-breakup full-chain validation | 是，但只用于验证当前样本 | fresh rerun + archive `Sn124 zpol g060` |
| proposal 级 PDC / NEBULA singles | 暂作 proxy | legacy `1.2T, 5deg` acceptance split |

对这条 local validation sample，`b<=9 fm` 的直接 Geant4 计数是：

- `PDC hits = 22174 / 22174 = 100%`
- `NEBULA hits = 8408 / 22174 = 37.92%`
- `coincidence = 8408 / 22174 = 37.92%`

这组数字只说明当前 selected-breakup 输入样本在现有几何下的 acceptance，不应直接拿来替代 proposal 主文中的 overall `eta_det = 4.65%`。

由 legacy acceptance split 可得：

- `PDC / coincidence ≈ 1.003`
- `NEBULA / coincidence ≈ 2.850`

把这个比例仅作为 provisional scaling 施加到 `3 mm` 的 full-chain coincidence anchor：

| 量 | 估算值 |
| --- | --- |
| coincidence rate | `2.724 kHz` |
| PDC singles proxy | `2.732 kHz` |
| NEBULA singles proxy | `7.763 kHz` |

这三项里，只有 coincidence rate 可以直接作为当前 planning 主值写进 proposal。PDC 和 NEBULA singles 只能作为 DAQ 通道量级的 provisional 估算，后面应当用同一条 `3deg_1.15T` full-chain 分析重新抽出。

## 6. 靶厚度与尺寸设计

### 6.1 先把“3 mm 参考厚度”和“实际库存约束”分开

当前几何宏 [3deg_1.15T.mac](/home/tian/workspace/dpol/smsimulator5.5/configs/simulation/geometry/3deg_1.15T.mac#L15) 里保留的是：

- `Target/Size 80 80 30 mm`
- `Target/SetTarget false`

这只是一个几何 placeholder，不是制造方案。

真正的制造约束来自库存：

- `Sn112 = 1.2 g`
- `Sn124 = 1.2 g`

如果把整块 `1.2 g` 金属锡做成圆盘靶，那么厚度和直径必须满足：

```text
A = m / (rho * d)
d = m / (rho * A)
```

### 6.2 若坚持做厚靶，直径会迅速变小

由 [inventory_backsolve.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/inventory_backsolve.csv) 可得：

| 厚度 | 对应圆盘直径 |
| --- | --- |
| `0.5 mm` | `20.45 mm` |
| `1.0 mm` | `14.46 mm` |
| `1.5 mm` | `11.80 mm` |
| `2.0 mm` | `10.22 mm` |
| `3.0 mm` | `8.35 mm` |

所以：

- `3 mm` 厚靶在 `1.2 g` 库存下只剩 `8.35 mm` 直径
- 这只能在束斑非常小、对中稳定且机械装卡余量足够时才有意义
- 它可以作为 physics / straggling 的参考点，但不应作为当前默认制造方案

### 6.3 更现实的三档圆盘方案

由 [target_design.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/target_design.csv) 可得：

| 圆盘直径 | 面密度 | 体密度换算厚度 | Sn124 coincidence rate | 16 h coincidence 总数 |
| --- | --- | --- | --- | --- |
| `12 mm` | `1061 mg/cm^2` | `1.451 mm` | `1.318 kHz` | `7.59e7` |
| `15 mm` | `679 mg/cm^2` | `0.929 mm` | `0.843 kHz` | `4.86e7` |
| `20 mm` | `382 mg/cm^2` | `0.523 mm` | `0.474 kHz` | `2.73e7` |

### 6.4 当前推荐

在“平衡统计量与分辨率”且“束斑尺寸还未锁定”的前提下，我建议把默认方案写成：

- 主推荐：`15 mm` 直径圆盘
- 对应厚度：约 `0.93 mm`
- 对应面密度：约 `679 mg/cm^2`

推荐它的原因很直接：

- 比 `20 mm` 靶保留更多统计量
- 又比 `12 mm` 靶在束斑未知时更宽容
- 相比 `3 mm` 厚靶，它不会把可用直径压缩到 `8.35 mm`

若后续证明束斑长期稳定在 `5 mm` 以内，可以再考虑向 `12 mm` 直径、`1.45 mm` 厚度方向推进。若束斑和对中仍不确定，则优先 `20 mm` 直径方案。

## 7. Sn112、Sn124、Pb208 应该怎么落数

### 7.1 Sn124

`Sn124` 是当前最稳的 planning anchor，因为本地草稿已经直接给出了：

- `sigma = 550 mb`
- `eta_coin = 4.65%`

所以本文的主数值全部锚定在 `Sn124`。

但如果问题是“仓库内现有数据能否独立支持 `4.65%` 这条效率”，答案是否定的。当前仓库能稳定复现的是另一条口径：

- `Sn124 zpol g060`
- `b<=9 fm`
- `b/bmax` 前缀抽样
- fresh coincidence efficiency `= 37.92%`
- archive coincidence efficiency `= 37.95%`

也就是说，现有仓库数据验证了 current selected-breakup full-chain chain，而没有验证 `polarization.tex` 中那条 `4.65%`。

再加上外部公共数据后，`Sn124` 的定位可以再说得更完整一点：

- 公开总反应截面 `sigma_R` 在 `37.9 - 97.4 MeV` 总氘核能量区间是 `2.282 - 2.343 b`
- 因而 manuscript 的 `550 mb` 大约对应这组公共 `sigma_R` 的 `23.5% - 24.1%`
- 这个比例作为 breakup planning fraction 是合理的，但它仍然不是 `190 MeV/u` 的直接实验替代值

### 7.2 Sn112

`Sn112` 的 QMD 输入在仓库里是有的，但当前没有一份与 `3deg_1.15T` 主线完全同口径的 rate anchor 可直接拿来写主表。因此本文对 `Sn112` 只保留一个 planning placeholder：

- 若暂时假设 `sigma(Sn112) = sigma(Sn124)`，那么由于原子数密度比例 `124 / 112 = 1.107`，`Sn112` 的 atom-density scaling rate 约比 `Sn124` 高 `10.7%`

这只是库存和数量级 planning，不是最终物理结论。最终文档必须把 `Sn112` 的 `sigma` 和 `eta` 替换成 Sn112 自己的 `ImQMD + Geant4` 提取结果。

### 7.3 Pb208

`Pb208` 是实验物理上必须做的比较靶，但当前这份 planning 包没有把 `Pb208` 的 `3deg_1.15T` full-chain cross section anchor 单独抽出来。因此：

- proposal 中的 DAQ / trigger 量级可以先沿用 `Sn124 3 mm reference` 作为 conservative envelope
- 真正写 `Pb208` 计数率表时，应该单独做 `Pb208` 的 `ImQMD -> Geant4 -> rate` 提取

外部公开数据还能补一条定量背景：

- `Pb208` 的公开低能总反应截面 `sigma_R` 在 `37.9 - 97.4 MeV` 总氘核能量区间是 `2.844 - 3.250 b`
- 因而如果暂拿 `550 mb` 去比，它只占这组公开 `sigma_R` 的 `16.9% - 19.3%`
- 这再次说明 `550 mb` 更像是某类 breakup 子通道的 planning 输入，而不是整个总反应截面

## 8. 最后给实验设计的建议

### 8.1 数据链建议

- 事件率和物理产额：以 `ImQMD` 为主
- 探测器 rate、trigger、coincidence：以 `Geant4` 为主
- 文献 sanity check：优先看 `EXFOR` 和同体系公开 `sigma_R` / breakup 文献
- 靶能损和厚度 feasibility：去 `ATIMA`

### 8.2 当前最适合写进文档的数字

- `3 mm` 参考厚度下：`R_coin ≈ 2.72 kHz`
- `3 mm` 参考厚度下：`N_coin(16 h) ≈ 1.57e8`
- `15 mm` 直径、`1.2 g` 库存受限现实方案：`thickness ≈ 0.93 mm`，`R_coin ≈ 0.843 kHz`，`N_coin(16 h) ≈ 4.86e7`
- 外部公共 `sigma_R` 对照下：`550 mb` 约等于 `Sn124` 低能总反应截面的 `23.5% - 24.1%`，约等于 `Pb208` 低能总反应截面的 `16.9% - 19.3%`

### 8.3 后续最该补的三项

1. 弄清楚 `polarization.tex` 里 `2,162,519 / 10 M -> 550 mb` 与当前仓库 raw `dbreak` 文件之间的绝对归一化映射。
2. 从当前 `3deg_1.15T` full-chain 结果里直接抽 proposal 口径的 `PDC singles / NEBULA singles / coincidence`，替代本文的 legacy proxy。
3. 给 `Sn112` 和 `Pb208` 分别补一条与本次 `Sn124 zpol g060` 同样明确的 local validation chain，并继续追 EXFOR/文献里是否存在可数字化的高能 breakup 微分截面表。

## 9. 复算命令

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
./scripts/analysis/run_polarization_validation_sn124_zpol_g060.sh
python3 scripts/analysis/calc_rate_target_design_190MeVu.py
```

脚本每次运行都会重写：

- [inputs.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/inputs.json)
- [rates.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/rates.csv)
- [target_design.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/target_design.csv)
- [rate_validation_comparison.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/rate_validation_comparison.csv)
- [external_database_cross_section_comparison.csv](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/external_database_cross_section_comparison.csv)
- [validation_summary.json](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/polarization_validation/validation_summary.json)
- [README_sources.md](/home/tian/workspace/dpol/smsimulator5.5/results/rate_target_design_190MeVu/README_sources.md)
