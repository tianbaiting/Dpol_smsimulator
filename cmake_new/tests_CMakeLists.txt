# tests/CMakeLists.txt
# 测试配置

# 查找 Google Test（可选）
find_package(GTest QUIET)

if(GTEST_FOUND)
    message(STATUS "Google Test found, building unit tests")
    add_subdirectory(unit)
else()
    message(STATUS "Google Test not found, skipping unit tests")
endif()

# 集成测试（不需要 GTest）
add_subdirectory(integration)
