# Analysis Tests - 分析库测试套件# Analysis Library Testing Guide



> **注意**：详细文档已整合到各代码文件的注释中。本README仅提供快速索引。## 概述



## 快速开始本测试套件为 `ParticleTrajectory` 和 `TargetReconstructor` 提供全面的单元测试和可视化调试功能。



### 1. 编译测试### 核心特性



```bash- ✅ **Google Test 框架**: 标准单元测试

cd build- ✅ **CTest 集成**: 自动化测试运行

cmake .. && make -j4- ✅ **双模式运行**:

```  - **性能模式** (默认): 快速测试，无额外开销

  - **可视化模式**: 生成优化过程图、轨迹图、函数全局图

### 2. 运行单元测试（快速，1-2秒）- ✅ **自动获取 GTest**: 如果系统未安装，CMake 自动从 GitHub 获取



```bash---

# 所有测试

ctest## 快速开始



# 特定测试### 1. 编译测试

./bin/test_ParticleTrajectory

./bin/test_TargetReconstructor```bash

./bin/test_TargetReconstructor_RealDatacd build

```cmake ..

make -j4

### 3. 运行可视化测试（生成图像，5-15秒）

# 仅编译测试

```bashmake test_ParticleTrajectory

export SM_TEST_VISUALIZATION=ONmake test_TargetReconstructor

./bin/test_TargetReconstructor```

```

### 2. 运行所有测试 (性能模式)

**输出**：`build/test_output/` 包含优化路径图、轨迹图等

```bash

---# 使用 CTest

ctest

## 测试文件说明

# 或直接运行

| 文件 | 描述 | 详细文档位置 |./tests/analysis/test_ParticleTrajectory

|------|------|------------|./tests/analysis/test_TargetReconstructor

| `test_ParticleTrajectory.cc` | 粒子轨迹传播测试 | 文件开头注释 |```

| `test_TargetReconstructor.cc` | 靶点重建测试（模拟数据） | 文件开头注释 |

| `test_TargetReconstructor_RealData.cc` | 真实数据测试 | 文件开头注释 |### 3. 运行带可视化的测试

| `run_event_display_realdata.C` | 3D事件显示脚本 | 文件开头注释（80+行完整文档）|

| `run_3d_display.sh` | Shell包装脚本 | 文件开头注释 |```bash

# 方法 1: 设置环境变量

---export SM_TEST_VISUALIZATION=ON

./tests/analysis/test_TargetReconstructor

## 3D事件显示

# 方法 2: 一次性运行

### 交互式运行SM_TEST_VISUALIZATION=ON ./tests/analysis/test_TargetReconstructor



```bash# 方法 3: 使用 CTest 的可视化目标

source setup.shctest -R Visualization -V

cd tests/analysis```



# 方式1：默认事件---

root -l run_event_display_realdata.C

## 测试说明

# 方式2：指定事件

root -l 'run_event_display_realdata.C(5)'### ParticleTrajectory 测试



# 方式3：自定义文件测试文件: `tests/analysis/test_ParticleTrajectory.cc`

root -l 'run_event_display_realdata.C(0, "/path/to/data.root")'

```#### 测试用例



### Shell脚本运行| 测试用例 | 描述 |

|---------|------|

```bash| `NeutralParticleStraightLine` | 验证中性粒子直线运动 |

./run_3d_display.sh 0                    # 事件0| `ChargedParticleCircularMotion` | 验证带电粒子在均匀磁场中圆周运动 |

./run_3d_display.sh 5 /path/to/data.root # 自定义文件| `LorentzForceDirection` | 验证洛伦兹力方向正确 |

```| `EnergyConservation` | 验证能量守恒 (磁场不做功) |

| `MinimumMomentumThreshold` | 测试动量阈值停止条件 |

**详细说明**：参见 `run_event_display_realdata.C` 文件开头的完整文档| `MaxTimeLimit` | 测试时间限制 |

| `MaxDistanceLimit` | 测试距离限制 |

---| `GetTrajectoryPoints` | 测试轨迹点提取功能 |

| `PerformanceTest` | 性能基准测试 |

## 可视化输出

#### 运行示例

