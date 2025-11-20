# apps/tools/CMakeLists.txt
# 工具程序

##############################################################################
# ROOT 宏脚本（不需要编译）
##############################################################################

# auto_pdc_wires 是 ROOT 宏，应该使用 .C 后缀
# 如果文件是 .cc，我们跳过编译，只安装为脚本
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/auto_pdc_wires.cc)
    message(STATUS "Found ROOT macro: auto_pdc_wires.cc (will be installed as script)")
    install(FILES auto_pdc_wires.cc 
        DESTINATION ${CMAKE_INSTALL_DATADIR}/smsimulator/macros
        RENAME auto_pdc_wires.C
    )
endif()

##############################################################################
# 实际的可执行工具可以在这里添加
##############################################################################

# 示例：如果有真正需要编译的工具
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/some_tool_with_main.cc)
#     add_executable(some_tool some_tool_with_main.cc)
#     target_link_libraries(some_tool PRIVATE ${ROOT_LIBRARIES})
#     install(TARGETS some_tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# endif()
