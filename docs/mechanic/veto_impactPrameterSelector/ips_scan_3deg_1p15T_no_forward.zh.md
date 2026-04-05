# IPS 几何模拟与位置优化说明（3deg_1.15T，no-forward，target-reference-only）

## 1. 文档目的

本文档记录 `veto / IPS` 探测器在 `3deg_1.15T` 几何配置下的 Geant4 模拟、输入数据整理、方向定义、扫描求解方法、事件选取方式、以及当前这一次扫描的结果。

这份文档对应的物理目标不是“前向通过后再做 veto”，而是：

- 尽量让 IPS 探测到深度非弹事件
- 同时尽量不探测到氘核弹性破裂事件

因此本次扫描采用的是 `no-forward` 版本判据，也就是：

- 不要求 `OK_PDC1 && OK_PDC2 && NEBULA hit`
- 直接对所有小 `b` 事件统计 IPS 命中率

相关原始说明在：

- `docs/mechanic/veto_impactPrameterSelector/veto_IPS.md`

本次扫描结果目录在：

- `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300`

对应的 full-statistics 重跑入口为：

- `scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward_fullstats.sh`

full-statistics 结果目录约定为：

- `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_fullstats`

---

## 2. 本轮扫描的最新结果

### 2.1 结果性质

这一次得到的是“当前脚本配置下的抽样扫描结果”，不是 full-statistics 全事件扫描结果。

原因是本次运行脚本固定使用：

- `beamOn = 300 per merged root`

因此：

- 每个合并后的 `elastic_bXX.root` 最多只送前 `300` 个 tree entry 进 Geant4
- 每个合并后的 `allevent_bXX.root` 最多只送前 `300` 个 tree entry 进 Geant4
- 所以本轮推荐位置是“采样优化”的结果，而不是“所有事件都跑完后的最终物理结论”

如果需要 full-statistics 结果，应改用：

- `scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward_fullstats.sh`

该脚本会把：

- `beamOn` 设为 `0`
- 让每个 merged ROOT 跑完整棵树
- 结果单独写到 `smallb_all_noForward_fullstats`，不覆盖当前 beamOn300 结果

### 2.2 当前推荐位置

本次 `3deg_1.15T`、`no-forward`、`target-reference-only` IPS 扫描的当前推荐位置为：

- `IPS axis offset = -40 mm`

这里的 `-40 mm` 指的是：

- 相对靶中心
- 沿局部束流轴方向定义的有符号距离
- 负号表示沿 `-ips_axis` 方向移动 `40 mm`

本次结果的核心数值为：

| 量 | 数值 |
| --- | --- |
| 推荐 offset | `-40.000 mm` |
| 弹性泄漏率 | `8 / 1717 = 0.00465929` |
| 小 `b` 事件 IPS 命中率 | `1253 / 2100 = 0.596667` |
| 小 `b` raw IPS 命中率 | `1253 / 2100 = 0.596667` |
| 约束阈值 | `elastic leakage < 0.005` |

次优点为：

| rank | offset | elastic leakage | small-b IPS rate |
| --- | --- | --- | --- |
| 1 | `-40 mm` | `0.00465929` | `0.596667` |
| 2 | `-45 mm` | `0.00465929` | `0.584762` |

### 2.3 为什么是 `-40 mm`

这次并不是“因为 `-40 mm` 第一个刚好满足弹性泄漏率要求，所以被选中”。

真正的排序规则是：

1. 先要求：
   - `elastic leakage < 0.005`
2. 在满足约束的候选里，最大化：
   - `smallb_selected_rate`
3. 若仍然并列，则：
   - 先比较更小的 `elastic leakage`
   - 再比较更小的 `|offset|`

所以：

- `-40 mm` 和 `-45 mm` 都满足弹性泄漏率要求
- 两者的弹性泄漏率在这次输出里相同，都是 `8 / 1717`
- 但 `-40 mm` 的 `small-b` 命中率更高，因此它排第 1

### 2.4 为什么 refine 主要出现在负 offset 一侧

