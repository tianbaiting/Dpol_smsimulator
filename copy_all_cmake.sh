#!/bin/bash
set -e

echo "复制 CMake 文件到新项目..."

NEW_DIR="smsimulator5.5_new"

# 复制主 CMakeLists.txt
cp CMakeLists_NEW.txt "${NEW_DIR}/CMakeLists.txt"
echo "✓ 主 CMakeLists.txt"

# 创建并复制 cmake 模块
mkdir -p "${NEW_DIR}/cmake"
cp cmake_new/FindANAROOT.cmake "${NEW_DIR}/cmake/"
cp cmake_new/FindXercesC.cmake "${NEW_DIR}/cmake/"
echo "✓ cmake 模块"

# 复制 libs CMakeLists
cp cmake_new/libs_CMakeLists.txt "${NEW_DIR}/libs/CMakeLists.txt"
echo "✓ libs/CMakeLists.txt"

# 复制 smg4lib CMakeLists
cp cmake_new/libs_smg4lib_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/CMakeLists.txt"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/data"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/action"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/construction"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/physics"
cp cmake_new/libs_smg4lib_data_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/data/CMakeLists.txt"
cp cmake_new/libs_smg4lib_action_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/action/CMakeLists.txt"
cp cmake_new/libs_smg4lib_construction_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/construction/CMakeLists.txt"
cp cmake_new/libs_smg4lib_physics_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/physics/CMakeLists.txt"
echo "✓ smg4lib CMakeLists"

# 复制其他 libs
cp cmake_new/libs_sim_deuteron_core_CMakeLists.txt "${NEW_DIR}/libs/sim_deuteron_core/CMakeLists.txt"
cp cmake_new/libs_analysis_CMakeLists.txt "${NEW_DIR}/libs/analysis/CMakeLists.txt"
echo "✓ sim_deuteron_core 和 analysis CMakeLists"

# 复制 apps CMakeLists
mkdir -p "${NEW_DIR}/apps/sim_deuteron"
mkdir -p "${NEW_DIR}/apps/run_reconstruction"
mkdir -p "${NEW_DIR}/apps/tools"
cp cmake_new/apps_CMakeLists.txt "${NEW_DIR}/apps/CMakeLists.txt"
cp cmake_new/apps_sim_deuteron_CMakeLists.txt "${NEW_DIR}/apps/sim_deuteron/CMakeLists.txt"
cp cmake_new/apps_run_reconstruction_CMakeLists.txt "${NEW_DIR}/apps/run_reconstruction/CMakeLists.txt"
cp cmake_new/apps_tools_CMakeLists.txt "${NEW_DIR}/apps/tools/CMakeLists.txt"
echo "✓ apps CMakeLists"

# 复制 tests CMakeLists
mkdir -p "${NEW_DIR}/tests/unit"
mkdir -p "${NEW_DIR}/tests/integration"
cp cmake_new/tests_CMakeLists.txt "${NEW_DIR}/tests/CMakeLists.txt"
cp cmake_new/tests_unit_CMakeLists.txt "${NEW_DIR}/tests/unit/CMakeLists.txt"
cp cmake_new/tests_integration_CMakeLists.txt "${NEW_DIR}/tests/integration/CMakeLists.txt"
echo "✓ tests CMakeLists"

echo ""
echo "✅ 所有 CMake 文件已复制完成！"
