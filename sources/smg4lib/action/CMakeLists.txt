project(smaction)

file(GLOB SRC_FILES "src/*.cc")
file(GLOB INC_FILES "include/*.hh")

add_library(smaction SHARED ${SRC_FILES})

target_include_directories(smaction PUBLIC 
    include
    ../include
    ../data/include
)

target_link_libraries(smaction PUBLIC 
    ${Geant4_LIBRARIES}
    smdata
)

install(FILES ${INC_FILES} DESTINATION include)
install(TARGETS smaction
    LIBRARY DESTINATION lib)