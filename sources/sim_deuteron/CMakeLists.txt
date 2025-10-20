project(sim_deuteron)

file(GLOB SRC_FILES "src/*.cc")

add_library(sim_deuteron-so SHARED ${SRC_FILES})
set_target_properties(sim_deuteron-so PROPERTIES OUTPUT_NAME sim_deuteron)
target_include_directories(sim_deuteron-so PRIVATE include)

add_executable(sim_deuteron sim_deuteron.cc)
target_include_directories(sim_deuteron PRIVATE include)

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

install(DIRECTORY include/
		  DESTINATION include)
install(TARGETS sim_deuteron
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS sim_deuteron-so
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