coarse 扫描先跑：

- `-200, -160, -120, -80, -40, 0, 40, 80, 120, 160, 200 mm`

程序会先取 coarse 排名前 2 的中心点，然后围绕这两个中心做 `±20 mm`、步长 `5 mm` 的细扫。

本次 refine 日志实际出现的是：

- `-100, -95, ..., -20 mm`

这说明：

- coarse 前两名的中心落在 `-80 mm` 和 `-40 mm` 附近
- 两个 refine 窗 `[-100,-60]` 和 `[-60,-20]` 正好连成一段
- 所以后面的细扫看起来几乎都在负 offset 一侧



---

## 3. 文档对应的代码与结果文件

### 3.1 几何与探测器实现

- IPS 子探测器几何：
  - `libs/smg4lib/src/construction/include/IPSConstruction.hh`
  - `libs/smg4lib/src/construction/src/IPSConstruction.cc`
- 总装配与放置：
  - `libs/sim_deuteron_core/src/DeutDetectorConstruction.cc`
- UI 命令：
  - `libs/sim_deuteron_core/src/DeutDetectorConstructionMessenger.cc`

### 3.2 数据转换与扫描

- QMD 输入转换：
  - `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`
- 扫描器：
  - `apps/tools/scan_ips_position.cc`
- 本次运行脚本：
  - `scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward.sh`

### 3.3 关键输出

- 最终文本摘要：
  - `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/best_ips_positions.txt`
- 数值汇总：
  - `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/ips_scan_summary.csv`
  - `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/ips_scan_summary.root`
- 运行进度日志：
  - `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/scan_progress.log`
- 运行清单：
  - `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/scan_manifest.txt`
- 保留下来的推荐点 Geant4 输出：
  - `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/kept_runs/recommended/g4output/scan`

需要特别注意：

- 当前 `ips_scan_summary.csv` / `ips_scan_summary.root` 记录所有已评估的 offset，而不是只写 shortlist
- `best_ips_positions.txt` 仍然只保留推荐点和 top 候选，适合人工快速查看
- `scan_progress.log` 会按运行顺序追加每个 coarse / refine / validation offset 的聚合指标
- 若某个 refine offset 命中缓存，日志里会显示 `cache_hit=1`，表示这个参数点没有重复跑 Geant4，只是复用了已算出的指标

---

## 4. IPS 几何定义

### 4.1 几何参数

IPS 的 Geant4 实现使用的是一个 32 路桶形阵列。参数定义在 `IPSConstruction.hh`：

| 参数 | 数值 |
| --- | --- |
| 模块数 | `32` |
| 单块宽度 | `20 mm` |
| 单块厚度 | `20 mm` |
| 单块长度 | `200 mm` |
| 模块中心到桶轴半径 | `135 mm` |

对应实现常量为：

```cpp
static constexpr int kModuleCount = 32;
static constexpr double kModuleWidthMm = 20.0;
static constexpr double kModuleThicknessMm = 20.0;
static constexpr double kModuleLengthMm = 200.0;
static constexpr double kRadiusMm = 135.0;
```

### 4.2 模块排布方式

`IPSConstruction::PlaceModules(...)` 采用等角度均匀排布：

- 第 `i` 路的方位角：
  - `phi_i = 2π i / 32`
- 模块横向中心位置：
  - `(R cos phi_i, R sin phi_i, 0)`
- 每块的 `copy number = i`

对应角间隔为：

- `360 / 32 = 11.25 deg`

### 4.3 模块朝向

这里要特别说明，因为它直接关系到“方向设定”的理解：

- 每个 IPS 模块是长条形塑闪
- 长度方向是局部 `z`
- 在桶坐标里，所有条块的长轴都平行于桶轴
- 然后整个 IPS 桶再整体跟着目标法向一起旋转

因此：

- IPS 不是“每根条沿径向伸出去”的结构
- 而是“32 根轴向条块围成一个圆桶”
- 条块长度方向与局部束流方向平行

### 4.4 材料

