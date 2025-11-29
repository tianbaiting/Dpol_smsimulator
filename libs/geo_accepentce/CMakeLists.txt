# CMakeLists.txt for Geometry Acceptance Analysis Library
cmake_minimum_required(VERSION 3.10)

project(GeoAcceptance)

# 查找 ROOT
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree MathCore Physics)
include(${ROOT_USE_FILE})

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/analysis/include
    ${CMAKE_SOURCE_DIR}/libs/smlogger/include
    ${ROOT_INCLUDE_DIRS}
)

# 库的源文件 (不包含main函数的文件)
set(LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BeamDeflectionCalculator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorAcceptanceCalculator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GeoAcceptanceManager.cc
)

# 创建共享库
add_library(GeoAcceptance SHARED ${LIB_SOURCES})

# 链接库 - 确保Analysis库先构建
add_dependencies(GeoAcceptance analysis smlogger)
target_link_libraries(GeoAcceptance
    analysis  # 注意：库名是小写的analysis
    smlogger
    ${ROOT_LIBRARIES}
)

# 设置输出目录
set_target_properties(GeoAcceptance PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 创建可执行程序 - 主分析程序
add_executable(RunGeoAcceptanceAnalysis 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RunGeoAcceptanceAnalysis.cc
)
target_link_libraries(RunGeoAcceptanceAnalysis
    GeoAcceptance
    analysis
    ${ROOT_LIBRARIES}
)
set_target_properties(RunGeoAcceptanceAnalysis PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 创建可执行程序 - 测试程序
add_executable(test_geo_acceptance 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_geo_acceptance.cc
)
target_link_libraries(test_geo_acceptance
    GeoAcceptance
    analysis
    smlogger
    ${ROOT_LIBRARIES}
)
set_target_properties(test_geo_acceptance PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装
install(TARGETS GeoAcceptance RunGeoAcceptanceAnalysis test_geo_acceptance
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/GeoAcceptance
    FILES_MATCHING PATTERN "*.hh"
)
