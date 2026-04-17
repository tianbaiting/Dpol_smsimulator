#include "PDCRkAnalysisInternal.hh"

#include "ParticleTrajectory.hh"

#include "TDecompSVD.h"
#include "TMatrixDSym.h"
#include "TMatrixDSymEigen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace analysis::pdc::anaroot_like::detail {
namespace {

// [EN] Build 2x2 row-major whitening W from (sigma_u, sigma_v, rho) such that
// W * [r_u, r_v]^T produces two unit-variance independent residuals.
// Done via explicit 2x2 Cholesky inversion — avoids ROOT matrix overhead for
// a hot path called once per plane. / [CN] 由 (σ_u, σ_v, ρ) 构造 2x2 白化矩阵。
bool Build2DWhitening(double sigma_u,
                      double sigma_v,
                      double rho,
                      std::array<double, 4>* whitening) {
    if (!whitening) return false;
    if (!std::isfinite(sigma_u) || !std::isfinite(sigma_v) ||
        sigma_u <= 0.0 || sigma_v <= 0.0) {
        return false;
    }
    if (!std::isfinite(rho) || std::abs(rho) >= 0.999999) {
        return false;
    }
    const double a = sigma_u * sigma_u;
    const double b = rho * sigma_u * sigma_v;
    const double d = sigma_v * sigma_v;
    // Cholesky: Sigma = L L^T, L = [[l00, 0], [l10, l11]]
    const double l00 = std::sqrt(a);
    const double l10 = b / l00;
    const double l11_sq = d - l10 * l10;
    if (!(l11_sq > 0.0)) {
        return false;
    }
    const double l11 = std::sqrt(l11_sq);
    // W = L^{-1} = (1 / (l00*l11)) * [[l11, 0], [-l10, l00]]
    const double inv_det = 1.0 / (l00 * l11);
    (*whitening)[0] = l11 * inv_det;  // 1/l00
    (*whitening)[1] = 0.0;
    (*whitening)[2] = -l10 * inv_det;
    (*whitening)[3] = l00 * inv_det;  // 1/l11
    return true;
}

std::array<double, 2> ApplyWhitening2D(const std::array<double, 4>& whitening,
                                       double r_u,
                                       double r_v) {
    return {whitening[0] * r_u + whitening[1] * r_v,
            whitening[2] * r_u + whitening[3] * r_v};
}

double ConditionNumberFromSVD(TDecompSVD& svd) {
    const TVectorD sigmas = svd.GetSig();
    double sigma_max = 0.0;
    double sigma_min = std::numeric_limits<double>::infinity();
    for (int i = 0; i < sigmas.GetNrows(); ++i) {
        const double sigma = sigmas(i);
        if (!std::isfinite(sigma) || sigma <= 0.0) {
            continue;
        }
        sigma_max = std::max(sigma_max, sigma);
        sigma_min = std::min(sigma_min, sigma);
    }
    if (!(sigma_max > 0.0) || !std::isfinite(sigma_min) || sigma_min <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return sigma_max / sigma_min;
}

double SegmentLengthToIndex(const std::vector<ParticleTrajectory::TrajectoryPoint>& traj,
                            int index) {
    if (traj.size() < 2 || index <= 0) {
        return 0.0;
    }
    const int capped = std::min<int>(index, static_cast<int>(traj.size()) - 1);
    double length = 0.0;
    for (int i = 1; i <= capped; ++i) {
        length += SafeNorm(
            traj[static_cast<std::size_t>(i)].position -
            traj[static_cast<std::size_t>(i - 1)].position
        );
    }
    return length;
}

double ComputeBrhoTm(const TLorentzVector& p4, double charge_e) {
    if (!std::isfinite(p4.P()) || p4.P() <= 0.0 ||
        std::abs(charge_e) <= 1.0e-12) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (p4.P() / 1000.0) /
           (kBrhoGeVOverCPerTm * std::abs(charge_e));
}

// [EN] Real symmetric sqrt via eigen decomposition (for sampling from a
// Gaussian with the given covariance). Returns false if any eigenvalue is
// non-positive. / [CN] 对称半正定矩阵开方，用于高斯采样。
bool SymmetricSqrt(const TMatrixD& covariance, TMatrixD* sqrt_out) {
    if (!sqrt_out) return false;
    const int n = covariance.GetNrows();
    if (covariance.GetNcols() != n || n <= 0) return false;
    TMatrixDSym sym(n);
    for (int row = 0; row < n; ++row) {
        for (int col = 0; col < n; ++col) {
            const double v = 0.5 * (covariance(row, col) + covariance(col, row));
            if (!std::isfinite(v)) return false;
            sym(row, col) = v;
        }
    }
    TMatrixDSymEigen eigen(sym);
    const TVectorD evals = eigen.GetEigenValues();
    const TMatrixD evecs = eigen.GetEigenVectors();
    TMatrixD diag(n, n);
    diag.Zero();
    for (int i = 0; i < n; ++i) {
        const double lam = evals(i);
        if (!std::isfinite(lam) || lam <= 0.0) {
            return false;
        }
        diag(i, i) = std::sqrt(lam);
    }
    // sqrt = V * diag(sqrt(lambda)) * V^T
    sqrt_out->ResizeTo(n, n);
    *sqrt_out = evecs * diag * TMatrixD(TMatrixD::kTransposed, evecs);
    return true;
}

IntervalEstimate BuildEmpiricalInterval(double center_estimate,
                                        std::vector<double>* samples) {
    IntervalEstimate est;
    if (!samples || samples->empty()) {
        return est;
    }
    std::sort(samples->begin(), samples->end());
    const std::size_t n = samples->size();
    auto quantile = [&](double q) -> double {
        if (n == 1) return (*samples)[0];
        const double pos = q * static_cast<double>(n - 1);
        const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
        const std::size_t hi = std::min(lo + 1, n - 1);
        const double frac = pos - static_cast<double>(lo);
        return (*samples)[lo] * (1.0 - frac) + (*samples)[hi] * frac;
    };
    est.valid = true;
    est.center = center_estimate;
    est.lower68 = quantile(0.1587);
    est.upper68 = quantile(0.8413);
    est.lower95 = quantile(0.0228);
    est.upper95 = quantile(0.9772);
    est.sigma = 0.5 * (est.upper68 - est.lower68);
    return est;
}

}  // namespace

bool IsFinite(const TVector3& value) {
    return std::isfinite(value.X()) && std::isfinite(value.Y()) &&
           std::isfinite(value.Z());
}

bool IsFinite(const TLorentzVector& value) {
    return std::isfinite(value.Px()) && std::isfinite(value.Py()) &&
           std::isfinite(value.Pz()) && std::isfinite(value.E());
}

double SafeSigma(double sigma, double fallback) {
    if (std::isfinite(sigma) && sigma > 0.0) {
        return sigma;
    }
    return fallback;
}

double SafeNorm(const TVector3& value) {
    return std::sqrt(std::max(0.0, value.Mag2()));
}

double Clamp(double value, double lower, double upper) {
    if (!std::isfinite(value)) {
        return lower;
    }
    return std::max(lower, std::min(value, upper));
}

void MatrixToArray(const TMatrixD& matrix, std::array<double, 9>* out) {
    if (!out || matrix.GetNrows() != 3 || matrix.GetNcols() != 3) {
        return;
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            (*out)[static_cast<std::size_t>(row * 3 + col)] = matrix(row, col);
        }
    }
}

