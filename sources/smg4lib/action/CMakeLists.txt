project(smaction)

file(GLOB SRC_FILES "src/*.cc")
file(GLOB INC_FILES "include/*.hh")

add_library(smaction SHARED ${SRC_FILES})

target_include_directories(smaction PUBLIC include)

target_link_libraries(smaction PUBLIC ${Geant4_LIBRARIES})

install(FILES ${INC_FILES} DESTINATION include)
install(TARGETS smaction
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})