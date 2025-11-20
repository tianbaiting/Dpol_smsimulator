# SMSimulator - 重构版本

## 新的项目结构

```
smsimulator/
├── cmake/              # CMake 查找脚本
├── libs/               # 核心 C++ 库
│   ├── smg4lib/       # Geant4 基础库
│   ├── sim_deuteron_core/  # 氘核模拟核心
│   └── analysis/      # 数据分析重建库
├── apps/              # 可执行程序
├── configs/           # 运行时配置
├── data/              # 数据文件
├── scripts/           # Python 工具
├── tests/             # 测试代码
└── docs/              # 文档
```

## 编译说明

```bash
# 设置环境
source setup.sh

# 创建构建目录
mkdir build && cd build

# 配置和编译
cmake ..
make -j$(nproc)

# 运行测试
ctest
```

## 迁移说明

本项目已从旧结构迁移到新结构。旧项目保存在 `smsimulator5.5/`，新项目在 `smsimulator5.5_new/`。

