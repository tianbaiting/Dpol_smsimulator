# 数据流（Data Flow）快速说明

此文档概述 sms simulator 中数据从输入到输出的完整流向（中文），并列出关键源码位置、验证命令和常见排查方法。放在：`d_work/macros/dpol/data_flow_README.md`。

## 目的与适用场景
- 快速理解：输入（dat / input root）如何被 `PrimaryGenerator` 读入、如何在敏感探测器生成 sim-hit（`TClonesArray`），以及如何通过 Converter/Initializer 写入最终输出 ROOT tree。
- 排查运行时问题（如 branch为空或段错误）。

## 快速检查清单
- [ ] 输入文件存在并且 `tree` 名称正确（通常为 `tree` 或 `BeamSimTree`）。
- [ ] `TBeamSimData`/`TBeamSimDataArray` 在输入文件有 branch（branch 名称与 macro 中 `/action/gun/tree/TreeName` 一致）。
- [ ] 敏感探测器（PDC、NEBULA）在 `DetectorConstruction` 中已注册为 `SetSensitiveDetector`。
- [ ] `SimDataInitializer` 已注册并在 Run 开始时为输出 `TTree` 定义 branch。
- [ ] `SimDataConverter` 在事件结束时被调用，并且没有访问到 `null` 的参数（例如 `NEBULAParameter`、`TDetectorSimParameter`）。

## 数据流分步（按生命周期）
### 1) 输入生成（dat -> input ROOT）
- 代码示例：`d_work/macros/dpol/EventGenerator/GenInputRoot_np_atime.cc`、`TransDat2root.cc`。
- 作用：将文本数据转为 `TFile`/`TTree` 并创建 branch `TBeamSimData` 或 `TBeamSimDataArray`（通常树名为 `tree`）。
- 验证：
  - 在 shell 中运行：
    ```bash
    root -l your_input.root
    TFile *f = TFile::Open("your_input.root"); f->ls();
    TTree *t = (TTree*)f->Get("tree"); t->Print();
    ```

### 2) Primary generator（读取 input tree 并发射初级粒子）
- 文件：`sim_deuteron/src/DeutPrimaryGeneratorAction.cc`（或 `PrimaryGeneratorActionBasic.*`）。
- 行为：读取 `TBeamSimDataArray`，对每个事件创建一个或多个初级粒子（`G4ParticleGun` 或自定义方法）。
- 验证：运行模拟后，输出 root 中应包含 `beam` 分支。

### 3) 事件运行：步进与敏感探测器记录 sim-hit
- 敏感探测器实现举例：
  - NEBULA: `smg4lib/construction/src/NEBULASD.cc`（示例里会 `new ((*SimDataArray)[n]) TSimData;` 并填充字段）
  - PDC/Fragment: `smg4lib/construction/include/FragmentSD.hh`（实现通常在相邻 src 目录）
- 存储结构：所有 sim-hit 都被放到 `SimDataManager` 管理的 `TClonesArray` 中（例如 `FragSimData`, `NEBULASimData`）。
- 目的：减少频繁 new/delete，提高 ROOT 写入效率。

### 4) 事件结束 / 转换 -> 写入输出 Tree
- 管理器：`SimDataManager`（负责注册 Initializer/Converter 并在事件或 Run 收尾时调用转换写入）。
- Initializer：例如 `FragSimDataInitializer`，在 Run 开始时为 `TTree` 定义 branch：
  ```cpp
  tree->Branch("FragSimData", &fFragSimDataArray);
  ```
- Converter：例如 `FragSimDataConverter_Basic`、`NEBULASimDataConverter_TArtNEBULAPla`：
  - 读取 `TClonesArray` 中的 sim-hit，做聚合/校准/坐标变换。
  - 生成分析用对象（如 `TArtNEBULAPla`）放入另一个 `TClonesArray`，或直接写入 tree 的分支。
  - 在合适时机调用 `tree->Fill()`。
- 清理：Converter 的 `ClearBuffer()`（通常会 `fNEBULAPlaArray->Delete()`）在事件结束后清空临时容器。