### 性能模式（默认）

- ✅ 快速（~100ms/事件）```bash

- ✅ 适合CI/CD# 运行所有 ParticleTrajectory 测试

- ❌ 无图像输出./tests/analysis/test_ParticleTrajectory



### 可视化模式（`SM_TEST_VISUALIZATION=ON`）# 运行特定测试

- ✅ 优化路径图./tests/analysis/test_ParticleTrajectory --gtest_filter=ParticleTrajectoryTest.EnergyConservation

- ✅ 全局函数图

- ✅ 3D轨迹图# 详细输出

- ❌ 慢（~5s/事件）./tests/analysis/test_ParticleTrajectory --gtest_filter=* --gtest_print_time=1

```

**图像输出目录**：`build/test_output/reconstruction_*/`

---

---

### TargetReconstructor 测试

## 高级用法

测试文件: `tests/analysis/test_TargetReconstructor.cc` (模拟数据测试)

### CTest标签测试文件: `tests/analysis/test_TargetReconstructor_RealData.cc` (真实数据测试)



```bash#### 测试用例 - 模拟数据

ctest -L unit           # 单元测试

ctest -L performance    # 性能测试| 测试用例 | 描述 | 可视化输出 |

ctest -L visualization  # 可视化测试|---------|------|-----------|

```| `NoMagneticField` | 无磁场情况下的简单重建 | ❌ |

| `ReconstructWithVisualization` | 带详细信息的重建 | ✅ 全局函数图、试探轨迹、最佳轨迹 |

### GTest过滤器| `TMinuitOptimizationWithPath` | TMinuit 优化算法测试 | ✅ 优化路径、最佳轨迹 |

| `GradientDescentWithStepRecording` | 梯度下降算法测试 | ✅ 优化步骤、下降路径 |

```bash| `PerformanceBenchmark` | 批量重建性能测试 | ❌ |

# 列出测试

./bin/test_TargetReconstructor --gtest_list_tests#### 测试用例 - 真实数据



# 运行特定测试| 测试用例 | 描述 | 可视化输出 |

./bin/test_TargetReconstructor --gtest_filter=*Minuit*|---------|------|-----------|

| `SingleEventReconstruction` | 单个事件完整重建分析 | ✅ TMinuit优化路径、3D轨迹 |

# 排除测试| `MultipleEventsAnalysis` | 批量事件分析（性能模式） | ❌ |

./bin/test_TargetReconstructor --gtest_filter=-*Performance*

```**真实数据测试详细文档**: 请参见 [REALDATA_TEST_GUIDE.md](REALDATA_TEST_GUIDE.md)



---#### 可视化输出



## 故障排查当 `SM_TEST_VISUALIZATION=ON` 时，测试将生成以下图像：



### 库加载失败**输出目录**: `build/test_output/`



```bash```

source setup.shtest_output/

echo $LD_LIBRARY_PATH  # 应包含 build/lib├── reconstruction_basic/         # 基础重建测试

```│   ├── c_opt_func.png           # 全局优化函数图

│   ├── c_multi_traj.png         # 多条试探轨迹对比

### 环境变量未设置│   ├── c_traj_3d.png            # 最佳轨迹 3D 图

│   └── *.root                   # ROOT 格式 (交互式查看)

```bash├── reconstruction_minuit/        # TMinuit 优化

source /path/to/smsimulator5.5/setup.sh│   ├── c_traj_3d.png

```│   └── c_opt_func.png

└── reconstruction_gradient/      # 梯度下降优化

### 详细故障排查    ├── c_opt_path.png           # 优化路径图 (显示每步下降)

    └── *.root

参见各文件开头注释的"故障排查"部分。```



---#### 生成的图像说明



## 性能基准1. **全局优化函数图** (`c_opt_func.png`)

   - X 轴: 动量 [MeV/c]

| 测试 | 模式 | 时间 |   - Y 轴: 到目标点的距离 [mm]

|-----|------|-----|   - 红色星号: 全局最小值

| ParticleTrajectory全部 | 性能 | ~500ms |   - 用途: 理解优化问题的全局景观

| TargetReconstructor | 性能 | ~1-2s |