Geant4 实现里当前使用的是：

- `G4_PLASTIC_SC_VINYLTOLUENE`

---

## 5. 方向设定与坐标定义

这一部分最重要，必须明确。

### 5.1 目标法向与束流方向

本次几何采用的原则是：

- `target` 法向就是束流法向
- IPS 桶轴也和这个方向平行

在代码里，目标和 IPS 使用的是同一个旋转：

```cpp
G4RotationMatrix target_rm;
target_rm.rotateY(-fTargetAngle);

G4RotationMatrix ips_rm;
ips_rm.rotateY(-fTargetAngle);
```

因此：

- 靶法向 = 局部束流方向
- IPS 轴向 = 局部束流方向

### 5.2 局部束流轴向的定义

在 `DeutDetectorConstruction.cc` 中，IPS 轴向向量定义为：

```cpp
G4ThreeVector ips_axis(0.0, 0.0, 1.0);
ips_axis.rotateY(-fTargetAngle);
```

也就是说：

- 默认参考轴是实验室坐标的 `+z`
- 然后整体绕 `y` 轴旋转 `-fTargetAngle`

对于 `3deg` 的情况：

- `fTargetAngle = 3 deg`
- 所以 IPS / target 共用的局部束流轴向是：
  - `R_y(-3 deg) * (0,0,1)`

按右手系写成近似单位向量，可记成：

- `(-sin 3 deg, 0, cos 3 deg)`
- 约等于 `(-0.0523, 0, 0.9986)`

### 5.3 IPS 中心位置

IPS 中心强制放在局部束流线上：

```cpp
const G4ThreeVector ips_center = fTargetPos + fIPSAxisOffset * ips_axis;
```

这条公式的物理含义是：

- 先取靶中心 `fTargetPos`
- 再沿局部束流方向 `ips_axis`
- 平移一个有符号距离 `fIPSAxisOffset`

因此：

- 当 `AxisOffset = 0` 时，IPS 中心位于靶中心所在束流轴位置
- 当 `AxisOffset > 0` 时，IPS 沿 `ips_axis` 方向移动
- 当 `AxisOffset < 0` 时，IPS 沿 `-ips_axis` 方向移动

### 5.4 这次 `-40 mm` 的物理含义

本次结果中的：

- `offset = -40 mm`

严格含义是：

- 相对靶中心，沿 `-ips_axis` 方向平移 `40 mm`

不要把“正负方向”直接口头翻译成“上游/下游”而不说明坐标定义；一手定义始终应当是：

- 正方向：`ips_axis = R_y(-fTargetAngle) * (0,0,1)`
- 负方向：`-ips_axis`

---

## 6. 3deg_1.15T 几何配置

本次扫描固定使用：

- `configs/simulation/geometry/3deg_1.15T.mac`

### 6.1 偶极磁铁

宏文件中使用：

- `/samurai/geometry/Dipole/Angle 30 deg`
- `/control/execute {SMSIMDIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T.mac`

而 `geometry_B115T.mac` 进一步指定：

- `FieldFile = 180703-1,15T-3000.table`
- `FieldFactor = 1`

因此这次扫描的磁场配置就是：

- `3deg`
- `1.15T`
- 场图 `180703-1,15T-3000.table`

### 6.2 靶设置

`3deg_1.15T.mac` 中靶参数为：

| 参数 | 数值 |
| --- | --- |
| 材料参数 | `Sn` |
| 尺寸 | `80 80 30 mm` |
| 位置 | `(-1.2488, 0.0009, -106.9458) cm` |
| 角度 | `3.00 deg` |

其中 `Target/Position` 和 `Target/Angle` 仍然作为主生成器与 IPS 对轴的参考参数使用；但在当前正式扫描和 WRL 导出里，靶并不作为真实材料放入 Geant4 世界。扫描宏会额外执行：

```text
/samurai/geometry/Target/SetTarget false
/samurai/geometry/IPS/SetIPS true
/samurai/geometry/IPS/AxisOffset <offset> mm
/samurai/geometry/Update
```

