// [EN] Usage: root -l -q 'scripts/analysis/check_pdc_reco_backend.C+("sim.root","run.mac","field.table",30.0,3000,10.0,5.0,"summary.csv")' / [CN] 用法: root -l -q 'scripts/analysis/check_pdc_reco_backend.C+("sim.root","run.mac","field.table",30.0,3000,10.0,5.0,"summary.csv")'

#include "TClonesArray.h"
#include "TFile.h"
#include "TSystem.h"
#include "TTree.h"

#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "PDCSimAna.hh"
#include "TBeamSimData.hh"
#include "../../libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using analysis::pdc::anaroot_like::PDCInputTrack;
using analysis::pdc::anaroot_like::PDCMomentumReconstructor;
using analysis::pdc::anaroot_like::RecoConfig;
using analysis::pdc::anaroot_like::RecoResult;
using analysis::pdc::anaroot_like::RkFitMode;
using analysis::pdc::anaroot_like::SolverStatus;
using analysis::pdc::anaroot_like::TargetConstraint;
using analysis::pdc::anaroot_like::IntervalEstimate;

struct TruthBeamInfo {
    bool hasProton = false;
    TVector3 protonMomentum = TVector3(0.0, 0.0, 0.0);
};

struct RunningStat {
    long long count = 0;
    double sum = 0.0;
    double sumSq = 0.0;

    void Fill(double value) {
        ++count;
        sum += value;
        sumSq += value * value;
    }

    double Mean() const {
        if (count <= 0) return 0.0;
        return sum / static_cast<double>(count);
    }

    double RMS() const {
        if (count <= 0) return 0.0;
        const double mean = Mean();
        const double variance = sumSq / static_cast<double>(count) - mean * mean;
        return (variance > 0.0) ? std::sqrt(variance) : 0.0;
    }
};

struct MethodSummary {
    long long attempts = 0;
    long long success = 0;
    long long usable = 0;
    long long uncertaintyValid = 0;
    long long posteriorValid = 0;
    long long coverage68Px = 0;
    long long coverage95Px = 0;
    long long credible68Px = 0;
    long long credible95Px = 0;
    RunningStat dpxSuccess;
    RunningStat dpxUsable;
    RunningStat pxSigma;
    RunningStat pxPosteriorSigma;
    RunningStat chi2Reduced;
    std::map<SolverStatus, long long> statusCount;
};

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string Trim(const std::string& input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::string ExpandPathToken(std::string token) {
    const char* smsDir = std::getenv("SMSIMDIR");
    const std::string sms = smsDir ? std::string(smsDir) : std::string();

    const std::string key1 = "{SMSIMDIR}";
    const std::string key2 = "$SMSIMDIR";

    std::size_t pos = token.find(key1);
    while (pos != std::string::npos) {
        token.replace(pos, key1.size(), sms);
        pos = token.find(key1, pos + sms.size());
    }

    pos = token.find(key2);
    while (pos != std::string::npos) {
        token.replace(pos, key2.size(), sms);
        pos = token.find(key2, pos + sms.size());
    }
    return token;
}

bool ResolveMacroPath(const std::string& rawPath, const std::filesystem::path& baseDir, std::filesystem::path* outPath) {
    if (!outPath) return false;
    std::filesystem::path path = ExpandPathToken(rawPath);
    if (!path.is_absolute()) {
        path = baseDir / path;
    }
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        *outPath = canonical;
        return true;
    }
    *outPath = path.lexically_normal();
    return std::filesystem::exists(*outPath);
}

bool LoadGeometryRecursive(GeometryManager& geo, const std::filesystem::path& macroPath, std::set<std::string>& visited) {
    const std::string key = macroPath.lexically_normal().string();
    if (visited.count(key)) return true;

    std::ifstream fin(macroPath);
    if (!fin.is_open()) {
        std::cerr << "[check_pdc_reco_backend] Cannot open geometry macro: " << macroPath << std::endl;
        return false;
    }
    visited.insert(key);

    std::string line;
    while (std::getline(fin, line)) {
        const std::string clean = Trim(line);
        if (clean.empty() || clean[0] == '#') continue;
        if (clean.rfind("/control/execute", 0) == 0) {
            std::stringstream ss(clean);
            std::string cmd;
            std::string includePathRaw;
            ss >> cmd >> includePathRaw;
            if (includePathRaw.empty()) continue;

            std::filesystem::path includePath;
            if (!ResolveMacroPath(includePathRaw, macroPath.parent_path(), &includePath)) {
                std::cerr << "[check_pdc_reco_backend] Warning: unresolved include macro: " << includePathRaw << std::endl;
                continue;
            }
            if (!LoadGeometryRecursive(geo, includePath, visited)) return false;
        }
    }

    return geo.LoadGeometry(macroPath.string());
}