double ReducedChi2(double chi2_raw, int ndf) {
    if (!std::isfinite(chi2_raw)) {
        return std::numeric_limits<double>::infinity();
    }
    return chi2_raw / std::max(1, ndf);
}

IntervalEstimate BuildGaussianInterval(double center, double sigma) {
    IntervalEstimate estimate;
    if (!std::isfinite(center) || !std::isfinite(sigma) || sigma < 0.0) {
        return estimate;
    }
    estimate.valid = true;
    estimate.center = center;
    estimate.sigma = sigma;
    estimate.lower68 = center - sigma;
    estimate.upper68 = center + sigma;
    estimate.lower95 = center - kSigma95 * sigma;
    estimate.upper95 = center + kSigma95 * sigma;
    return estimate;
}

RkLeastSquaresAnalyzer::RkLeastSquaresAnalyzer(MagneticField* magnetic_field,
                                               const PDCInputTrack& track,
                                               const TargetConstraint& target,
                                               const RecoConfig& config)
    : fMagneticField(magnetic_field),
      fTrack(track),
      fTarget(target),
      fConfig(config),
      fLayout(BuildRkFitLayout()) {}

bool RkLeastSquaresAnalyzer::ValidateInputs(std::string* reason) const {
    if (!fMagneticField) {
        if (reason) *reason = "magnetic field pointer is null";
        return false;
    }
    if (!fTrack.IsValid() || !IsFinite(fTrack.pdc1) || !IsFinite(fTrack.pdc2)) {
        if (reason) *reason = "track points are invalid";
        return false;
    }
    if (!IsFinite(fTarget.target_position)) {
        if (reason) *reason = "target position is invalid";
        return false;
    }
    if (!std::isfinite(fTarget.mass_mev) || fTarget.mass_mev <= 0.0) {
        if (reason) *reason = "target mass is invalid";
        return false;
    }
    if (!std::isfinite(fTarget.charge_e) || std::abs(fTarget.charge_e) < 1.0e-9) {
        if (reason) *reason = "target charge is invalid";
        return false;
    }
    if (!std::isfinite(fConfig.p_min_mevc) || !std::isfinite(fConfig.p_max_mevc) ||
        fConfig.p_min_mevc <= 0.0 || fConfig.p_max_mevc <= fConfig.p_min_mevc) {
        if (reason) *reason = "momentum range is invalid";
        return false;
    }
    if (fConfig.max_iterations <= 0) {
        if (reason) *reason = "max_iterations must be positive";
        return false;
    }
    if (!std::isfinite(fConfig.rk_step_mm) || fConfig.rk_step_mm <= 0.0) {
        if (reason) *reason = "rk_step_mm must be positive";
        return false;
    }
    if (!SupportsCurrentMode()) {
        if (reason) *reason = "RK least-squares analysis does not support two-point-backprop mode";
        return false;
    }
    return true;
}

bool RkLeastSquaresAnalyzer::BuildMeasurementModel(std::string* reason) {
    fMeasurement = RkMeasurementModel{};
    fMeasurement.sigma_u_mm = SafeSigma(fTarget.pdc_sigma_u_mm, 2.0);
    fMeasurement.sigma_v_mm = SafeSigma(fTarget.pdc_sigma_v_mm, 2.0);
    fMeasurement.uv_correlation = fTarget.pdc_uv_correlation;
    if (!std::isfinite(fMeasurement.uv_correlation)) {
        fMeasurement.uv_correlation = 0.0;
    }
    fMeasurement.uv_correlation = Clamp(fMeasurement.uv_correlation, -0.99, 0.99);

    // [EN] Resolve wire directions: explicit overrides win; otherwise derive
    // from pdc_angle_deg using the 3D formula consistent with PDCSimAna
    // (u = (x_rot+y)/√2, v = (x_rot-y)/√2 → lab-frame directions below).
    // / [CN] 丝方向：优先使用显式 u_dir/v_dir；否则由 pdc_angle_deg 推导
    // 与 PDCSimAna 一致的 3D 丝方向（包含 z 分量以保留曲率信息）。
    TVector3 u_dir = fTarget.pdc_u_dir;
    TVector3 v_dir = fTarget.pdc_v_dir;
    if (u_dir.Mag2() < 1.0e-12 || v_dir.Mag2() < 1.0e-12) {
        const double angle_rad = (std::isfinite(fTarget.pdc_angle_deg)
                                      ? fTarget.pdc_angle_deg
                                      : 57.0) * M_PI / 180.0;
        const double inv_sqrt2 = 1.0 / std::sqrt(2.0);
        u_dir = TVector3(inv_sqrt2 * std::cos(angle_rad), inv_sqrt2, inv_sqrt2 * std::sin(angle_rad));
        v_dir = TVector3(inv_sqrt2 * std::cos(angle_rad), -inv_sqrt2, inv_sqrt2 * std::sin(angle_rad));
    } else {
        u_dir = u_dir.Unit();
        v_dir = v_dir.Unit();
    }
    if (u_dir.Mag2() < 1.0e-12 || v_dir.Mag2() < 1.0e-12) {
        if (reason) *reason = "PDC wire directions are degenerate";
        return false;
    }
    fMeasurement.u_dir = u_dir;
    fMeasurement.v_dir = v_dir;

    if (!Build2DWhitening(fMeasurement.sigma_u_mm,
                          fMeasurement.sigma_v_mm,
                          fMeasurement.uv_correlation,
                          &fMeasurement.whitening)) {
        if (reason) *reason = "failed to build 2D whitening matrix for PDC (u,v)";
        return false;
    }
    return true;
}

RkFitLayout RkLeastSquaresAnalyzer::BuildRkFitLayout() const {
    RkFitLayout layout;
    if (fConfig.rk_fit_mode == RkFitMode::kFixedTargetPdcOnly ||
        fConfig.rk_fit_mode == RkFitMode::kTwoPointBackprop) {
        // [EN] Only (u, v, p) are free; target xy held at nominal. 4 PDC
        // residuals − 3 params = ndf 1. kTwoPointBackprop uses the same layout
        // as kFixedTargetPdcOnly (the legacy 1D scan is replaced). / [CN]
        // 仅 (u, v, p) 自由；ndf = 1。kTwoPointBackprop 使用与 kFixedTargetPdcOnly
        // 相同的参数化（替代 legacy 1D 扫描）。
        layout.active_parameter_indices = {2, 3, 4, 0, 0};
        layout.parameter_count = 3;
        layout.residual_count = 4;
        layout.include_start_xy_constraint = false;
        layout.fixed_target_position = true;
        return layout;
    }
    // [EN] kThreePointFree: (dx, dy, u, v, p) with soft target xy prior.
    // 4 PDC + 2 target xy = 6 residuals − 5 params = ndf 1. / [CN] 自由模式。
    layout.active_parameter_indices = {0, 1, 2, 3, 4};
    layout.parameter_count = 5;
    layout.residual_count = 6;
    layout.include_start_xy_constraint = true;
    layout.fixed_target_position = false;
    return layout;
}

