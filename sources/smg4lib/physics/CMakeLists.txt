project(smphysics)

file(GLOB SRC_FILES "src/*.cc")
file(GLOB INC_FILES "include/*.hh")

add_library(smphysics SHARED ${SRC_FILES})

target_include_directories(smphysics PUBLIC include)

target_link_libraries(smphysics PUBLIC ${Geant4_LIBRARIES})

install(FILES ${INC_FILES} DESTINATION include)
install(TARGETS smphysics
    LIBRARY DESTINATION lib)