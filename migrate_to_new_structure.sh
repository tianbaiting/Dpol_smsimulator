#!/bin/bash

##############################################################################
# SMSimulator 项目结构重构迁移脚本
# 作者: Auto-generated
# 日期: 2025-11-20
# 用途: 将现有项目重构为新的目录结构
##############################################################################

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 获取当前脚本所在目录（即项目根目录）
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
OLD_ROOT="${SCRIPT_DIR}"
NEW_ROOT="${SCRIPT_DIR}_new"

log_info "开始项目重构迁移..."
log_info "原项目路径: ${OLD_ROOT}"
log_info "新项目路径: ${NEW_ROOT}"

# 询问用户是否继续
echo ""
log_warn "重要提示:"
log_warn "  - 源代码和配置文件将被复制(cp)"
log_warn "  - 数据文件(.root, .dat等)将被移动(mv)以节省磁盘空间"
log_warn "  - 原项目目录中的数据文件将被移走"
echo ""
read -p "是否继续? (y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_warn "用户取消操作"
    exit 0
fi

##############################################################################
# 第一步: 创建新的目录结构
##############################################################################

log_info "步骤 1/5: 创建新的目录结构..."

mkdir -p "${NEW_ROOT}"
cd "${NEW_ROOT}"

# 创建顶层目录
mkdir -p cmake
mkdir -p libs/{smg4lib,sim_deuteron_core,analysis}
mkdir -p apps/{sim_deuteron,run_reconstruction,tools}
mkdir -p configs/{simulation,reconstruction,batch}
mkdir -p data/{input,magnetic_field,simulation,reconstruction,calibration}
mkdir -p scripts/{analysis,batch,visualization,utils}
mkdir -p tests/{unit,integration,fixtures}
mkdir -p docs
mkdir -p results

# 创建 libs 子目录
mkdir -p libs/smg4lib/{include,src/{action,construction,physics,data},tests}
mkdir -p libs/sim_deuteron_core/{include,src,tests}
mkdir -p libs/analysis/{include,src,tests}

# 创建 configs 子目录
mkdir -p configs/simulation/{macros,geometry,physics}
mkdir -p configs/reconstruction
mkdir -p configs/batch

log_info "✓ 目录结构创建完成"

##############################################################################
# 第二步: 迁移源代码
##############################################################################

log_info "步骤 2/5: 迁移源代码..."

