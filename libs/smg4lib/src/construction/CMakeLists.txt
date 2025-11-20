# libs/smg4lib/src/construction/CMakeLists.txt

project(smconstruction)

# 查找所有源文件
file(GLOB_RECURSE CONSTRUCTION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
file(GLOB_RECURSE CONSTRUCTION_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")

# 创建共享库
add_library(smconstruction SHARED ${CONSTRUCTION_SOURCES})

# 设置包含目录
target_include_directories(smconstruction PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/src/data/include>
    $<INSTALL_INTERFACE:include>
)

# 链接依赖
target_link_libraries(smconstruction PUBLIC
    smdata
    ${ROOT_LIBRARIES}
    ${Geant4_LIBRARIES}
    ${XercesC_LIBRARIES}
)

# 设置属性
set_target_properties(smconstruction PROPERTIES
    SOVERSION 5
)

# 安装
install(TARGETS smconstruction
    EXPORT SMSimulatorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg4lib/construction)