### 6.3 为什么必须 `Target=false`

这里的 `Target=false` 表示：

- 不放实体靶体积
- 但保留 target 的位置和角度参数

这样做的原因是：

- IMQMD 输入已经是反应后的末态
- 如果再把 `Sn` 靶实体放进 Geant4，末态粒子会在靶里发生第二次材料反应
- 这会污染 IPS 的真正 veto 优化目标

所以这次正式结果都必须基于：

- `Target/SetTarget false`

### 6.4 PDC / NEBULA

该几何同时使用：

- `PDC Angle = 69 deg`
- `PDC Position1 = 70 0 400 cm`
- `PDC Position2 = 70 0 500 cm`
- `NEBULA_Dayone.csv`
- `NEBULA_Detectors_Dayone.csv`

需要注意的是：

- 本次 `no-forward` 扫描不会用 PDC/NEBULA 作为触发门条件
- 但这些探测器仍然保留在同一套实验几何里

---

## 7. 输入数据与预处理

### 7.1 数据来源

本次扫描使用两类输入：

1. `elastic`
   - 清洗后的 `dbreak*.dat`
   - 用来估计“氘核弹性破裂事件误打到 IPS 的概率”
2. `all-event`
   - `geminiout*.dat`
   - 用来估计“小 b 深度非弹事件打到 IPS 的效率”

原始路径来自：

- `data/qmdrawdata/qmdrawdata/allevent`

转换后的 Geant4 ROOT 输入存放在：

- `data/simulation/ips_scan/sn124_3deg_1.15T/input/representative`
- `data/simulation/ips_scan/sn124_3deg_1.15T/input/balanced`

### 7.2 弹性破裂 unphysical breakup 清洗

由于 ImQMD 会给出一部分非物理 breakup，需要先清洗。

当前转换器实现的规则是：

- 若为 `y` 极化：
  - 用 `|pyp - pyn| < 150 MeV/c`
- 若为 `z` 极化：
  - 用 `|pzp - pzn| < 150 MeV/c`


- `y` 极化且 `phi_random` 时，看 `y` 分量
- `z` 极化时，看 `z` 分量

### 7.3 y 极化的旋转设置

本次转换器默认参数为：

- `rotate_ypol = false`
- `rotate_zpol = true`

而这次扫描实际用的是：

- `ypol`

所以没有对 `y` 极化事件额外做 `phi` 旋转归一化。

### 7.4 all-event 的 Geant4 输入构造

`geminiout` 的处理方式为：

- 逐行读入 fragment
- 按 `ISIM` 分组
- 每个 `ISIM` 写成一个 Geant4 event
- `bimp` 存入 `beam.fUserDouble`

附加规则：

- 若遇到 `z=0, n=0` 终止行，直接跳过
- 若遇到中性多中子团簇（`z=0, a>1`），则展开为共速自由中子

---

## 8. 本次实际用了哪些事件

这一节最关键，因为这次并没有把所有事件都送进 Geant4。

### 8.1 本次使用的物理样本

本次脚本使用的是：

- `d+Sn124-ypolE190g050ypn`
- `d+Sn124-ypolE190g050ynp`

并且先在每个 `b` 桶内把二者合并成 balanced ROOT：

- `elastic_b01.root ... elastic_b10.root`
- `allevent_b01.root ... allevent_b07.root`

### 8.2 elastic 合并后总事件数与实际送入 Geant4 的事件数

本次 `elastic` 合并后总事件数为：

- `19305`

但真正送进 Geant4 的 `elastic` 事件数只有：

- `1717`

分桶是：

- `b01 14`
- `b02 21`
- `b03 30`
- `b04 62`
- `b05 300`，原本有 `421`
- `b06 300`，原本有 `6107`
- `b07 300`，原本有 `8878`
- `b08 300`，原本有 `2694`
- `b09 300`，原本有 `988`
- `b10 90`

也就是说：

- 对条目数少于 `300` 的桶，整桶都跑了
- 对条目数大于 `300` 的桶，只跑了前 `300` 个 tree entry