bool RkLeastSquaresAnalyzer::Initialize(std::string* reason) {
    if (!ValidateInputs(reason)) {
        return false;
    }
    if (!BuildMeasurementModel(reason)) {
        return false;
    }
    fInitialState = BuildInitialState();
    fInitialized = true;
    return true;
}

bool RkLeastSquaresAnalyzer::SupportsCurrentMode() const {
    return true;
}

RkParameterState RkLeastSquaresAnalyzer::BuildInitialState() {
    // [EN] Straight-line direction from target to farthest PDC — only a rough
    // guess because the magnetic field deflects protons by ~25° at typical
    // momenta. We use this as the center of a coarse grid search. / [CN]
    // 靶点到最远 PDC 的直线方向仅作粗网格搜索的中心；磁场偏转 ~25°，直线方向
    // 与真实出射方向差异很大。
    const double dist1 = SafeNorm(fTrack.pdc1 - fTarget.target_position);
    const double dist2 = SafeNorm(fTrack.pdc2 - fTarget.target_position);
    TVector3 init_dir = ((dist1 > dist2) ? fTrack.pdc1 : fTrack.pdc2) -
                        fTarget.target_position;
    if (init_dir.Mag2() < 1.0e-12) {
        init_dir = fTrack.pdc2 - fTrack.pdc1;
    }
    if (init_dir.Mag2() < 1.0e-12) {
        init_dir.SetXYZ(0.0, 0.0, 1.0);
    }
    init_dir = init_dir.Unit();
    if (std::abs(init_dir.Z()) < 1.0e-6) {
        init_dir.SetZ((init_dir.Z() >= 0.0) ? 1.0e-6 : -1.0e-6);
        init_dir = init_dir.Unit();
    }
    const double u_center = init_dir.X() / init_dir.Z();
    const double v_center = init_dir.Y() / init_dir.Z();

    // [EN] Coarse grid search over (u, v, p) to find a starting point that
    // is close enough for Levenberg-Marquardt to converge to the global
    // minimum.  The magnetic field bends trajectories primarily in the xz
    // plane, so u needs a wider scan range than v. / [CN] 粗网格搜描 (u,v,p)
    // 以找到足够接近全局最小的起点；磁场弯曲主要在 xz 平面故 u 范围更宽。
    constexpr int kGridU = 7;
    constexpr int kGridV = 3;
    constexpr int kGridP = 5;
    constexpr double kUHalfRange = 1.0;
    constexpr double kVHalfRange = 0.3;
    constexpr double kMomentumGrid[kGridP] = {200.0, 400.0, 700.0, 1200.0, 2000.0};

    // [EN] Collect grid candidates sorted by chi2, keep top N for multi-start.
    // / [CN] 收集网格候选按chi2排序，保留前N个用于多起点优化。
    constexpr int kMaxCandidates = 5;
    struct GridCandidate {
        RkParameterState state;
        double chi2 = std::numeric_limits<double>::infinity();
    };
    std::vector<GridCandidate> candidates;
    candidates.reserve(kGridU * kGridV * kGridP);

    for (int iu = 0; iu < kGridU; ++iu) {
        const double u = u_center + kUHalfRange * (2.0 * iu / (kGridU - 1) - 1.0);
        for (int iv = 0; iv < kGridV; ++iv) {
            const double v = v_center + kVHalfRange * (2.0 * iv / (kGridV - 1) - 1.0);
            for (int ip = 0; ip < kGridP; ++ip) {
                RkParameterState trial;
                trial.u = u;
                trial.v = v;
                trial.p = Clamp(kMomentumGrid[ip], fConfig.p_min_mevc, fConfig.p_max_mevc);
                const RkEvalResult eval = Evaluate(trial);
                if (eval.valid) {
                    candidates.push_back({trial, eval.chi2_raw});
                }
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const GridCandidate& a, const GridCandidate& b) {
                  return a.chi2 < b.chi2;
              });

    fInitialCandidates.clear();
    const int n_keep = std::min(kMaxCandidates, static_cast<int>(candidates.size()));
    for (int i = 0; i < n_keep; ++i) {
        fInitialCandidates.push_back(ClampState(candidates[static_cast<std::size_t>(i)].state));
    }

    if (fInitialCandidates.empty()) {
        RkParameterState fallback;
        fallback.u = u_center;
        fallback.v = v_center;
        fallback.p = Clamp(fConfig.initial_p_mevc, fConfig.p_min_mevc, fConfig.p_max_mevc);
        fInitialCandidates.push_back(ClampState(fallback));
    }

    return fInitialCandidates.front();
}

RkParameterState RkLeastSquaresAnalyzer::ClampState(const RkParameterState& state) const {
    RkParameterState clipped = state;
    clipped.dx = Clamp(clipped.dx, -200.0, 200.0);
    clipped.dy = Clamp(clipped.dy, -200.0, 200.0);
    clipped.u = Clamp(clipped.u, -2.5, 2.5);
    clipped.v = Clamp(clipped.v, -2.5, 2.5);
    clipped.p = Clamp(clipped.p, fConfig.p_min_mevc, fConfig.p_max_mevc);
    return clipped;
}

double RkLeastSquaresAnalyzer::GetParameter(const RkParameterState& state,
                                            int full_index) const {
    if (full_index == 0) return state.dx;
    if (full_index == 1) return state.dy;
    if (full_index == 2) return state.u;
    if (full_index == 3) return state.v;
    return state.p;
}

RkParameterState RkLeastSquaresAnalyzer::AddDelta(const RkParameterState& state,
                                                  int full_index,
                                                  double delta) const {
    RkParameterState shifted = state;
    if (full_index == 0) shifted.dx += delta;
    if (full_index == 1) shifted.dy += delta;
    if (full_index == 2) shifted.u += delta;
    if (full_index == 3) shifted.v += delta;
    if (full_index == 4) shifted.p += delta;
    return shifted;
}

TVector3 RkLeastSquaresAnalyzer::BuildMomentumVector(const RkParameterState& state) const {
    const double p_mag = state.p;
    if (!std::isfinite(p_mag) || p_mag <= 0.0) {
        return TVector3(0.0, 0.0, 0.0);
    }
    const double denom = std::sqrt(1.0 + state.u * state.u + state.v * state.v);
    if (!std::isfinite(denom) || denom <= 1.0e-12) {
        return TVector3(0.0, 0.0, 0.0);
    }
    const double pz = p_mag / denom;
    return TVector3(state.u * pz, state.v * pz, pz);
}

RkEvalResult RkLeastSquaresAnalyzer::Evaluate(const RkParameterState& raw_state) const {
    RkEvalResult eval;
    const RkParameterState state = ClampState(raw_state);
    const TVector3 start_pos = fLayout.fixed_target_position
        ? fTarget.target_position
        : TVector3(fTarget.target_position.X() + state.dx,
                   fTarget.target_position.Y() + state.dy,
                   fTarget.target_position.Z());
    const TVector3 momentum = BuildMomentumVector(state);
    if (!IsFinite(start_pos) || momentum.Mag2() < 1.0e-12 || !IsFinite(momentum)) {
        return eval;
    }

    const double mass = std::isfinite(fTarget.mass_mev) ? fTarget.mass_mev : kDefaultMassMeV;
    const double energy = std::sqrt(momentum.Mag2() + mass * mass);
    TLorentzVector p4(momentum.X(), momentum.Y(), momentum.Z(), energy);
    if (!IsFinite(p4)) {
        return eval;
    }

    ParticleTrajectory tracer(fMagneticField);
    tracer.SetStepSize(fConfig.rk_step_mm);
    // [EN] Keep transport window long enough to pass both PDC planes under strong curvature. / [CN] 足够长的传输窗口。
    const double far_dist = std::max(SafeNorm(fTrack.pdc1 - start_pos),
                                     SafeNorm(fTrack.pdc2 - start_pos));
    tracer.SetMaxDistance(std::max(1500.0, far_dist + 1500.0));
    tracer.SetMaxTime(400.0);
    tracer.SetMinMomentum(5.0);

    eval.trajectory = tracer.CalculateTrajectory(start_pos, p4, fTarget.charge_e, mass);
    if (eval.trajectory.size() < 2) {
        return eval;
    }

    double best1 = std::numeric_limits<double>::infinity();
    double best2 = std::numeric_limits<double>::infinity();
    int idx1 = -1;
    int idx2 = -1;
    for (int i = 0; i < static_cast<int>(eval.trajectory.size()); ++i) {
        const auto& point = eval.trajectory[static_cast<std::size_t>(i)];
        const double d1 = (point.position - fTrack.pdc1).Mag2();
        const double d2 = (point.position - fTrack.pdc2).Mag2();
        if (d1 < best1) {
            best1 = d1;
            idx1 = i;
        }
        if (d2 < best2) {
            best2 = d2;
            idx2 = i;
        }
    }

    if (idx1 < 0 || idx2 < 0) {
        return eval;
    }

    eval.idx_pdc1 = idx1;
    eval.idx_pdc2 = idx2;

    // [EN] Project trajectory-minus-hit displacement into the wire frame.
    // Each plane contributes two whitened residuals in (u, v). The out-of-plane
    // component is discarded — it is not a real measurement. / [CN] 将轨迹到
    // 命中点位移投影到丝坐标系，每个平面产生两个残差；离面分量不计入。
    const TVector3 diff1 = eval.trajectory[static_cast<std::size_t>(idx1)].position - fTrack.pdc1;
    const TVector3 diff2 = eval.trajectory[static_cast<std::size_t>(idx2)].position - fTrack.pdc2;
    const double r_u1 = diff1.Dot(fMeasurement.u_dir);
    const double r_v1 = diff1.Dot(fMeasurement.v_dir);
    const double r_u2 = diff2.Dot(fMeasurement.u_dir);
    const double r_v2 = diff2.Dot(fMeasurement.v_dir);
    const auto w1 = ApplyWhitening2D(fMeasurement.whitening, r_u1, r_v1);
    const auto w2 = ApplyWhitening2D(fMeasurement.whitening, r_u2, r_v2);
    eval.residuals[0] = w1[0];
    eval.residuals[1] = w1[1];
    eval.residuals[2] = w2[0];
    eval.residuals[3] = w2[1];
    eval.used_measurement_covariance = true;

    if (fLayout.include_start_xy_constraint) {
        const double target_sigma = SafeSigma(fTarget.target_sigma_xy_mm, 5.0);
        eval.residuals[4] = state.dx / target_sigma;
        eval.residuals[5] = state.dy / target_sigma;
    }

    eval.residual_count = fLayout.residual_count;
    eval.ndf = fLayout.residual_count - fLayout.parameter_count;
    eval.chi2_raw = 0.0;
    for (int i = 0; i < eval.residual_count; ++i) {
        eval.chi2_raw += eval.residuals[static_cast<std::size_t>(i)] *
                         eval.residuals[static_cast<std::size_t>(i)];
    }
    eval.chi2_reduced = ReducedChi2(eval.chi2_raw, eval.ndf);
    // [EN] In-plane wire-frame distance (unwhitened, mm) — the physical
    // notion of "how far off the fit is at the hit". / [CN] 平面内丝坐标
    // 未白化距离 (mm)。
    const double in_plane1 = std::sqrt(r_u1 * r_u1 + r_v1 * r_v1);
    const double in_plane2 = std::sqrt(r_u2 * r_u2 + r_v2 * r_v2);
    eval.min_distance_mm = std::max(in_plane1, in_plane2);

    const double d_target_1 = SafeNorm(fTrack.pdc1 - fTarget.target_position);
    const double d_target_2 = SafeNorm(fTrack.pdc2 - fTarget.target_position);
    const int idx_far = (d_target_1 > d_target_2) ? idx1 : idx2;
    eval.path_length_mm = SegmentLengthToIndex(eval.trajectory, idx_far);
    eval.p4_at_target = p4;
    eval.brho_tm = ComputeBrhoTm(p4, fTarget.charge_e);
    eval.valid = true;
    return eval;
}

double RkLeastSquaresAnalyzer::ParameterStep(const RkParameterState& state,
                                             int full_index) const {
    if (full_index == 0 || full_index == 1) {
        return std::max(0.1, 0.05 * SafeSigma(fTarget.target_sigma_xy_mm, 5.0));
    }
    if (full_index == 2 || full_index == 3) {
        const double local = std::abs(GetParameter(state, full_index));
        return std::max(1.0e-4, 0.01 * std::max(local, 0.05));
    }
    // [EN] |p| step: 0.1% of current value, floor 0.5 MeV/c.
    return std::max(0.5, std::abs(state.p) * 1.0e-3);
}

double RkLeastSquaresAnalyzer::ParameterLowerBound(int full_index) const {
    if (full_index == 0 || full_index == 1) return -200.0;
    if (full_index == 2 || full_index == 3) return -2.5;
    return fConfig.p_min_mevc;
}

double RkLeastSquaresAnalyzer::ParameterUpperBound(int full_index) const {
    if (full_index == 0 || full_index == 1) return 200.0;
    if (full_index == 2 || full_index == 3) return 2.5;
    return fConfig.p_max_mevc;
}

bool RkLeastSquaresAnalyzer::BuildResidualJacobian(const RkParameterState& state,
                                                   const RkEvalResult& eval,
                                                   TMatrixD* jacobian) const {
    if (!jacobian) {
        return false;
    }
    jacobian->ResizeTo(fLayout.residual_count, fLayout.parameter_count);
    jacobian->Zero();
    for (int local_j = 0; local_j < fLayout.parameter_count; ++local_j) {
        const int full_j = fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)];
        const double step = ParameterStep(state, full_j);
        RkParameterState plus = ClampState(AddDelta(state, full_j, step));
        RkParameterState minus = ClampState(AddDelta(state, full_j, -step));
        const double plus_value = GetParameter(plus, full_j);
        const double minus_value = GetParameter(minus, full_j);
        const double denominator = plus_value - minus_value;
        if (!std::isfinite(denominator) || std::abs(denominator) < 1.0e-12) {
            continue;
        }

        const RkEvalResult eval_plus = Evaluate(plus);
        const RkEvalResult eval_minus = Evaluate(minus);
        if (eval_plus.valid && eval_minus.valid) {
            for (int i = 0; i < eval.residual_count; ++i) {
                (*jacobian)(i, local_j) =
                    (eval_plus.residuals[static_cast<std::size_t>(i)] -
                     eval_minus.residuals[static_cast<std::size_t>(i)]) /
                    denominator;
            }
            continue;
        }
        if (!eval_plus.valid) {
            continue;
        }
        const double forward_denom = plus_value - GetParameter(state, full_j);
        if (!std::isfinite(forward_denom) || std::abs(forward_denom) < 1.0e-12) {
            continue;
        }
        for (int i = 0; i < eval.residual_count; ++i) {
            (*jacobian)(i, local_j) =
                (eval_plus.residuals[static_cast<std::size_t>(i)] -
                 eval.residuals[static_cast<std::size_t>(i)]) /
                forward_denom;
        }
    }
    return true;
}

