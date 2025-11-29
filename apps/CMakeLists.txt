# apps/CMakeLists.txt
# 所有可执行程序

add_subdirectory(sim_deuteron)

if(BUILD_ANALYSIS)
    add_subdirectory(run_reconstruction)
endif()

# 工具程序
add_subdirectory(tools)

