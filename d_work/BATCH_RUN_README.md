# Batch Run Scripts for Y-Polarization Data

这个文件夹包含用于批量运行Y极化数据的脚本。

## 文件说明

- `batch_run_ypol.sh` - Bash版本的批量运行脚本
- `batch_run_ypol.py` - Python版本的批量运行脚本（推荐）
- 生成的临时宏文件将存储在 `macros_temp/` 目录

## 输入输出结构

### 输入文件
位置: `$SMSIMDIR/d_work/rootfiles/ypol/`

文件格式: `ypol_np_{target}_g{gamma}.root`

当前文件:
```
ypol_np_Pb208_g050.root
ypol_np_Pb208_g060.root
ypol_np_Pb208_g070.root
ypol_np_Pb208_g080.root
ypol_np_Xe130_g050.root
ypol_np_Xe130_g060.root
ypol_np_Xe130_g070.root
ypol_np_Xe130_g080.root
```

### 输出文件
位置: `$SMSIMDIR/d_work/output_tree/ypol/`

目录结构:
```
output_tree/ypol/
├── Pb208_g050/
│   ├── ypol_np_Pb208_g0500000.root
│   └── ypol_np_Pb208_g050.log
├── Pb208_g060/
│   ├── ypol_np_Pb208_g0600000.root
│   └── ypol_np_Pb208_g060.log
├── Xe130_g050/
│   ├── ypol_np_Xe130_g0500000.root
│   └── ypol_np_Xe130_g050.log
└── ...
```

## 使用方法

### 方法1: 使用Python脚本（推荐）

```bash
# 运行1000个事件（默认）
./batch_run_ypol.py

# 运行指定数量的事件
./batch_run_ypol.py 5000

# 或者
python3 batch_run_ypol.py 5000
```

### 方法2: 使用Bash脚本

```bash
# 运行1000个事件（默认）
./batch_run_ypol.sh

# 运行指定数量的事件
./batch_run_ypol.sh 5000
```

## 功能特点

1. **自动处理所有文件**: 自动扫描ypol文件夹中的所有ROOT文件
2. **结构化输出**: 按target和gamma组织输出文件
3. **日志记录**: 每次运行都会生成日志文件，便于调试
4. **进度显示**: 彩色输出显示处理进度和状态
5. **错误处理**: 捕获并报告运行错误
6. **清理选项**: 运行完成后可选择删除临时宏文件

## 生成的宏文件示例

每个输入文件会生成一个对应的宏文件，例如 `run_ypol_np_Pb208_g050.mac`:

```
# Auto-generated macro for ypol_np_Pb208_g050
# Generated on: 2025-11-09 12:00:00

# File settings
/action/file/OverWrite y
/action/file/RunName ypol_np_Pb208_g050
/action/file/SaveDirectory $SMSIMDIR/d_work/output_tree/ypol/Pb208_g050

# Load geometry
/control/execute $SMSIMDIR/d_work/geometry/5deg_1.2T.mac

# Tracking settings
/tracking/storeTrajectory 1

# Gun settings - use Tree input
/action/gun/Type Tree
/action/gun/tree/InputFileName $SMSIMDIR/d_work/rootfiles/ypol/ypol_np_Pb208_g050.root
/action/gun/tree/TreeName tree

# Run simulation
/action/gun/tree/beamOn 1000

# Exit
exit
```

## 输出文件说明

每个target+gamma组合会生成:
- `{run_name}0000.root` - 模拟输出的ROOT文件
- `{run_name}.log` - 运行日志文件

## 注意事项

1. 确保 `SMSIMDIR` 环境变量已设置
2. 确保 `smsimulator` 可执行文件存在于 `$SMSIMDIR/bin/`
3. 确保几何配置文件存在: `$SMSIMDIR/d_work/geometry/5deg_1.2T.mac`
4. 运行前确保有足够的磁盘空间

## 故障排查

如果脚本运行失败:

1. 检查 `SMSIMDIR` 是否正确设置:
   ```bash
   echo $SMSIMDIR
   ```

2. 检查 smsimulator 是否存在:
   ```bash
   ls -l $SMSIMDIR/bin/smsimulator
   ```

3. 查看日志文件了解具体错误:
   ```bash
   cat output_tree/ypol/{target}_{gamma}/{run_name}.log
   ```

4. 手动运行单个宏文件测试:
   ```bash
   cd $SMSIMDIR/d_work
   $SMSIMDIR/bin/smsimulator macros_temp/run_ypol_np_Pb208_g050.mac
   ```

## 后续分析

运行完成后，可以使用以下方式分析输出:

```bash
# 查看所有输出文件
ls -lh output_tree/ypol/*/*.root

# 使用ROOT查看单个文件
root -l output_tree/ypol/Pb208_g050/ypol_np_Pb208_g0500000.root
```

## 修改和定制

如需修改模拟参数，编辑脚本中的 `create_macro_file` 函数，例如:
- 添加可视化选项
- 修改几何配置
- 调整追踪设置

---
Created: 2025-11-09
Last updated: 2025-11-09