TMatrixD RkLeastSquaresAnalyzer::BuildNormalMatrix(const TMatrixD& jacobian) const {
    TMatrixD normal(fLayout.parameter_count, fLayout.parameter_count);
    normal.Zero();
    for (int i = 0; i < jacobian.GetNrows(); ++i) {
        for (int j = 0; j < jacobian.GetNcols(); ++j) {
            for (int k = 0; k < jacobian.GetNcols(); ++k) {
                normal(j, k) += jacobian(i, j) * jacobian(i, k);
            }
        }
    }
    return normal;
}

TVectorD RkLeastSquaresAnalyzer::BuildGradient(const TMatrixD& jacobian,
                                               const RkEvalResult& eval) const {
    TVectorD gradient(fLayout.parameter_count);
    gradient.Zero();
    for (int i = 0; i < eval.residual_count; ++i) {
        for (int j = 0; j < fLayout.parameter_count; ++j) {
            gradient(j) += jacobian(i, j) * eval.residuals[static_cast<std::size_t>(i)];
        }
    }
    return gradient;
}

TMatrixD RkLeastSquaresAnalyzer::BuildMomentumJacobian(const RkParameterState& state) const {
    TMatrixD jacobian(static_cast<int>(kMomentumDim), fLayout.parameter_count);
    jacobian.Zero();
    const TVector3 base = BuildMomentumVector(state);
    for (int local_j = 0; local_j < fLayout.parameter_count; ++local_j) {
        const int full_j = fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)];
        const double step = ParameterStep(state, full_j);
        RkParameterState plus = ClampState(AddDelta(state, full_j, step));
        RkParameterState minus = ClampState(AddDelta(state, full_j, -step));
        const double plus_value = GetParameter(plus, full_j);
        const double minus_value = GetParameter(minus, full_j);
        double denominator = plus_value - minus_value;
        TVector3 delta(0.0, 0.0, 0.0);
        if (std::isfinite(denominator) && std::abs(denominator) >= 1.0e-12) {
            delta = BuildMomentumVector(plus) - BuildMomentumVector(minus);
        } else {
            denominator = plus_value - GetParameter(state, full_j);
            if (!std::isfinite(denominator) || std::abs(denominator) < 1.0e-12) {
                continue;
            }
            delta = BuildMomentumVector(plus) - base;
        }
        jacobian(0, local_j) = delta.X() / denominator;
        jacobian(1, local_j) = delta.Y() / denominator;
        jacobian(2, local_j) = delta.Z() / denominator;
    }
    return jacobian;
}