### 8.3 all-event 合并后总事件数与实际送入 Geant4 的事件数

本次 `all-event` 合并后总事件数为：

- `140000`

但真正送进 Geant4 的 `all-event` 事件数只有：

- `2100`

分桶是：

- `b01..b07` 每个桶都只跑了 `300`
- 但每个桶原本都是 `20000`

所以：

- `7 * 300 = 2100`
- 当前 small-b 命中率分母 `2100` 的来源就是这里

### 8.4 为什么不是“所有事件都算完”

原因很简单：

- 当前脚本固定传入了 `--beam-on 300`

也就是：

```bash
--beam-on 300
```

扫描器会把它写成：

```text
/action/gun/tree/beamOn 300
```

而 `TreeBeamOn` 的语义是：

- 如果 `beamOn` 小于 `1`，或大于 tree 总条目数，就跑完整棵树
- 否则只跑前 `beamOn` 条 tree entry

因此这次不是“随机抽样 300 个事件”，而是：

- 每个 merged ROOT 只跑前 `300` 个 tree entry

这一点非常重要，因为它意味着：

- 本轮 `-40 mm` 的推荐值是“当前输入顺序下的抽样扫描结果”
- 不是对全部 `elastic + all-event` 事件做 full-statistics 扫描后得到的最终结论

### 8.5 这次结果里的分母为什么是 `1717` 和 `2100`

因为本次采用的是：

- `require-forward = false`

所以：

- `selected == all events`

因此：

- 弹性泄漏率分母就是实际送入 Geant4 的 `elastic` 事件数 `1717`
- small-b 命中率分母就是实际送入 Geant4 的 `all-event` 事件数 `2100`

---

## 9. 扫描目标函数与判据

### 9.1 为什么这次不能加前向门

本次 IPS 的物理角色是：

- 探测深度非弹事件
- 给出 veto 信号

所以本次扫描不应该先筛成“前向通过事件”，否则会把很多本来应该由 IPS 打到的大角度反应产物提前筛掉。

因此本次使用：

- `require-forward = false`

在扫描器里，这意味着：

```cpp
const bool selected = require_forward ? forward : true;
```

### 9.2 信号定义

本次信号样本定义为：

- `all-event`
- `bimp <= 7`
- 不要求前向通过

即：

```cpp
const bool is_small_b = std::isfinite(bimp) && bimp <= small_b_max;
```

其中：

- `small_b_max = 7.0`

### 9.3 背景定义

本次背景样本定义为：

- 所有清洗后的 `dbreak`
- 不要求前向通过

### 9.4 IPS 命中定义

IPS 命中的判据是：

- `FragSimData` 中存在任一条
- `fDetectorName == "IPS"`
- `fEnergyDeposit > 0`

### 9.5 优化约束与排序规则

本次扫描器使用的约束与目标为：

1. 先要求：
   - `elastic leakage < 0.005`
2. 在满足约束的候选里，最大化：
   - `smallb_selected_rate`
3. 若仍然并列，则：
   - 先比较更小的 `elastic leakage`
   - 再比较更小的 `|offset|`

所以这是一个“约束优化”问题，而不是单纯最大化 IPS 命中率。

---

## 10. 本次实际扫描配置

### 10.1 网格参数

本次脚本中的网格参数为：

| 项目 | 数值 |
| --- | --- |
| coarse range | `-200 mm` 到 `+200 mm` |
| coarse step | `40 mm` |
| refine 策略 | `top2 ±20 mm` |
| refine step | `5 mm` |
| beamOn | `300` per merged root |

因此：

- coarse 点：`-200, -160, ..., +200`
- refine 点：围绕 coarse 前两名再细扫

### 10.2 本次实际 refine 网格

从运行日志可见，本次 refine 实际跑的是：

- `-100, -95, -90, ..., -20 mm`

这说明：

- coarse 前两名的中心落在 `-80 mm` 和 `-40 mm` 附近
- 两个 refine 窗正好拼接成一段连续负 offset 区间

---

