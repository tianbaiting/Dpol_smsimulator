#include "PDCMomentumReconstructor.hh"

#include <algorithm>
#include <cmath>
#include <limits>

namespace analysis::pdc::anaroot_like {

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
}  // namespace analysis::pdc::anaroot_like
