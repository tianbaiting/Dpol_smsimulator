project(run_reconstruction)

set(RECONSTRUCT_TARGET_MOMENTUM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)

if(EXISTS ${RECONSTRUCT_TARGET_MOMENTUM_SRC})
    add_executable(reconstruct_target_momentum ${RECONSTRUCT_TARGET_MOMENTUM_SRC})
    target_link_libraries(reconstruct_target_momentum PRIVATE
        analysis
        analysis_pdc_reco
        smdata
        ${ROOT_LIBRARIES}
    )
    install(TARGETS reconstruct_target_momentum
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
else()
    message(STATUS "run_reconstruction/main.cc not found, skipping...")
endif()
