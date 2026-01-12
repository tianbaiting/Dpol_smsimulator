
SMSimulator

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/tianbaiting/Dpol_smsimulator)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/tianbaiting/Dpol_smsimulator)

<a href="https://deepwiki.com/tianbaiting/Dpol_smsimulator"><img src="https://deepwiki.com/badge.svg" alt="Ask DeepWiki"></a>
🚀 快速命令

  编译项目:      ./build.sh
  运行模拟:      ./run_sim.sh
  运行测试:      ./test.sh
  清理构建:      ./clean.sh



📂 重要目录
```
  libs/                     C++ 库源码
    ├─ smg4lib/            Geant4 核心库
    ├─ sim_deuteron_core/  氘核模拟核心
    └─ analysis/           数据分析库

  apps/                     可执行程序
    ├─ sim_deuteron/       主模拟程序
    └─ tools/              工具程序

  configs/                  配置文件
    ├─ simulation/macros/  Geant4 宏文件
    └─ simulation/geometry/ 几何配置

  data/                     数据目录
    ├─ input/              输入数据
    ├─ simulation/         模拟输出
    └─ reconstruction/     重建输出

  scripts/                  Python 工具
    ├─ analysis/           分析脚本
    ├─ batch/              批处理脚本
    └─ visualization/      可视化脚本
```

🔧 编译选项

  Release 模式:   cmake .. -DCMAKE_BUILD_TYPE=Release
  Debug 模式:     cmake .. -DCMAKE_BUILD_TYPE=Debug
  不构建测试:     cmake .. -DBUILD_TESTS=OFF
  不用 ANAROOT:   cmake .. -DWITH_ANAROOT=OFF


📝 运行示例

  基本模拟:
    ./run_sim.sh configs/simulation/macros/simulation.mac

  可视化模拟:
    ./run_sim.sh configs/simulation/macros/simulation_vis.mac

  交互模式:
    ./run_sim.sh configs/simulation/macros/simulation_interactive.mac

  直接运行:
    ./bin/sim_deuteron your_macro.mac



🧪 测试

  运行所有测试:   ./test.sh
  详细测试:       cd build && ctest --verbose
  单个测试:       cd build && ctest -R test_name


📊 数据分析

  ROOT 分析:
    cd configs/simulation/macros
    root -l run_display.C

  Python 分析:
    cd scripts/analysis
    python analyze_*.py

  批量运行:
    cd scripts/batch
    python batch_run_ypol.py


🔍 调试

  详细编译:       make VERBOSE=1
  CMake 调试:     cmake .. --trace
  检查依赖:       ldd bin/sim_deuteron


📚 文档

  构建指南:       docs/BUILD_GUIDE.md
  使用说明:       HOW_TO_BUILD_AND_RUN.md
  DPOL 报告:      docs/DPOL_Analysis_Report.md
  磁场说明:       docs/MagneticField_README.md
  粒子轨迹:       docs/PARTICLE_TRAJECTORY_SUMMARY.md


⚡ 常用工作流

  【开发流程】
  1. 编辑源码
  2. ./build.sh
  3. ./test.sh
  4. ./run_sim.sh configs/simulation/macros/test.mac

  【批量模拟】
  1. 编辑配置文件
  2. cd scripts/batch
  3. python batch_run_ypol.py
  4. cd ../analysis && python analyze_results.py

  【重新编译单个模块】
  1. cd build
  2. make <target_name>
  3. 例如: make sim_deuteron


🐛 常见问题

  Q: 找不到 ROOT
  A: source $ROOTSYS/bin/thisroot.sh

  Q: 找不到 Geant4
  A: source /path/to/geant4.sh

  Q: 找不到 ANAROOT
  A: export TARTSYS=/path/to/anaroot
     或 cmake .. -DWITH_ANAROOT=OFF

  Q: 编译失败
  A: 1) 检查环境变量
     2) 尝试 make VERBOSE=1 查看详细信息
     3) 删除 build 目录重新编译


💡 提示

  • 首次编译需要较长时间，请耐心等待
  • 使用 -j$(nproc) 可以并行编译
  • Debug 模式有更多调试信息但运行较慢
  • 定期运行 ./test.sh 确保代码正确性
  • 查看 CMake 配置摘要确认依赖是否正确找到





仓库: https://github.com/tianbaiting/Dpol_smsimulator

