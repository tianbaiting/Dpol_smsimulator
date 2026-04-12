# apps/tools/CMakeLists.txt
# 工具程序

##############################################################################
# ROOT 宏脚本（不需要编译）
##############################################################################

# auto_pdc_wires 是 ROOT 宏，应该使用 .C 后缀
# 如果文件是 .cc，我们跳过编译，只安装为脚本
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/auto_pdc_wires.cc)
    message(STATUS "Found ROOT macro: auto_pdc_wires.cc (will be installed as script)")
    install(FILES auto_pdc_wires.cc 
        DESTINATION ${CMAKE_INSTALL_DATADIR}/smsimulator/macros
        RENAME auto_pdc_wires.C
    )
endif()

##############################################################################
# 实际的可执行工具可以在这里添加
##############################################################################

# QMD rawdata -> g4input generator (standalone executable)
set(QMD_GEN_SRC ${CMAKE_SOURCE_DIR}/scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc)
if(EXISTS ${QMD_GEN_SRC})
    add_executable(GenInputRoot_qmdrawdata ${QMD_GEN_SRC})
    target_link_libraries(GenInputRoot_qmdrawdata PRIVATE smdata spdlog::spdlog ${ROOT_LIBRARIES})
    install(TARGETS GenInputRoot_qmdrawdata RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

set(IPS_SCAN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/scan_ips_position.cc)
if(EXISTS ${IPS_SCAN_SRC})
    add_executable(scan_ips_position ${IPS_SCAN_SRC})
    target_link_libraries(scan_ips_position PRIVATE smdata ${ROOT_LIBRARIES})
    install(TARGETS scan_ips_position RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

set(IPS_WRL_PREP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/prepare_ips_wrl_examples.cc)
if(EXISTS ${IPS_WRL_PREP_SRC})
    add_executable(prepare_ips_wrl_examples ${IPS_WRL_PREP_SRC})
    target_link_libraries(prepare_ips_wrl_examples PRIVATE smdata spdlog::spdlog ${ROOT_LIBRARIES})
    install(TARGETS prepare_ips_wrl_examples RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Geant4 PDC position scan summary analyzer (ROOT tree -> CSV metrics)
set(PDC_SCAN_ANALYZER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/analyze_pdc_scan_root.cc)
if(EXISTS ${PDC_SCAN_ANALYZER_SRC})
    add_executable(analyze_pdc_scan_root ${PDC_SCAN_ANALYZER_SRC})
    target_link_libraries(analyze_pdc_scan_root PRIVATE ${ROOT_LIBRARIES})
    install(TARGETS analyze_pdc_scan_root RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Legacy NN-only compatibility wrapper for the canonical reconstruction CLI
set(RECONSTRUCT_SN_NN_SRC ${CMAKE_SOURCE_DIR}/apps/run_reconstruction/main.cc)
if(EXISTS ${RECONSTRUCT_SN_NN_SRC})
    add_executable(reconstruct_sn_nn ${RECONSTRUCT_SN_NN_SRC})
    target_compile_definitions(reconstruct_sn_nn PRIVATE
        SMSIM_RECON_FORCE_BACKEND="nn"
        SMSIM_RECON_LOG_TAG="reconstruct_sn_nn"
        SMSIM_RECON_LEGACY_DEFAULTS=1
    )
    target_link_libraries(reconstruct_sn_nn PRIVATE
        analysis
        analysis_pdc_reco
        smdata
        ${ROOT_LIBRARIES}
    )
    install(TARGETS reconstruct_sn_nn RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Evaluate proton reconstruction accuracy from reco ROOT files
set(EVAL_TARGET_MOMENTUM_RECO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/evaluate_target_momentum_reco.cc)
if(EXISTS ${EVAL_TARGET_MOMENTUM_RECO_SRC})
    add_executable(evaluate_target_momentum_reco ${EVAL_TARGET_MOMENTUM_RECO_SRC})
    target_compile_definitions(evaluate_target_momentum_reco PRIVATE
        SMSIM_EVAL_LOG_TAG="evaluate_target_momentum_reco"
    )
    target_link_libraries(evaluate_target_momentum_reco PRIVATE ${ROOT_LIBRARIES})
    install(TARGETS evaluate_target_momentum_reco RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    add_executable(evaluate_reconstruct_sn_nn ${EVAL_TARGET_MOMENTUM_RECO_SRC})
    target_compile_definitions(evaluate_reconstruct_sn_nn PRIVATE
        SMSIM_EVAL_LOG_TAG="evaluate_reconstruct_sn_nn"
    )
    target_link_libraries(evaluate_reconstruct_sn_nn PRIVATE ${ROOT_LIBRARIES})
    install(TARGETS evaluate_reconstruct_sn_nn RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

set(ANALYZE_PDC_RK_ERROR_SRC ${CMAKE_CURRENT_SOURCE_DIR}/analyze_pdc_rk_error.cc)
if(EXISTS ${ANALYZE_PDC_RK_ERROR_SRC})
    add_executable(analyze_pdc_rk_error ${ANALYZE_PDC_RK_ERROR_SRC})
    target_link_libraries(analyze_pdc_rk_error PRIVATE
        analysis
        analysis_pdc_reco
        ${ROOT_LIBRARIES}
    )
    install(TARGETS analyze_pdc_rk_error RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
