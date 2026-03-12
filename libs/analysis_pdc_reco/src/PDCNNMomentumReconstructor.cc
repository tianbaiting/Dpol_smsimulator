#include "PDCNNMomentumReconstructor.hh"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace analysis::pdc::anaroot_like {
namespace {

using Json = nlohmann::json;

constexpr double kBrhoGeVOverCPerTm = 0.299792458;
constexpr double kDefaultMassMeV = 938.2720813;

double Clamp(double value, double lower, double upper) {
    if (!std::isfinite(value)) {
        return lower;
    }
    return std::max(lower, std::min(value, upper));
}

template <std::size_t N>
bool ParseFixedArray(const Json& root, const char* key, std::array<double, N>* out, std::string* reason) {
    if (!root.contains(key) || !root.at(key).is_array() || root.at(key).size() != N) {
        if (reason) {
            std::ostringstream oss;
            oss << "invalid or missing array: " << key;
            *reason = oss.str();
        }
        return false;
    }
    for (std::size_t i = 0; i < N; ++i) {
        const Json& value = root.at(key).at(i);
        if (!value.is_number()) {
            if (reason) {
                std::ostringstream oss;
                oss << "non-numeric value in " << key << "[" << i << "]";
                *reason = oss.str();
            }
            return false;
        }
        (*out)[i] = value.get<double>();
    }
    return true;
}

}  // namespace

bool PDCNNMomentumReconstructor::LoadModel(const std::string& json_path, std::string* reason) {
    fLoaded = false;
    fUseTargetNormalization = false;
    fLoadedPath.clear();
    fLayers.clear();
    fYMean = {0.0, 0.0, 0.0};
    fYStd = {1.0, 1.0, 1.0};

    std::ifstream fin(json_path);
    if (!fin.is_open()) {
        if (reason) {
            *reason = "failed to open model json: " + json_path;
        }
        return false;
    }

    Json root;
    try {
        fin >> root;
    } catch (const std::exception& ex) {
        if (reason) {
            *reason = std::string("failed to parse model json: ") + ex.what();
        }
        return false;
    }

    if (!ParseFixedArray(root, "x_mean", &fXMean, reason)) {
        return false;
    }
    if (!ParseFixedArray(root, "x_std", &fXStd, reason)) {
        return false;
    }
    const bool has_target_stats = root.contains("y_mean") || root.contains("y_std");
    if (has_target_stats) {
        const std::string target_normalization = root.value("target_normalization", std::string("zscore"));
        if (target_normalization == "zscore") {
            if (!ParseFixedArray(root, "y_mean", &fYMean, reason)) {
                return false;
            }
            if (!ParseFixedArray(root, "y_std", &fYStd, reason)) {
                return false;
            }
            for (std::size_t i = 0; i < fYStd.size(); ++i) {
                if (!std::isfinite(fYStd[i]) || std::abs(fYStd[i]) < 1.0e-8) {
                    fYStd[i] = 1.0;
                }
            }
            fUseTargetNormalization = true;
        } else if (target_normalization != "none") {
            if (reason) {
                *reason = "unsupported target_normalization: " + target_normalization;
            }
            return false;
        }
    }

    for (std::size_t i = 0; i < fXStd.size(); ++i) {
        if (!std::isfinite(fXStd[i]) || std::abs(fXStd[i]) < 1.0e-8) {
            fXStd[i] = 1.0;
        }
    }

    if (!root.contains("layers") || !root.at("layers").is_array() || root.at("layers").empty()) {
        if (reason) {
            *reason = "missing non-empty layers array in model json";
        }
        return false;
    }

    const Json& layers_json = root.at("layers");
    fLayers.reserve(layers_json.size());
    for (std::size_t idx = 0; idx < layers_json.size(); ++idx) {
        const Json& layer = layers_json.at(idx);
        if (!layer.is_object()) {
            if (reason) {
                std::ostringstream oss;
                oss << "layer[" << idx << "] must be an object";
                *reason = oss.str();
            }
            return false;
        }
        if (!layer.contains("in_dim") || !layer.contains("out_dim") || !layer.contains("weights") || !layer.contains("bias")) {
            if (reason) {
                std::ostringstream oss;
                oss << "layer[" << idx << "] missing keys";
                *reason = oss.str();
            }
            return false;
        }

        DenseLayer parsed;
        parsed.in_dim = layer.at("in_dim").get<int>();
        parsed.out_dim = layer.at("out_dim").get<int>();
        if (parsed.in_dim <= 0 || parsed.out_dim <= 0) {
            if (reason) {
                std::ostringstream oss;
                oss << "layer[" << idx << "] has non-positive dimensions";
                *reason = oss.str();
            }
            return false;
        }

        if (!layer.at("weights").is_array() || !layer.at("bias").is_array()) {
            if (reason) {
                std::ostringstream oss;
                oss << "layer[" << idx << "] weights/bias must be arrays";
                *reason = oss.str();
            }
            return false;
        }

        parsed.weights.reserve(layer.at("weights").size());
        for (const auto& v : layer.at("weights")) {
            if (!v.is_number()) {
                if (reason) {
                    std::ostringstream oss;
                    oss << "layer[" << idx << "] has non-numeric weight";
                    *reason = oss.str();
                }
                return false;
            }
            parsed.weights.push_back(v.get<double>());
        }

        parsed.bias.reserve(layer.at("bias").size());
        for (const auto& v : layer.at("bias")) {
            if (!v.is_number()) {
                if (reason) {
                    std::ostringstream oss;
                    oss << "layer[" << idx << "] has non-numeric bias";
                    *reason = oss.str();
                }
                return false;
            }
            parsed.bias.push_back(v.get<double>());
        }

        if (static_cast<int>(parsed.bias.size()) != parsed.out_dim ||
            static_cast<int>(parsed.weights.size()) != parsed.out_dim * parsed.in_dim) {
            if (reason) {
                std::ostringstream oss;
                oss << "layer[" << idx << "] shape mismatch: in_dim=" << parsed.in_dim
                    << ", out_dim=" << parsed.out_dim
                    << ", weights=" << parsed.weights.size()
                    << ", bias=" << parsed.bias.size();
                *reason = oss.str();
            }
            return false;
        }

        if (!fLayers.empty() && fLayers.back().out_dim != parsed.in_dim) {
            if (reason) {
                std::ostringstream oss;
                oss << "layer[" << idx << "] in_dim=" << parsed.in_dim
                    << " does not match previous out_dim=" << fLayers.back().out_dim;
                *reason = oss.str();
            }
            return false;
        }

        fLayers.push_back(std::move(parsed));
    }

    if (fLayers.front().in_dim != 6 || fLayers.back().out_dim != 3) {
        if (reason) {
            std::ostringstream oss;
            oss << "model dimensions mismatch: expected 6->...->3, got "
                << fLayers.front().in_dim << "->...->" << fLayers.back().out_dim;
            *reason = oss.str();
        }
        fLayers.clear();
        return false;
    }

    fLoaded = true;
    fLoadedPath = json_path;
    return true;
}