TMatrixD RkLeastSquaresAnalyzer::BuildPriorPrecision() const {
    // [EN] Only |p| carries a Gaussian prior (from RecoConfig::momentum_prior).
    // u, v, dx, dy have no prior — the PDC hits + target constraint already
    // determine them without needing a fake regularization. / [CN] 只对 |p|
    // 施加先验，其它参数无先验。
    TMatrixD precision(fLayout.parameter_count, fLayout.parameter_count);
    precision.Zero();
    const MomentumPrior& prior = fConfig.momentum_prior;
    if (!prior.enabled || !std::isfinite(prior.sigma_mev_c) || prior.sigma_mev_c <= 0.0) {
        return precision;
    }
    const int p_local = FindLocalParameter(4);
    if (p_local >= 0) {
        precision(p_local, p_local) = 1.0 / (prior.sigma_mev_c * prior.sigma_mev_c);
    }
    return precision;
}

TVectorD RkLeastSquaresAnalyzer::BuildPriorGradient(const RkParameterState& state) const {
    // [EN] Gradient of -log(prior). Only the |p| entry is non-zero. Used by
    // MCMC / Laplace-at-MAP paths; ordinary Laplace-at-MLE does not need this.
    // [CN] -log(prior) 的梯度；只有 |p| 项非零。
    TVectorD gradient(fLayout.parameter_count);
    gradient.Zero();
    const MomentumPrior& prior = fConfig.momentum_prior;
    if (!prior.enabled || !std::isfinite(prior.sigma_mev_c) || prior.sigma_mev_c <= 0.0) {
        return gradient;
    }
    const int p_local = FindLocalParameter(4);
    if (p_local >= 0) {
        gradient(p_local) =
            (state.p - prior.center_mev_c) / (prior.sigma_mev_c * prior.sigma_mev_c);
    }
    return gradient;
}

int RkLeastSquaresAnalyzer::FindLocalParameter(int full_index) const {
    for (int local = 0; local < fLayout.parameter_count; ++local) {
        if (fLayout.active_parameter_indices[static_cast<std::size_t>(local)] == full_index) {
            return local;
        }
    }
    return -1;
}

bool RkLeastSquaresAnalyzer::HasActiveParameter(int full_index) const {
    return FindLocalParameter(full_index) >= 0;
}

bool RkLeastSquaresAnalyzer::CovarianceIsPositiveDefinite(const TMatrixD& covariance) const {
    if (covariance.GetNrows() != covariance.GetNcols() || covariance.GetNrows() <= 0) {
        return false;
    }
    TMatrixDSym sym(covariance.GetNrows());
    for (int row = 0; row < covariance.GetNrows(); ++row) {
        for (int col = 0; col < covariance.GetNcols(); ++col) {
            const double value = 0.5 * (covariance(row, col) + covariance(col, row));
            if (!std::isfinite(value)) {
                return false;
            }
            sym(row, col) = value;
        }
    }
    TMatrixDSymEigen eigen(sym);
    const TVectorD eigen_values = eigen.GetEigenValues();
    for (int i = 0; i < eigen_values.GetNrows(); ++i) {
        if (!(eigen_values(i) > 1.0e-12) || !std::isfinite(eigen_values(i))) {
            return false;
        }
    }
    return true;
}

