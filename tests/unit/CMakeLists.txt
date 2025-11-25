# tests/unit/CMakeLists.txt
# 单元测试

include(GoogleTest)

##############################################################################
# 测试可执行文件
##############################################################################

# 示例测试文件 - 目前为空，可在未来添加
# 实际的 analysis 测试在 tests/analysis/ 中

# 如果需要添加其他单元测试，在此处添加
# 例如:
# if(BUILD_ANALYSIS)
#     add_executable(test_my_component test_my_component.cc)
#     target_link_libraries(test_my_component PRIVATE
#         analysis
#         GTest::GTest
#         GTest::Main
#         ${ROOT_LIBRARIES}
#     )
#     gtest_discover_tests(test_my_component)
# endif()

message(STATUS "Unit tests directory configured (currently empty)")
message(STATUS "Analysis-specific tests are in tests/analysis/")

##############################################################################
# 添加更多单元测试...
##############################################################################