| TargetReconstructor | 可视化 | ~5-15s |2. **优化路径图** (`c_opt_path.png`)

| RealData单事件 | 性能 | ~100ms |   - 蓝色曲线: 全局函数

| RealData单事件 | 可视化 | ~5s |   - 红色路径: 优化算法走过的路径

   - 绿色点: 起始点

---   - 紫色点: 终点

   - 用途: 调试优化算法收敛性

## 参考

3. **多轨迹对比图** (`c_multi_traj.png`)

- **代码文档**：各文件开头的详细注释   - 左图: XZ 投影

- **Google Test**：https://google.github.io/googletest/   - 右图: YZ 投影

- **Analysis库**：`libs/analysis/README.md`   - 多条彩色线: 不同动量的试探轨迹

   - 红色星号: 目标点
   - 用途: 理解不同动量下的轨迹行为

4. **3D 轨迹图** (`c_traj_3d.png`)
   - 蓝色曲线: 粒子轨迹
   - 绿色点: 起点 (PDC)
   - 红色点: 终点
   - 紫色点: 目标点
   - 用途: 空间可视化

---

## 高级用法

### 1. 使用 CTest 标签运行特定测试

```bash
# 只运行单元测试
ctest -L unit

# 只运行性能测试
ctest -L performance

# 只运行可视化测试
ctest -L visualization

# 运行所有 analysis 相关测试
ctest -L analysis

# 详细输出
ctest -L unit -V
```

### 2. 运行特定的 GTest 测试

```bash
# 列出所有测试用例
./tests/analysis/test_TargetReconstructor --gtest_list_tests

# 运行匹配的测试
./tests/analysis/test_TargetReconstructor --gtest_filter=*Minuit*

# 排除某些测试
./tests/analysis/test_TargetReconstructor --gtest_filter=-*Performance*

# 重复运行 (测试稳定性)
./tests/analysis/test_TargetReconstructor --gtest_repeat=10
```

### 3. 可视化模式的性能影响

可视化模式会计算额外数据，对性能有显著影响：

| 模式 | 每事件时间 | 输出 | 用途 |
|-----|----------|-----|-----|
| 性能模式 (默认) | ~50-200 ms | 只有测试结果 | CI/CD、快速验证 |
| 可视化模式 | ~2-10 秒 | 大量图像、详细数据 | 算法调试、论文图表 |

**建议**:
- 日常开发和 CI: 使用性能模式
- 调试算法问题: 使用可视化模式
- 准备论文图表: 使用可视化模式，手动调整 ROOT 图像

---

## 查看可视化结果

### 方法 1: PNG 图像

```bash
# 使用系统图像查看器
eog build/test_output/reconstruction_basic/*.png

# 或
xdg-open build/test_output/reconstruction_basic/c_opt_func.png
```

### 方法 2: ROOT 交互式查看

```bash
cd build/test_output/reconstruction_basic

root -l
# 在 ROOT 提示符下:
root [0] TFile* f = TFile::Open("c_opt_func.root")
root [1] f->ls()
root [2] c_opt_func->Draw()

# 可以缩放、移动、调整样式
root [3] c_opt_func->GetXaxis()->SetRangeUser(500, 1500)
root [4] c_opt_func->Modified()
root [5] c_opt_func->Update()

# 保存修改后的图像
root [6] c_opt_func->SaveAs("modified_plot.pdf")
```

---

## 性能基准

在典型硬件上 (Intel i7, 16GB RAM):

| 测试 | 事件数 | 总时间 | 平均时间/事件 |
|-----|-------|-------|-------------|
| ParticleTrajectory 全部 | 9 个测试 | ~500 ms | - |
| TargetReconstructor (性能模式) | 10 events | ~1-2 秒 | ~100-200 ms |
| TargetReconstructor (可视化) | 1 event | ~5-15 秒 | ~5-15 秒 |

---

## 故障排除

### 问题 1: GTest 未找到

**症状**:
```
-- Google Test not found, skipping unit tests
```

**解决**:
CMake 会自动从 GitHub 获取 GTest。如果失败:

