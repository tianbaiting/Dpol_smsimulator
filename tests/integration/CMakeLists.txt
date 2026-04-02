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
# 测试3: PDC 靶点动量 Geant4 端到端回归
##############################################################################

set(PDC_TARGET_MOMENTUM_E2E_SRC ${CMAKE_CURRENT_SOURCE_DIR}/test_pdc_target_momentum_e2e.cc)
set(PDC_TARGET_GEOMETRY_MACRO
    ${CMAKE_SOURCE_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
)
set(PDC_TARGET_FIELD_MAP
    ${CMAKE_SOURCE_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table
)
set(PDC_TARGET_NN_MODEL
    ${CMAKE_SOURCE_DIR}/data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model_cpp.json
)

if(TARGET smdata AND TARGET sim_deuteron AND TARGET reconstruct_target_momentum AND TARGET evaluate_target_momentum_reco)
if(EXISTS ${PDC_TARGET_MOMENTUM_E2E_SRC} AND EXISTS ${PDC_TARGET_GEOMETRY_MACRO} AND EXISTS ${PDC_TARGET_FIELD_MAP} AND EXISTS ${PDC_TARGET_NN_MODEL})

add_executable(test_pdc_target_momentum_e2e
    test_pdc_target_momentum_e2e.cc
)

target_include_directories(test_pdc_target_momentum_e2e PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/smg4lib/include
    ${ROOT_INCLUDE_DIRS}
)

target_link_libraries(test_pdc_target_momentum_e2e PRIVATE
    smdata
    ${ROOT_LIBRARIES}
)

set_target_properties(test_pdc_target_momentum_e2e PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output/pdc_target_momentum_scan)

function(add_pdc_target_momentum_e2e_test name backend output_tag rk_fit_mode require_gate require_min_matched)
    set(test_args
        --backend ${backend}
        --smsimdir ${CMAKE_SOURCE_DIR}
        --output-dir ${CMAKE_BINARY_DIR}/test_output/pdc_target_momentum_scan/${output_tag}
        --sim-bin $<TARGET_FILE:sim_deuteron>
        --reco-bin $<TARGET_FILE:reconstruct_target_momentum>
        --eval-bin $<TARGET_FILE:evaluate_target_momentum_reco>
        --geometry-macro ${PDC_TARGET_GEOMETRY_MACRO}
        --field-map ${PDC_TARGET_FIELD_MAP}
        --nn-model-json ${PDC_TARGET_NN_MODEL}
        --threshold-px 20
        --threshold-py 15
        --threshold-pz 20
        --seed-a 20260326
        --seed-b 20260327
        --require-gate-pass ${require_gate}
        --require-min-matched ${require_min_matched}
    )

    if(NOT "${rk_fit_mode}" STREQUAL "")
        list(APPEND test_args --rk-fit-mode ${rk_fit_mode})
    endif()

    add_test(
        NAME ${name}
        COMMAND test_pdc_target_momentum_e2e ${test_args}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    set_tests_properties(${name} PROPERTIES
        LABELS "integration;analysis;reconstruction;pdc"
        TIMEOUT 300
    )
endfunction()

# [EN] Keep these Geant4 end-to-end tests report-first until the canonical B115T fixtures are accurate enough for hard gating. / [CN] 在canonical B115T资产精度足够稳定前，这组Geant4端到端测试先以报表回归为主，不启用硬门禁。
add_pdc_target_momentum_e2e_test(PdcTargetMomentumE2E.RK.TwoPointBackprop rk rk_two_point_backprop two-point-backprop 0 1)
add_pdc_target_momentum_e2e_test(PdcTargetMomentumE2E.RK.FixedTargetPdcOnly rk rk_fixed_target_pdc_only fixed-target-pdc-only 0 1)
add_pdc_target_momentum_e2e_test(PdcTargetMomentumE2E.RK.ThreePointFree rk rk_three_point_free three-point-free 0 1)
add_pdc_target_momentum_e2e_test(PdcTargetMomentumE2E.MultiDim multidim multidim "" 0 0)
add_pdc_target_momentum_e2e_test(PdcTargetMomentumE2E.NN nn nn "" 0 0)

message(STATUS "PDC target-momentum E2E tests configured")

else()
message(WARNING "PDC target-momentum E2E assets missing, skipping Geant4 reconstruction regression tests")
endif()
else()
message(WARNING "Required runtime targets missing, skipping PDC target-momentum E2E tests")
endif()

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
