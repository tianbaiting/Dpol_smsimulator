# FindXercesC.cmake
# 查找 Xerces-C XML 解析库

find_path(XercesC_INCLUDE_DIR
    NAMES xercesc/util/XercesVersion.hpp
    PATHS
        /usr/include
        /usr/local/include
        $ENV{XERCESCROOT}/include
)

find_library(XercesC_LIBRARY
    NAMES xerces-c xerces-c_3
    PATHS
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
        $ENV{XERCESCROOT}/lib
)

# 获取版本信息
if(XercesC_INCLUDE_DIR AND EXISTS "${XercesC_INCLUDE_DIR}/xercesc/util/XercesVersion.hpp")
    file(STRINGS "${XercesC_INCLUDE_DIR}/xercesc/util/XercesVersion.hpp" 
         XercesC_VERSION_LINE 
         REGEX "^#define XERCES_VERSION_MAJOR")
    string(REGEX REPLACE "^#define XERCES_VERSION_MAJOR ([0-9]+)" "\\1" 
           XercesC_VERSION_MAJOR "${XercesC_VERSION_LINE}")
    set(XercesC_VERSION "${XercesC_VERSION_MAJOR}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XercesC
    REQUIRED_VARS XercesC_LIBRARY XercesC_INCLUDE_DIR
)

if(XercesC_FOUND)
    set(XercesC_LIBRARIES ${XercesC_LIBRARY})
    set(XercesC_INCLUDE_DIRS ${XercesC_INCLUDE_DIR})
    
    if(NOT TARGET XercesC::XercesC)
        add_library(XercesC::XercesC UNKNOWN IMPORTED)
        set_target_properties(XercesC::XercesC PROPERTIES
            IMPORTED_LOCATION "${XercesC_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${XercesC_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(XercesC_INCLUDE_DIR XercesC_LIBRARY)
