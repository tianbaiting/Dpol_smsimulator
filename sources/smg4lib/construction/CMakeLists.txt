project(smconstruction)

file(GLOB SRC_FILES "src/*.cc")
file(GLOB INC_FILES "include/*.hh")

add_library(smconstruction SHARED ${SRC_FILES})

target_include_directories(smconstruction PUBLIC include)

target_link_libraries(smconstruction PUBLIC ${Geant4_LIBRARIES})

install(FILES ${INC_FILES} DESTINATION include)
install(TARGETS smconstruction
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})