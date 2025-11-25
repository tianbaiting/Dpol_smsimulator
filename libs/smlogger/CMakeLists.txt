# SMLogger - 异步日志系统库

project(smlogger)

# 源文件
set(SMLOGGER_SOURCES
    src/SMLogger.cc
)

# 头文件
set(SMLOGGER_HEADERS
    include/SMLogger.hh
)

# 创建共享库
add_library(smlogger SHARED ${SMLOGGER_SOURCES})

# 包含目录
target_include_directories(smlogger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

##############################################################################
# 查找或获取 spdlog
##############################################################################

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    message(STATUS "spdlog not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0
        GIT_SHALLOW TRUE
    )
    # 设置 spdlog 选项 - 使用 PIC 编译
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_PIC ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(spdlog)
    message(STATUS "spdlog fetched successfully")
else()
    message(STATUS "spdlog found: ${spdlog_VERSION}")
endif()

# 链接 spdlog
target_link_libraries(smlogger PUBLIC spdlog::spdlog)

# 设置编译选项
target_compile_features(smlogger PUBLIC cxx_std_17)

# 设置库属性
set_target_properties(smlogger PROPERTIES
    SOVERSION 5
    OUTPUT_NAME "smlogger"
)

##############################################################################
# 安装
##############################################################################

install(TARGETS smlogger
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${SMLOGGER_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smsimulator)

##############################################################################
# 测试（可选）
##############################################################################

if(BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()
