# tests/unit/CMakeLists.txt
# 单元测试

include(GoogleTest)

##############################################################################
# 测试可执行文件
##############################################################################

# Analysis-specific tests live in tests/analysis/.

# Default-value regression tests for DeutDetectorConstruction
add_executable(test_DeutDetectorConstruction_defaults
    test_DeutDetectorConstruction_defaults.cc
)
target_link_libraries(test_DeutDetectorConstruction_defaults PRIVATE
    sim_deuteron_core
    smg4lib
    GTest::gtest
    GTest::gtest_main
    ${Geant4_LIBRARIES}
    ${ROOT_LIBRARIES}
)
gtest_discover_tests(test_DeutDetectorConstruction_defaults
    PROPERTIES LABELS "unit"
)

# Macro-config regression test: no .mac sets Target/Position without Target/SetTarget
add_test(NAME test_no_bare_target_macros
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_no_bare_target_macros.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(test_no_bare_target_macros PROPERTIES LABELS "unit")

# QMDInputMetadata index layout regression
add_executable(test_qmd_input_metadata_indices
    test_qmd_input_metadata_indices.cc
)
target_link_libraries(test_qmd_input_metadata_indices PRIVATE
    smg4lib
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(test_qmd_input_metadata_indices PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/smg4lib/include
)
gtest_discover_tests(test_qmd_input_metadata_indices
    PROPERTIES LABELS "unit"
)

add_executable(test_qmd_rotation
    test_qmd_rotation.cc
)
target_include_directories(test_qmd_rotation PRIVATE
    ${CMAKE_SOURCE_DIR}/scripts/event_generator/dpol_eventgen
)
target_link_libraries(test_qmd_rotation PRIVATE
    GTest::gtest
    GTest::gtest_main
)
gtest_discover_tests(test_qmd_rotation
    PROPERTIES LABELS "unit"
)

# RED-phase test for NEBULAPlusSimParameterReader (P1.4).
# Compile will fail until P1.5 implements the reader header + source.
add_executable(test_NEBULAPlusSimParameterReader
    test_NEBULAPlusSimParameterReader.cc
)
target_link_libraries(test_NEBULAPlusSimParameterReader PRIVATE
    sim_deuteron_core
    smg4lib
    GTest::gtest
    GTest::gtest_main
    ${Geant4_LIBRARIES}
    ${ROOT_LIBRARIES}
)
gtest_discover_tests(test_NEBULAPlusSimParameterReader
    PROPERTIES LABELS "unit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
