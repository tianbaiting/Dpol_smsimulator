# tests/unit/CMakeLists.txt
# 单元测试

include(GoogleTest)

##############################################################################
# 测试可执行文件
##############################################################################

# Analysis-specific tests live in tests/analysis/.

# Default-value regression tests for DeutDetectorConstruction
add_executable(test_DeutDetectorConstruction_defaults
    test_DeutDetectorConstruction_defaults.cc
)
target_link_libraries(test_DeutDetectorConstruction_defaults PRIVATE
    sim_deuteron_core
    smg4lib
    GTest::gtest
    GTest::gtest_main
    ${Geant4_LIBRARIES}
    ${ROOT_LIBRARIES}
)
gtest_discover_tests(test_DeutDetectorConstruction_defaults
    PROPERTIES LABELS "unit"
)
