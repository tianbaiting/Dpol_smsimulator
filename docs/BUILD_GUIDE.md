# SMSimulator 5.5 构建指南

本版本支持两种构建方式：传统 Make 和现代 CMake。

## 快速开始

### 准备工作
```bash
# 设置环境变量
. setup.sh

# 确保依赖已安装
# - Geant4 (10.x+)
# - ROOT (6.x+)  
# - ANAROOT (设置 TARTSYS 环境变量)
# - Xerces-C
```

### 方法 A: CMake 构建（推荐）
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**优势:**
- 自动依赖管理
- 并行编译
- 自动复制可执行文件到 `bin/` 目录
- 更好的错误诊断
- 支持现代 IDE

### 方法 B: 传统 Make 构建
```bash
cd smg4lib
make
cd ../sim_deuteron  
make
```

**特点:**
- 兼容旧版本工作流程
- 手动控制构建过程
- 需要手动管理依赖顺序

## 项目结构

```
smsimulator5.5/
├── CMakeLists.txt           # 根 CMake 配置
├── sources/                 # 源代码目录
│   ├── smg4lib/            # 共享库模块
│   │   ├── data/           # ROOT 数据类 + 字典
│   │   ├── action/         # Geant4 动作类
│   │   ├── construction/   # 几何构造
│   │   └── physics/        # 物理过程
│   └── sim_deuteron/       # 主程序
├── bin/                    # 可执行文件输出
├── lib/                    # 库文件输出
└── build/                  # CMake 构建目录
```

## 构建输出

### CMake 构建结果
- `build/sources/sim_deuteron/sim_deuteron` → 自动复制到 `bin/sim_deuteron`
- `build/sources/smg4lib/*/libsm*.so` → 共享库
- ROOT 字典文件自动生成

### Make 构建结果  
- `sim_deuteron/sim_deuteron` 自动复制到 `bin/sim_deuteron`
- `smg4lib/lib/libsm*.so` → 共享库

## 故障排除

### CMake 常见问题
```bash
# 清理构建
rm -rf build && mkdir build && cd build

# 详细输出
make VERBOSE=1

# 检查依赖
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
```

### Make 常见问题
```bash
# 清理构建
make clean

# 检查环境变量
echo $ROOTSYS $G4INSTALL $TARTSYS
```

## 性能对比

| 特性 | CMake | Make |
|------|-------|------|
| 首次构建时间 | 快 | 中等 |
| 增量构建 | 很快 | 快 |
| 依赖管理 | 自动 | 手动 |
| 并行编译 | 支持 | 部分支持 |
| IDE 集成 | 优秀 | 基础 |

## 建议

- **新用户**: 使用 CMake 构建
- **现有工作流程**: 可继续使用 Make
- **开发环境**: 推荐 CMake + 现代 IDE
- **生产环境**: 两种方式均可