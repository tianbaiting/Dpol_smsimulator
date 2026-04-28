#ifndef ANALYSIS_NEBULA_FRAME_ROTATION_HH
#define ANALYSIS_NEBULA_FRAME_ROTATION_HH

#include "RecoEvent.hh"

#include <cmath>

// [EN] Helpers to rotate NEBULAReconstructor results from the Geant4 lab frame
// into the event-generator "beam-as-Z" (target) frame. Mirrors the
// PDCFrameRotation API in libs/analysis_pdc_reco/ — applied at the same write-stage
// in apps/run_reconstruction/main.cc to keep neutron and proton in the same frame
// as truth_proton_p4 / truth_neutron_p4.
//
// Why this is needed: the forward simulator applies dir.rotateY(-angle_tgt) to
// the primary momentum direction in a local copy (DeutPrimaryGeneratorAction.cc),
// so truth_neutron_p4 in the output ROOT tree remains in the target frame. The
// NEBULA reco machinery, in contrast, computes neutron flight direction in the
// lab frame from PDC2-side hits → NEBULA module hits. Without rotation back,
// reco_neutron carries a systematic Δpx ≈ -p_z·sin(α_tgt) ≈ -33 MeV/c bias —
// the same kind of bias the proton path had before the 2026-04-19 frame fix.
//
// [CN] 把 NEBULA 重建的中子结果从 Geant4 lab 系旋到事件发生器的 beam-as-Z 系
// （靶系）。与 PDCFrameRotation 的 API 对称；同样在 apps/run_reconstruction/main.cc
// 写出阶段调用以保证 reco_neutron 与 truth_neutron_p4 同坐标系。
namespace analysis::nebula {

// [EN] Rotate a single RecoNeutron's flight direction by R_y(α). beta, energy,
// timeOfFlight, flightLength are scalars and unaffected. position is a hit
// location in lab-frame detector coordinates and is NOT rotated (it carries
// physical detector geometry, not a momentum component).
// [CN] 把单条 RecoNeutron 的飞行方向绕 y 轴旋转。beta/energy/ToF/flightLength
// 是标量不变；position 是探测器 lab 系的击中位置，不旋转。
inline void RotateRecoNeutronY(RecoNeutron& neutron, double angle_rad) {
    neutron.direction.RotateY(angle_rad);
}

// [EN] Rotate every RecoNeutron in a RecoEvent in-place. Convenience wrapper
// matching the pattern of analysis::pdc::anaroot_like::RotateRecoResultToTargetFrame.
// [CN] 对 RecoEvent 中所有中子原地旋转的便捷封装。
inline void RotateRecoNeutronsToTargetFrame(RecoEvent& event, double angle_rad) {
    for (auto& neutron : event.neutrons) {
        RotateRecoNeutronY(neutron, angle_rad);
    }
}

}  // namespace analysis::nebula

#endif  // ANALYSIS_NEBULA_FRAME_ROTATION_HH
