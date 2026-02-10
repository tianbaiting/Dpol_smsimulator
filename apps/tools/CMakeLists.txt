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

# QMD rawdata -> g4input generator (standalone executable)
set(QMD_GEN_SRC ${CMAKE_SOURCE_DIR}/scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc)
if(EXISTS ${QMD_GEN_SRC})
    add_executable(GenInputRoot_qmdrawdata ${QMD_GEN_SRC})
    target_link_libraries(GenInputRoot_qmdrawdata PRIVATE smdata spdlog::spdlog ${ROOT_LIBRARIES})
    install(TARGETS GenInputRoot_qmdrawdata RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
