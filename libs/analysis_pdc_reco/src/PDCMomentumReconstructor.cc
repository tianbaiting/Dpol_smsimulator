#include "PDCMomentumReconstructor.hh"
#include "PDCNNMomentumReconstructor.hh"

#include "SMLogger.hh"

#include "TDecompSVD.h"
#include "TMatrixD.h"
#include "TVectorD.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

namespace analysis::pdc::anaroot_like {
namespace {

constexpr std::size_t kResidualCount = 8;
constexpr std::size_t kParameterCount = 5;
constexpr double kBrhoGeVOverCPerTm = 0.299792458;
constexpr double kDefaultMassMeV = 938.2720813;

double SafeSigma(double sigma, double fallback) {
    if (std::isfinite(sigma) && sigma > 0.0) {
        return sigma;
    }
    return fallback;
}

double SafeNorm(const TVector3& value) {
    return std::sqrt(std::max(0.0, value.Mag2()));
}

double SegmentLengthToIndex(const std::vector<ParticleTrajectory::TrajectoryPoint>& traj, int index) {
    if (traj.size() < 2 || index <= 0) {
        return 0.0;
    }
    const int capped = std::min<int>(index, static_cast<int>(traj.size()) - 1);
    double length = 0.0;
    for (int i = 1; i <= capped; ++i) {
        length += SafeNorm(traj[i].position - traj[i - 1].position);
    }
    return length;
}

double DistancePointToLine(const TVector3& point, const TVector3& line_origin, const TVector3& line_direction) {
    if (line_direction.Mag2() < 1.0e-16) {
        return SafeNorm(point - line_origin);
    }
    const TVector3 unit = line_direction.Unit();
    return SafeNorm((point - line_origin).Cross(unit));
}

std::string FormatPathStatus(const char* env_name, const char* value) {
    std::ostringstream oss;
    oss << env_name << "=" << (value ? value : "<unset>");
    return oss.str();
}

}  // namespace

PDCMomentumReconstructor::PDCMomentumReconstructor(MagneticField* magnetic_field)
    : fMagneticField(magnetic_field) {}

bool PDCMomentumReconstructor::IsFinite(const TVector3& value) {
    return std::isfinite(value.X()) && std::isfinite(value.Y()) && std::isfinite(value.Z());
}

bool PDCMomentumReconstructor::IsFinite(const TLorentzVector& value) {
    return std::isfinite(value.Px()) && std::isfinite(value.Py()) && std::isfinite(value.Pz()) && std::isfinite(value.E());
}

double PDCMomentumReconstructor::Clamp(double value, double lower, double upper) {
    if (!std::isfinite(value)) {
        return lower;
    }
    return std::max(lower, std::min(value, upper));
}

bool PDCMomentumReconstructor::ValidateInputs(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config,
    std::string* reason
) const {
    if (!fMagneticField) {
        if (reason) *reason = "magnetic field pointer is null";
        return false;
    }
    if (!track.IsValid() || !IsFinite(track.pdc1) || !IsFinite(track.pdc2)) {
        if (reason) *reason = "track points are invalid";
        return false;
    }
    if (!IsFinite(target.target_position)) {
        if (reason) *reason = "target position is invalid";
        return false;
    }
    if (!std::isfinite(target.mass_mev) || target.mass_mev <= 0.0) {
        if (reason) *reason = "target mass is invalid";
        return false;
    }
    if (!std::isfinite(target.charge_e) || std::abs(target.charge_e) < 1.0e-9) {
        if (reason) *reason = "target charge is invalid";
        return false;
    }
    if (!std::isfinite(config.p_min_mevc) || !std::isfinite(config.p_max_mevc) || config.p_min_mevc <= 0.0 ||
        config.p_max_mevc <= config.p_min_mevc) {
        if (reason) *reason = "momentum range is invalid";
        return false;
    }
    if (config.max_iterations <= 0) {
        if (reason) *reason = "max_iterations must be positive";
        return false;
    }
    if (!std::isfinite(config.rk_step_mm) || config.rk_step_mm <= 0.0) {
        if (reason) *reason = "rk_step_mm must be positive";
        return false;
    }
    return true;
}

PDCMomentumReconstructor::ParameterState PDCMomentumReconstructor::BuildInitialState(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    const double dist1 = SafeNorm(track.pdc1 - target.target_position);
    const double dist2 = SafeNorm(track.pdc2 - target.target_position);
    const TVector3 farther = (dist1 > dist2) ? track.pdc1 : track.pdc2;
    TVector3 init_dir = farther - target.target_position;
    if (init_dir.Mag2() < 1.0e-12) {
        init_dir = track.pdc2 - track.pdc1;
    }
    if (init_dir.Mag2() < 1.0e-12) {
        init_dir.SetXYZ(0.0, 0.0, 1.0);
    }
    init_dir = init_dir.Unit();
    if (std::abs(init_dir.Z()) < 1.0e-6) {
        init_dir.SetZ((init_dir.Z() >= 0.0) ? 1.0e-6 : -1.0e-6);
        init_dir = init_dir.Unit();
    }

    const double p_init = Clamp(config.initial_p_mevc, config.p_min_mevc, config.p_max_mevc);
    ParameterState state;
    state.dx = 0.0;
    state.dy = 0.0;
    state.u = init_dir.X() / init_dir.Z();
    state.v = init_dir.Y() / init_dir.Z();
    state.q = target.charge_e / p_init;
    return ClampState(state, target, config);
}

PDCMomentumReconstructor::ParameterState PDCMomentumReconstructor::ClampState(
    const ParameterState& state,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    ParameterState clipped = state;
    const double invp_min = std::abs(target.charge_e) / config.p_max_mevc;
    const double invp_max = std::abs(target.charge_e) / config.p_min_mevc;
    const double q_sign = (target.charge_e >= 0.0) ? 1.0 : -1.0;

    clipped.dx = Clamp(clipped.dx, -200.0, 200.0);
    clipped.dy = Clamp(clipped.dy, -200.0, 200.0);
    clipped.u = Clamp(clipped.u, -2.5, 2.5);
    clipped.v = Clamp(clipped.v, -2.5, 2.5);

    double q_abs = std::abs(clipped.q);
    if (!std::isfinite(q_abs) || q_abs < invp_min) q_abs = invp_min;
    if (q_abs > invp_max) q_abs = invp_max;
    clipped.q = q_sign * q_abs;
    return clipped;
}

TVector3 PDCMomentumReconstructor::BuildMomentumVector(const ParameterState& state, double charge_e) const {
    const double q_abs = std::abs(state.q);
    if (!std::isfinite(q_abs) || q_abs < 1.0e-12) {
        return TVector3(0.0, 0.0, 0.0);
    }
    const double p_mag = std::abs(charge_e) / q_abs;
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

double PDCMomentumReconstructor::ResidualChi2(const std::array<double, 8>& residuals) {
    double sum = 0.0;
    for (const double value : residuals) {
        sum += value * value;
    }
    constexpr double ndf = static_cast<double>(kResidualCount) - static_cast<double>(kParameterCount);
    return sum / std::max(1.0, ndf);
}

PDCMomentumReconstructor::EvalResult PDCMomentumReconstructor::EvaluateState(
    const ParameterState& state,
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    EvalResult eval;
    const TVector3 start_pos(target.target_position.X() + state.dx, target.target_position.Y() + state.dy, target.target_position.Z());
    const TVector3 momentum = BuildMomentumVector(state, target.charge_e);
    if (!IsFinite(start_pos) || momentum.Mag2() < 1.0e-12 || !IsFinite(momentum)) {
        return eval;
    }

    const double mass = std::isfinite(target.mass_mev) ? target.mass_mev : kDefaultMassMeV;
    const double energy = std::sqrt(momentum.Mag2() + mass * mass);
    TLorentzVector p4(momentum.X(), momentum.Y(), momentum.Z(), energy);
    if (!IsFinite(p4)) {
        return eval;
    }

    ParticleTrajectory tracer(fMagneticField);
    tracer.SetStepSize(config.rk_step_mm);
    // [EN] Extend trajectory length enough to intersect both PDC planes even under strong bending. / [CN] 延长轨迹长度以保证在强弯转情况下仍能穿过两个PDC平面。
    const double far_dist = std::max(SafeNorm(track.pdc1 - start_pos), SafeNorm(track.pdc2 - start_pos));
    tracer.SetMaxDistance(std::max(1500.0, far_dist + 1500.0));
    tracer.SetMaxTime(400.0);
    tracer.SetMinMomentum(5.0);

    eval.trajectory = tracer.CalculateTrajectory(start_pos, p4, target.charge_e, mass);
    if (eval.trajectory.size() < 2) {
        return eval;
    }

    double best1 = std::numeric_limits<double>::infinity();
    double best2 = std::numeric_limits<double>::infinity();
    int idx1 = -1;
    int idx2 = -1;

    for (int i = 0; i < static_cast<int>(eval.trajectory.size()); ++i) {
        const auto& point = eval.trajectory[static_cast<std::size_t>(i)];
        const double d1 = (point.position - track.pdc1).Mag2();
        const double d2 = (point.position - track.pdc2).Mag2();
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

    const TVector3 diff1 = eval.trajectory[static_cast<std::size_t>(idx1)].position - track.pdc1;
    const TVector3 diff2 = eval.trajectory[static_cast<std::size_t>(idx2)].position - track.pdc2;

    const double pdc_sigma = SafeSigma(target.pdc_sigma_mm, 2.0);
    const double target_sigma = SafeSigma(target.target_sigma_xy_mm, 5.0);

    eval.residuals[0] = diff1.X() / pdc_sigma;
    eval.residuals[1] = diff1.Y() / pdc_sigma;
    eval.residuals[2] = diff1.Z() / pdc_sigma;
    eval.residuals[3] = diff2.X() / pdc_sigma;
    eval.residuals[4] = diff2.Y() / pdc_sigma;
    eval.residuals[5] = diff2.Z() / pdc_sigma;
    // [EN] Anchor target offsets to avoid drifting into unconstrained solutions in 5D fit. / [CN] 给靶点偏移加约束，避免5维拟合漂移到欠约束解。
    eval.residuals[6] = state.dx / target_sigma;
    eval.residuals[7] = state.dy / target_sigma;

    eval.chi2 = ResidualChi2(eval.residuals);
    eval.min_distance_mm = std::max(std::sqrt(best1), std::sqrt(best2));

    const double d_target_1 = SafeNorm(track.pdc1 - target.target_position);
    const double d_target_2 = SafeNorm(track.pdc2 - target.target_position);
    const int idx_far = (d_target_1 > d_target_2) ? idx1 : idx2;
    eval.path_length_mm = SegmentLengthToIndex(eval.trajectory, idx_far);

    eval.p4_at_target = p4;
    if (std::abs(target.charge_e) > 1.0e-12) {
        eval.brho_tm = (p4.P() / 1000.0) / (kBrhoGeVOverCPerTm * std::abs(target.charge_e));
    } else {
        eval.brho_tm = std::numeric_limits<double>::quiet_NaN();
    }
    eval.valid = true;
    return eval;
}

RecoResult PDCMomentumReconstructor::Reconstruct(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult best;
    best.method_used = SolveMethod::kAutoChain;
    best.status = SolverStatus::kNotAvailable;
    best.message = "no solver enabled";

    auto prefer_result = [&best](const RecoResult& candidate) {
        const auto rank = [](SolverStatus status) {
            switch (status) {
                case SolverStatus::kSuccess: return 3;
                case SolverStatus::kNotConverged: return 2;
                case SolverStatus::kInvalidInput: return 1;
                case SolverStatus::kNotAvailable: return 0;
            }
            return -1;
        };
        if (rank(candidate.status) > rank(best.status)) {
            best = candidate;
        }
    };

    if (config.enable_nn) {
        const RecoResult nn = ReconstructNN(track, target, config);
        if (nn.status == SolverStatus::kSuccess) {
            return nn;
        }
        prefer_result(nn);
    }

    if (config.enable_rk) {
        const RecoResult rk = ReconstructRK(track, target, config);
        if (rk.status == SolverStatus::kSuccess) {
            return rk;
        }
        prefer_result(rk);
    }

    if (config.enable_multi_dim) {
        const RecoResult multidim = ReconstructMultiDim(track, target, config);
        if (multidim.status == SolverStatus::kSuccess) {
            return multidim;
        }
        prefer_result(multidim);
    }

    if (config.enable_matrix) {
        const RecoResult matrix = ReconstructMatrix(track, target, config);
        if (matrix.status == SolverStatus::kSuccess) {
            return matrix;
        }
        prefer_result(matrix);
    }

    return best;
}

RecoResult PDCMomentumReconstructor::ReconstructNN(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kNeuralNetwork;

    std::string model_path = config.nn_model_json_path;
    if (model_path.empty()) {
        const char* env_model = std::getenv("PDC_NN_MODEL_JSON");
        if (env_model) {
            model_path = env_model;
        }
    }

    if (model_path.empty()) {
        result.status = SolverStatus::kNotAvailable;
        result.message = "NN model json path is empty (set RecoConfig::nn_model_json_path or PDC_NN_MODEL_JSON)";
        return result;
    }

    if (!fNNReconstructor || fNNModelPath != model_path || !fNNReconstructor->IsLoaded()) {
        auto nn = std::make_unique<PDCNNMomentumReconstructor>();
        std::string reason;
        if (!nn->LoadModel(model_path, &reason)) {
            result.status = SolverStatus::kNotAvailable;
            result.message = reason;
            return result;
        }
        fNNReconstructor = std::move(nn);
        fNNModelPath = model_path;
    }

    return fNNReconstructor->Reconstruct(track, target, config);
}

RecoResult PDCMomentumReconstructor::ReconstructRK(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kRungeKutta;

    std::string reason;
    if (!ValidateInputs(track, target, config, &reason)) {
        result.status = SolverStatus::kInvalidInput;
        result.message = reason;
        return result;
    }

    ParameterState state = BuildInitialState(track, target, config);
    EvalResult current = EvaluateState(state, track, target, config);
    if (!current.valid) {
        result.status = SolverStatus::kNotConverged;
        result.message = "initial RK evaluation failed";
        return result;
    }

    double lambda = Clamp(config.lm_lambda_init, config.lm_lambda_min, config.lm_lambda_max);
    int accepted_iterations = 0;

    for (int iter = 0; iter < config.max_iterations; ++iter) {
        TMatrixD jacobian(static_cast<int>(kResidualCount), static_cast<int>(kParameterCount));
        const std::array<double, kParameterCount> step = {
            0.2,
            0.2,
            1.0e-3,
            1.0e-3,
            std::max(1.0e-6, std::abs(state.q) * 2.0e-3)
        };

        for (std::size_t j = 0; j < kParameterCount; ++j) {
            ParameterState perturbed = state;
            if (j == 0) perturbed.dx += step[j];
            if (j == 1) perturbed.dy += step[j];
            if (j == 2) perturbed.u += step[j];
            if (j == 3) perturbed.v += step[j];
            if (j == 4) perturbed.q += ((state.q >= 0.0) ? step[j] : -step[j]);
            perturbed = ClampState(perturbed, target, config);

            const EvalResult eval_perturbed = EvaluateState(perturbed, track, target, config);
            if (!eval_perturbed.valid) {
                for (std::size_t i = 0; i < kResidualCount; ++i) {
                    jacobian(static_cast<int>(i), static_cast<int>(j)) = 0.0;
                }
                continue;
            }
            for (std::size_t i = 0; i < kResidualCount; ++i) {
                jacobian(static_cast<int>(i), static_cast<int>(j)) =
                    (eval_perturbed.residuals[i] - current.residuals[i]) / step[j];
            }
        }

        TMatrixD normal(static_cast<int>(kParameterCount), static_cast<int>(kParameterCount));
        TVectorD gradient(static_cast<int>(kParameterCount));
        normal.Zero();
        gradient.Zero();

        for (int i = 0; i < static_cast<int>(kResidualCount); ++i) {
            for (int j = 0; j < static_cast<int>(kParameterCount); ++j) {
                gradient(j) += jacobian(i, j) * current.residuals[static_cast<std::size_t>(i)];
                for (int k = 0; k < static_cast<int>(kParameterCount); ++k) {
                    normal(j, k) += jacobian(i, j) * jacobian(i, k);
                }
            }
        }
        for (int j = 0; j < static_cast<int>(kParameterCount); ++j) {
            normal(j, j) += lambda * std::max(1.0, normal(j, j));
        }

        TVectorD rhs(static_cast<int>(kParameterCount));
        for (int j = 0; j < static_cast<int>(kParameterCount); ++j) {
            rhs(j) = -gradient(j);
        }

        TDecompSVD solver(normal);
        Bool_t ok = kFALSE;
        const TVectorD delta = solver.Solve(rhs, ok);
        if (!ok) {
            lambda = std::min(config.lm_lambda_max, lambda * 10.0);
            continue;
        }

        ParameterState candidate = state;
        candidate.dx += delta(0);
        candidate.dy += delta(1);
        candidate.u += delta(2);
        candidate.v += delta(3);
        candidate.q += delta(4);
        candidate = ClampState(candidate, target, config);

        const EvalResult candidate_eval = EvaluateState(candidate, track, target, config);
        if (candidate_eval.valid && candidate_eval.chi2 < current.chi2) {
            const double improvement = current.chi2 - candidate_eval.chi2;
            state = candidate;
            current = candidate_eval;
            lambda = std::max(config.lm_lambda_min, lambda / 3.0);
            ++accepted_iterations;
            const double delta_norm =
                std::sqrt(delta(0) * delta(0) + delta(1) * delta(1) + delta(2) * delta(2) + delta(3) * delta(3) + delta(4) * delta(4));
            if (improvement < 1.0e-8 || delta_norm < 1.0e-5) {
                break;
            }
        } else {
            lambda = std::min(config.lm_lambda_max, lambda * 4.0);
            if (lambda >= config.lm_lambda_max) {
                break;
            }
        }
    }

    result.iterations = accepted_iterations;
    result.chi2 = current.chi2;
    result.min_distance_mm = current.min_distance_mm;
    result.path_length_mm = current.path_length_mm;
    result.brho_tm = current.brho_tm;
    result.p4_at_target = current.p4_at_target;

    if (!current.valid || !IsFinite(result.p4_at_target)) {
        result.status = SolverStatus::kNotConverged;
        result.message = "RK evaluation became invalid";
        return result;
    }

    if (current.min_distance_mm <= config.tolerance_mm) {
        result.status = SolverStatus::kSuccess;
        result.message = "RK fit converged";
    } else {
        result.status = SolverStatus::kNotConverged;
        result.message = "RK fit reached iteration limit";
    }
    return result;
}

RecoResult PDCMomentumReconstructor::ReconstructMultiDim(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kMultiDimFit;

    std::string reason;
    if (!ValidateInputs(track, target, config, &reason)) {
        result.status = SolverStatus::kInvalidInput;
        result.message = reason;
        return result;
    }

    result.status = SolverStatus::kNotAvailable;
    result.message = "MultiDimFit interface is reserved but not implemented in this revision";
    return result;
}

std::optional<PDCMomentumReconstructor::MatrixModel> PDCMomentumReconstructor::LoadMatrixModelFromEnv(std::string* reason) const {
    const char* mat0_path = std::getenv("SAMURAI_MATRIX0TH_FILE");
    const char* mat1_path = std::getenv("SAMURAI_MATRIX1ST_FILE");
    if (!mat0_path || !mat1_path) {
        if (reason) {
            *reason = FormatPathStatus("SAMURAI_MATRIX0TH_FILE", mat0_path) + ", " +
                      FormatPathStatus("SAMURAI_MATRIX1ST_FILE", mat1_path);
        }
        return std::nullopt;
    }

    std::ifstream mat0_stream(mat0_path);
    std::ifstream mat1_stream(mat1_path);
    if (!mat0_stream.is_open() || !mat1_stream.is_open()) {
        if (reason) {
            std::ostringstream oss;
            oss << "failed to open matrix files: " << mat0_path << ", " << mat1_path;
            *reason = oss.str();
        }
        return std::nullopt;
    }

    double mat0_a = 0.0;
    double mat0_b = 0.0;
    if (!(mat0_stream >> mat0_a >> mat0_b)) {
        if (reason) {
            *reason = "invalid format in SAMURAI_MATRIX0TH_FILE";
        }
        return std::nullopt;
    }
    (void)mat0_a;
    (void)mat0_b;

    double mat[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (!(mat1_stream >> mat[r][c])) {
                if (reason) {
                    *reason = "invalid format in SAMURAI_MATRIX1ST_FILE";
                }
                return std::nullopt;
            }
        }
    }

    const double a11 = mat[0][1];
    const double a12 = mat[0][2];
    const double a21 = mat[1][1];
    const double a22 = mat[1][2];
    const double det = a11 * a22 - a12 * a21;
    if (!std::isfinite(det) || std::abs(det) < 1.0e-12) {
        if (reason) {
            *reason = "matrix inversion failed for mat2";
        }
        return std::nullopt;
    }

    MatrixModel model;
    model.mat1[0] = mat[0][0];
    model.mat1[1] = mat[1][0];
    model.inv_mat2[0] =  a22 / det;
    model.inv_mat2[1] = -a12 / det;
    model.inv_mat2[2] = -a21 / det;
    model.inv_mat2[3] =  a11 / det;
    return model;
}

RecoResult PDCMomentumReconstructor::ReconstructMatrix(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kMatrixFallback;

    std::string reason;
    if (!ValidateInputs(track, target, config, &reason)) {
        result.status = SolverStatus::kInvalidInput;
        result.message = reason;
        return result;
    }

    const std::optional<MatrixModel> model_opt = LoadMatrixModelFromEnv(&reason);
    if (!model_opt.has_value()) {
        result.status = SolverStatus::kNotAvailable;
        result.message = reason;
        return result;
    }
    const MatrixModel& model = *model_opt;

    const double dist1 = SafeNorm(track.pdc1 - target.target_position);
    const double dist2 = SafeNorm(track.pdc2 - target.target_position);
    const TVector3 fdc1 = (dist1 <= dist2) ? track.pdc1 : track.pdc2;  // near side
    const TVector3 fdc2 = (dist1 <= dist2) ? track.pdc2 : track.pdc1;  // far side

    const double dz = fdc2.Z() - fdc1.Z();
    if (!std::isfinite(dz) || std::abs(dz) < 1.0e-6) {
        result.status = SolverStatus::kInvalidInput;
        result.message = "matrix fallback requires non-zero dz between PDC points";
        return result;
    }

    const double x1 = fdc1.X();
    const double x2 = fdc2.X();
    const double a2 = (fdc2.X() - fdc1.X()) / dz;

    const double rhs0 = x2 - x1 * model.mat1[0];
    const double rhs1 = a2 - x1 * model.mat1[1];
    const double delta0 = model.inv_mat2[0] * rhs0 + model.inv_mat2[1] * rhs1;
    const double delta1 = model.inv_mat2[2] * rhs0 + model.inv_mat2[3] * rhs1;
    (void)delta0;

    const double brho_tm = config.center_brho_tm * (1.0 + delta1);
    const double p_mag_mevc = std::abs(brho_tm) * kBrhoGeVOverCPerTm * 1000.0 * std::abs(target.charge_e);
    if (!std::isfinite(p_mag_mevc) || p_mag_mevc <= 0.0) {
        result.status = SolverStatus::kNotConverged;
        result.message = "matrix fallback produced invalid momentum";
        return result;
    }

    TVector3 direction = fdc2 - fdc1;
    if (direction.Mag2() < 1.0e-12) {
        direction = fdc1 - target.target_position;
    }
    if (direction.Mag2() < 1.0e-12) {
        direction.SetXYZ(0.0, 0.0, 1.0);
    }
    direction = direction.Unit();

    const TVector3 momentum = direction * p_mag_mevc;
    const double mass = std::isfinite(target.mass_mev) ? target.mass_mev : kDefaultMassMeV;
    const double energy = std::sqrt(momentum.Mag2() + mass * mass);

    result.p4_at_target.SetXYZT(momentum.X(), momentum.Y(), momentum.Z(), energy);
    result.brho_tm = brho_tm;
    result.path_length_mm = SafeNorm(fdc2 - target.target_position);
    result.min_distance_mm = DistancePointToLine(target.target_position, fdc1, direction);
    result.chi2 = std::numeric_limits<double>::quiet_NaN();
    result.iterations = 1;
    result.status = SolverStatus::kSuccess;
    result.message = "matrix fallback solved from SAMURAI matrix files";
    return result;
}

}  // namespace analysis::pdc::anaroot_like