TruthBeamInfo ExtractTruthBeam(const std::vector<TBeamSimData>* beam, double targetAngleRad) {
    TruthBeamInfo info;
    if (!beam) return info;

    for (const auto& p : *beam) {
        const std::string pname = ToLower(p.fParticleName.Data());
        if (p.fZ == 1 && p.fA == 1 || pname == "proton" || p.fPDGCode == 2212) {
            TVector3 momentum = p.fMomentum.Vect();
            momentum.RotateY(-targetAngleRad);
            info.hasProton = true;
            info.protonMomentum = momentum;
            break;
        }
    }
    return info;
}

std::string StatusToString(SolverStatus status) {
    switch (status) {
        case SolverStatus::kSuccess: return "success";
        case SolverStatus::kNotConverged: return "not_converged";
        case SolverStatus::kNotAvailable: return "not_available";
        case SolverStatus::kInvalidInput: return "invalid_input";
    }
    return "unknown";
}

bool IsUsableResult(const RecoResult& result) {
    if (!(result.status == SolverStatus::kSuccess || result.status == SolverStatus::kNotConverged)) {
        return false;
    }
    return std::isfinite(result.p4_at_target.Px()) &&
           std::isfinite(result.p4_at_target.Py()) &&
           std::isfinite(result.p4_at_target.Pz()) &&
           std::isfinite(result.p4_at_target.E()) &&
           result.p4_at_target.P() > 0.0;
}

bool CoversInterval(const IntervalEstimate& interval, double truth, bool use95) {
    if (!interval.valid) {
        return false;
    }
    if (use95) {
        return interval.lower95 <= truth && truth <= interval.upper95;
    }
    return interval.lower68 <= truth && truth <= interval.upper68;
}

void UpdateSummary(MethodSummary& summary, const RecoResult& result, double truePx) {
    ++summary.attempts;
    ++summary.statusCount[result.status];
    if (result.status == SolverStatus::kSuccess) {
        ++summary.success;
        summary.dpxSuccess.Fill(result.p4_at_target.Px() - truePx);
    }
    if (IsUsableResult(result)) {
        ++summary.usable;
        summary.dpxUsable.Fill(result.p4_at_target.Px() - truePx);
    }
    if (std::isfinite(result.chi2_reduced)) {
        summary.chi2Reduced.Fill(result.chi2_reduced);
    }
    if (result.uncertainty_valid && result.px_interval.valid) {
        ++summary.uncertaintyValid;
        summary.pxSigma.Fill(result.px_interval.sigma);
        if (CoversInterval(result.px_interval, truePx, false)) ++summary.coverage68Px;
        if (CoversInterval(result.px_interval, truePx, true)) ++summary.coverage95Px;
    }
    if (result.posterior_valid && result.px_credible.valid) {
        ++summary.posteriorValid;
        summary.pxPosteriorSigma.Fill(result.px_credible.sigma);
        if (CoversInterval(result.px_credible, truePx, false)) ++summary.credible68Px;
        if (CoversInterval(result.px_credible, truePx, true)) ++summary.credible95Px;
    }
}

