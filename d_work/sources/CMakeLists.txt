# CMake script for PDCAnalysisTools

# Set minimum required CMake version
cmake_minimum_required(VERSION 3.10)

# Set project name
project(PDCAnalysisTools)

# Find ROOT package
find_package(ROOT REQUIRED)

# Include ROOT's CMake file
include(${ROOT_USE_FILE})

# Set C++ standard to 17 as required by ROOT
set(CMAKE_CXX_STANDARD 17)
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
# 需要显式链接 Eve, Geom, Tree, Physics, Minuit 等库
target_link_libraries(PDCAnalysisTools 
    ${ROOT_LIBRARIES}
    ROOT::Core
    ROOT::Tree
    ROOT::Physics
    ROOT::Geom
    ROOT::GeomPainter
    ROOT::Eve
    ROOT::EG
    ROOT::Gui
    ROOT::RGL
    ROOT::Minuit
)

# Installation (optional)
# install(TARGETS PDCAnalysisTools DESTINATION lib)
# install(DIRECTORY include/ DESTINATION include)

message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "ROOTSYS: ${ROOTSYS}")
