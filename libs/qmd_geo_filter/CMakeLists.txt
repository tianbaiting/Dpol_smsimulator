# CMakeLists.txt for QMD Geometry Filter Library
cmake_minimum_required(VERSION 3.10)

project(QMDGeoFilter)

# 查找 ROOT
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree MathCore Physics Hist Graf)
include(${ROOT_USE_FILE})

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/geo_accepentce/include
    ${CMAKE_SOURCE_DIR}/libs/analysis/include
    ${CMAKE_SOURCE_DIR}/libs/smlogger/include
    ${ROOT_INCLUDE_DIRS}
)

# 库的源文件
set(LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QMDGeoFilter.cc
)

# 创建共享库
add_library(QMDGeoFilter SHARED ${LIB_SOURCES})

# 链接库 - 依赖 GeoAcceptance 库
add_dependencies(QMDGeoFilter GeoAcceptance analysis smlogger)
target_link_libraries(QMDGeoFilter
    GeoAcceptance
    analysis
    smlogger
    ${ROOT_LIBRARIES}
)

# 设置输出目录
set_target_properties(QMDGeoFilter PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 创建可执行程序
add_executable(RunQMDGeoFilter 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RunQMDGeoFilter.cc
)
target_link_libraries(RunQMDGeoFilter
    QMDGeoFilter
    GeoAcceptance
    analysis
    smlogger
    ${ROOT_LIBRARIES}
)
set_target_properties(RunQMDGeoFilter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装
install(TARGETS QMDGeoFilter RunQMDGeoFilter
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/QMDGeoFilter
    FILES_MATCHING PATTERN "*.hh"
)

# 可选：Python 绑定保留（简化版）
# 如果需要 Python 接口，可以通过 pybind11 或 ctypes 包装
