# tests/integration/CMakeLists.txt
# PDC位置计算测试程序

message(STATUS "Configuring integration tests...")

##############################################################################
# PDC位置计算测试
##############################################################################

# 测试1: 旋转一致性测试 (独立测试，不依赖外部库)
# 放在前面，避免被return()跳过
##############################################################################

add_executable(test_rotation_consistency
    test_rotation_consistency.cc
)

target_include_directories(test_rotation_consistency PRIVATE
    ${ROOT_INCLUDE_DIRS}
)

target_link_libraries(test_rotation_consistency PRIVATE
    ${ROOT_LIBRARIES}
)

set_target_properties(test_rotation_consistency PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "Rotation consistency test configured: test_rotation_consistency")

##############################################################################
# 测试2: PDC位置测试 (完整测试，依赖磁场库)
##############################################################################

# 检查是否有必要的库
if(NOT TARGET analysis)
    message(WARNING "analysis library not found, skipping PDC position test")
else()
if(NOT TARGET GeoAcceptance)
    message(WARNING "GeoAcceptance library not found, skipping PDC position test")
else()

add_executable(test_pdc_position
    test_pdc_position.cc
)

target_include_directories(test_pdc_position PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/analysis/include
    ${CMAKE_SOURCE_DIR}/libs/geo_accepentce/include
    ${CMAKE_SOURCE_DIR}/libs/smlogger/include
    ${ROOT_INCLUDE_DIRS}
)

target_link_libraries(test_pdc_position PRIVATE
    GeoAcceptance
    analysis
    smlogger
    ${ROOT_LIBRARIES}
)

# 设置输出目录
set_target_properties(test_pdc_position PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "PDC position test configured: test_pdc_position")

endif()  # GeoAcceptance
endif()  # analysis

##############################################################################
# 便捷运行脚本
##############################################################################

# 创建运行脚本
file(WRITE ${CMAKE_BINARY_DIR}/bin/run_pdc_test.sh
"#!/bin/bash
# PDC位置计算测试运行脚本

# 默认参数
FIELD_MAP=\"${CMAKE_SOURCE_DIR}/configs/simulation/geometry/filed_map/180626-1,00T-6000.table\"
ROTATION_ANGLE=30

# 解析参数
if [ \$# -ge 1 ]; then
    FIELD_MAP=\"\$1\"
fi
if [ \$# -ge 2 ]; then
    ROTATION_ANGLE=\"\$2\"
fi

echo \"======================================\"
echo \"PDC Position Calculation Test\"
echo \"======================================\"
echo \"Field map: \$FIELD_MAP\"
echo \"Rotation angle: \$ROTATION_ANGLE degrees\"
echo \"\"

cd ${CMAKE_BINARY_DIR}/bin
./test_pdc_position \"\$FIELD_MAP\" \"\$ROTATION_ANGLE\"
")

# 创建旋转测试脚本
file(WRITE ${CMAKE_BINARY_DIR}/bin/run_rotation_test.sh
"#!/bin/bash
# 旋转一致性测试运行脚本

echo \"======================================\"
echo \"Rotation Consistency Test\"
echo \"======================================\"

cd ${CMAKE_BINARY_DIR}/bin
./test_rotation_consistency
")

message(STATUS "Run scripts created in ${CMAKE_BINARY_DIR}/bin/")
