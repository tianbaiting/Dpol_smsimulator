# libs/smg4lib/src/physics/CMakeLists.txt

project(smphysics)

# 查找所有源文件
file(GLOB_RECURSE PHYSICS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
file(GLOB_RECURSE PHYSICS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")

# 创建共享库
add_library(smphysics SHARED ${PHYSICS_SOURCES})

# 设置包含目录
target_include_directories(smphysics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/include>
    $<INSTALL_INTERFACE:include>
)

# 链接依赖
target_link_libraries(smphysics PUBLIC
    ${Geant4_LIBRARIES}
)

# 设置属性
set_target_properties(smphysics PROPERTIES
    SOVERSION 5
)

# 安装
install(TARGETS smphysics
    EXPORT SMSimulatorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg4lib/physics)
