# SMSimulator 5.5 - 构建和运行指南

## 📦 项目结构已完成重构

本项目已完成现代化重构，采用模块化的 CMake 构建系统。

## 🚀 快速开始

### 1. 编译项目

```bash
./build.sh
```

这个脚本会：
- 设置环境变量（如果 `setup.sh` 存在）
- 清理旧的构建文件
- 配置 CMake（Release 模式）
- 编译所有库和应用程序

### 2. 运行模拟

```bash
# 使用默认宏文件
./run_sim.sh

# 或指定宏文件
./run_sim.sh configs/simulation/macros/simulation_vis.mac
```

### 3. 运行测试

```bash
./test.sh
```

### 4. 清理构建

```bash
./clean.sh
```

## 📂 项目结构

```
smsimulator5.5_new/
├── CMakeLists.txt          # 主构建配置
├── build.sh                # 一键编译脚本
├── run_sim.sh              # 运行模拟脚本
├── test.sh                 # 测试脚本
├── clean.sh                # 清理脚本
│
├── cmake/                  # CMake 查找模块
├── libs/                   # C++ 库源码
│   ├── smg4lib/           # Geant4 核心库
│   ├── sim_deuteron_core/ # 氘核模拟核心
│   └── analysis/          # 数据分析库
│
├── apps/                   # 可执行程序
│   ├── sim_deuteron/      # 主模拟程序
│   ├── run_reconstruction/# 重建程序
│   └── tools/             # 工具程序
│
├── configs/                # 配置文件
│   ├── simulation/        # 模拟配置
│   │   ├── macros/       # Geant4 宏文件
│   │   ├── geometry/     # 几何配置
│   │   └── physics/      # 物理配置
│   └── reconstruction/    # 重建配置
│
├── data/                   # 数据目录
│   ├── input/             # 输入数据（QMD等）
│   ├── magnetic_field/    # 磁场数据
│   ├── simulation/        # 模拟输出
│   └── reconstruction/    # 重建输出
│
├── scripts/                # 工具
│   ├── analysis/
│   ├── batch/
│   ├── visualization/
│   └── utils/
│
└── tests/                  # 测试代码
```

## 🔧 详细编译步骤

如果你想手动控制编译过程：

```bash
# 1. 设置环境（如果需要）
source setup.sh

# 2. 创建构建目录
mkdir -p build && cd build

# 3. 配置 CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_APPS=ON \
    -DBUILD_ANALYSIS=ON \
    -DWITH_ANAROOT=ON \
    -DWITH_GEANT4_UIVIS=ON

# 4. 编译
make -j$(nproc)

# 5. 可选：运行测试
ctest --output-on-failure
```

## ⚙️ CMake 配置选项

编译时可以自定义以下选项：

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TESTS` | ON | 构建测试代码 |
| `BUILD_APPS` | ON | 构建应用程序 |
| `BUILD_ANALYSIS` | ON | 构建分析库 |
| `WITH_ANAROOT` | ON | 启用 ANAROOT 支持 |
| `WITH_GEANT4_UIVIS` | ON | 启用 Geant4 UI/Vis |
| `CMAKE_BUILD_TYPE` | Release | 构建类型（Release/Debug）|

示例：

```bash
# Debug 模式编译
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 不构建测试
cmake .. -DBUILD_TESTS=OFF

# 不使用 ANAROOT
cmake .. -DWITH_ANAROOT=OFF
```

## 📝 运行模拟的示例

### 交互式模拟（带可视化）

```bash
./run_sim.sh configs/simulation/macros/simulation_vis.mac
```

### 批量运行

```bash
cd scripts/batch
python batch_run_ypol.py
```

### 使用自定义宏文件

```bash
./bin/sim_deuteron your_custom.mac
```

## 🧪 测试

项目包含两种测试：

1. **单元测试** - 测试单个函数和类
2. **集成测试** - 测试完整的工作流程

运行所有测试：

```bash
./test.sh
```

或在 build 目录中：

```bash
cd build
ctest --verbose
```

## 📊 数据分析

分析数据使用 ROOT 和 Python 脚本：

### 使用 ROOT

```bash
cd configs/simulation/macros
root -l run_display.C
```

### 使用 Python

```bash
cd scripts/analysis
python your_analysis_script.py
```

## 🐛 故障排除

### 问题：找不到 ANAROOT

**解决方案**：
```bash
export TARTSYS=/path/to/anaroot
```

或者在编译时禁用：
```bash
cmake .. -DWITH_ANAROOT=OFF
```

### 问题：找不到 Geant4

**解决方案**：
```bash
source /path/to/geant4-install/bin/geant4.sh
```

### 问题：编译错误

**解决方案**：
1. 检查环境变量是否正确设置
2. 查看 CMake 配置摘要
3. 尝试 Debug 模式编译获得更多信息：
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   make VERBOSE=1
   ```

## 📚 相关文档

- [构建指南](docs/BUILD_GUIDE.md)
- [磁场配置](docs/MagneticField_README.md)
- [粒子轨迹](docs/PARTICLE_TRAJECTORY_SUMMARY.md)
- [DPOL 分析报告](docs/DPOL_Analysis_Report.md)

## 🎯 常用工作流

### 开发新功能

```bash
# 1. 编辑源码
# 2. 重新编译
./build.sh

# 3. 运行测试
./test.sh

# 4. 运行模拟验证
./run_sim.sh configs/simulation/macros/test.mac
```

### 参数优化

```bash
# 1. 修改配置文件
vim configs/simulation/geometry/geom_deuteron.mac

# 2. 批量运行
cd scripts/batch
python batch_run_ypol.py

# 3. 分析结果
python ../analysis/analyze_results.py
```

## 🆘 获取帮助

- 查看详细文档：`docs/` 目录
- 检查 CMake 配置：查看构建时的输出
- 查看示例：`configs/simulation/macros/` 目录

## ✨ 新结构的优势

- ✅ 模块化设计，易于维护
- ✅ 清晰的职责分离
- ✅ 自动化的依赖管理
- ✅ 完善的测试框架
- ✅ 灵活的构建选项
- ✅ 符合现代 C++ 规范

