# 🎯 磁场旋转和可视化功能完成！

## ✅ 已实现的功能

### 1. 磁场旋转功能
- ✅ **默认旋转角度**: 30度（绕Y轴负方向）
- ✅ **动态角度设置**: `SetRotationAngle(angle)` 函数
- ✅ **坐标系转换**: 
  - 实验室坐标系 ↔ 磁铁坐标系
  - 自动处理位置和磁场的旋转变换
- ✅ **双接口设计**:
  - `GetField()`: 获取旋转后的磁场（实验室坐标系）
  - `GetFieldRaw()`: 获取原始磁场（磁铁坐标系）

### 2. 磁场可视化工具
- ✅ **4种可视化图**:
  1. XZ平面磁场强度分布 (Y=0)
  2. XY平面磁场强度分布 (Z=0)  
  3. By分量分布 (XZ平面)
  4. 沿X轴磁场分布曲线
- ✅ **多格式输出**: PNG 和 PDF
- ✅ **参数化角度**: 支持任意旋转角度绘制

## 📁 新增文件

```
sources/include/MagneticField.hh       - 更新：添加旋转功能
sources/src/MagneticField.cc           - 更新：实现旋转算法
macros/plot_magnetic_field_func.C      - 磁场绘制函数
macros/test_rotation.C                 - 旋转功能测试
plot_mag_field.sh                      - 磁场绘制运行脚本
```

## 🚀 使用方法

### 快速绘制磁场分布
```bash
# 使用默认30度旋转
./plot_mag_field.sh

# 使用指定角度
./plot_mag_field.sh 45    # 45度旋转
./plot_mag_field.sh 0     # 无旋转
```

### 在ROOT中使用旋转功能
```bash
root -l
```

然后：
```cpp
// 加载库
root [0] .x macros/load_magnetic_field_lib.C

// 创建磁场对象
root [1] MagneticField* field = new MagneticField()

// 加载磁场数据
root [2] field->LoadFromROOTFile("magnetic_field_test.root")

// 设置旋转角度
root [3] field->SetRotationAngle(45.0)  // 45度旋转

// 获取旋转后的磁场
root [4] TVector3 B = field->GetField(100, 0, 0)
root [5] B.Print()

// 获取原始磁场（无旋转）
root [6] TVector3 B_raw = field->GetFieldRaw(100, 0, 0) 
root [7] B_raw.Print()

// 绘制磁场分布
root [8] .x macros/plot_magnetic_field_func.C
root [9] plot_magnetic_field_func(30.0)  // 30度旋转
```

## 🔄 旋转原理

### 坐标变换
**磁铁绕Y轴负方向旋转角度θ**：

**位置变换（实验室→磁铁）**：
```
x' = x*cos(θ) + z*sin(θ)
y' = y  
z' = -x*sin(θ) + z*cos(θ)
```

**磁场变换（磁铁→实验室）**：
```
Bx = Bx'*cos(θ) - Bz'*sin(θ)
By = By'
Bz = Bx'*sin(θ) + Bz'*cos(θ)
```

### 函数说明
- `SetRotationAngle(angle)`: 设置旋转角度（度）
- `GetField(x,y,z)`: 获取实验室坐标系中的磁场
- `GetFieldRaw(x,y,z)`: 获取磁铁坐标系中的原始磁场
- `RotateToMagnetFrame()`: 位置坐标变换
- `RotateToLabFrame()`: 磁场坐标变换

## 📊 输出说明

### 可视化图像
运行绘制脚本后生成：
- `magnetic_field_[角度]deg.png` - PNG格式图像
- `magnetic_field_[角度]deg.pdf` - PDF格式图像

### 统计信息
脚本会输出：
- 中心点磁场值（实验室坐标系）
- 中心点磁场值（磁铁坐标系） 
- 旋转角度信息
- 磁场强度统计

## 🎯 典型应用

### 1. 研究磁场旋转效应
```bash
./plot_mag_field.sh 0     # 无旋转
./plot_mag_field.sh 30    # 30度旋转 
./plot_mag_field.sh 60    # 60度旋转
```

### 2. 粒子轨迹计算
```cpp
// 在轨迹重建中使用
TVector3 position(x, y, z);
TVector3 B_field = magField->GetField(position);
// B_field 已经考虑了磁铁旋转
```

### 3. 对比分析
```cpp
// 对比旋转前后的磁场
TVector3 B_rotated = field->GetField(pos);      // 旋转后
TVector3 B_original = field->GetFieldRaw(pos);  // 原始磁场
TVector3 difference = B_rotated - B_original;
```

## ⚡ 性能特点

- **快速查询**: O(1) 时间复杂度的磁场插值
- **内存高效**: 旋转变换不增加额外存储
- **精确计算**: 使用三角函数进行精确旋转变换
- **缓存友好**: 预计算cos和sin值

## 🔧 技术细节

### 默认设置
- **默认角度**: 30度（可在构造函数中修改）
- **旋转方向**: 绕Y轴负方向（符合右手定则）
- **角度单位**: 度（内部转换为弧度）

### 精度验证
- 0度旋转时，`GetField()` 和 `GetFieldRaw()` 结果应该相同
- 旋转变换满足正交性质
- 磁场强度在旋转前后保持不变

## 🎉 总结

现在你有了一个完整的磁场处理系统：

1. **磁场数据加载** - 支持SAMURAI格式和ROOT文件
2. **旋转功能** - 模拟磁铁的旋转效应  
3. **可视化工具** - 多种图表展示磁场分布
4. **高效查询** - 快速的磁场插值算法

这个系统完全满足粒子物理实验中对磁场处理的需求！🚀

## 📝 使用示例

```bash
# 立即开始使用
cd /home/tian/workspace/dpol/smsimulator5.5/d_work

# 绘制默认30度旋转的磁场分布
./plot_mag_field.sh

# 绘制不同角度对比
./plot_mag_field.sh 0
./plot_mag_field.sh 45  
./plot_mag_field.sh 90
```

享受你的磁场可视化工具吧！✨