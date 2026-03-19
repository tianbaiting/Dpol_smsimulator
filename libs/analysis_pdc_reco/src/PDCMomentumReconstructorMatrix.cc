#include "PDCMomentumReconstructor.hh"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

namespace analysis::pdc::anaroot_like {
namespace {

constexpr double kBrhoGeVOverCPerTm = 0.299792458;
constexpr double kDefaultMassMeV = 938.2720813;

double SafeNorm(const TVector3& value) {
    return std::sqrt(std::max(0.0, value.Mag2()));
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
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (!(mat1_stream >> mat[row][col])) {
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
    model.inv_mat2[0] = a22 / det;
    model.inv_mat2[1] = -a12 / det;
    model.inv_mat2[2] = -a21 / det;
    model.inv_mat2[3] = a11 / det;
    return model;
}

RecoResult PDCMomentumReconstructor::ReconstructMatrix(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kMatrixFallback;
    result.chi2 = std::numeric_limits<double>::quiet_NaN();
    result.chi2_raw = std::numeric_limits<double>::quiet_NaN();
    result.chi2_reduced = std::numeric_limits<double>::quiet_NaN();
    result.ndf = 0;

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
    const TVector3 fdc1 = (dist1 <= dist2) ? track.pdc1 : track.pdc2;
    const TVector3 fdc2 = (dist1 <= dist2) ? track.pdc2 : track.pdc1;

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
    result.iterations = 1;
    result.status = SolverStatus::kSuccess;
    result.message = "matrix fallback solved from SAMURAI matrix files";
    return result;
}

}  // namespace analysis::pdc::anaroot_like
