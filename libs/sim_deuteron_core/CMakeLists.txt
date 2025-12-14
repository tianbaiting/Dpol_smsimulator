# libs/sim_deuteron_core/CMakeLists.txt
# 氘核模拟核心库

project(sim_deuteron_core)

##############################################################################
# 收集源文件
##############################################################################

file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
file(GLOB_RECURSE CORE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")

##############################################################################
# 创建共享库
##############################################################################

add_library(sim_deuteron_core SHARED ${CORE_SOURCES})

# 别名（便于子项目使用）
add_library(SMSimulator::sim_deuteron_core ALIAS sim_deuteron_core)

# 设置包含目录
target_include_directories(sim_deuteron_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smlogger/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/src/action/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/src/construction/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/src/data/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/src/physics/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/src/devices>
    $<INSTALL_INTERFACE:include>
)

# 链接依赖库
target_link_libraries(sim_deuteron_core PUBLIC
    smlogger
    smaction
    smconstruction
    smdata
    smphysics
    smdevice
    ${ROOT_LIBRARIES}
    ${Geant4_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${XercesC_LIBRARIES}
)

# 如果有 ANAROOT，添加依赖
if(WITH_ANAROOT)
    target_link_libraries(sim_deuteron_core PUBLIC ${ANAROOT_LIBRARIES})
endif()

# 设置库属性
set_target_properties(sim_deuteron_core PROPERTIES
    SOVERSION 5
    OUTPUT_NAME sim_deuteron_core
)

##############################################################################
# 安装
##############################################################################

install(TARGETS sim_deuteron_core
    EXPORT SMSimulatorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sim_deuteron_core)

##############################################################################
# 测试（可选）
##############################################################################

if(BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()
