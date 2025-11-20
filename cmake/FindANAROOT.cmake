# FindANAROOT.cmake
# 查找 ANAROOT 包

# 检查环境变量 TARTSYS
if(DEFINED ENV{TARTSYS})
    set(ANAROOT_ROOT_DIR $ENV{TARTSYS})
    
    # 设置包含目录
    set(ANAROOT_INCLUDE_DIR ${ANAROOT_ROOT_DIR}/include)
    
    # 设置库目录
    set(ANAROOT_LIBRARY_DIR ${ANAROOT_ROOT_DIR}/lib)
    
    # 查找库
    find_library(ANAROOT_SAMURAI_LIBRARY
        NAMES anasamurai
        PATHS ${ANAROOT_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(ANAROOT_BRIPS_LIBRARY
        NAMES anabrips
        PATHS ${ANAROOT_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(ANAROOT_CORE_LIBRARY
        NAMES anacore
        PATHS ${ANAROOT_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    find_library(ANAROOT_XMLPARSER_LIBRARY
        NAMES XMLParser
        PATHS ${ANAROOT_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )
    
    # 设置库列表
    set(ANAROOT_LIBRARIES
        ${ANAROOT_SAMURAI_LIBRARY}
        ${ANAROOT_BRIPS_LIBRARY}
        ${ANAROOT_CORE_LIBRARY}
        ${ANAROOT_XMLPARSER_LIBRARY}
    )
    
    # 检查是否找到
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(ANAROOT
        REQUIRED_VARS
            ANAROOT_ROOT_DIR
            ANAROOT_INCLUDE_DIR
            ANAROOT_LIBRARY_DIR
            ANAROOT_SAMURAI_LIBRARY
            ANAROOT_BRIPS_LIBRARY
            ANAROOT_CORE_LIBRARY
    )
    
    mark_as_advanced(
        ANAROOT_ROOT_DIR
        ANAROOT_INCLUDE_DIR
        ANAROOT_LIBRARY_DIR
        ANAROOT_LIBRARIES
    )
    
else()
    message(WARNING "TARTSYS environment variable not defined. Cannot find ANAROOT.")
    set(ANAROOT_FOUND FALSE)
endif()
