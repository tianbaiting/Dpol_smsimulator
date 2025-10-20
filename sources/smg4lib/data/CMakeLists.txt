project(smdata)

file(GLOB SRC_FILES "src/*.cc")
file(GLOB INC_FILES "include/*.hh")

# For ROOT dictionary
file(GLOB TINC_FILES "include/T*.hh")

# Make smrootdata_linkdef.hh
message(STATUS "Making dictionary smrootdata for ROOT")
file(WRITE smdata_linkdef.hh "#if defined(__CINT__) || defined(__CLING__)\n")
file(APPEND smdata_linkdef.hh "#pragma link off all globals;\n")
file(APPEND smdata_linkdef.hh "#pragma link off all classes;\n")
file(APPEND smdata_linkdef.hh "#pragma link off all functions;\n")

file(APPEND smdata_linkdef.hh "#pragma link C++ class vector<TBeamSimData>;\n")
list(APPEND CMAKE_MESSAGE_INDENT "  ")
foreach(loop_var IN LISTS TINC_FILES)
  message(STATUS "${loop_var}")
  get_filename_component(loop_var_filename "${loop_var}" NAME_WE)
  file(APPEND smdata_linkdef.hh "#pragma link C++ class ${loop_var_filename}+;\n")
endforeach()
list(POP_BACK CMAKE_MESSAGE_INDENT)
file(APPEND smdata_linkdef.hh "#endif\n")

ROOT_GENERATE_DICTIONARY(G__smrootdata
  ${TINC_FILES}      
  LINKDEF smdata_linkdef.hh
)

add_library(smdata SHARED ${SRC_FILES} G__smrootdata.cxx)
target_include_directories(smdata PUBLIC include)

install(FILES ${INC_FILES} DESTINATION include)
install(TARGETS smdata
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (${ROOT_VERSION} VERSION_GREATER "6.0")
   install(
      FILES
      ${CMAKE_CURRENT_BINARY_DIR}/libsmdata_rdict.pcm
      ${CMAKE_CURRENT_BINARY_DIR}/libsmdata.rootmap
      DESTINATION lib/)
endif ()