## 11. 本次结果的详细解释

### 11.1 为什么 `selected_rate` 和 `raw_rate` 一样

因为本次采用的是：

- `require-forward = false`

所以：

- `selected == all events`

因此：

- `smallb_selected_total == smallb_raw_total`
- `smallb_selected_rate == smallb_raw_rate`

在本次推荐点 `-40 mm` 上，二者都等于：

- `1253 / 2100 = 0.596667`

### 11.2 为什么 `-40 mm` 胜过 `-45 mm`

对比前两名：

| offset | elastic leakage | small-b IPS rate |
| --- | --- | --- |
| `-40 mm` | `0.00465929` | `0.596667` |
| `-45 mm` | `0.00465929` | `0.584762` |

可见：

- 两者弹性泄漏率相同，且都满足 `elastic leakage < 0.005`
- 但 `-40 mm` 的 small-b 命中率更高

所以 `-40 mm` 是更优的折中点。

### 11.3 为什么当前结果和旧文档中的 `+20 mm` 不同

旧文档记录的是上一版运行结果，当时：

- 目标函数还是旧版本
- 或者扫描对象还是含有历史配置残留

当前这版已经明确改成：

- `Target=false`
- `no-forward`
- `target` 只作为位置和角度参考，不作为实际材料

因此：

- 旧的 `+20 mm` 结果已经过时
- 当前应采用本轮最新的 `-40 mm` 采样扫描结果

### 11.4 当前结果的边界

必须强调：

- 这次结果不是“所有事件都跑完后的 full-statistics 结论”
- 它只是在 `beamOn = 300 per merged root` 的采样配置下得到的当前最优点

如果后续要做真正的最终定点，建议再跑一次：

- `beamOn = 0`

这样每个 merged ROOT 会自动跑完整棵树。

---

## 12. 复现实验步骤

### 12.1 环境

建议先进入项目环境：

```bash
micromamba activate anaroot-env
```

### 12.2 编译

如果已经配置过 `build/`，直接增量编译：

```bash
cmake --build build --target scan_ips_position -j4
cmake --build build --target sim_deuteron -j4
cmake --build build --target GenInputRoot_qmdrawdata -j4
cmake --build build --target prepare_ips_wrl_examples -j4
```

### 12.3 重新生成 Geant4 输入（如需要）

若要从原始 `qmdrawdata` 重新生成 ROOT 输入，可使用：

```bash
build/bin/GenInputRoot_qmdrawdata   --mode ypol   --source both   --input-base data/qmdrawdata/qmdrawdata/allevent   --output-base data/simulation/ips_scan/sn124_3deg_1.15T/input/representative   --target-filter d+Sn124-ypolE190g050
```

### 12.4 运行 no-forward IPS 扫描

```bash
scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward.sh
```

它会自动完成：

1. 合并 `ypn` 与 `ynp`
2. 生成 balanced root 输入
3. 调用 `scan_ips_position`
4. 对每个 offset 生成临时 Geant4 宏
5. 运行 `sim_deuteron`
6. 读取输出 ROOT
7. 计算 `elastic leakage` 与 `small-b IPS rate`
8. 输出最终推荐位置

### 12.5 当前脚本的统计量限制

当前脚本固定使用：

- `beamOn = 300 per merged root`

所以如果要跑全统计，应该把这项改成：

- `beamOn = 0`

或者任何大于对应 tree entry 数的值。

### 12.6 结果输出

最终结果会写到：

```text
data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300
```

其中：

- `best_ips_positions.txt`：推荐位置和 top 候选
- `ips_scan_summary.csv`：所有已评估 offset 的全量参数表
- `ips_scan_summary.root`：与 CSV 对应的 ROOT 全量汇总
- `scan_progress.log`：按运行顺序记录每个 offset 的 `elastic_leakage`、`smallb_selected_rate`、`smallb_raw_rate`
- `ips_scan_summary.csv/root` 新增 `stage` 字段，用于区分 `coarse`、`refine`、`coarse+refine`、`validation`
- `scan_progress.log` 新增 `cache_hit` 字段；若为 `1`，表示该 stage 复用了已有指标，没有重复跑 Geant4
- `scan_manifest.txt`：运行参数与时间
- `kept_runs/recommended/`：推荐点保留的 Geant4 输出

