cmake_minimum_required(VERSION 3.12)  # 降低版本要求以兼容更多系统
project(smsimulator CXX)

set(CMAKE_CXX_STANDARD 17)

# 包含GNUInstallDirs以获取标准安装目录变量
include(GNUInstallDirs)

# 安全检查：防止在源码目录构建
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(WARNING "In-source builds are not recommended. Please create a separate build directory.")
    message(WARNING "To clean up: rm -rf CMakeCache.txt CMakeFiles/")
endif()

# 确保bin目录存在
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Find ROOT (removed Imt component to avoid TBB version conflicts)
find_package(ROOT REQUIRED COMPONENTS Core RIO Net Hist Graf Graf3d Gpad Tree Rint Postscript Matrix Physics MathCore Thread)
include(${ROOT_USE_FILE})

# Workaround for TBB linking issue: ROOT's libImt.so may require TBB but we don't use it
# Remove libImt from ROOT_LIBRARIES if present
list(FILTER ROOT_LIBRARIES EXCLUDE REGEX ".*libImt.*")

# Find Geant4 package with all UI and Vis drivers (like B1 example)
option(WITH_GEANT4_UIVIS "Build with Geant4 UI and Vis drivers" ON)
if(WITH_GEANT4_UIVIS)
  find_package(Geant4 REQUIRED ui_all vis_all)
else()
  find_package(Geant4 REQUIRED)
endif()

# Setup Geant4 include directories and compile definitions
include(${Geant4_USE_FILE})
include_directories(${Geant4_INCLUDE_DIRS})
# Find ANAROOT
if(DEFINED ENV{TARTSYS})
  message(STATUS "Found ANAROOT, TARTSYS = " $ENV{TARTSYS})
  include_directories($ENV{TARTSYS}/include)
  link_directories($ENV{TARTSYS}/lib)
  set(ANAROOT_LIBRARIES anasamurai anabrips anacore XMLParser)
else()
  message(FATAL_ERROR "$TARTSYS is not defined. please define the environment for ANAROOT.")
endif()

# Find Xerces-C
find_package(XercesC REQUIRED)
include_directories(${XERCESC_INCLUDE_DIR})

# Add smg4lib and sim_deuteron
add_subdirectory(sources)