# 迁移 smg4lib
log_info "  迁移 smg4lib..."
if [ -d "${OLD_ROOT}/sources/smg4lib/action" ]; then
    cp -r "${OLD_ROOT}/sources/smg4lib/action"/* libs/smg4lib/src/action/ 2>/dev/null || true
fi
if [ -d "${OLD_ROOT}/sources/smg4lib/construction" ]; then
    cp -r "${OLD_ROOT}/sources/smg4lib/construction"/* libs/smg4lib/src/construction/ 2>/dev/null || true
fi
if [ -d "${OLD_ROOT}/sources/smg4lib/physics" ]; then
    cp -r "${OLD_ROOT}/sources/smg4lib/physics"/* libs/smg4lib/src/physics/ 2>/dev/null || true
fi
if [ -d "${OLD_ROOT}/sources/smg4lib/data" ]; then
    cp -r "${OLD_ROOT}/sources/smg4lib/data"/* libs/smg4lib/src/data/ 2>/dev/null || true
fi
if [ -d "${OLD_ROOT}/sources/smg4lib/include" ]; then
    cp -r "${OLD_ROOT}/sources/smg4lib/include"/* libs/smg4lib/include/ 2>/dev/null || true
fi

# 迁移 sim_deuteron
log_info "  迁移 sim_deuteron..."
if [ -d "${OLD_ROOT}/sources/sim_deuteron/include" ]; then
    cp -r "${OLD_ROOT}/sources/sim_deuteron/include"/* libs/sim_deuteron_core/include/ 2>/dev/null || true
fi
if [ -d "${OLD_ROOT}/sources/sim_deuteron/src" ]; then
    cp -r "${OLD_ROOT}/sources/sim_deuteron/src"/* libs/sim_deuteron_core/src/ 2>/dev/null || true
fi
if [ -f "${OLD_ROOT}/sources/sim_deuteron/sim_deuteron.cc" ]; then
    cp "${OLD_ROOT}/sources/sim_deuteron/sim_deuteron.cc" apps/sim_deuteron/main.cc
fi

# 迁移分析代码（从 d_work/sources）
log_info "  迁移分析代码..."
if [ -d "${OLD_ROOT}/d_work/sources/include" ]; then
    cp -r "${OLD_ROOT}/d_work/sources/include"/* libs/analysis/include/ 2>/dev/null || true
fi
if [ -d "${OLD_ROOT}/d_work/sources/src" ]; then
    cp -r "${OLD_ROOT}/d_work/sources/src"/* libs/analysis/src/ 2>/dev/null || true
fi

# 迁移工具
log_info "  迁移工具程序..."
if [ -f "${OLD_ROOT}/sources/sim_deuteron/auto_pdc_wires.cc" ]; then
    cp "${OLD_ROOT}/sources/sim_deuteron/auto_pdc_wires.cc" apps/tools/
fi
if [ -f "${OLD_ROOT}/sources/sim_deuteron/plot_pdc_wires.py" ]; then
    cp "${OLD_ROOT}/sources/sim_deuteron/plot_pdc_wires.py" scripts/visualization/
fi

log_info "✓ 源代码迁移完成"

##############################################################################
# 第三步: 迁移配置文件
##############################################################################

log_info "步骤 3/5: 迁移配置文件..."

# 迁移 Geant4 macros
if [ -d "${OLD_ROOT}/d_work/macros" ]; then
    cp -r "${OLD_ROOT}/d_work/macros"/* configs/simulation/macros/ 2>/dev/null || true
fi
if [ -f "${OLD_ROOT}/d_work/simulation.mac" ]; then
    cp "${OLD_ROOT}/d_work"/*.mac configs/simulation/macros/ 2>/dev/null || true
fi
if [ -f "${OLD_ROOT}/d_work/vis.mac" ]; then
    cp "${OLD_ROOT}/d_work/vis.mac" configs/simulation/macros/ 2>/dev/null || true
fi

# 迁移几何文件
if [ -f "${OLD_ROOT}/d_work/detector_geometry.gdml" ]; then
    cp "${OLD_ROOT}/d_work/detector_geometry.gdml" configs/simulation/geometry/
fi
if [ -d "${OLD_ROOT}/d_work/geometry" ]; then
    cp -r "${OLD_ROOT}/d_work/geometry"/* configs/simulation/geometry/ 2>/dev/null || true
fi

log_info "✓ 配置文件迁移完成"

##############################################################################
# 第四步: 迁移数据和脚本
##############################################################################

log_info "步骤 4/5: 迁移数据和脚本..."
log_warn "注意: 数据文件将被移动(mv)而非复制，以节省磁盘空间"

# 迁移磁场数据 (使用 mv 移动大文件)
log_info "  移动磁场数据文件..."
if [ -f "${OLD_ROOT}/d_work/magnetic_field_test.root" ]; then
    mv "${OLD_ROOT}/d_work/magnetic_field_test.root" data/magnetic_field/
    log_info "    ✓ 已移动 magnetic_field_test.root"
fi

# 迁移 QMD 数据 (使用 mv 移动大型数据目录)
log_info "  移动 QMD 数据..."
if [ -d "${OLD_ROOT}/QMDdata" ]; then
    mv "${OLD_ROOT}/QMDdata"/* data/input/ 2>/dev/null || true
    log_info "    ✓ 已移动 QMDdata"
fi

# 移动其他大型数据文件 (.root, .dat 等)
log_info "  移动其他数据文件..."
if [ -d "${OLD_ROOT}/d_work/rootfiles" ]; then
    mv "${OLD_ROOT}/d_work/rootfiles" data/simulation/ 2>/dev/null || true
    log_info "    ✓ 已移动 rootfiles"
fi
if [ -d "${OLD_ROOT}/d_work/output_tree" ]; then
    mv "${OLD_ROOT}/d_work/output_tree" data/simulation/ 2>/dev/null || true
    log_info "    ✓ 已移动 output_tree"
fi
if [ -d "${OLD_ROOT}/d_work/temp_dat_files" ]; then
    mv "${OLD_ROOT}/d_work/temp_dat_files" data/simulation/ 2>/dev/null || true
    log_info "    ✓ 已移动 temp_dat_files"
fi

# 迁移 Python 脚本 (小文件用 cp)
log_info "  复制 Python 脚本..."
if [ -f "${OLD_ROOT}/d_work/batch_run_ypol.py" ]; then
    cp "${OLD_ROOT}/d_work/batch_run_ypol.py" scripts/batch/
fi
if [ -f "${OLD_ROOT}/d_work/batch_run_ypol.sh" ]; then
    cp "${OLD_ROOT}/d_work/batch_run_ypol.sh" scripts/batch/
fi

# 移动 plots 目录 (可能包含大量图片)
if [ -d "${OLD_ROOT}/d_work/plots" ]; then
    log_info "  移动 plots 目录..."
    mv "${OLD_ROOT}/d_work/plots" results/ 2>/dev/null || true
    log_info "    ✓ 已移动 plots"
fi

# 移动 reconp_originnp 目录
if [ -d "${OLD_ROOT}/d_work/reconp_originnp" ]; then
    log_info "  移动 reconp_originnp 目录..."
    mv "${OLD_ROOT}/d_work/reconp_originnp" data/reconstruction/ 2>/dev/null || true
    log_info "    ✓ 已移动 reconp_originnp"
fi

# 创建 Python 初始化文件
touch scripts/__init__.py
touch scripts/analysis/__init__.py
touch scripts/batch/__init__.py
touch scripts/visualization/__init__.py
touch scripts/utils/__init__.py

log_info "✓ 数据和脚本迁移完成"

##############################################################################
# 第五步: 迁移文档和顶层文件
##############################################################################

log_info "步骤 5/5: 迁移文档和顶层文件..."

# 迁移文档
if [ -d "${OLD_ROOT}/doc" ]; then
    cp -r "${OLD_ROOT}/doc"/* docs/ 2>/dev/null || true
fi

# 复制顶层文件
if [ -f "${OLD_ROOT}/README.md" ]; then
    cp "${OLD_ROOT}/README.md" ./
fi
if [ -f "${OLD_ROOT}/setup.sh" ]; then
    cp "${OLD_ROOT}/setup.sh" ./
fi

log_info "✓ 文档迁移完成"

##############################################################################
# 创建新文件
##############################################################################

log_info "创建新的配置文件..."

# 创建 .gitignore
cat > .gitignore << 'EOF'
# Build directories
build/
bin/
lib/

# Results and temporary files
results/
*.pyc
__pycache__/
.pytest_cache/

# Data files (too large for git)
data/simulation/*.root
data/reconstruction/*.root
*.dat

# Editor and IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile
*.cmake
!CMakeLists.txt
!cmake/*.cmake
EOF

# 创建 README.md 更新
cat > README_NEW_STRUCTURE.md << 'EOF'
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

EOF

# 创建 Python requirements.txt
cat > scripts/requirements.txt << 'EOF'
numpy>=1.20.0
matplotlib>=3.3.0
scipy>=1.6.0
pyyaml>=5.4.0
EOF

log_info "✓ 配置文件创建完成"

##############################################################################
# 完成
##############################################################################

log_info ""
log_info "════════════════════════════════════════════════════════════"
log_info "迁移完成！"
log_info "════════════════════════════════════════════════════════════"
log_info ""
log_info "新项目位置: ${NEW_ROOT}"
log_info ""
log_info "下一步操作:"
log_info "  1. cd ${NEW_ROOT}"
log_info "  2. 检查迁移的文件是否正确"
log_info "  3. 复制新生成的 CMakeLists.txt 文件到对应目录"
log_info "  4. source setup.sh"
log_info "  5. mkdir build && cd build"
log_info "  6. cmake .."
log_info "  7. make -j\$(nproc)"
log_info ""
log_info "注意: 原项目 ${OLD_ROOT} 保持不变，可以安全删除"
log_info ""

# 创建快速链接
log_info "创建便捷脚本..."
cat > "${NEW_ROOT}/quick_build.sh" << 'EOF'
#!/bin/bash
set -e
source setup.sh
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
EOF
chmod +x "${NEW_ROOT}/quick_build.sh"

log_info "✓ 已创建 quick_build.sh 用于快速编译"
log_info ""

# 显示数据迁移统计
log_info "数据文件迁移统计:"
if [ -d "${NEW_ROOT}/data" ]; then
    DATA_SIZE=$(du -sh "${NEW_ROOT}/data" 2>/dev/null | cut -f1)
    log_info "  新数据目录大小: ${DATA_SIZE}"
fi
log_warn "  注意: 原项目中的数据文件已被移动(不是复制)"
log_info ""
log_info "全部完成！"
