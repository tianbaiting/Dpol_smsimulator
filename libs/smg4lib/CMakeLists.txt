# libs/smg4lib/CMakeLists.txt
# Geant4 核心库

project(smg4lib)

##############################################################################
# 子模块库
##############################################################################

# 构建 data（其他模块依赖它）
add_subdirectory(src/data)

# 构建其他模块
add_subdirectory(src/action)
add_subdirectory(src/construction)
add_subdirectory(src/physics)
add_subdirectory(src/devices)

##############################################################################
# 创建接口库（用于统一链接）
##############################################################################

add_library(smg4lib INTERFACE)
target_link_libraries(smg4lib INTERFACE
    smdata
    smaction
    smconstruction
    smphysics
    smdevice
)

# 导出头文件路径
target_include_directories(smg4lib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

##############################################################################
# 安装
##############################################################################

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg4lib)

# 可选：构建测试
if(BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()
