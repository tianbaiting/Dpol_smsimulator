#!/bin/bash
# 生成文件验证脚本

echo "════════════════════════════════════════════════════════════════"
echo "  SMSimulator 项目重构 - 文件生成验证"
echo "════════════════════════════════════════════════════════════════"
echo ""

CURRENT_DIR=$(pwd)
ERRORS=0

# 检查函数
check_file() {
    if [ -f "$1" ]; then
        echo "✓ $1"
        return 0
    else
        echo "✗ $1 (缺失)"
        ERRORS=$((ERRORS + 1))
        return 1
    fi
}

echo "【1】检查主脚本..."
check_file "auto_migrate_and_setup.sh"
check_file "migrate_to_new_structure.sh"
check_file "generate_guides.sh"
echo ""

echo "【2】检查主 CMake 文件..."
check_file "CMakeLists_NEW.txt"
echo ""

echo "【3】检查 cmake_new/ 目录中的文件..."
check_file "cmake_new/FindANAROOT.cmake"
check_file "cmake_new/FindXercesC.cmake"
check_file "cmake_new/libs_CMakeLists.txt"
check_file "cmake_new/libs_smg4lib_CMakeLists.txt"
check_file "cmake_new/libs_smg4lib_data_CMakeLists.txt"
check_file "cmake_new/libs_smg4lib_action_CMakeLists.txt"
check_file "cmake_new/libs_smg4lib_construction_CMakeLists.txt"
check_file "cmake_new/libs_smg4lib_physics_CMakeLists.txt"
check_file "cmake_new/libs_sim_deuteron_core_CMakeLists.txt"
check_file "cmake_new/libs_analysis_CMakeLists.txt"
check_file "cmake_new/apps_CMakeLists.txt"
check_file "cmake_new/apps_sim_deuteron_CMakeLists.txt"
check_file "cmake_new/apps_run_reconstruction_CMakeLists.txt"
check_file "cmake_new/apps_tools_CMakeLists.txt"
check_file "cmake_new/tests_CMakeLists.txt"
check_file "cmake_new/tests_unit_CMakeLists.txt"
check_file "cmake_new/tests_integration_CMakeLists.txt"
echo ""

echo "【4】检查文档..."
check_file "GENERATED_FILES_SUMMARY.md"
check_file "README_MIGRATION.txt"
echo ""

echo "【5】检查脚本执行权限..."
for script in auto_migrate_and_setup.sh migrate_to_new_structure.sh generate_guides.sh; do
    if [ -x "$script" ]; then
        echo "✓ $script 有执行权限"
    else
        echo "✗ $script 没有执行权限"
        ERRORS=$((ERRORS + 1))
    fi
done
echo ""

echo "════════════════════════════════════════════════════════════════"
if [ $ERRORS -eq 0 ]; then
    echo "✅ 所有文件已正确生成！"
    echo ""
    echo "下一步："
    echo "  执行: ./auto_migrate_and_setup.sh"
    echo "  或查看: cat README_MIGRATION.txt"
else
    echo "⚠️  发现 $ERRORS 个问题，请检查上述输出"
fi
echo "════════════════════════════════════════════════════════════════"
echo ""

# 显示文件统计
echo "📊 文件统计："
echo "  脚本文件: $(ls -1 *.sh 2>/dev/null | wc -l)"
echo "  CMake 文件: $(ls -1 cmake_new/*.txt cmake_new/*.cmake 2>/dev/null | wc -l)"
echo "  文档文件: $(ls -1 *.md *.txt 2>/dev/null | wc -l)"
echo ""
