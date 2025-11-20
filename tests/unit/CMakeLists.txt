# tests/unit/CMakeLists.txt
# 单元测试

include(GoogleTest)

##############################################################################
# 测试可执行文件
##############################################################################

# 示例：测试 GeometryManager
if(BUILD_ANALYSIS)
    add_executable(test_geometry_manager test_geometry_manager.cc)
    target_link_libraries(test_geometry_manager PRIVATE
        analysis
        GTest::GTest
        GTest::Main
        ${ROOT_LIBRARIES}
    )
    gtest_discover_tests(test_geometry_manager)
endif()

# 示例：测试轨迹计算
if(BUILD_ANALYSIS)
    add_executable(test_trajectory test_trajectory.cc)
    target_link_libraries(test_trajectory PRIVATE
        analysis
        GTest::GTest
        GTest::Main
        ${ROOT_LIBRARIES}
    )
    gtest_discover_tests(test_trajectory)
endif()

##############################################################################
# 添加更多单元测试...
##############################################################################
