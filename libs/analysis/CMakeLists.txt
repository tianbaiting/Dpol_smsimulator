# libs/analysis/CMakeLists.txt
# 数据分析和重建库

project(analysis)

##############################################################################
# 收集源文件
##############################################################################

file(GLOB ANALYSIS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
file(GLOB ANALYSIS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")

##############################################################################
# 生成 ROOT 字典
##############################################################################

# 需要生成字典的头文件列表
set(ANALYSIS_DICT_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MagneticField.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EventProcessor.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/include/RecoEvent.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/include/PDCSimAna.hh
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NEBULAReconstructor.hh
)

# 生成 ROOT 字典
ROOT_GENERATE_DICTIONARY(G__Analysis
    ${ANALYSIS_DICT_HEADERS}
    MODULE analysis
    LINKDEF ${CMAKE_CURRENT_SOURCE_DIR}/include/AnalysisLinkDef.h
)

# 将字典源文件添加到源列表
list(APPEND ANALYSIS_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/G__Analysis.cxx)

##############################################################################
# 创建共享库
##############################################################################

add_library(analysis SHARED ${ANALYSIS_SOURCES})

# 别名
add_library(SMSimulator::analysis ALIAS analysis)

# 设置包含目录
target_include_directories(analysis PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smlogger/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/include>
    $<INSTALL_INTERFACE:include>
)

# 链接依赖
target_link_libraries(analysis PUBLIC
    smlogger
    ${ROOT_LIBRARIES}
)

# 链接必要的 ROOT 组件
if(TARGET ROOT::Geom)
    target_link_libraries(analysis PUBLIC ROOT::Geom)
endif()
if(TARGET ROOT::Eve)
    target_link_libraries(analysis PUBLIC ROOT::Eve)
endif()
if(TARGET ROOT::Minuit)
    target_link_libraries(analysis PUBLIC ROOT::Minuit)
endif()

# 如果有 ANAROOT
if(WITH_ANAROOT)
    target_link_libraries(analysis PUBLIC ${ANAROOT_LIBRARIES})
endif()

# 设置属性
set_target_properties(analysis PROPERTIES
    SOVERSION 5
    OUTPUT_NAME pdcanalysis
)

##############################################################################
# 安装
##############################################################################

install(TARGETS analysis
    EXPORT SMSimulatorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/analysis)

##############################################################################
# 测试
##############################################################################

if(BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()
