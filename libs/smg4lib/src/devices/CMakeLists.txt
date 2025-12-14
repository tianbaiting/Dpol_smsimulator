project(smdevice)

# Collect sources and headers (recursively to pick up include/ subdirs)
file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
file(GLOB_RECURSE INC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")

add_library(smdevice SHARED ${SRC_FILES})

# Public include dirs so other modules can find device headers
target_include_directories(smdevice PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/include>
    $<INSTALL_INTERFACE:include>
)

# Link against common libs (follow pattern used by other smg4lib submodules)
target_link_libraries(smdevice PUBLIC
    smdata
    ${ROOT_LIBRARIES}
    ${Geant4_LIBRARIES}
    ${XercesC_LIBRARIES}
)

set_target_properties(smdevice PROPERTIES
    SOVERSION 5
)

install(FILES ${INC_FILES} DESTINATION include)
install(TARGETS smdevice
    EXPORT SMSimulatorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

