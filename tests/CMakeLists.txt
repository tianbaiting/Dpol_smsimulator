# tests/CMakeLists.txt
# 测试配置

# 查找 Google Test
find_package(GTest QUIET)

if(NOT GTEST_FOUND)
    message(STATUS "Google Test not found locally, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
    
    message(STATUS "Google Test fetched successfully")
    set(GTEST_FOUND TRUE)
endif()

if(GTEST_FOUND)
    message(STATUS "Google Test available, building unit tests")
    enable_testing()
    include(GoogleTest)
    
    add_subdirectory(unit)
    
    # Analysis 库测试
    if(BUILD_ANALYSIS)
        add_subdirectory(analysis)
    endif()
else()
    message(WARNING "Google Test could not be found or fetched, skipping unit tests")
endif()

# 集成测试（不需要 GTest）
add_subdirectory(integration)