## 输出 (示例) 与其来源对照
- `beam`  <- 来自 input tree (`TBeamSimDataArray`)。
- `FragSimData` <- 来自 PDC/Fragment 的 `FragmentSD::ProcessHits`（每个 step 一个 `TFragSimData`）。
- `PDC1U/PDC1X/...` <- 来自 `FragSimDataConverter_Basic` 的分组/聚合后量（按 PDC 层/视角分支）。
- `NEBULAPla` <- 来自 `NEBULASimDataConverter_TArtNEBULAPla`（将 `NEBULASimDataArray` 聚合为 `TArtNEBULAPla`）。
- `RunParameter` / `FragParameter` / `NEBULAParameter` <- 运行/几何参数，由相应 Parameter 类在 Run 初始化时写入。

## 关键源码位置（文件路径）
- 输入生成：
  - `d_work/macros/dpol/EventGenerator/GenInputRoot_np_atime.cc`
  - `d_work/macros/dpol/EventGenerator/TransDat2root.cc`
- Primary generator：
  - `sim_deuteron/src/DeutPrimaryGeneratorAction.cc`

- 敏感探测器：
  - NEBULA: `smg4lib/construction/src/NEBULASD.cc`
  - PDC/Fragment: `smg4lib/construction/include/FragmentSD.hh`（实现文件在相邻 src/）
- SimData 管理/初始化/转换：
  - `sim_deuteron/src/SimDataManager.cc`
  - `smg4lib/data/sources/src/FragSimDataInitializer.cc`
  - `smg4lib/data/sources/src/FragSimDataConverter_Basic.cc`
  - `smg4lib/data/sources/src/NEBULASimDataConverter_TArtNEBULAPla.cc`
- 注册点（应用入口）：
  - `sim_deuteron/sim_deuteron.cc`（注册 Initializer/Converter）

## 常见问题与排查步骤
1. 输出 branch 为空或缺少 `NEBULAPla`：
   - 检查 `NEBULASimDataConverter_TArtNEBULAPla::Initialize()` 是否返回 0（找到 `NEBULAParameter` & `NEBULASimDataArray`）。
   - 检查是否在宏里关闭了步存储：`/action/data/NEBULA/StoreSteps false` 或 `NEBULASimDataInitializer::SetDataStore(false)`。

2. 段错误（segfault）在 Converter：
   - 常见原因：`FindDetectorSimParameter(ID)` 返回 null，但代码仍然访问 `prm->...`。
   - 解决（已做示例修补）：在 `ConvertSimData()` 中检查 `NEBULAParameter` 与 `prm` 是否为 null，若为空则打印警告并跳过该 ID。

3. 输入 Tree 名称或 branch 名称不一致：
   - 检查宏 `vis.mac` / `simulation.mac` 中的 `/action/gun/tree/TreeName` 与生成 input file 中 `TTree("tree",...)` 是否一致。

4. 库加载错误（运行时报 `cannot open shared object file`）：
   - 先运行 `. ./setup.sh`，确保 `LD_LIBRARY_PATH` 包含 `smg4lib/lib` 或其他库路径。
   - 用 `ldd ../bin/sim_deuteron | grep not\ found` 检查缺失库。

## 常用验证命令（复制粘贴）
- 在 ROOT 中查看输出 file：
```bash
root -l output_tree/test0000.root
# 在 root 提示符
TFile *f = TFile::Open("output_tree/test0000.root"); f->ls();
TTree *t = (TTree*)f->Get("tree"); t->Print(); t->Show(0);
```

- 查看 FragSimData 条目：
```root
TClonesArray *arr=0; t->SetBranchAddress("FragSimData", &arr); t->GetEntry(0); arr->GetEntries(); arr->At(0)->Print();
```

- 重新编译并运行（在项目根目录）：
```bash
. ./setup.sh
cd smg4lib && make -j4
cd ../sim_deuteron && make -j4
cd ../d_work
../bin/sim_deuteron vis.mac
```

## 推荐的下一步（调试建议）
- 若你遇到 segfault：先在 `NEBULASimDataConverter_TArtNEBULAPla::ConvertSimData()` 添加 `if (!NEBULAParameter) ...` 和对 `prm` 的空检查（已示例修改）。
- 如需更详细事件级调试，在 `ProcessHits` 或 Converter 中临时加 `G4cout` 或 `std::cerr` 打印（仅用于短量事件）。
- 若需我帮忙：
  - (A) 展示 `FragmentSD::ProcessHits` 的实现并解释字段映射；
  - (B) 直接在某个输出文件上运行验证命令并把结果贴回；
  - (C) 添加更详细的 debug 打印并跑 1-5 个事件以验证流程。


