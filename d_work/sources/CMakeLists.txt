# CMake script for PDCAnalysisTools

# Set minimum required CMake version
cmake_minimum_required(VERSION 3.10)

# Set project name
project(PDCAnalysisTools)

# Find ROOT package
find_package(ROOT REQUIRED COMPONENTS Eve)

# Include ROOT's CMake file
include(${ROOT_USE_FILE})

# Set C++ standard to 14 as required by ROOT
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define include directories
include_directories(
    include
    $ENV{G4SMLIBDIR}/include
)

# Glob source and header files
file(GLOB_RECURSE SOURCES "src/*.cc")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hh")

# Generate ROOT dictionary
ROOT_GENERATE_DICTIONARY(G__PDCAnalysisTools ${HEADERS} LINKDEF src/LinkDef.h)

# Add the shared library
add_library(PDCAnalysisTools SHARED ${SOURCES} G__PDCAnalysisTools.cxx)

# Link against ROOT libraries
target_link_libraries(PDCAnalysisTools ${ROOT_LIBRARIES})

# Installation (optional)
# install(TARGETS PDCAnalysisTools DESTINATION lib)
# install(DIRECTORY include/ DESTINATION include)

message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "ROOTSYS: ${ROOTSYS}")
