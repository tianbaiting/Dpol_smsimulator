project(analysis_pdc_reco)

set(PDC_RECO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PDCNNMomentumReconstructor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PDCMomentumReconstructor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PDCMomentumReconstructorNN.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PDCMomentumReconstructorRK.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PDCMomentumReconstructorMultiDim.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PDCRecoRuntime.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PDCRecoFactory.cc
)

add_library(analysis_pdc_reco SHARED ${PDC_RECO_SOURCES})
add_library(SMSimulator::analysis_pdc_reco ALIAS analysis_pdc_reco)

target_include_directories(analysis_pdc_reco PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/analysis/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/smlogger/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(analysis_pdc_reco PUBLIC
    analysis
    smlogger
    ${ROOT_LIBRARIES}
)

if(TARGET ROOT::Minuit)
    target_link_libraries(analysis_pdc_reco PUBLIC ROOT::Minuit)
endif()

set_target_properties(analysis_pdc_reco PROPERTIES
    SOVERSION 1
    OUTPUT_NAME analysis_pdc_reco
)

install(TARGETS analysis_pdc_reco
    EXPORT SMSimulatorTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/analysis_pdc_reco)
