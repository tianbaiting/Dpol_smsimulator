# libs/smg4lib/src/data/CMakeLists.txt

project(smdata)

# 查找所有源文件
file(GLOB_RECURSE DATA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
file(GLOB_RECURSE DATA_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")
file(GLOB TINC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/T*.hh")

# 生成 ROOT 字典
message(STATUS "Making dictionary smrootdata for ROOT")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/smdata_linkdef.hh "#if defined(__CINT__) || defined(__CLING__)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/smdata_linkdef.hh "#pragma link off all globals;\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/smdata_linkdef.hh "#pragma link off all classes;\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/smdata_linkdef.hh "#pragma link off all functions;\n")

file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/smdata_linkdef.hh "#pragma link C++ class vector<TBeamSimData>;\n")
foreach(loop_var IN LISTS TINC_FILES)
  get_filename_component(loop_var_filename "${loop_var}" NAME_WE)
  file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/smdata_linkdef.hh "#pragma link C++ class ${loop_var_filename}+;\n")
  message(STATUS "  Adding to dictionary: ${loop_var_filename}")
endforeach()
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/smdata_linkdef.hh "#endif\n")

ROOT_GENERATE_DICTIONARY(G__smrootdata
  ${TINC_FILES}      
  LINKDEF ${CMAKE_CURRENT_BINARY_DIR}/smdata_linkdef.hh
)

# 创建共享库（包含字典）
add_library(smdata SHARED ${DATA_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/G__smrootdata.cxx)

# 设置包含目录
target_include_directories(smdata PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smg4lib/include>
    $<INSTALL_INTERFACE:include>
)

# 链接依赖
target_link_libraries(smdata PUBLIC
    ${ROOT_LIBRARIES}
    ${Geant4_LIBRARIES}
)

# 如果有 ANAROOT，添加依赖（NEBULASimDataConverter_TArtNEBULAPla 需要）
if(WITH_ANAROOT)
    target_include_directories(smdata PUBLIC ${ANAROOT_INCLUDE_DIR})
    target_link_libraries(smdata PUBLIC ${ANAROOT_LIBRARIES})
endif()

# 设置属性
set_target_properties(smdata PROPERTIES
    SOVERSION 5
)

# 安装
install(TARGETS smdata
    EXPORT SMSimulatorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/smg4lib/data)

# 安装 ROOT 字典文件
if(${ROOT_VERSION} VERSION_GREATER "6.0")
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/libsmdata_rdict.pcm
        ${CMAKE_CURRENT_BINARY_DIR}/libsmdata.rootmap
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