```bash
# 手动安装 (Ubuntu/Debian)
sudo apt-get install libgtest-dev

# 或编译安装
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp lib/*.a /usr/lib
```

### 问题 2: 可视化图像未生成

**症状**: 测试运行但没有 PNG/ROOT 文件

**检查**:
```bash
# 确认环境变量设置
echo $SM_TEST_VISUALIZATION

# 确认输出目录存在
ls -la build/test_output/

# 查看测试详细输出
SM_TEST_VISUALIZATION=ON ./tests/analysis/test_TargetReconstructor --gtest_filter=*Visualization* 2>&1 | tee test.log
```

### 问题 3: ROOT 图形显示问题

**症状**: `Error in <TCanvas::Constructor>: X11 display error`

**解决**:
```bash
# 设置批处理模式 (无 X11 显示)
export ROOT_BATCH_MODE=1

# 或在测试代码中使用 ROOT::EnableImplicitMT()
```

### 问题 4: 测试失败

**症状**: 某些测试 FAILED

**调试步骤**:
```bash
# 1. 运行失败的测试并详细输出
./tests/analysis/test_TargetReconstructor --gtest_filter=FailedTest --gtest_print_time=1

# 2. 启用可视化查看问题
SM_TEST_VISUALIZATION=ON ./tests/analysis/test_TargetReconstructor --gtest_filter=FailedTest

# 3. 检查全局函数图，看是否有多个局部极小值
# 4. 调整优化参数 (在测试代码中)
```

---

## 扩展测试

### 添加新的测试用例

```cpp
// 在 test_TargetReconstructor.cc 中添加

TEST_F(TargetReconstructorTest, MyNewTest) {
    // 设置
    TVector3 targetPos(0, 0, -500);
    TVector3 trueMom(300, 100, 1000);
    
    // ... 创建轨迹
    
    // 重建
    auto result = reconstructor->ReconstructAtTarget(...);
    
    // 断言
    EXPECT_LT(result.finalDistance, 5.0);
    
    // 可视化 (如果启用)
    if (enableVisualization && visualizer) {
        auto grid = visualizer->CalculateGlobalGrid(...);
        auto* canvas = visualizer->PlotOptimizationFunction1D(grid, "My Test");
        visualizer->SavePlots("test_output/my_test");
    }
}
```

### 自定义可视化

```cpp
// 在测试中使用 ReconstructionVisualizer

ReconstructionVisualizer viz;

// 计算全局网格 (更高分辨率)
auto grid = viz.CalculateGlobalGrid(
    reconstructor, track, targetPos,
    50.0,    // pMin
    3000.0,  // pMax
    500      // nSamples (更密集)
);

// 自定义绘图
TCanvas* c = viz.PlotOptimizationFunction1D(grid, "High-Res Function");
c->SetLogy();  // 对数刻度
c->SaveAs("my_plot.pdf");
```

---

## 持续集成 (CI)

在 CI 管道中使用性能模式:

```yaml
# .gitlab-ci.yml 或 .github/workflows/test.yml

test:
  script:
    - mkdir build && cd build
    - cmake ..
    - make -j4
    - ctest -L unit --output-on-failure
    # 性能模式，无可视化开销
  artifacts:
    reports:
      junit: build/test_results/*.xml
```

---

## 参考

- [Google Test 文档](https://google.github.io/googletest/)
- [CMake CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [ROOT TCanvas](https://root.cern.ch/doc/master/classTCanvas.html)
- SMSimulator Analysis Library: `libs/analysis/README.md`

---

## 总结

### 性能模式 (默认)
```bash
ctest -L unit
```
- ✅ 快速 (~1-2 秒)
- ✅ 适合 CI/CD
- ✅ 验证正确性
- ❌ 无调试信息

### 可视化模式
```bash
SM_TEST_VISUALIZATION=ON ctest -R Visualization -V
```
- ✅ 详细调试信息
- ✅ 优化过程图
- ✅ 全局函数景观
- ✅ 轨迹可视化
- ❌ 慢 (~5-15 秒/测试)
- ❌ 生成大量文件

**最佳实践**: 开发时用性能模式快速验证，遇到问题时切换到可视化模式深入调试。
