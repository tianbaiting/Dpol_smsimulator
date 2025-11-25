# tests/integration/CMakeLists.txt
# 集成测试

##############################################################################
# 简单的集成测试（不需要 GTest）
##############################################################################

# 测试：检查 sim_deuteron 是否能正常运行
add_test(
    NAME test_sim_deuteron_help
    COMMAND sim_deuteron --help
)

# 测试：运行一个简单的模拟
if(EXISTS ${CMAKE_SOURCE_DIR}/configs/simulation/macros/test.mac)
    add_test(
        NAME test_simple_simulation
        COMMAND sim_deuteron ${CMAKE_SOURCE_DIR}/configs/simulation/macros/test.mac
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(test_simple_simulation PROPERTIES
        TIMEOUT 300
    )
endif()

##############################################################################
# 更复杂的集成测试可以使用脚本
##############################################################################

# 示例：使用 Python 脚本进行测试
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(
        NAME test_full_chain
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_full_chain.py
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()
