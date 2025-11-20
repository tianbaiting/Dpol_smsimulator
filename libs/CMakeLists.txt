# libs/CMakeLists.txt
# 构建所有库

# 先构建 smg4lib（其他库可能依赖它）
add_subdirectory(smg4lib)

# 构建 sim_deuteron_core
add_subdirectory(sim_deuteron_core)

# 构建分析库（可选）
if(BUILD_ANALYSIS)
    add_subdirectory(analysis)
endif()