void PrintSummaryLine(const std::string& title, const MethodSummary& summary) {
    const double successRate = (summary.attempts > 0)
        ? static_cast<double>(summary.success) / static_cast<double>(summary.attempts)
        : 0.0;
    const double usableRate = (summary.attempts > 0)
        ? static_cast<double>(summary.usable) / static_cast<double>(summary.attempts)
        : 0.0;

    std::cout << "[check_pdc_reco_backend] " << title
              << " attempts=" << summary.attempts
              << " success=" << summary.success
              << " (" << std::fixed << std::setprecision(4) << successRate << ")"
              << " usable=" << summary.usable
              << " (" << usableRate << ")" << std::endl;
    std::cout << "  dPx(success): count=" << summary.dpxSuccess.count
              << " mean=" << summary.dpxSuccess.Mean()
              << " rms=" << summary.dpxSuccess.RMS() << std::endl;
    std::cout << "  dPx(usable ): count=" << summary.dpxUsable.count
              << " mean=" << summary.dpxUsable.Mean()
              << " rms=" << summary.dpxUsable.RMS() << std::endl;
    if (summary.uncertaintyValid > 0) {
        std::cout << "  px interval sigma mean=" << summary.pxSigma.Mean()
                  << " coverage68=" << (static_cast<double>(summary.coverage68Px) / summary.uncertaintyValid)
                  << " coverage95=" << (static_cast<double>(summary.coverage95Px) / summary.uncertaintyValid)
                  << std::endl;
    }
    if (summary.posteriorValid > 0) {
        std::cout << "  px credible sigma mean=" << summary.pxPosteriorSigma.Mean()
                  << " credible68=" << (static_cast<double>(summary.credible68Px) / summary.posteriorValid)
                  << " credible95=" << (static_cast<double>(summary.credible95Px) / summary.posteriorValid)
                  << std::endl;
    }
    if (summary.chi2Reduced.count > 0) {
        std::cout << "  chi2_reduced mean=" << summary.chi2Reduced.Mean()
                  << " rms=" << summary.chi2Reduced.RMS() << std::endl;
    }
    for (const auto& [status, count] : summary.statusCount) {
        std::cout << "  status[" << StatusToString(status) << "]=" << count << std::endl;
    }
}

} // namespace