bool PDCNNMomentumReconstructor::Forward(
    const std::array<double, 6>& features,
    std::array<double, 3>* momentum,
    std::string* reason
) const {
    if (!fLoaded || fLayers.empty()) {
        if (reason) {
            *reason = "model is not loaded";
        }
        return false;
    }
    if (!momentum) {
        if (reason) {
            *reason = "output momentum pointer is null";
        }
        return false;
    }

    std::vector<double> activations(6, 0.0);
    for (std::size_t i = 0; i < features.size(); ++i) {
        const double x = (features[i] - fXMean[i]) / fXStd[i];
        if (!std::isfinite(x)) {
            if (reason) {
                std::ostringstream oss;
                oss << "non-finite normalized feature at index " << i;
                *reason = oss.str();
            }
            return false;
        }
        activations[i] = x;
    }

    for (std::size_t layer_idx = 0; layer_idx < fLayers.size(); ++layer_idx) {
        const DenseLayer& layer = fLayers[layer_idx];
        if (static_cast<int>(activations.size()) != layer.in_dim) {
            if (reason) {
                std::ostringstream oss;
                oss << "activation dim mismatch at layer " << layer_idx;
                *reason = oss.str();
            }
            return false;
        }

        std::vector<double> next(static_cast<std::size_t>(layer.out_dim), 0.0);
        for (int out = 0; out < layer.out_dim; ++out) {
            double sum = layer.bias[static_cast<std::size_t>(out)];
            const std::size_t row_offset = static_cast<std::size_t>(out * layer.in_dim);
            for (int in = 0; in < layer.in_dim; ++in) {
                sum += layer.weights[row_offset + static_cast<std::size_t>(in)] *
                       activations[static_cast<std::size_t>(in)];
            }
            if (!std::isfinite(sum)) {
                if (reason) {
                    std::ostringstream oss;
                    oss << "non-finite activation at layer " << layer_idx << ", neuron " << out;
                    *reason = oss.str();
                }
                return false;
            }
            if (layer_idx + 1 < fLayers.size() && sum < 0.0) {
                // [EN] Mirror training-time ReLU after each hidden linear layer to keep inference numerically consistent. / [CN] 在每个隐藏线性层后应用与训练一致的ReLU，保证推理数值一致性。
                sum = 0.0;
            }
            next[static_cast<std::size_t>(out)] = sum;
        }
        activations.swap(next);
    }

    if (activations.size() != 3) {
        if (reason) {
            *reason = "output activation size is not 3";
        }
        return false;
    }

    for (std::size_t i = 0; i < 3; ++i) {
        double value = activations[i];
        if (fUseTargetNormalization) {
            value = value * fYStd[i] + fYMean[i];
        }
        if (!std::isfinite(value)) {
            if (reason) {
                std::ostringstream oss;
                oss << "non-finite output momentum at index " << i;
                *reason = oss.str();
            }
            return false;
        }
        (*momentum)[i] = value;
    }
    return true;
}

