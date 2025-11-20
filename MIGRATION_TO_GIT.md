# Git 迁移指南 - 保留完整历史

## 方案一：在原仓库中创建重构分支（推荐）

这种方法可以保留完整的 Git 历史，并且能清晰地看到重构前后的对比。

### 步骤：

```bash
# 1. 进入旧项目目录
cd /home/tian/workspace/dpol/smsimulator5.5

# 2. 确保当前分支干净
git status
git add -A
git commit -m "Backup before restructuring"

# 3. 创建重构分支
git checkout -b restructure-cmake

# 4. 删除旧结构的文件（但保留 .git）
find . -maxdepth 1 ! -name '.git' ! -name '.' ! -name '..' -exec rm -rf {} +

# 5. 复制新结构的所有文件
cp -r /home/tian/workspace/dpol/smsimulator5.5_new/* .
cp -r /home/tian/workspace/dpol/smsimulator5.5_new/.gitignore .

# 6. 添加所有改变
git add -A

# 7. 提交重构
git commit -m "Major restructuring: Modern CMake-based modular architecture

- Reorganized into libs/apps/configs/data hierarchy
- Implemented modular library system (smdata, smaction, etc.)
- Added ROOT dictionary generation
- Created comprehensive CMake build system
- Separated data, configs, and source code
- Added helper scripts (build.sh, run_sim.sh, etc.)
- Updated documentation"

# 8. 查看改变
git log --oneline --graph -10

# 9. （可选）合并回主分支
git checkout main
git merge restructure-cmake
```

### 优点：
- ✅ 保留完整的 Git 历史
- ✅ 可以随时对比重构前后
- ✅ 可以用 `git diff` 查看所有改变
- ✅ 保留所有 commit 信息和作者信息
- ✅ 可以回退到任何历史版本

---

## 方案二：将新目录初始化为新仓库

如果您想要全新开始，但保留旧仓库作为参考：

```bash
# 1. 初始化新仓库
cd /home/tian/workspace/dpol/smsimulator5.5_new
git init

# 2. 添加所有文件
git add -A

# 3. 首次提交
git commit -m "Initial commit: Restructured SMSimulator with modern CMake

Complete project restructuring:
- Modular library architecture
- Modern CMake build system  
- Separated configs, data, and source code
- ROOT dictionary generation
- Comprehensive documentation"

# 4. 添加远程仓库（如果有）
git remote add origin <your-remote-url>

# 5. 推送
git push -u origin main
```

### 优点：
- ✅ 干净的开始
- ✅ 没有历史包袱
- ✅ 简单直接

### 缺点：
- ❌ 丢失所有历史记录
- ❌ 无法追溯以前的改变

---

## 方案三：使用 git-filter-repo 重写历史（高级）

保留历史的同时重写整个仓库结构：

```bash
# 需要安装 git-filter-repo
pip install git-filter-repo

# 这个方法比较复杂，通常不推荐，除非你有特殊需求
```

---

## 推荐流程

**我强烈推荐使用方案一**，因为：

1. **保留历史**：所有的开发历史、commit 信息都保留
2. **可追溯性**：可以看到每个功能是何时添加的
3. **可对比**：用 `git diff main restructure-cmake` 看所有改变
4. **可回退**：如果发现问题可以回到旧版本
5. **团队协作**：其他开发者可以看到完整的项目演进

### 具体操作：

```bash
# 一键执行方案一
cd /home/tian/workspace/dpol/smsimulator5.5

# 备份当前状态
git add -A
git commit -m "Backup before major restructuring" || true

# 创建并切换到重构分支
git checkout -b restructure-cmake

# 清空当前目录（保留 .git 和 d_work）
find . -maxdepth 1 \
  ! -name '.git' \
  ! -name 'd_work' \
  ! -name 'anaroot' \
  ! -name '.' \
  ! -name '..' \
  -exec rm -rf {} +

# 复制新结构
rsync -av --exclude='.git' \
  /home/tian/workspace/dpol/smsimulator5.5_new/ \
  /home/tian/workspace/dpol/smsimulator5.5/

# 添加并提交
git add -A
git commit -m "Major restructuring: Modern CMake-based architecture

Changes:
- libs/: Modular library structure (smg4lib, sim_deuteron_core, analysis)
- apps/: Separate application code
- configs/: Centralized configuration management
- data/: Organized data directories (simulation, reconstruction, input)
- CMake: Modern build system with find modules
- Scripts: Helper scripts (build.sh, run_sim.sh, etc.)
- Docs: Comprehensive documentation
- Tests: Test infrastructure

Built and tested successfully with:
- Geant4 11.2.2
- ROOT 6.36.04
- CMake 3.31"

# 查看状态
echo "✅ 重构分支创建完成！"
git log --oneline -5
```

---

## 迁移后的工作流

```bash
# 在重构分支工作
git checkout restructure-cmake
# ... 做修改 ...
git add -A
git commit -m "Your changes"

# 需要回到旧版本查看某些东西
git checkout main

# 重构稳定后，合并到主分支
git checkout main
git merge restructure-cmake
```

---

## 清理旧的 smsimulator5.5_new 目录

重构完成并确认无误后：

```bash
# 确保新分支已经推送到远程
cd /home/tian/workspace/dpol/smsimulator5.5
git push origin restructure-cmake

# 删除临时目录
rm -rf /home/tian/workspace/dpol/smsimulator5.5_new

# 以后直接使用
cd /home/tian/workspace/dpol/smsimulator5.5
git checkout restructure-cmake  # 使用新结构
```