bool RkLeastSquaresAnalyzer::InvertWithSVD(const TMatrixD& matrix,
                                           TMatrixD* inverse,
                                           double* condition_number) const {
    if (!inverse || matrix.GetNrows() != matrix.GetNcols()) {
        return false;
    }
    TDecompSVD svd(matrix);
    if (condition_number) {
        *condition_number = ConditionNumberFromSVD(svd);
    }
    const int dimension = matrix.GetNrows();
    TMatrixD local_inverse(dimension, dimension);
    for (int col = 0; col < dimension; ++col) {
        TVectorD basis(dimension);
        basis.Zero();
        basis(col) = 1.0;
        Bool_t ok = kFALSE;
        const TVectorD solution = svd.Solve(basis, ok);
        if (!ok) {
            return false;
        }
        for (int row = 0; row < dimension; ++row) {
            local_inverse(row, col) = solution(row);
        }
    }
    inverse->ResizeTo(dimension, dimension);
    *inverse = local_inverse;
    return true;
}

void RkLeastSquaresAnalyzer::StoreStateCovariance(const TMatrixD& state_covariance,
                                                  std::array<double, 25>* out) const {
    if (!out) {
        return;
    }
    out->fill(0.0);
    for (int local_row = 0; local_row < fLayout.parameter_count; ++local_row) {
        const int full_row = fLayout.active_parameter_indices[static_cast<std::size_t>(local_row)];
        for (int local_col = 0; local_col < fLayout.parameter_count; ++local_col) {
            const int full_col = fLayout.active_parameter_indices[static_cast<std::size_t>(local_col)];
            (*out)[static_cast<std::size_t>(full_row * static_cast<int>(kParameterCount) + full_col)] =
                state_covariance(local_row, local_col);
        }
    }
}

void RkLeastSquaresAnalyzer::FillMomentumUncertainty(const RkParameterState& state,
                                                     const TMatrixD& state_covariance,
                                                     std::array<double, 9>* covariance_out,
                                                     IntervalEstimate* px_interval,
                                                     IntervalEstimate* py_interval,
                                                     IntervalEstimate* pz_interval,
                                                     IntervalEstimate* p_interval) const {
    // [EN] Nonlinear Monte Carlo propagation: draw M Gaussian samples of the
    // state and push each through BuildMomentumVector (pure algebra, no RK).
    // Returns empirical covariance + asymmetric 68/95 intervals. Jacobian
    // propagation is used as a fallback if the state covariance is not PSD.
    // [CN] 非线性蒙卡传播：从高斯采样状态，走代数的 BuildMomentumVector，
    // 得到经验协方差与非对称 68/95 分位区间。协方差非正定时退化到 Jacobian。
    auto fill_from_jacobian = [&]() {
        const TMatrixD momentum_jacobian = BuildMomentumJacobian(state);
        const TMatrixD momentum_covariance =
            momentum_jacobian * state_covariance *
            TMatrixD(TMatrixD::kTransposed, momentum_jacobian);
        if (covariance_out) {
            MatrixToArray(momentum_covariance, covariance_out);
        }
        const TVector3 momentum = BuildMomentumVector(state);
        const double sigma_px = (momentum_covariance(0, 0) > 0.0) ? std::sqrt(momentum_covariance(0, 0)) : 0.0;
        const double sigma_py = (momentum_covariance(1, 1) > 0.0) ? std::sqrt(momentum_covariance(1, 1)) : 0.0;
        const double sigma_pz = (momentum_covariance(2, 2) > 0.0) ? std::sqrt(momentum_covariance(2, 2)) : 0.0;
        if (px_interval) *px_interval = BuildGaussianInterval(momentum.X(), sigma_px);
        if (py_interval) *py_interval = BuildGaussianInterval(momentum.Y(), sigma_py);
        if (pz_interval) *pz_interval = BuildGaussianInterval(momentum.Z(), sigma_pz);
        if (p_interval) {
            const double p_mag = momentum.Mag();
            double variance_p = 0.0;
            if (p_mag > 1.0e-12) {
                const double gx = momentum.X() / p_mag;
                const double gy = momentum.Y() / p_mag;
                const double gz = momentum.Z() / p_mag;
                variance_p =
                    gx * (momentum_covariance(0, 0) * gx + momentum_covariance(0, 1) * gy + momentum_covariance(0, 2) * gz) +
                    gy * (momentum_covariance(1, 0) * gx + momentum_covariance(1, 1) * gy + momentum_covariance(1, 2) * gz) +
                    gz * (momentum_covariance(2, 0) * gx + momentum_covariance(2, 1) * gy + momentum_covariance(2, 2) * gz);
            }
            *p_interval = BuildGaussianInterval(p_mag, (variance_p > 0.0) ? std::sqrt(variance_p) : 0.0);
        }
    };

    TMatrixD sqrt_cov;
    if (!SymmetricSqrt(state_covariance, &sqrt_cov)) {
        fill_from_jacobian();
        return;
    }

    const int n_params = fLayout.parameter_count;
    constexpr int kNumSamples = 256;
    std::mt19937 rng(0xC0FFEEu);
    std::normal_distribution<double> normal(0.0, 1.0);

    std::vector<double> px_samples;
    std::vector<double> py_samples;
    std::vector<double> pz_samples;
    std::vector<double> p_samples;
    px_samples.reserve(kNumSamples);
    py_samples.reserve(kNumSamples);
    pz_samples.reserve(kNumSamples);
    p_samples.reserve(kNumSamples);

    double sum_px = 0.0, sum_py = 0.0, sum_pz = 0.0;
    double sum_pxx = 0.0, sum_pyy = 0.0, sum_pzz = 0.0;
    double sum_pxy = 0.0, sum_pxz = 0.0, sum_pyz = 0.0;

    for (int s = 0; s < kNumSamples; ++s) {
        TVectorD z(n_params);
        for (int i = 0; i < n_params; ++i) {
            z(i) = normal(rng);
        }
        const TVectorD delta = sqrt_cov * z;
        RkParameterState sample_state = state;
        for (int local_j = 0; local_j < n_params; ++local_j) {
            const int full_j = fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)];
            sample_state = AddDelta(sample_state, full_j, delta(local_j));
        }
        sample_state = ClampState(sample_state);
        const TVector3 p_sample = BuildMomentumVector(sample_state);
        if (!IsFinite(p_sample) || p_sample.Mag2() < 1.0e-24) {
            continue;
        }
        px_samples.push_back(p_sample.X());
        py_samples.push_back(p_sample.Y());
        pz_samples.push_back(p_sample.Z());
        p_samples.push_back(p_sample.Mag());
        sum_px += p_sample.X();
        sum_py += p_sample.Y();
        sum_pz += p_sample.Z();
        sum_pxx += p_sample.X() * p_sample.X();
        sum_pyy += p_sample.Y() * p_sample.Y();
        sum_pzz += p_sample.Z() * p_sample.Z();
        sum_pxy += p_sample.X() * p_sample.Y();
        sum_pxz += p_sample.X() * p_sample.Z();
        sum_pyz += p_sample.Y() * p_sample.Z();
    }

    if (px_samples.size() < 8) {
        fill_from_jacobian();
        return;
    }

    const double n = static_cast<double>(px_samples.size());
    const double mean_px = sum_px / n;
    const double mean_py = sum_py / n;
    const double mean_pz = sum_pz / n;
    const double var_px = std::max(0.0, sum_pxx / n - mean_px * mean_px);
    const double var_py = std::max(0.0, sum_pyy / n - mean_py * mean_py);
    const double var_pz = std::max(0.0, sum_pzz / n - mean_pz * mean_pz);
    const double cov_pxy = sum_pxy / n - mean_px * mean_py;
    const double cov_pxz = sum_pxz / n - mean_px * mean_pz;
    const double cov_pyz = sum_pyz / n - mean_py * mean_pz;

    if (covariance_out) {
        (*covariance_out)[0] = var_px;
        (*covariance_out)[1] = cov_pxy;
        (*covariance_out)[2] = cov_pxz;
        (*covariance_out)[3] = cov_pxy;
        (*covariance_out)[4] = var_py;
        (*covariance_out)[5] = cov_pyz;
        (*covariance_out)[6] = cov_pxz;
        (*covariance_out)[7] = cov_pyz;
        (*covariance_out)[8] = var_pz;
    }

    const TVector3 central = BuildMomentumVector(state);
    if (px_interval) *px_interval = BuildEmpiricalInterval(central.X(), &px_samples);
    if (py_interval) *py_interval = BuildEmpiricalInterval(central.Y(), &py_samples);
    if (pz_interval) *pz_interval = BuildEmpiricalInterval(central.Z(), &pz_samples);
    if (p_interval) *p_interval = BuildEmpiricalInterval(central.Mag(), &p_samples);
}