RecoResult PDCNNMomentumReconstructor::Reconstruct(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kNeuralNetwork;
    result.chi2 = std::numeric_limits<double>::quiet_NaN();
    result.chi2_raw = std::numeric_limits<double>::quiet_NaN();
    result.chi2_reduced = std::numeric_limits<double>::quiet_NaN();
    result.ndf = 0;
    result.min_distance_mm = std::numeric_limits<double>::quiet_NaN();
    result.path_length_mm = std::numeric_limits<double>::quiet_NaN();
    result.iterations = 1;

    if (!fLoaded || fLayers.empty()) {
        result.status = SolverStatus::kNotAvailable;
        result.message = "NN model is not loaded";
        return result;
    }

    if (!track.IsValid()) {
        result.status = SolverStatus::kInvalidInput;
        result.message = "track points are invalid";
        return result;
    }

    const std::array<double, 6> features{
        track.pdc1.X(), track.pdc1.Y(), track.pdc1.Z(),
        track.pdc2.X(), track.pdc2.Y(), track.pdc2.Z()
    };
    for (std::size_t i = 0; i < features.size(); ++i) {
        if (!std::isfinite(features[i])) {
            result.status = SolverStatus::kInvalidInput;
            std::ostringstream oss;
            oss << "non-finite feature at index " << i;
            result.message = oss.str();
            return result;
        }
    }

    std::array<double, 3> pred{0.0, 0.0, 0.0};
    std::string reason;
    if (!Forward(features, &pred, &reason)) {
        result.status = SolverStatus::kNotConverged;
        result.message = reason;
        return result;
    }

    TVector3 momentum(pred[0], pred[1], pred[2]);
    double p_mag = momentum.Mag();
    if (!std::isfinite(p_mag) || p_mag <= 0.0) {
        result.status = SolverStatus::kNotConverged;
        result.message = "NN predicted invalid momentum magnitude";
        return result;
    }

    if (std::isfinite(config.p_min_mevc) && std::isfinite(config.p_max_mevc) &&
        config.p_min_mevc > 0.0 && config.p_max_mevc > config.p_min_mevc &&
        (p_mag < config.p_min_mevc || p_mag > config.p_max_mevc)) {
        const double p_clamped = Clamp(p_mag, config.p_min_mevc, config.p_max_mevc);
        momentum *= (p_clamped / p_mag);
        p_mag = p_clamped;
    }

    const double mass = (std::isfinite(target.mass_mev) && target.mass_mev > 0.0) ? target.mass_mev : kDefaultMassMeV;
    const double energy = std::sqrt(momentum.Mag2() + mass * mass);
    if (!std::isfinite(energy)) {
        result.status = SolverStatus::kNotConverged;
        result.message = "failed to build on-shell energy from NN momentum";
        return result;
    }

    result.p4_at_target.SetPxPyPzE(momentum.X(), momentum.Y(), momentum.Z(), energy);
    if (std::isfinite(target.charge_e) && std::abs(target.charge_e) > 1.0e-12) {
        result.brho_tm = (p_mag / 1000.0) / (kBrhoGeVOverCPerTm * std::abs(target.charge_e));
    } else {
        result.brho_tm = std::numeric_limits<double>::quiet_NaN();
    }
    result.status = SolverStatus::kSuccess;
    result.message = "NN inference solved target momentum";
    return result;
}

}  // namespace analysis::pdc::anaroot_like
