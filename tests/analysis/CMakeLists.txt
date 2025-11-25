# Analysis Library Tests

# 可视化工具库 (仅在测试中使用)
add_library(reconstruction_visualizer STATIC
    src/ReconstructionVisualizer.cc
)

target_include_directories(reconstruction_visualizer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/analysis/include
)

target_link_libraries(reconstruction_visualizer
    ${ROOT_LIBRARIES}
    analysis
)

# ParticleTrajectory 单元测试
add_executable(test_ParticleTrajectory
    test_ParticleTrajectory.cc
)

target_link_libraries(test_ParticleTrajectory
    GTest::gtest
    GTest::gtest_main
    analysis
    ${ROOT_LIBRARIES}
    ROOT::Geom
    ROOT::Eve
    ROOT::Minuit
)

gtest_discover_tests(test_ParticleTrajectory
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;analysis;trajectory"
)

# TargetReconstructor 单元测试
add_executable(test_TargetReconstructor
    test_TargetReconstructor.cc
)

target_include_directories(test_TargetReconstructor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(test_TargetReconstructor
    GTest::gtest
    GTest::gtest_main
    analysis
    reconstruction_visualizer
    ${ROOT_LIBRARIES}
    ROOT::Geom
    ROOT::Eve
    ROOT::Minuit
)

gtest_discover_tests(test_TargetReconstructor
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;analysis;reconstruction"
        ENVIRONMENT "SM_TEST_VISUALIZATION=OFF"  # 默认关闭可视化
)

# TargetReconstructor 真实数据测试
add_executable(test_TargetReconstructor_RealData
    test_TargetReconstructor_RealData.cc
)

target_include_directories(test_TargetReconstructor_RealData PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(test_TargetReconstructor_RealData
    GTest::gtest
    GTest::gtest_main
    analysis
    reconstruction_visualizer
    ${ROOT_LIBRARIES}
    ROOT::Geom
    ROOT::Eve
    ROOT::Minuit
)

gtest_discover_tests(test_TargetReconstructor_RealData
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "unit;analysis;reconstruction;realdata"
        ENVIRONMENT "SM_TEST_VISUALIZATION=OFF"  # 默认关闭可视化
)

# 带可视化的测试 (单独的 CTest 目标)
add_test(
    NAME test_TargetReconstructor_Visualization
    COMMAND test_TargetReconstructor
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_tests_properties(test_TargetReconstructor_Visualization
    PROPERTIES
        LABELS "visualization;analysis;reconstruction"
        ENVIRONMENT "SM_TEST_VISUALIZATION=ON"
)

# 真实数据可视化测试
add_test(
    NAME test_TargetReconstructor_RealData_Visualization
    COMMAND test_TargetReconstructor_RealData
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_tests_properties(test_TargetReconstructor_RealData_Visualization
    PROPERTIES
        LABELS "visualization;analysis;reconstruction;realdata"
        ENVIRONMENT "SM_TEST_VISUALIZATION=ON"
)

# 性能基准测试
add_test(
    NAME test_TargetReconstructor_Performance
    COMMAND test_TargetReconstructor --gtest_filter=TargetReconstructorTest.PerformanceBenchmark
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_tests_properties(test_TargetReconstructor_Performance
    PROPERTIES
        LABELS "performance;analysis;benchmark"
        ENVIRONMENT "SM_TEST_VISUALIZATION=OFF"
)

# 安装测试可执行文件 (可选)
install(TARGETS 
    test_ParticleTrajectory
    test_TargetReconstructor
    test_TargetReconstructor_RealData
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/tests
)

# Note: debug_minuit_steps.cc is a standalone debugging tool
# Uncomment the following section if you create debug_minuit_steps.cc
# add_executable(debug_minuit_steps
#     debug_minuit_steps.cc
# )
# 
# target_link_libraries(debug_minuit_steps
#     analysis
#     ${ROOT_LIBRARIES}
#     ROOT::Geom
#     ROOT::Eve
#     ROOT::Minuit
# )

# install(TARGETS debug_minuit_steps
#     RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/tests
# )

# Create test output directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output/reconstruction_viz)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output/reconstruction_basic)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output/reconstruction_minuit)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output/reconstruction_gradient)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output/reconstruction_realdata_event0)

message(STATUS "Analysis tests configured:")
message(STATUS "  - test_ParticleTrajectory: Unit tests for trajectory calculation")
message(STATUS "  - test_TargetReconstructor: Unit tests for reconstruction (performance mode)")
message(STATUS "  - test_TargetReconstructor_RealData: Real data reconstruction tests")
message(STATUS "  - test_TargetReconstructor_Visualization: With detailed plots (run manually)")
message(STATUS "  - test_TargetReconstructor_RealData_Visualization: Real data with plots")
message(STATUS "  - test_TargetReconstructor_Performance: Benchmark tests")
message(STATUS "")
message(STATUS "To enable visualization in tests:")
message(STATUS "  export SM_TEST_VISUALIZATION=ON")
message(STATUS "  ./test_TargetReconstructor")
message(STATUS "  ./test_TargetReconstructor_RealData")
message(STATUS "")
message(STATUS "To run specific test labels:")
message(STATUS "  ctest -L unit           # Run all unit tests")
message(STATUS "  ctest -L performance    # Run benchmark tests")
message(STATUS "  ctest -L visualization  # Run visualization tests")
message(STATUS "  ctest -L realdata       # Run real data tests")