void check_pdc_reco_backend(const char* sim_root,
                            const char* geom_macro,
                            const char* field_table,
                            double field_rotation_deg = 30.0,
                            int max_events = 3000,
                            double rk_step_mm = 10.0,
                            double tolerance_mm = 5.0,
                            const char* out_csv = "") {
    gSystem->Load("libsmdata.so");
    gSystem->Load("libpdcanalysis.so");
    gSystem->Load("libanalysis_pdc_reco.so");

    GeometryManager geo;
    std::filesystem::path geomPath;
    std::set<std::string> visited;
    if (!ResolveMacroPath(geom_macro, std::filesystem::current_path(), &geomPath)) {
        std::cerr << "[check_pdc_reco_backend] Failed to resolve geometry macro: " << geom_macro << std::endl;
        return;
    }
    if (!LoadGeometryRecursive(geo, geomPath, visited)) {
        std::cerr << "[check_pdc_reco_backend] Failed to load geometry macro: " << geom_macro << std::endl;
        return;
    }
    const TVector3 targetPos = geo.GetTargetPosition();
    const double targetAngleRad = geo.GetTargetAngleRad();

    MagneticField magField;
    if (!magField.LoadFieldMap(field_table)) {
        std::cerr << "[check_pdc_reco_backend] Failed to load field table: " << field_table << std::endl;
        return;
    }
    magField.SetRotationAngle(field_rotation_deg);

    PDCSimAna pdcAna(geo);
    pdcAna.SetSmearing(0.5, 0.5);

    PDCMomentumReconstructor reconstructor(&magField);

    RecoConfig cfgAuto;
    cfgAuto.enable_rk = true;
    cfgAuto.enable_multi_dim = true;
    cfgAuto.enable_matrix = true;
    cfgAuto.initial_p_mevc = 627.0;
    cfgAuto.max_iterations = 60;
    cfgAuto.tolerance_mm = tolerance_mm;
    cfgAuto.rk_step_mm = rk_step_mm;

    RecoConfig cfgRK = cfgAuto;
    cfgRK.enable_multi_dim = false;
    cfgRK.enable_matrix = false;

    RecoConfig cfgRKFixedTarget = cfgRK;
    cfgRKFixedTarget.rk_fit_mode = RkFitMode::kFixedTargetPdcOnly;

    TFile inFile(sim_root, "READ");
    if (inFile.IsZombie()) {
        std::cerr << "[check_pdc_reco_backend] Failed to open input: " << sim_root << std::endl;
        return;
    }
    TTree* tree = dynamic_cast<TTree*>(inFile.Get("tree"));
    if (!tree) {
        std::cerr << "[check_pdc_reco_backend] Missing tree 'tree' in " << sim_root << std::endl;
        return;
    }

    TClonesArray* fragArray = nullptr;
    std::vector<TBeamSimData>* beam = nullptr;
    tree->SetBranchAddress("FragSimData", &fragArray);
    if (tree->GetBranch("beam")) tree->SetBranchAddress("beam", &beam);

    const Long64_t nEntries = tree->GetEntries();
    const Long64_t nToProcess = (max_events > 0 && nEntries > max_events) ? max_events : nEntries;

    MethodSummary autoSummary;
    MethodSummary rkSummary;
    MethodSummary rkFixedTargetSummary;

    long long truthProtonCount = 0;
    long long truthAndTrackCount = 0;

    std::cout << "[check_pdc_reco_backend] Processing events: " << nToProcess << std::endl;
    for (Long64_t i = 0; i < nToProcess; ++i) {
        tree->GetEntry(i);
        if (i > 0 && i % 1000 == 0) {
            std::cout << "[check_pdc_reco_backend] Event " << i << "/" << nToProcess << std::endl;
        }

        const TruthBeamInfo truth = ExtractTruthBeam(beam, targetAngleRad);
        if (!truth.hasProton) continue;
        ++truthProtonCount;

        const RecoEvent pdcEvent = pdcAna.ProcessEvent(fragArray);
        if (pdcEvent.tracks.empty()) continue;
        ++truthAndTrackCount;

        const RecoTrack& track = pdcEvent.tracks.front();
        PDCInputTrack inputTrack;
        inputTrack.pdc1 = track.start;
        inputTrack.pdc2 = track.end;

        TargetConstraint constraint;
        constraint.target_position = targetPos;
        constraint.mass_mev = 938.2720813;
        constraint.charge_e = 1.0;
        constraint.pdc_sigma_mm = 2.0;
        constraint.target_sigma_xy_mm = 5.0;

        const RecoResult autoResult = reconstructor.Reconstruct(inputTrack, constraint, cfgAuto);
        const RecoResult rkResult = reconstructor.ReconstructRK(inputTrack, constraint, cfgRK);
        const RecoResult rkFixedTargetResult = reconstructor.ReconstructRK(inputTrack, constraint, cfgRKFixedTarget);

        UpdateSummary(autoSummary, autoResult, truth.protonMomentum.X());
        UpdateSummary(rkSummary, rkResult, truth.protonMomentum.X());
        UpdateSummary(rkFixedTargetSummary, rkFixedTargetResult, truth.protonMomentum.X());
    }

    std::cout << "[check_pdc_reco_backend] Truth proton events: " << truthProtonCount << std::endl;
    std::cout << "[check_pdc_reco_backend] Truth+PDC-track events: " << truthAndTrackCount << std::endl;
    PrintSummaryLine("ANAROOT-like AutoChain", autoSummary);
    PrintSummaryLine("ANAROOT-like RK only", rkSummary);
    PrintSummaryLine("ANAROOT-like RK fixed target", rkFixedTargetSummary);

    if (out_csv && out_csv[0] != '\0') {
        std::ofstream csv(out_csv);
        if (!csv.is_open()) {
            std::cerr << "[check_pdc_reco_backend] Failed to open output CSV: " << out_csv << std::endl;
            return;
        }
        csv << "metric,count,mean,rms\n";
        csv << "truth_proton_events," << truthProtonCount << ",0,0\n";
        csv << "truth_and_pdc_track_events," << truthAndTrackCount << ",0,0\n";
        csv << "auto_attempts," << autoSummary.attempts << ",0,0\n";
        csv << "auto_success_rate," << autoSummary.attempts << ","
            << (autoSummary.attempts > 0 ? static_cast<double>(autoSummary.success) / autoSummary.attempts : 0.0)
            << ",0\n";
        csv << "auto_usable_rate," << autoSummary.attempts << ","
            << (autoSummary.attempts > 0 ? static_cast<double>(autoSummary.usable) / autoSummary.attempts : 0.0)
            << ",0\n";
        csv << "auto_dpx_success," << autoSummary.dpxSuccess.count << ","
            << autoSummary.dpxSuccess.Mean() << "," << autoSummary.dpxSuccess.RMS() << "\n";
        csv << "auto_dpx_usable," << autoSummary.dpxUsable.count << ","
            << autoSummary.dpxUsable.Mean() << "," << autoSummary.dpxUsable.RMS() << "\n";
        csv << "auto_chi2_reduced," << autoSummary.chi2Reduced.count << ","
            << autoSummary.chi2Reduced.Mean() << "," << autoSummary.chi2Reduced.RMS() << "\n";
        csv << "auto_px_sigma," << autoSummary.pxSigma.count << ","
            << autoSummary.pxSigma.Mean() << "," << autoSummary.pxSigma.RMS() << "\n";
        csv << "auto_px_coverage68," << autoSummary.uncertaintyValid << ","
            << (autoSummary.uncertaintyValid > 0 ? static_cast<double>(autoSummary.coverage68Px) / autoSummary.uncertaintyValid : 0.0)
            << ",0\n";
        csv << "auto_px_coverage95," << autoSummary.uncertaintyValid << ","
            << (autoSummary.uncertaintyValid > 0 ? static_cast<double>(autoSummary.coverage95Px) / autoSummary.uncertaintyValid : 0.0)
            << ",0\n";
        csv << "auto_px_credible_sigma," << autoSummary.pxPosteriorSigma.count << ","
            << autoSummary.pxPosteriorSigma.Mean() << "," << autoSummary.pxPosteriorSigma.RMS() << "\n";
        csv << "auto_px_credible68," << autoSummary.posteriorValid << ","
            << (autoSummary.posteriorValid > 0 ? static_cast<double>(autoSummary.credible68Px) / autoSummary.posteriorValid : 0.0)
            << ",0\n";
        csv << "auto_px_credible95," << autoSummary.posteriorValid << ","
            << (autoSummary.posteriorValid > 0 ? static_cast<double>(autoSummary.credible95Px) / autoSummary.posteriorValid : 0.0)
            << ",0\n";
        csv << "rk_attempts," << rkSummary.attempts << ",0,0\n";
        csv << "rk_success_rate," << rkSummary.attempts << ","
            << (rkSummary.attempts > 0 ? static_cast<double>(rkSummary.success) / rkSummary.attempts : 0.0)
            << ",0\n";
        csv << "rk_usable_rate," << rkSummary.attempts << ","
            << (rkSummary.attempts > 0 ? static_cast<double>(rkSummary.usable) / rkSummary.attempts : 0.0)
            << ",0\n";
        csv << "rk_dpx_success," << rkSummary.dpxSuccess.count << ","
            << rkSummary.dpxSuccess.Mean() << "," << rkSummary.dpxSuccess.RMS() << "\n";
        csv << "rk_dpx_usable," << rkSummary.dpxUsable.count << ","
            << rkSummary.dpxUsable.Mean() << "," << rkSummary.dpxUsable.RMS() << "\n";
        csv << "rk_chi2_reduced," << rkSummary.chi2Reduced.count << ","
            << rkSummary.chi2Reduced.Mean() << "," << rkSummary.chi2Reduced.RMS() << "\n";
        csv << "rk_px_sigma," << rkSummary.pxSigma.count << ","
            << rkSummary.pxSigma.Mean() << "," << rkSummary.pxSigma.RMS() << "\n";
        csv << "rk_px_coverage68," << rkSummary.uncertaintyValid << ","
            << (rkSummary.uncertaintyValid > 0 ? static_cast<double>(rkSummary.coverage68Px) / rkSummary.uncertaintyValid : 0.0)
            << ",0\n";
        csv << "rk_px_coverage95," << rkSummary.uncertaintyValid << ","
            << (rkSummary.uncertaintyValid > 0 ? static_cast<double>(rkSummary.coverage95Px) / rkSummary.uncertaintyValid : 0.0)
            << ",0\n";
        csv << "rk_px_credible_sigma," << rkSummary.pxPosteriorSigma.count << ","
            << rkSummary.pxPosteriorSigma.Mean() << "," << rkSummary.pxPosteriorSigma.RMS() << "\n";
        csv << "rk_px_credible68," << rkSummary.posteriorValid << ","
            << (rkSummary.posteriorValid > 0 ? static_cast<double>(rkSummary.credible68Px) / rkSummary.posteriorValid : 0.0)
            << ",0\n";
        csv << "rk_px_credible95," << rkSummary.posteriorValid << ","
            << (rkSummary.posteriorValid > 0 ? static_cast<double>(rkSummary.credible95Px) / rkSummary.posteriorValid : 0.0)
            << ",0\n";
        csv << "rk_fixed_target_attempts," << rkFixedTargetSummary.attempts << ",0,0\n";
        csv << "rk_fixed_target_success_rate," << rkFixedTargetSummary.attempts << ","
            << (rkFixedTargetSummary.attempts > 0 ? static_cast<double>(rkFixedTargetSummary.success) / rkFixedTargetSummary.attempts : 0.0)
            << ",0\n";
        csv << "rk_fixed_target_usable_rate," << rkFixedTargetSummary.attempts << ","
            << (rkFixedTargetSummary.attempts > 0 ? static_cast<double>(rkFixedTargetSummary.usable) / rkFixedTargetSummary.attempts : 0.0)
            << ",0\n";
        csv << "rk_fixed_target_dpx_success," << rkFixedTargetSummary.dpxSuccess.count << ","
            << rkFixedTargetSummary.dpxSuccess.Mean() << "," << rkFixedTargetSummary.dpxSuccess.RMS() << "\n";
        csv << "rk_fixed_target_dpx_usable," << rkFixedTargetSummary.dpxUsable.count << ","
            << rkFixedTargetSummary.dpxUsable.Mean() << "," << rkFixedTargetSummary.dpxUsable.RMS() << "\n";
        csv << "rk_fixed_target_chi2_reduced," << rkFixedTargetSummary.chi2Reduced.count << ","
            << rkFixedTargetSummary.chi2Reduced.Mean() << "," << rkFixedTargetSummary.chi2Reduced.RMS() << "\n";
        csv << "rk_fixed_target_px_sigma," << rkFixedTargetSummary.pxSigma.count << ","
            << rkFixedTargetSummary.pxSigma.Mean() << "," << rkFixedTargetSummary.pxSigma.RMS() << "\n";
        csv << "rk_fixed_target_px_coverage68," << rkFixedTargetSummary.uncertaintyValid << ","
            << (rkFixedTargetSummary.uncertaintyValid > 0 ? static_cast<double>(rkFixedTargetSummary.coverage68Px) / rkFixedTargetSummary.uncertaintyValid : 0.0)
            << ",0\n";
        csv << "rk_fixed_target_px_coverage95," << rkFixedTargetSummary.uncertaintyValid << ","
            << (rkFixedTargetSummary.uncertaintyValid > 0 ? static_cast<double>(rkFixedTargetSummary.coverage95Px) / rkFixedTargetSummary.uncertaintyValid : 0.0)
            << ",0\n";
        csv << "rk_fixed_target_px_credible_sigma," << rkFixedTargetSummary.pxPosteriorSigma.count << ","
            << rkFixedTargetSummary.pxPosteriorSigma.Mean() << "," << rkFixedTargetSummary.pxPosteriorSigma.RMS() << "\n";
        csv << "rk_fixed_target_px_credible68," << rkFixedTargetSummary.posteriorValid << ","
            << (rkFixedTargetSummary.posteriorValid > 0 ? static_cast<double>(rkFixedTargetSummary.credible68Px) / rkFixedTargetSummary.posteriorValid : 0.0)
            << ",0\n";
        csv << "rk_fixed_target_px_credible95," << rkFixedTargetSummary.posteriorValid << ","
            << (rkFixedTargetSummary.posteriorValid > 0 ? static_cast<double>(rkFixedTargetSummary.credible95Px) / rkFixedTargetSummary.posteriorValid : 0.0)
            << ",0\n";

        for (const auto& [status, count] : autoSummary.statusCount) {
            csv << "auto_status_" << StatusToString(status) << "," << count << ",0,0\n";
        }
        for (const auto& [status, count] : rkSummary.statusCount) {
            csv << "rk_status_" << StatusToString(status) << "," << count << ",0,0\n";
        }
        for (const auto& [status, count] : rkFixedTargetSummary.statusCount) {
            csv << "rk_fixed_target_status_" << StatusToString(status) << "," << count << ",0,0\n";
        }
        csv.close();
        std::cout << "[check_pdc_reco_backend] Wrote CSV: " << out_csv << std::endl;
    }
}
