# ANAROOT SAMURAI 重建方法说明

anaROOT tbtDebian电脑上有两个路径,
/home/tian/software/anaroot/
/home/tian/workspace/dpol/anaroot/

samurai 的项目 还在 /home/tian/workspace/dpol/otherProject/kondo_smsimulator/

samurai的FDC重建解法,

## 1. 重建解法总览
 
在 `TArtRecoFragment::ReconstructData` 中，SAMURAI 片段重建按可用条件选择三种路径：

1. Runge-Kutta 全局拟合（优先）
2. MultiDimFit 多维多项式拟合（次优）
3. 0th/1st 阶矩阵回退（兜底）

核心入口：
- `src/sources/Reconstruction/SAMURAI/src/TArtRecoFragment.cc`

## 2. FDC 带电粒子动量重建（主流程）

### 2.1 FDC 命中与漂移长度

- FDC 原始命中由 `TArtCalibFDC1Hit`/`TArtCalibFDC2Hit` 装载到 `TArtDCHit`。
- 漂移长度在 `TArtCalibDCTrack::SetHitsBuffer` 里由 STC/分布/漂移速度模型换算。

相关代码：
- `src/sources/Reconstruction/SAMURAI/src/TArtCalibFDC1Hit.cc`
- `src/sources/Reconstruction/SAMURAI/src/TArtCalibDCTrack.cc`

### 2.2 FDC 局部轨迹拟合

- 各视角（X/U/V）枚举左右漂移歧义，做 1D 直线最小二乘，按 `chi2` 选优。
- 再把多个 1D 结果耦合成 3D 轨迹参数。

关键函数：
- `CalcTrack1D`
- `CalcTrackWith1DTracks`
- `RecoTrack`

相关代码：
- `src/sources/Reconstruction/SAMURAI/src/TArtCalibDCTrack.cc`

### 2.3 全局磁场追迹与动量求解

- 从 FDC1/FDC2 最优轨迹初始化 `TArtCalcGlobalTrack`。
- 在 `FieldMan + SAMURAIFieldMap` 提供的磁场图上进行 Runge-Kutta 传播。
- 目标函数为加权残差平方和 `chi2`，通过 SVD/LM 方式迭代更新 5 个参数（含 `q~1/p`）。
- 收敛后把“靶点处”主动量 `PrimaryMomentum` 写入 `TArtFragment`。

相关代码：
- `src/sources/Reconstruction/SAMURAI/src/TArtCalcGlobalTrack.cc`
- `src/sources/Reconstruction/SAMURAI/src/RungeKuttaUtilities.cc`
- `src/sources/Reconstruction/SAMURAI/src/FieldMan.cc`
- `src/sources/Reconstruction/SAMURAI/src/SAMURAIFieldMap.cc`
- `src/sources/Reconstruction/SAMURAI/src/TArtRecoFragment.cc`

## 3. MultiDimFit 方法

### 3.1 运行时读取位置

`TArtRecoFragment` 通过环境变量读取两组参数文件：

- `SAMURAI_MULTIDIM_FILE_RIG`（刚度 `Brho`）
- `SAMURAI_MULTIDIM_FILE_LEN`（路径长）

默认设置（本仓库）：
- `setup.sh`:
  - `SAMURAI_MULTIDIM_FILE_RIG=db/samurai_fun_rig.C`
  - `SAMURAI_MULTIDIM_FILE_LEN=db/samurai_fun_len.C`

参数解析入口：
- `ReadParameterMultiDimFit`
- `ReadParameters`

相关代码：
- `src/sources/Reconstruction/SAMURAI/src/TArtRecoFragment.cc`
- `setup.sh`

### 3.2 重建原理

该方法本质是离线训练的 6 维多项式代理模型：

1. 构造输入向量 `xin[6]`（FDC1/FDC2 位置角度 + BDC 外推靶点信息）。
2. 对每维按 `[XMin, XMax]` 归一化。
3. 按 `gPower` 指数表构造多项式基并乘以 `gCoefficient` 累加。
4. 输出 `Brho` 或路径长度。

实现函数：
- `GetRigidityFitResult`
- `GetPathLengthFitResult`

对应宏实现：
- `__GetFitResult__Macro__`

相关代码：
- `src/sources/Reconstruction/SAMURAI/src/TArtRecoFragment.cc`

### 3.3 参数文件怎么生成

仓库内 `db/samurai_fun_rig.C` 与 `db/samurai_fun_len.C` 文件头注明：

- 由 ROOT 的 `TMultiDimFit::MakeRealCode` 生成；
- 上游输入来自 Geant4 仿真树（示例注释中给出 `simtree_rigidity.C` / `simtree_length.C`）。

因此生成流程是：

1. 用 Geant4 产生训练样本（轨迹/刚度/程长）。
2. 在 ROOT 中用 `TMultiDimFit` 训练。
3. 调用 `MakeRealCode` 导出 `samurai_fun_*.C`。
4. 将环境变量指向新文件，重建时自动加载。

样例文件：
- `db/samurai_fun_rig.C`
- `db/samurai_fun_len.C`

说明：仓库中保留了可直接使用的导出结果文件，但未提供一套完整、可直接运行的“重新训练脚本”；训练步骤由上游 Geant4 + ROOT 流程完成。

## 4. 0th/1st 阶矩阵回退法

### 4.1 运行时读取

环境变量：
- `SAMURAI_MATRIX0TH_FILE`
- `SAMURAI_MATRIX1ST_FILE`