double RkLeastSquaresAnalyzer::EvaluateChi2Raw(const RkParameterState& state) const {
    const RkEvalResult eval = Evaluate(state);
    return eval.valid ? eval.chi2_raw : std::numeric_limits<double>::infinity();
}

double RkLeastSquaresAnalyzer::EvaluateLogPrior(const RkParameterState& raw_state) const {
    // [EN] Only |p| carries a prior. Returns 0 if disabled (flat). / [CN] 只对 |p| 有先验。
    const MomentumPrior& prior = fConfig.momentum_prior;
    if (!prior.enabled || !std::isfinite(prior.sigma_mev_c) || prior.sigma_mev_c <= 0.0) {
        return 0.0;
    }
    const RkParameterState state = ClampState(raw_state);
    const double dp = state.p - prior.center_mev_c;
    return -0.5 * (dp * dp) / (prior.sigma_mev_c * prior.sigma_mev_c);
}

bool RkLeastSquaresAnalyzer::Solve(bool compute_uncertainty,
                                   bool compute_posterior_laplace,
                                   RkSolveResult* result) const {
    if (fInitialCandidates.size() <= 1) {
        return Solve(fInitialState, compute_uncertainty, compute_posterior_laplace, result);
    }
    // [EN] Multi-start: run LM from each grid candidate (without uncertainty),
    // then re-run the winner with full uncertainty.  The second LM pass from the
    // already-converged state costs 0–1 iterations. / [CN] 多起点：先不计算误差
    // 地从各候选跑 LM，选 chi2 最小的结果，再对赢家重跑一次带误差传播。
    RkSolveResult best;
    for (const auto& candidate : fInitialCandidates) {
        RkSolveResult trial;
        if (SolveImpl(candidate, -1, 0.0, false, false, &trial)) {
            if (!best.valid || trial.eval.chi2_raw < best.eval.chi2_raw) {
                best = trial;
            }
        }
    }
    if (!best.valid) {
        return false;
    }
    if (!compute_uncertainty && !compute_posterior_laplace) {
        *result = best;
        return true;
    }
    return SolveImpl(best.state, -1, 0.0, compute_uncertainty, compute_posterior_laplace, result);
}

bool RkLeastSquaresAnalyzer::Solve(const RkParameterState& initial_state,
                                   bool compute_uncertainty,
                                   bool compute_posterior_laplace,
                                   RkSolveResult* result) const {
    return SolveImpl(initial_state, -1, 0.0, compute_uncertainty, compute_posterior_laplace, result);
}

bool RkLeastSquaresAnalyzer::SolveWithFixedParameter(const RkParameterState& initial_state,
                                                     int fixed_full_index,
                                                     double fixed_value,
                                                     bool compute_uncertainty,
                                                     bool compute_posterior_laplace,
                                                     RkSolveResult* result) const {
    return SolveImpl(initial_state, fixed_full_index, fixed_value, compute_uncertainty, compute_posterior_laplace, result);
}

