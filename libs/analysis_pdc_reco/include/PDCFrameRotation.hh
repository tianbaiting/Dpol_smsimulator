#ifndef ANALYSIS_PDC_FRAME_ROTATION_HH
#define ANALYSIS_PDC_FRAME_ROTATION_HH

#include "PDCRecoTypes.hh"

#include "TVector3.h"

#include <array>
#include <cmath>

// [EN] Helpers to rotate PDC reconstruction results from the Geant4 lab frame
// into the event-generator "beam-as-Z" (target) frame. The forward simulation
// applies dir.rotateY(-angle_tgt) to the primary momentum direction in a local
// copy (DeutPrimaryGeneratorAction.cc), so the truth momentum stored in the
// output ROOT tree remains in the target frame. The reconstruction framework,
// in contrast, works entirely in the lab frame (hits, magnetic field, RK
// integration). To make truth vs. reco comparisons meaningful, reco output
// must be rotated by R_y(+angle_tgt) at the write stage.
// [CN] 把 PDC 重建结果从 Geant4 lab 系旋到事件发生器的 beam-as-Z（靶）系。
// 正向模拟只对 primary 动量的局部拷贝施加 dir.rotateY(-angle_tgt)，因此输出
// ROOT 里的 truth 保持靶系；重建框架内部全在 lab 系工作，为使 truth 与 reco
// 的比较一致，写输出前必须对 reco 施加 R_y(+angle_tgt)。
namespace analysis::pdc::anaroot_like {

// [EN] Apply R_y(α) to a row-major 3x3 covariance: Σ' = R Σ R^T.
// Matches ROOT's TVector3::RotateY(α) (x' = c·x + s·z, z' = -s·x + c·z).
// [CN] 对行优先 3x3 协方差施加 R_y(α)；与 TVector3::RotateY 一致。
inline std::array<double, 9> RotateCovarianceY(const std::array<double, 9>& cov, double angle_rad) {
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    const double r[9] = { c, 0.0, s,
                          0.0, 1.0, 0.0,
                         -s, 0.0, c };
    double m[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m[3*i + j] = r[3*i + 0] * cov[0*3 + j]
                       + r[3*i + 1] * cov[1*3 + j]
                       + r[3*i + 2] * cov[2*3 + j];
        }
    }
    std::array<double, 9> out{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out[3*i + j] = m[3*i + 0] * r[3*j + 0]
                         + m[3*i + 1] * r[3*j + 1]
                         + m[3*i + 2] * r[3*j + 2];
        }
    }
    return out;
}

// [EN] Build a symmetric Gaussian interval: 68% = ±σ, 95% = ±1.96σ.
// [CN] 对称高斯区间。
inline IntervalEstimate MakeGaussianInterval(double center, double sigma) {
    IntervalEstimate e;
    if (!std::isfinite(center) || !std::isfinite(sigma) || sigma < 0.0) {
        return e;
    }
    constexpr double kSigma95 = 1.959963984540054;
    e.valid = true;
    e.center = center;
    e.sigma = sigma;
    e.lower68 = center - sigma;
    e.upper68 = center + sigma;
    e.lower95 = center - kSigma95 * sigma;
    e.upper95 = center + kSigma95 * sigma;
    return e;
}

