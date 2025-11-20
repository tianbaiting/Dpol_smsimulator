# MagneticField 类使用指南

## 🎯 功能概述

MagneticField 类用于读取和插值 SAMURAI 实验的磁场数据文件，提供高效的三线性插值算法。

## 📁 文件结构

```
sources/include/MagneticField.hh    # 头文件
sources/src/MagneticField.cc        # 实现文件
macros/load_magnetic_field_lib.C    # 库加载脚本
macros/test_magnetic_field_func.C   # 测试脚本
test_mag_field.sh                   # 测试运行脚本
```

## 🚀 快速开始

### 1. 加载和测试
```bash
cd /home/tian/workspace/dpol/smsimulator5.5/d_work
./test_mag_field.sh
```

### 2. 在 ROOT 中使用
```cpp
// 加载库
root [0] .x macros/load_magnetic_field_lib.C

// 创建磁场对象
root [1] MagneticField* magField = new MagneticField()

// 加载磁场文件
root [2] magField->LoadFieldMap("/path/to/180626-1,20T-3000.table")

// 获取某点的磁场
root [3] TVector3 field = magField->GetField(0, 0, 0)
root [4] field.Print()
```

## 📋 主要功能

### 1. 磁场文件加载
```cpp
MagneticField* magField = new MagneticField();
bool success = magField->LoadFieldMap("path/to/field_file.table");
```

### 2. 磁场查询
```cpp
// 方法1: 直接传入坐标 (mm)
TVector3 field = magField->GetField(x, y, z);

// 方法2: 传入TVector3对象
TVector3 position(x, y, z);
TVector3 field = magField->GetField(position);
```

### 3. 范围检查
```cpp
bool inRange = magField->IsInRange(x, y, z);
if (inRange) {
    TVector3 field = magField->GetField(x, y, z);
}
```

### 4. 获取磁场信息
```cpp
// 网格信息
int nx = magField->GetNx();    // X方向网格数
int ny = magField->GetNy();    // Y方向网格数  
int nz = magField->GetNz();    // Z方向网格数

// 空间范围
double xmin = magField->GetXmin();  // X最小值 [mm]
double xmax = magField->GetXmax();  // X最大值 [mm]
// ... Y, Z 类似

// 打印详细信息
magField->PrintInfo();
```

### 5. ROOT文件操作
```cpp
// 保存为ROOT文件(快速加载)
magField->SaveAsROOTFile("field.root");

// 从ROOT文件加载
MagneticField* magField2 = new MagneticField();
magField2->LoadFromROOTFile("field.root");
```

## 📊 磁场文件格式

支持的文件格式：
```
301 81 301 2                    # Nx Ny Nz NFields
1 X [MM]                        # 列说明
2 Y [MM]
3 Z [MM]  
4 BX [1]
5 BY [1]
6 BZ [1]
0                               # 分隔符
0.0 -400.0 0.0 0.0 1.211 0.0   # 数据: X Y Z BX BY BZ
...
```

## ⚡ 性能特点

- **内存效率**: 使用 `std::vector` 动态管理内存
- **快速插值**: 三线性插值算法，O(1) 时间复杂度
- **范围检查**: 自动检查查询点是否在磁场范围内
- **ROOT集成**: 完全兼容 ROOT 框架

## 🔧 技术细节

### 坐标系统
- **单位**: 毫米 (mm) 用于位置，特斯拉 (T) 用于磁场
- **索引**: 使用 (ix, iy, iz) 网格索引系统
- **插值**: 三线性插值，8个相邻网格点

### 内存使用
对于 301×81×301 网格（730万点）：
- 磁场数据: ~560 MB (3×8字节×730万)
- 总内存: ~600 MB

## 🎯 使用示例

### 基本使用
```cpp
// 创建磁场对象
MagneticField* field = new MagneticField();

// 加载数据
field->LoadFieldMap("geometry/filed_map/180626-1,20T-3000.table");

// 查询磁场
TVector3 B = field->GetField(100, -200, 50);  // 位置 (100,-200,50) mm
cout << "磁场强度: " << B.Mag() << " T" << endl;

// 清理
delete field;
```

### 批量处理
```cpp
// 创建测试点
vector<TVector3> points;
for(int i = -100; i <= 100; i += 10) {
    points.push_back(TVector3(i, 0, 0));
}

// 批量查询
for(auto& pos : points) {
    if(field->IsInRange(pos)) {
        TVector3 B = field->GetField(pos);
        cout << "位置: " << pos.X() << " 磁场: " << B.Mag() << endl;
    }
}
```

## ⚠️ 注意事项

1. **文件大小**: 原始磁场文件约 900MB，加载需要数分钟
2. **内存使用**: 加载后占用约 600MB 内存
3. **范围检查**: 查询范围外的点返回零磁场
4. **ROOT文件**: 建议首次加载后保存为ROOT文件以提高后续加载速度

## 🔍 故障排除

### 编译错误
```bash
cd sources/build && cmake .. && make -j4
```

### 加载失败  
- 检查文件路径是否正确
- 确认文件格式是否符合要求
- 检查文件权限

### 内存不足
- 考虑使用更少的网格点
- 或者分块加载大文件

## ✅ 测试验证

运行完整测试：
```bash
./test_mag_field.sh
```

成功输出应包含：
- 库加载成功
- 磁场文件加载进度
- 测试点的磁场值
- ROOT文件保存和验证

🎉 现在你可以使用 MagneticField 类进行磁场计算了！