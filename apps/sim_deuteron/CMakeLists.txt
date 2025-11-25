# apps/sim_deuteron/CMakeLists.txt
# 主模拟程序

project(sim_deuteron_app)

##############################################################################
# 创建可执行文件
##############################################################################

add_executable(sim_deuteron main.cc)

# 设置包含目录
target_include_directories(sim_deuteron PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/sim_deuteron_core/include
    ${CMAKE_SOURCE_DIR}/libs/smg4lib/include
    ${CMAKE_SOURCE_DIR}/libs/smg4lib/src/action/include
    ${CMAKE_SOURCE_DIR}/libs/smg4lib/src/construction/include
    ${CMAKE_SOURCE_DIR}/libs/smg4lib/src/data/include
    ${CMAKE_SOURCE_DIR}/libs/smg4lib/src/physics/include
)

# 链接库
target_link_libraries(sim_deuteron PRIVATE
    sim_deuteron_core
    smaction
    smconstruction
    smdata
    smphysics
    ${ROOT_LIBRARIES}
    ${Geant4_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${XercesC_LIBRARIES}
)

if(WITH_ANAROOT)
    target_link_libraries(sim_deuteron PRIVATE ${ANAROOT_LIBRARIES})
endif()

##############################################################################
# 安装
##############################################################################

install(TARGETS sim_deuteron
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 复制到项目 bin 目录（保持旧习惯）
add_custom_command(TARGET sim_deuteron POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sim_deuteron> ${CMAKE_SOURCE_DIR}/bin/
    COMMENT "Copying sim_deuteron to bin directory"
)

##############################################################################
# 安装宏文件到运行目录
##############################################################################

# 创建符号链接到配置目录（便于开发）
add_custom_command(TARGET sim_deuteron POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink 
        ${CMAKE_SOURCE_DIR}/configs/simulation
        ${CMAKE_BINARY_DIR}/bin/configs
    COMMENT "Creating symlink to simulation configs"
    VERBATIM
)