---

## 13. Offset 0 WRL 示例导出

为了直观看 `IPS offset = 0 mm` 时的几何和典型轨迹，本项目额外导出了一组 WRL 示例。

输出目录为：

- `data/simulation/ips_wrl/sn124_3deg_1.15T_offset0`

目录结构为：

```text
data/simulation/ips_wrl/sn124_3deg_1.15T_offset0/
├── inputs/       # 用于 WRL 导出的 mini ROOT / 单粒子 ROOT
├── macros/       # 每张 WRL 对应的 Geant4 宏
├── wrl/          # 最终 WRL 文件
├── logs/         # Geant4 运行日志
└── selection/    # 事件选择清单与导出记录
```

导出脚本为：

- `scripts/analysis/export_ips_wrl_examples_3deg115T_offset0.sh`

准备工具为：

- `apps/tools/prepare_ips_wrl_examples.cc`

### 13.1 导出的 WRL 文件

本次固定导出 9 个 WRL：

1. `offset0_geometry_only.wrl`
2. `proton_px_scan_pz627_offset0.wrl`
3. `neutron_px_scan_pz627_offset0.wrl`
4. `elastic_b01_typical_offset0.wrl`
5. `elastic_b05_typical_offset0.wrl`
6. `elastic_b10_typical_offset0.wrl`
7. `allevent_b01_typical_offset0.wrl`
8. `allevent_b04_typical_offset0.wrl`
9. `allevent_b07_typical_offset0.wrl`

### 13.2 单粒子轨迹图的定义

单粒子图固定采用：

- `IPS offset = 0 mm`
- `Target/Angle = 3 deg`
- `Target/SetTarget false`
- `pz = 627 MeV/c`
- `py = 0`
- `px = +150, +100, +50, 0, -50, -100, -150 MeV/c`

并且：

- 质子单独一张合图
- 中子单独一张合图

输入 ROOT 中不手工旋转动量，实验室系方向由 `sim_deuteron` 的 `UseTargetParameters=true` 配合 `Target/Angle` 自动处理。

### 13.3 WRL 代表事件的选取规则

elastic 图固定取：

- `elastic_b01`
- `elastic_b05`
- `elastic_b10`

每个桶内按：

- 先计算 `|p_p - p_n|` 的三维相对动量大小
- 再选择最接近该桶中位数的事件

allevent 图固定取：

- `allevent_b01`
- `allevent_b04`
- `allevent_b07`

每个桶内按：

- 优先在 `offset0` 的 Geant4 输出里 `IPS hit = true` 的事件里选
- 然后按带电碎片数 `Nch (Z > 0)` 最接近桶中位数来选
- 并列时优先碎片总数更多的事件

这次重新导出之后，三张 allevent 图都选到了：

- `IPS hit = true` 的代表事件

### 13.4 WRL 选择记录

所有被选中的代表事件都会写入：

- `data/simulation/ips_wrl/sn124_3deg_1.15T_offset0/selection/selection_manifest.csv`

这里会记录：

- 来源 ROOT
- tree entry
- `original_event_id`
- `source_file_index`
- `bimp`
- 碎片数
- 带电碎片数
- 是否在 `offset0` 物理输出中命中 IPS

导出记录还会写入：

- `data/simulation/ips_wrl/sn124_3deg_1.15T_offset0/selection/export_manifest.txt`

其中明确记了：

- `target_material = disabled_reference_only`

---

## 14. 一句话总结

本次 `3deg_1.15T` IPS 模拟的最新结论是：

> 在"no forward"、`target` 只作位置与方向参考、并且每个 merged ROOT 只取前 `300` 个事件”的当前采样扫描定义下，IPS 的最佳位置是沿局部束流负方向相对靶中心移动 `-40 mm`。