// [EN] Rotate a (px, py, pz) interval triple from lab → target frame under the
// Gaussian-diagonal approximation. Centers are rotated by R_y(+α). A diagonal
// covariance is built from the input σ's, rotated by R_y, and its diagonal is
// used as the new σ's. Asymmetric lower/upper quantile information is lost
// (Gaussian fallback); use this only when the caller already accepts that
// tradeoff. The `p` (|p|) interval is rotation-invariant and unchanged.
// [CN] 把 (px,py,pz) 区间从 lab 系旋到靶系，采用对角高斯近似：中心按
// R_y(+α) 旋转；由 σ 组成对角协方差后旋转，取新的对角作为新的 σ；丢失
// 非对称分位信息。|p| 旋转不变。
inline void RotateMomentumIntervalTripletY(IntervalEstimate& px,
                                           IntervalEstimate& py,
                                           IntervalEstimate& pz,
                                           double angle_rad) {
    const bool any_valid = px.valid || py.valid || pz.valid;
    if (!any_valid) return;

    TVector3 c(px.center, py.center, pz.center);
    c.RotateY(angle_rad);

    std::array<double, 9> diag{};
    diag[0] = (px.valid && px.sigma > 0.0) ? px.sigma * px.sigma : 0.0;
    diag[4] = (py.valid && py.sigma > 0.0) ? py.sigma * py.sigma : 0.0;
    diag[8] = (pz.valid && pz.sigma > 0.0) ? pz.sigma * pz.sigma : 0.0;
    const std::array<double, 9> rot = RotateCovarianceY(diag, angle_rad);

    const double sx = (rot[0] > 0.0) ? std::sqrt(rot[0]) : 0.0;
    const double sy = (rot[4] > 0.0) ? std::sqrt(rot[4]) : 0.0;
    const double sz = (rot[8] > 0.0) ? std::sqrt(rot[8]) : 0.0;

    if (px.valid) px = MakeGaussianInterval(c.X(), sx);
    if (py.valid) py = MakeGaussianInterval(c.Y(), sy);
    if (pz.valid) pz = MakeGaussianInterval(c.Z(), sz);
}

// [EN] Rotate a RecoResult from lab frame to target frame (R_y(+α)).
// credible intervals are rebuilt as Gaussian fallbacks from the rotated
// covariance; asymmetric MC/MCMC quantile information is not preserved
// because raw samples are not retained in RecoResult. p-related fields
// (|p|, p_interval, p_credible) are rotation-invariant and passed through.
// [CN] 把 RecoResult 从 lab 系旋到靶系；credible 区间用旋转后协方差的高斯
// 近似重建，|p| 相关字段旋转不变。
inline RecoResult RotateRecoResultToTargetFrame(const RecoResult& in, double angle_rad) {
    RecoResult r = in;
    TVector3 p3 = r.p4_at_target.Vect();
    p3.RotateY(angle_rad);
    r.p4_at_target.SetVect(p3);  // E unchanged: |p| invariant under rotation.

    r.momentum_covariance = RotateCovarianceY(in.momentum_covariance, angle_rad);
    r.posterior_momentum_covariance = RotateCovarianceY(in.posterior_momentum_covariance, angle_rad);

    const double sigma_px = (r.momentum_covariance[0] > 0.0) ? std::sqrt(r.momentum_covariance[0]) : 0.0;
    const double sigma_py = (r.momentum_covariance[4] > 0.0) ? std::sqrt(r.momentum_covariance[4]) : 0.0;
    const double sigma_pz = (r.momentum_covariance[8] > 0.0) ? std::sqrt(r.momentum_covariance[8]) : 0.0;

    if (r.px_interval.valid) r.px_interval = MakeGaussianInterval(p3.X(), sigma_px);
    if (r.py_interval.valid) r.py_interval = MakeGaussianInterval(p3.Y(), sigma_py);
    if (r.pz_interval.valid) r.pz_interval = MakeGaussianInterval(p3.Z(), sigma_pz);
    // p_interval unchanged: |p| invariant under rotation.

    const double post_sigma_px = (r.posterior_momentum_covariance[0] > 0.0) ? std::sqrt(r.posterior_momentum_covariance[0]) : 0.0;
    const double post_sigma_py = (r.posterior_momentum_covariance[4] > 0.0) ? std::sqrt(r.posterior_momentum_covariance[4]) : 0.0;
    const double post_sigma_pz = (r.posterior_momentum_covariance[8] > 0.0) ? std::sqrt(r.posterior_momentum_covariance[8]) : 0.0;

    if (r.px_credible.valid) r.px_credible = MakeGaussianInterval(p3.X(), post_sigma_px);
    if (r.py_credible.valid) r.py_credible = MakeGaussianInterval(p3.Y(), post_sigma_py);
    if (r.pz_credible.valid) r.pz_credible = MakeGaussianInterval(p3.Z(), post_sigma_pz);
    // p_credible unchanged.

    return r;
}

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_FRAME_ROTATION_HH
