project(sim_deuteron)

file(GLOB SRC_FILES "src/*.cc")

add_library(sim_deuteron-so SHARED ${SRC_FILES})
set_target_properties(sim_deuteron-so PROPERTIES OUTPUT_NAME sim_deuteron)
target_include_directories(sim_deuteron-so PRIVATE 
    include
    ../smg4lib/include
    ../smg4lib/action/include
    ../smg4lib/construction/include
    ../smg4lib/data/include
    ../smg4lib/physics/include
)

add_executable(sim_deuteron sim_deuteron.cc)
target_include_directories(sim_deuteron PRIVATE 
    include
    ../smg4lib/include
    ../smg4lib/action/include
    ../smg4lib/construction/include
    ../smg4lib/data/include
    ../smg4lib/physics/include
)

target_link_libraries(
  sim_deuteron-so
  smaction
  smconstruction
  smdata
  smphysics
  ${ANAROOT_LIBRARIES} 
  ${ROOT_LIBRARIES} 
  ${Geant4_LIBRARIES}
  ${XercesC_LIBRARIES}
)

target_link_libraries(
  sim_deuteron 
  sim_deuteron-so
  smaction
  smconstruction
  smdata
  smphysics
  ${ANAROOT_LIBRARIES} 
  ${ROOT_LIBRARIES} 
  ${Geant4_LIBRARIES}
  ${XercesC_LIBRARIES}
)

# Copy sim_deuteron to bin directory after build
add_custom_command(TARGET sim_deuteron POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sim_deuteron> ${CMAKE_SOURCE_DIR}/bin/
    COMMENT "Copying sim_deuteron to bin directory"
)

install(DIRECTORY include/
		  DESTINATION include)
install(TARGETS sim_deuteron
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS sim_deuteron-so
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
