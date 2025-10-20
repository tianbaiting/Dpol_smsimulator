cmake_minimum_required(VERSION 3.16)
project(smsimulator CXX)

set(CMAKE_CXX_STANDARD 17)

# Find ROOT
find_package(ROOT REQUIRED COMPONENTS Core RIO Net Hist Graf Graf3d Gpad Tree Rint Postscript Matrix Physics MathCore Thread)
include(${ROOT_USE_FILE})

# Find Geant4
find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})
### for Geant4 ---------------------------
find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})
include_directories(${Geant4_INCLUDE_DIRS})
# Find ANAROOT
if(DEFINED ENV{TARTSYS})
  message(STATUS "Found ANAROOT, TARTSYS = " $ENV{TARTSYS})
  include_directories($ENV{TARTSYS}/include)
  set(ANAROOT_LIBRARIES anasamurai anabrips anacore XMLParser)
else()
  message(FATAL_ERROR "$TARTSYS is not defined. please define the environment for ANAROOT.")
endif()

# Find Xerces-C
find_package(XercesC REQUIRED)
include_directories(${XERCESC_INCLUDE_DIR})

# Add smg4lib and sim_deuteron
add_subdirectory(sources)