加载代码：
- `src/sources/Reconstruction/SAMURAI/src/TArtRecoFragment.cc`

### 4.2 文件来源

在 `tbt_try/Macros/SAMURAI/RKtrace/setup.sh` 中，这两个变量被设置到类似路径：

- `/home/samurai/.../geant4/matrix_calc/$brho/mat0th.txt`
- `/home/samurai/.../geant4/matrix_calc/$brho/mat1st.txt`

这表明该矩阵通常来自 Geant4 的 `matrix_calc` 光学计算流程（外部离线产物），而 ANAROOT 侧负责读取和应用。

### 4.3 计算思路

- 读取 0 阶偏置和 1 阶响应矩阵。
- 用 FDC 轨迹量构造线性关系，求解 `delta` 等一阶光学近似量。
- 当 RK/MultiDimFit 条件不满足时作为兜底估计。

## 5. 三种方法对比（实践建议）

1. 精度优先：Runge-Kutta 全局拟合（依赖磁场图、几何和较完整命中）。
2. 速度优先：MultiDimFit（在线计算快，但受训练相空间与参数版本约束）。
3. 健壮兜底：0th/1st 阶矩阵（模型简单，适合缺省场景或快速检查）。

## 6. 关键文件索引

- `src/sources/Reconstruction/SAMURAI/src/TArtRecoFragment.cc`
- `src/sources/Reconstruction/SAMURAI/src/TArtCalcGlobalTrack.cc`
- `src/sources/Reconstruction/SAMURAI/src/TArtCalibDCTrack.cc`
- `src/sources/Reconstruction/SAMURAI/src/RungeKuttaUtilities.cc`
- `src/sources/Reconstruction/SAMURAI/src/FieldMan.cc`
- `src/sources/Reconstruction/SAMURAI/src/SAMURAIFieldMap.cc`
- `db/samurai_fun_rig.C`
- `db/samurai_fun_len.C`
- `setup.sh`
- `tbt_try/Macros/SAMURAI/RKtrace/setup.sh`

## 7. kondo_smsimulator 仓库中 FDC“重建动量”的实际实现（补充）

### 7.1 数据流（探测器步进 -> 分析量）

- `FragmentSD::ProcessHits` 仅保留 `parentid==0` 且带电粒子，记录各探测器体积的 `pre/post position` 与 `pre/post momentum` 到 `FragSimData(TSimData)`。
- `sim_samurai21` 在 `TANA` 编译开关下使用 `SAMURAI21FragSimDataConverter`；非 `TANA` 时使用 `FragSimDataConverter_Basic`。

### 7.2 FDC 轨迹变量定义

- FDC1：`fdc1x/fdc1y = 0.5*(pre + post)`，并用 `dz>139 mm` 作为 `OK_FDC1` 条件。
- FDC2：先将 `pos_pre/pos_post/pos_fdc2` 统一绕 `Y` 轴旋转 `-fFDC2Angle` 到 FDC2 局部系，再计算
  - `fdc2x/fdc2y = 0.5*(pre + post)`
  - `fdc2a/fdc2b = (Δx/Δz, Δy/Δz)`
- 在 `SAMURAI21FragSimDataConverter` 中，`fdc2a/fdc2b` 乘 `1000` 后以 `mrad` 输出；`FragSimDataConverter_Basic` 中则是无量纲斜率。
- `OK_FDC2` 条件为 `dz>634 mm`，并与窗口命中做逻辑与：`fOK_FDC2 *= fOK_Window`。

### 7.3 `frag_brho` 的来源（关键）

在 `SAMURAI21FragSimDataConverter::ConvertSimData` 中：

- `fFragBrho = TanaKinematics::Mome2Brho(data->fPostMomentum.P(), data->fZ); // temptemptemp`

这意味着该量由 Geant4 步进后的 `postMomentum`（真值动量）直接换算得到，属于监督标签/真值参考，不是由 FDC1/FDC2 命中反演出来的在线重建结果。

### 7.4 与输入 Brho 的一致性（用于校验）

- 束流发生器支持 `/action/gun/Brho`，并通过 `P = c * Brho * Z` 设定入射动量（见 `PrimaryGeneratorActionBasic`）。
- 可使用 `work/g4mac/brho_func/*.mac` 在已知输入 Brho 条件下产生样本，再用 `(fdc observables) -> frag_brho` 做经验函数/代理模型拟合。

### 7.5 实践结论

1. 本仓库提供了 FDC 观测量提取与真值 Brho 标注，适合做重建函数训练与系统学验证。
2. 本仓库未实现 ANAROOT `TArtRecoFragment` 那套 RK/MultiDimFit/矩阵反演；实验数据重建应仍走 ANAROOT 主流程。

### 7.6 代码定位（kondo_smsimulator）

- `sources/smg4lib/data/FragmentSD.cc`
- `sources/projects/sim_samurai21/src/SAMURAI21FragSimDataConverter.cc`
- `sources/smg4lib/data/FragSimDataConverter_Basic.cc`
- `sources/projects/sim_samurai21/src/SAMURAI21DetectorConstruction.cc`
- `sources/projects/sim_samurai21/sim_samurai21.cc`
- `sources/smg4lib/action/PrimaryGeneratorActionBasic.cc`
- `work/g4mac/brho_func/29f_func.mac`
- `work/g4mac/brho_func/29f_emp.mac`
- `work/g4mac/brho_func/29ne_emp.mac`
