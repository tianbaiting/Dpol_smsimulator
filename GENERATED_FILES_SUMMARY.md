# SMSimulator 项目重构 - 生成文件总结

## 已生成的文件清单

### 1. 迁移脚本

📁 **主脚本**
- `migrate_to_new_structure.sh` - 项目结构迁移脚本（核心）
- `auto_migrate_and_setup.sh` - 一键自动化迁移和配置
- `generate_guides.sh` - 生成指南文档

### 2. CMake 配置文件

📁 **根 CMake**
- `CMakeLists_NEW.txt` - 新的主 CMakeLists.txt

📁 **cmake/ 模块** (在 `cmake_new/` 目录)
- `FindANAROOT.cmake` - ANAROOT 查找模块
- `FindXercesC.cmake` - Xerces-C 查找模块

📁 **libs/** (在 `cmake_new/` 目录)
- `libs_CMakeLists.txt` - libs/ 主构建文件
- `libs_smg4lib_CMakeLists.txt` - smg4lib 库
- `libs_smg4lib_data_CMakeLists.txt` - data 子模块
- `libs_smg4lib_action_CMakeLists.txt` - action 子模块
- `libs_smg4lib_construction_CMakeLists.txt` - construction 子模块
- `libs_smg4lib_physics_CMakeLists.txt` - physics 子模块
- `libs_sim_deuteron_core_CMakeLists.txt` - 氘核模拟核心库
- `libs_analysis_CMakeLists.txt` - 分析库

📁 **apps/** (在 `cmake_new/` 目录)
- `apps_CMakeLists.txt` - apps/ 主构建文件
- `apps_sim_deuteron_CMakeLists.txt` - 主模拟程序
- `apps_run_reconstruction_CMakeLists.txt` - 重建程序
- `apps_tools_CMakeLists.txt` - 工具程序

📁 **tests/** (在 `cmake_new/` 目录)
- `tests_CMakeLists.txt` - tests/ 主构建文件
- `tests_unit_CMakeLists.txt` - 单元测试
- `tests_integration_CMakeLists.txt` - 集成测试

## 使用流程

### 方法 1: 一键自动化（推荐）

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
chmod +x auto_migrate_and_setup.sh
./auto_migrate_and_setup.sh
```

这个脚本会自动完成：
1. ✅ 生成指南文档
2. ✅ 运行结构迁移
3. ✅ 复制所有 CMake 文件到正确位置
4. ✅ 创建辅助脚本 (build.sh, test.sh, clean.sh)
5. ✅ 创建示例配置文件
6. ✅ 生成完整的 README

完成后进入新项目：
```bash
cd smsimulator5.5_new
./build.sh
```

### 方法 2: 手动步骤

如果你想更细致地控制过程：

#### 步骤 1: 生成文档
```bash
chmod +x generate_guides.sh
./generate_guides.sh
```

#### 步骤 2: 运行迁移
```bash
chmod +x migrate_to_new_structure.sh
./migrate_to_new_structure.sh
```

#### 步骤 3: 复制 CMake 文件
```bash
# 复制所有 CMake 文件到 smsimulator5.5_new/
# (详细命令见 MIGRATION_GUIDE.md)
```

#### 步骤 4: 编译
```bash
cd smsimulator5.5_new
source setup.sh
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 新项目结构预览

```
smsimulator5.5_new/
├── CMakeLists.txt          ← 从 CMakeLists_NEW.txt 复制
├── README.md
├── build.sh               ← 自动生成的编译脚本
├── test.sh                ← 自动生成的测试脚本
├── clean.sh               ← 自动生成的清理脚本
│
├── cmake/
│   ├── FindANAROOT.cmake
│   └── FindXercesC.cmake
│
├── libs/
│   ├── CMakeLists.txt
│   ├── smg4lib/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   └── src/
│   │       ├── data/CMakeLists.txt
│   │       ├── action/CMakeLists.txt
│   │       ├── construction/CMakeLists.txt
│   │       └── physics/CMakeLists.txt
│   ├── sim_deuteron_core/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   └── src/
│   └── analysis/
│       ├── CMakeLists.txt
│       ├── include/
│       └── src/
│
├── apps/
│   ├── CMakeLists.txt
│   ├── sim_deuteron/
│   │   ├── CMakeLists.txt
│   │   └── main.cc
│   ├── run_reconstruction/
│   │   └── CMakeLists.txt
│   └── tools/
│       └── CMakeLists.txt
│
├── configs/
│   ├── simulation/
│   │   ├── macros/
│   │   ├── geometry/
│   │   └── physics/
│   └── reconstruction/
│
├── data/
│   ├── input/
│   ├── magnetic_field/
│   ├── simulation/
│   └── reconstruction/
│
├── scripts/
│   ├── requirements.txt
│   ├── analysis/
│   ├── batch/
│   ├── visualization/
│   └── utils/
│
├── tests/
│   ├── CMakeLists.txt
│   ├── unit/
│   │   └── CMakeLists.txt
│   └── integration/
│       └── CMakeLists.txt
│
└── docs/
```

## CMake 配置特性

### 模块化设计
- 每个库都是独立的 CMake 目标
- 清晰的依赖关系
- 支持选择性构建

### 编译选项
```cmake
-DBUILD_TESTS=ON/OFF          # 构建测试
-DBUILD_APPS=ON/OFF           # 构建应用程序
-DBUILD_ANALYSIS=ON/OFF       # 构建分析库
-DWITH_ANAROOT=ON/OFF         # ANAROOT 支持
-DWITH_GEANT4_UIVIS=ON/OFF    # Geant4 可视化
```

### 自动化特性
- ✅ 自动查找依赖（ROOT, Geant4, ANAROOT, Xerces-C）
- ✅ 自动配置包含路径
- ✅ 自动处理库链接顺序
- ✅ 自动复制可执行文件到 bin/
- ✅ 自动创建符号链接到配置文件
- ✅ 支持安装到系统目录

## 重要说明

### ⚠️ 注意事项

1. **原项目保留**: 迁移脚本不会修改原 `smsimulator5.5/` 目录
2. **环境变量**: 确保 `TARTSYS`, `ROOTSYS`, `G4INSTALL` 等已正确设置
3. **路径检查**: 迁移后检查源码中的相对路径引用
4. **头文件**: CMake 已配置包含路径，大部分 `#include` 不需要修改

### ✅ 优势

1. **清晰的职责分离**: 代码、配置、数据分离
2. **便于团队协作**: 标准化的项目结构
3. **易于测试**: 独立的测试框架
4. **灵活的构建**: 模块化的 CMake 配置
5. **现代化**: 符合 C++ 项目最佳实践

## 快速参考

### 编译命令
```bash
./build.sh                    # 完整构建
./test.sh                     # 运行测试
./clean.sh                    # 清理构建
```

### CMake 命令
```bash
cmake .. -DBUILD_TESTS=OFF    # 不构建测试
cmake .. -DCMAKE_BUILD_TYPE=Debug  # Debug 模式
make VERBOSE=1                # 详细编译信息
```

### 运行程序
```bash
./bin/sim_deuteron configs/simulation/macros/simulation.mac
```

## 获取帮助

生成的文档：
- `MIGRATION_GUIDE.md` - 详细的迁移指南
- `QUICK_START.md` - 快速开始指南
- `README.md` - 项目概览

## 文件映射关系

### 原项目 → 新项目

```
sources/smg4lib/          → libs/smg4lib/
sources/sim_deuteron/     → libs/sim_deuteron_core/ + apps/sim_deuteron/
d_work/sources/           → libs/analysis/
d_work/macros/            → configs/simulation/macros/
d_work/geometry/          → configs/simulation/geometry/
d_work/magnetic_field_*   → data/magnetic_field/
QMDdata/                  → data/input/
doc/                      → docs/
```

## 下一步

1. ✅ 运行 `auto_migrate_and_setup.sh`
2. ✅ 进入 `smsimulator5.5_new/`
3. ✅ 执行 `./build.sh`
4. ✅ 验证编译成功
5. ✅ 运行测试确认功能正常
6. ✅ 开始使用新结构开发！

---

**生成时间**: 2025-11-20
**版本**: 1.0
**项目**: SMSimulator v5.5
