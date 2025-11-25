#!/bin/bash
# Git 迁移脚本 - 将重构后的项目合并到原 Git 仓库
# 保留完整的历史记录

set -e  # 遇到错误立即退出

OLD_DIR="$SMSIMDIR"
NEW_DIR="$SMSIMDIR_new"
BRANCH_NAME="restructure-cmake"

echo "=========================================="
echo "SMSimulator Git 迁移脚本"
echo "=========================================="
echo ""

# 检查目录是否存在
if [ ! -d "$OLD_DIR" ]; then
    echo "❌ 错误：旧目录不存在：$OLD_DIR"
    exit 1
fi

if [ ! -d "$NEW_DIR" ]; then
    echo "❌ 错误：新目录不存在：$NEW_DIR"
    exit 1
fi

# 进入旧目录
cd "$OLD_DIR"

# 检查是否是 Git 仓库
if [ ! -d ".git" ]; then
    echo "❌ 错误：$OLD_DIR 不是 Git 仓库"
    exit 1
fi

echo "📁 当前目录：$OLD_DIR"
echo ""

# 显示当前状态
echo "当前 Git 状态："
git status --short

# 询问是否继续
echo ""
read -p "是否继续迁移？这将创建新分支 '$BRANCH_NAME' (y/n): " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "❌ 取消操作"
    exit 0
fi

# 步骤 1: 保存当前工作
echo ""
echo "步骤 1/6: 备份当前状态..."
git add -A
if git diff --cached --quiet; then
    echo "✓ 没有需要提交的改变"
else
    git commit -m "Backup before major restructuring - $(date '+%Y-%m-%d %H:%M:%S')" || true
    echo "✓ 当前状态已备份"
fi

# 步骤 2: 检查分支是否已存在
echo ""
echo "步骤 2/6: 检查分支..."
if git show-ref --verify --quiet "refs/heads/$BRANCH_NAME"; then
    echo "⚠️  分支 '$BRANCH_NAME' 已存在"
    read -p "是否删除并重新创建？(y/n): " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git branch -D "$BRANCH_NAME"
        echo "✓ 已删除旧分支"
    else
        echo "❌ 取消操作"
        exit 0
    fi
fi

# 步骤 3: 创建并切换到新分支
echo ""
echo "步骤 3/6: 创建重构分支..."
git checkout -b "$BRANCH_NAME"
echo "✓ 已创建并切换到分支: $BRANCH_NAME"

# 步骤 4: 清空当前目录（保留重要文件）
echo ""
echo "步骤 4/6: 清理旧文件结构..."
# 保留 .git, d_work, anaroot, otherProject
find . -maxdepth 1 \
  ! -name '.git' \
  ! -name 'd_work' \
  ! -name 'anaroot' \
  ! -name 'otherProject' \
  ! -name '.' \
  ! -name '..' \
  -exec rm -rf {} + 2>/dev/null || true
echo "✓ 旧文件结构已清理"

# 步骤 5: 复制新结构
echo ""
echo "步骤 5/6: 复制新项目结构..."
# 使用 rsync 复制，排除 .git
rsync -av --exclude='.git' \
  --exclude='d_work' \
  --exclude='build/' \
  --exclude='*.log' \
  "$NEW_DIR/" "$OLD_DIR/"
echo "✓ 新结构已复制"

# 步骤 6: 提交改变
echo ""
echo "步骤 6/6: 提交重构..."
git add -A

# 创建详细的提交信息
COMMIT_MSG="Major restructuring: Modern CMake-based modular architecture

This is a complete project restructuring to modernize the build system
and improve code organization.

## New Structure:
- libs/: Modular library architecture
  - smg4lib/: Core Geant4 simulation libraries
    - data/: Data structures with ROOT dictionaries
    - action/: Geant4 action classes
    - construction/: Detector construction
    - physics/: Physics lists
  - sim_deuteron_core/: Deuteron-specific simulation
  - analysis/: PDC analysis and reconstruction tools

- apps/: Application executables
  - sim_deuteron/: Main simulation program
  - tools/: Utility applications and ROOT macros

- configs/: Configuration files
  - simulation/macros/: Geant4 macro files
  - simulation/geometry/: Geometry configurations
  - batch/: Batch processing configs

- data/: Data management
  - simulation/output_tree/: Simulation outputs
  - simulation/rootfiles/: Input ROOT files
  - reconstruction/: Reconstruction results
  - input/rawdata/: Raw experimental data
  - magnetic_field/: Magnetic field maps
  - calibration/: Calibration parameters

## Build System:
- Modern CMake 3.16+ with modular design
- Custom find modules for ANAROOT and XercesC
- ROOT dictionary generation for custom classes
- Separate library compilation
- Proper dependency management

## Helper Scripts:
- build.sh: Quick build script
- clean.sh: Clean build artifacts
- run_sim.sh: Run simulations
- test.sh: Run tests
- setup.sh: Environment configuration

## Documentation:
- HOW_TO_BUILD_AND_RUN.md: Build and usage guide
- RUNNING_GUIDE.md: Detailed running instructions
- QUICK_REFERENCE.txt: Quick command reference
- README_NEW_STRUCTURE.md: Structure explanation

## Testing:
Successfully built and tested with:
- Geant4 11.2.2
- ROOT 6.36.04
- CMake 3.31
- GCC 14.2.0
- ANAROOT (via TARTSYS)

Date: $(date '+%Y-%m-%d %H:%M:%S')
"

git commit -m "$COMMIT_MSG"
echo "✓ 重构已提交"

# 显示结果
echo ""
echo "=========================================="
echo "✅ 迁移完成！"
echo "=========================================="
echo ""
echo "当前分支: $(git branch --show-current)"
echo ""
echo "最近的提交："
git log --oneline -5
echo ""
echo "=========================================="
echo "后续操作："
echo "=========================================="
echo ""
echo "1. 查看改变："
echo "   git diff main..$BRANCH_NAME --stat"
echo ""
echo "2. 测试新结构："
echo "   source setup.sh"
echo "   ./build.sh"
echo "   ./bin/sim_deuteron configs/simulation/macros/test_pencil.mac"
echo ""
echo "3. 确认无误后合并到主分支："
echo "   git checkout main"
echo "   git merge $BRANCH_NAME"
echo ""
echo "4. 推送到远程："
echo "   git push origin $BRANCH_NAME"
echo "   git push origin main"
echo ""
echo "5. 清理临时目录（确认无误后）："
echo "   rm -rf $NEW_DIR"
echo ""