bool RkLeastSquaresAnalyzer::SolveImpl(const RkParameterState& raw_initial_state,
                                       int fixed_full_index,
                                       double fixed_value,
                                       bool compute_uncertainty,
                                       bool compute_posterior_laplace,
                                       RkSolveResult* result) const {
    if (!fInitialized || !result) {
        return false;
    }

    RkParameterState state = ClampState(raw_initial_state);
    if (fixed_full_index >= 0) {
        state = AddDelta(state, fixed_full_index, fixed_value - GetParameter(state, fixed_full_index));
        state = ClampState(state);
        state = AddDelta(state, fixed_full_index, fixed_value - GetParameter(state, fixed_full_index));
    }

    RkEvalResult current = Evaluate(state);
    if (!current.valid) {
        return false;
    }

    double lambda = Clamp(fConfig.lm_lambda_init, fConfig.lm_lambda_min, fConfig.lm_lambda_max);
    int accepted_iterations = 0;
    for (int iter = 0; iter < fConfig.max_iterations; ++iter) {
        TMatrixD jacobian(fLayout.residual_count, fLayout.parameter_count);
        BuildResidualJacobian(state, current, &jacobian);
        TMatrixD normal = BuildNormalMatrix(jacobian);
        TVectorD gradient = BuildGradient(jacobian, current);
        for (int j = 0; j < fLayout.parameter_count; ++j) {
            normal(j, j) += lambda * std::max(1.0, normal(j, j));
        }

        if (fixed_full_index >= 0) {
            const int fixed_local = FindLocalParameter(fixed_full_index);
            if (fixed_local >= 0) {
                for (int j = 0; j < fLayout.parameter_count; ++j) {
                    normal(fixed_local, j) = 0.0;
                    normal(j, fixed_local) = 0.0;
                }
                normal(fixed_local, fixed_local) = 1.0;
                gradient(fixed_local) = 0.0;
            }
        }

        TVectorD rhs(fLayout.parameter_count);
        for (int j = 0; j < fLayout.parameter_count; ++j) {
            rhs(j) = -gradient(j);
        }

        TDecompSVD solver(normal);
        Bool_t ok = kFALSE;
        const TVectorD delta = solver.Solve(rhs, ok);
        if (!ok) {
            lambda = std::min(fConfig.lm_lambda_max, lambda * 10.0);
            continue;
        }

        RkParameterState candidate = state;
        for (int local_j = 0; local_j < fLayout.parameter_count; ++local_j) {
            const int full_j = fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)];
            if (fixed_full_index >= 0 && full_j == fixed_full_index) {
                continue;
            }
            candidate = AddDelta(candidate, full_j, delta(local_j));
        }
        if (fixed_full_index >= 0) {
            candidate = AddDelta(candidate, fixed_full_index, fixed_value - GetParameter(candidate, fixed_full_index));
        }
        candidate = ClampState(candidate);
        if (fixed_full_index >= 0) {
            candidate = AddDelta(candidate, fixed_full_index, fixed_value - GetParameter(candidate, fixed_full_index));
            candidate = ClampState(candidate);
        }

        const RkEvalResult candidate_eval = Evaluate(candidate);
        if (candidate_eval.valid && candidate_eval.chi2_raw < current.chi2_raw) {
            const double improvement = current.chi2_raw - candidate_eval.chi2_raw;
            state = candidate;
            current = candidate_eval;
            lambda = std::max(fConfig.lm_lambda_min, lambda / 3.0);
            ++accepted_iterations;
            double delta_norm_sq = 0.0;
            for (int local_j = 0; local_j < fLayout.parameter_count; ++local_j) {
                if (fixed_full_index >= 0 &&
                    fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)] == fixed_full_index) {
                    continue;
                }
                delta_norm_sq += delta(local_j) * delta(local_j);
            }
            if (improvement < 1.0e-8 || std::sqrt(delta_norm_sq) < 1.0e-5) {
                break;
            }
        } else {
            lambda = std::min(fConfig.lm_lambda_max, lambda * 4.0);
            if (lambda >= fConfig.lm_lambda_max) {
                break;
            }
        }
    }

    result->valid = true;
    result->state = state;
    result->eval = current;
    result->accepted_iterations = accepted_iterations;

    if (compute_uncertainty) {
        TMatrixD jacobian(fLayout.residual_count, fLayout.parameter_count);
        if (BuildResidualJacobian(state, current, &jacobian)) {
            const TMatrixD normal = BuildNormalMatrix(jacobian);
            result->normal_matrix.ResizeTo(normal.GetNrows(), normal.GetNcols());
            result->normal_matrix = normal;
            TMatrixD state_covariance(fLayout.parameter_count, fLayout.parameter_count);
            double condition_number = std::numeric_limits<double>::quiet_NaN();
            if (InvertWithSVD(normal, &state_covariance, &condition_number)) {
                result->condition_number = condition_number;
                result->state_covariance.ResizeTo(state_covariance.GetNrows(),
                                                 state_covariance.GetNcols());
                result->state_covariance = state_covariance;
                StoreStateCovariance(state_covariance, &result->full_state_covariance);
                FillMomentumUncertainty(state,
                                        state_covariance,
                                        &result->momentum_covariance,
                                        &result->px_interval,
                                        &result->py_interval,
                                        &result->pz_interval,
                                        &result->p_interval);

                if (compute_posterior_laplace) {
                    TMatrixD posterior_normal = normal;
                    posterior_normal += BuildPriorPrecision();
                    TMatrixD posterior_covariance(fLayout.parameter_count, fLayout.parameter_count);
                    double posterior_condition = std::numeric_limits<double>::quiet_NaN();
                    if (InvertWithSVD(posterior_normal, &posterior_covariance, &posterior_condition)) {
                        result->posterior_condition_number = posterior_condition;
                        result->posterior_state_covariance.ResizeTo(
                            posterior_covariance.GetNrows(),
                            posterior_covariance.GetNcols()
                        );
                        result->posterior_state_covariance = posterior_covariance;
                        FillMomentumUncertainty(state,
                                                posterior_covariance,
                                                &result->posterior_momentum_covariance,
                                                &result->px_credible,
                                                &result->py_credible,
                                                &result->pz_credible,
                                                &result->p_credible);
                    }
                }
            }
        }
    }

    return true;
}

void RkLeastSquaresAnalyzer::FillRecoResult(const RkSolveResult& solve_result,
                                            RecoResult* result) const {
    if (!result) {
        return;
    }
    *result = RecoResult{};
    result->method_used = SolveMethod::kRungeKutta;
    result->iterations = solve_result.accepted_iterations;
    result->chi2 = solve_result.eval.chi2_reduced;
    result->chi2_raw = solve_result.eval.chi2_raw;
    result->chi2_reduced = solve_result.eval.chi2_reduced;
    result->ndf = solve_result.eval.ndf;
    result->used_measurement_covariance = solve_result.eval.used_measurement_covariance;
    result->min_distance_mm = solve_result.eval.min_distance_mm;
    result->path_length_mm = solve_result.eval.path_length_mm;
    result->brho_tm = solve_result.eval.brho_tm;
    result->p4_at_target = solve_result.eval.p4_at_target;
    result->fit_start_position = fLayout.fixed_target_position
        ? fTarget.target_position
        : TVector3(fTarget.target_position.X() + solve_result.state.dx,
                   fTarget.target_position.Y() + solve_result.state.dy,
                   fTarget.target_position.Z());
    result->normal_condition_number = solve_result.condition_number;
    result->state_covariance = solve_result.full_state_covariance;
    result->momentum_covariance = solve_result.momentum_covariance;
    result->posterior_momentum_covariance = solve_result.posterior_momentum_covariance;
    result->px_interval = solve_result.px_interval;
    result->py_interval = solve_result.py_interval;
    result->pz_interval = solve_result.pz_interval;
    result->p_interval = solve_result.p_interval;
    result->px_credible = solve_result.px_credible;
    result->py_credible = solve_result.py_credible;
    result->pz_credible = solve_result.pz_credible;
    result->p_credible = solve_result.p_credible;
    result->uncertainty_valid =
        solve_result.px_interval.valid && solve_result.py_interval.valid &&
        solve_result.pz_interval.valid && solve_result.p_interval.valid;
    result->posterior_valid =
        solve_result.px_credible.valid && solve_result.py_credible.valid &&
        solve_result.pz_credible.valid && solve_result.p_credible.valid;
    if (solve_result.eval.min_distance_mm <= fConfig.tolerance_mm) {
        result->status = SolverStatus::kSuccess;
        result->message = "RK fit converged";
    } else {
        result->status = SolverStatus::kNotConverged;
        result->message = "RK fit reached iteration limit";
    }
}

}  // namespace analysis::pdc::anaroot_like::detail
