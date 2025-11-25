# TargetReconstructor 测试运行指南

## 📋 目录
1. [基础运行](#基础运行)
2. [可视化调试模式](#可视化调试模式)
3. [查看生成的图像](#查看生成的图像)
4. [使用 ROOT 交互查看](#使用-root-交互查看)
5. [高级用法](#高级用法)

---

## 1️⃣ 基础运行

### 快速运行所有测试
```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
./bin/test_TargetReconstructor
```

这会运行所有测试，但**不会生成可视化图像**（为了性能）。

---

## 2️⃣ 可视化调试模式

### 启用可视化（关键！）

如果你想看到 TMinuit 优化过程的图像，需要设置环境变量：

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build

# 设置可视化环境变量
export SM_TEST_VISUALIZATION=ON

# 运行测试
./bin/test_TargetReconstructor
```

### 一行命令运行
```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build && SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor
```

---

## 3️⃣ 查看生成的图像

### 输出文件位置

可视化图像保存在：
```
/home/tian/workspace/dpol/smsimulator5.5/build/test_output/reconstruction_minuit/
```

### 生成的文件

1. **c_opt_path.png** - TMinuit 优化路径图
   - 黑色虚线：全局 loss function
   - 红色线：TMinuit 的优化路径（带箭头）
   - 绿色点划线：真实动量位置
   - 绿星：起点
   - 蓝星：终点
   - 蓝色标签：关键步骤编号

2. **c_traj_3d.png** - 3D 轨迹对比图
   - 绿色实线：真实轨迹
   - 红色虚线：重建轨迹
   - 品红星：靶点位置
   - 蓝色标记：PDC 探测器位置

3. **对应的 .root 文件** - 可以用 ROOT 交互打开

### 用图像查看器打开
```bash
# 使用系统默认图像查看器
xdg-open test_output/reconstruction_minuit/c_opt_path.png
xdg-open test_output/reconstruction_minuit/c_traj_3d.png

# 或用其他工具
eog test_output/reconstruction_minuit/c_opt_path.png
```

---

## 4️⃣ 使用 ROOT 交互查看

### 打开 ROOT 文件
```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build
root test_output/reconstruction_minuit/c_opt_path.root
```

然后在 ROOT 命令行：
```cpp
// 查看画布
c_opt_path->Draw()

// 放大某个区域
c_opt_path->cd()
gPad->SetLogx()  // X 轴对数坐标
gPad->SetLogy()  // Y 轴对数坐标

// 保存为不同格式
c_opt_path->SaveAs("my_plot.pdf")
c_opt_path->SaveAs("my_plot.eps")
```

### 打开 3D 轨迹
```bash
root test_output/reconstruction_minuit/c_traj_3d.root
```

在 ROOT 中：
```cpp
// 查看轨迹
c_traj_3d->Draw()

// 旋转视角（如果是 3D 图）
gPad->GetView()->RotateView(30, 45)
```

---

## 5️⃣ 高级用法

### 只运行特定测试

```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build

# 运行 TMinuit 优化路径测试（带可视化）
SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor --gtest_filter="*TMinuitOptimizationWithPath*"

# 运行重建可视化测试
SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor --gtest_filter="*ReconstructWithVisualization*"

# 运行梯度下降测试
SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor --gtest_filter="*GradientDescentWithStepRecording*"
```

### 查看测试列表
```bash
./bin/test_TargetReconstructor --gtest_list_tests
```

### 详细输出
```bash
SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor --gtest_verbose
```

### 使用 CTest 运行
```bash
cd /home/tian/workspace/dpol/smsimulator5.5/build

# 运行所有单元测试
ctest -L unit -V

# 运行可视化测试（需要手动启用 SM_TEST_VISUALIZATION）
SM_TEST_VISUALIZATION=ON ctest -R TargetReconstructor -V
```

---

## 📊 理解输出

### 控制台输出示例

当运行 `SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor` 时，你会看到：

```
=== TMinuit Optimization Path ===
Total iterations: 38
Step  Momentum(MeV/c)   Loss(mm^2)  Distance(mm)
     0        1000.00      1.454e+02       12.06
     1        1050.00      1.234e+02       11.11
     ...
    37        1259.02      1.189e+00        1.09

Initial momentum: 1000.00 MeV/c
Final momentum: 1259.02 MeV/c
Momentum change: 259.02 MeV/c

=== Generating Visualization ===
✓ Plots saved to test_output/reconstruction_minuit/
```

### 图像说明

**优化路径图 (c_opt_path.png)**：
- X 轴：动量 (MeV/c)
- Y 轴：到靶点的距离 (mm)
- 显示 TMinuit 如何从初始猜测（1000 MeV/c）逐步找到最优解（~1259 MeV/c）
- 绿色垂直线标记真实动量位置

**轨迹对比图 (c_traj_3d.png)**：
- 绿色线：根据真实动量计算的轨迹
- 红色线：根据重建动量计算的轨迹
- 两者应该非常接近，表示重建准确

---

## 🔧 故障排除

### 问题1：没有生成图像
**原因**：忘记设置 `SM_TEST_VISUALIZATION=ON`  
**解决**：确保运行前导出环境变量

### 问题2：找不到磁场文件
**错误信息**：`Failed to load magnetic field`  
**解决**：检查磁场文件路径：
```bash
ls ../configs/simulation/geometry/filed_map/180626-1,20T-3000.root
```

### 问题3：ROOT 文件打不开
**原因**：ROOT 版本不兼容  
**解决**：确保使用相同的 ROOT 版本（6.36.04）

---

## 📝 常用命令速查

```bash
# 1. 编译测试
cd /home/tian/workspace/dpol/smsimulator5.5/build
cmake .. && make test_TargetReconstructor -j4

# 2. 运行测试（带可视化）
SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor

# 3. 查看优化路径图
xdg-open test_output/reconstruction_minuit/c_opt_path.png

# 4. 查看轨迹对比图
xdg-open test_output/reconstruction_minuit/c_traj_3d.png

# 5. 用 ROOT 交互查看
root test_output/reconstruction_minuit/c_opt_path.root

# 6. 清理重新编译
make clean
cmake .. && make test_TargetReconstructor -j4
```

---

## 🎯 推荐工作流程

1. **首次运行**：
   ```bash
   cd /home/tian/workspace/dpol/smsimulator5.5/build
   SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor
   ```

2. **查看结果**：
   ```bash
   xdg-open test_output/reconstruction_minuit/c_opt_path.png &
   xdg-open test_output/reconstruction_minuit/c_traj_3d.png &
   ```

3. **详细分析**（可选）：
   ```bash
   root test_output/reconstruction_minuit/c_opt_path.root
   ```

4. **修改代码后重新测试**：
   ```bash
   make test_TargetReconstructor -j4
   SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor
   ```

---

好了！现在你可以开始运行和调试了 🚀
