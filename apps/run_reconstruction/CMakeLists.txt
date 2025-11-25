# apps/run_reconstruction/CMakeLists.txt
# 重建分析程序

project(run_reconstruction)

##############################################################################
# 创建可执行文件
##############################################################################

# 如果有主文件，创建可执行文件
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)
    add_executable(run_reconstruction main.cc)

    target_include_directories(run_reconstruction PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/analysis/include
        ${CMAKE_SOURCE_DIR}/libs/smg4lib/include
    )

    target_link_libraries(run_reconstruction PRIVATE
        analysis
        ${ROOT_LIBRARIES}
    )

    if(WITH_ANAROOT)
        target_link_libraries(run_reconstruction PRIVATE ${ANAROOT_LIBRARIES})
    endif()

    install(TARGETS run_reconstruction
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # 复制到 bin 目录
    add_custom_command(TARGET run_reconstruction POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy 
            $<TARGET_FILE:run_reconstruction> 
            ${CMAKE_SOURCE_DIR}/bin/
        COMMENT "Copying run_reconstruction to bin directory"
    )
else()
    message(STATUS "run_reconstruction/main.cc not found, skipping...")
endif()
