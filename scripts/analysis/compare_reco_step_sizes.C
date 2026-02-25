#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TSystem.h"
#include "TString.h"
#include "TMath.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TROOT.h"

#include "TBeamSimData.hh"
#include "TSimData.hh"
#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "TargetReconstructor.hh"
#include "RecoEvent.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct PdcPoint {
    bool valid = false;
    int stepNo = 0;
    TVector3 position = TVector3(0, 0, 0);
};

struct PdcPair {
    PdcPoint pdc1;
    PdcPoint pdc2;
    bool IsValid() const { return pdc1.valid && pdc2.valid; }
};

std::vector<double> ParseCsvDoubles(const char* csv) {
    std::vector<double> values;
    if (!csv) return values;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) continue;
        values.push_back(std::atof(item.c_str()));
    }
    return values;
}

void EnsureIncludePaths() {
    const char* sms = std::getenv("SMSIMDIR");
    if (!sms) return;
    std::string inc1 = std::string("-I") + sms + "/libs/analysis/include";
    std::string inc2 = std::string("-I") + sms + "/libs/smg4lib/include";
    gSystem->AddIncludePath(inc1.c_str());
    gSystem->AddIncludePath(inc2.c_str());
}

void LoadAnalysisLib() {
    const char* sms = std::getenv("SMSIMDIR");
    if (!sms) return;
    std::string candidate1 = std::string(sms) + "/build/lib/libpdcanalysis.so";
    std::string candidate2 = std::string(sms) + "/lib/libpdcanalysis.so";
    if (gSystem->Load(candidate1.c_str()) >= 0) return;
    if (gSystem->Load(candidate2.c_str()) >= 0) return;
    gSystem->Load("libpdcanalysis.so");
}

bool IsPdcHit(const TSimData* hit) {
    if (!hit) return false;
    std::string det = hit->fDetectorName.Data();
    std::string mod = hit->fModuleName.Data();
    std::transform(det.begin(), det.end(), det.begin(), [](unsigned char c){ return std::tolower(c); });
    std::transform(mod.begin(), mod.end(), mod.begin(), [](unsigned char c){ return std::tolower(c); });
    return (det.find("pdc") != std::string::npos) || (mod.find("pdc") != std::string::npos) || (hit->fID == 0) || (hit->fID == 1);
}

int ResolvePdcIndex(const TSimData* hit) {
    if (!hit) return -1;
    if (hit->fID == 0) return 1;
    if (hit->fID == 1) return 2;

    std::string det = hit->fDetectorName.Data();
    std::string mod = hit->fModuleName.Data();
    std::transform(det.begin(), det.end(), det.begin(), [](unsigned char c){ return std::tolower(c); });
    std::transform(mod.begin(), mod.end(), mod.begin(), [](unsigned char c){ return std::tolower(c); });

    if (det.find("pdc1") != std::string::npos || mod.find("pdc1") != std::string::npos) return 1;
    if (det.find("pdc2") != std::string::npos || mod.find("pdc2") != std::string::npos) return 2;
    return -1;
}

PdcPair ExtractPdcPair(TClonesArray* fragArray) {
    PdcPair pair;
    if (!fragArray) return pair;

    const int n = fragArray->GetEntriesFast();
    for (int i = 0; i < n; ++i) {
        auto* hit = dynamic_cast<TSimData*>(fragArray->At(i));
        if (!hit || !IsPdcHit(hit)) continue;

        const int which = ResolvePdcIndex(hit);
        if (which < 0) continue;

        PdcPoint candidate;
        candidate.valid = true;
        candidate.stepNo = hit->fStepNo;
        // [EN] Use pre-step position to approximate the measured deposit coordinate at detector entrance. / [CN] 使用步前位置近似探测器入射处的沉积坐标。
        candidate.position = hit->fPrePosition;

        PdcPoint* current = (which == 1) ? &pair.pdc1 : &pair.pdc2;
        if (!current->valid || candidate.stepNo < current->stepNo) {
            *current = candidate;
        }
    }

    return pair;
}

bool ExtractProtonMomentum(const std::vector<TBeamSimData>* beam,
                           double targetAngleRad,
                           TVector3& outMom,
                           int& outBeamIndex) {
    if (!beam) return false;
    for (size_t i = 0; i < beam->size(); ++i) {
        const auto& p = beam->at(i);
        if (p.fZ != 1 || p.fA != 1) continue;
        outMom = p.fMomentum.Vect();
        // [EN] Rotate into reconstruction frame, consistent with existing analysis path. / [CN] 旋转到重建坐标系，与现有分析流程保持一致。
        outMom.RotateY(-targetAngleRad);
        outBeamIndex = static_cast<int>(i);
        return true;
    }
    return false;
}

double MinDistanceToTrajectory(const std::vector<TrajectoryPoint>& trajectory,
                               const TVector3& point) {
    if (trajectory.empty()) return std::numeric_limits<double>::max();
    double minDist = std::numeric_limits<double>::max();
    for (const auto& tp : trajectory) {
        const double d = (tp.position - point).Mag();
        if (d < minDist) minDist = d;
    }
    return minDist;
}

double Mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / static_cast<double>(values.size());
}

double Rms(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum2 = 0.0;
    for (double v : values) sum2 += v * v;
    return std::sqrt(sum2 / static_cast<double>(values.size()));
}

double Max(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return *std::max_element(values.begin(), values.end());
}

std::string BuildKey(const std::string& method, double step) {
    std::ostringstream oss;
    oss << method << "|" << std::fixed << std::setprecision(6) << step;
    return oss.str();
}

} // namespace

void compare_reco_step_sizes(const char* sim_root,
                             const char* geom_macro,
                             const char* field_table,
                             double field_rotation_deg,
                             const char* step_sizes_csv,
                             const char* out_root,
                             const char* out_csv = "",
                             double minuit_p_init = 627.0,
                             double tol = 5.0,
                             int minuit_max_iterations = 1200,
                             int threepoint_max_iterations = 300,
                             double threepoint_lr = 0.05,
                             double pdc_sigma = 0.5,
                             double target_sigma = 5.0,
                             int max_events = -1)
{
    EnsureIncludePaths();
    LoadAnalysisLib();
    gSystem->Load("libsmdata.so");

    if (!sim_root || !geom_macro || !field_table || !step_sizes_csv || !out_root) {
        std::cerr << "compare_reco_step_sizes: missing required argument" << std::endl;
        return;
    }

    const std::vector<double> stepSizes = ParseCsvDoubles(step_sizes_csv);
    if (stepSizes.empty()) {
        std::cerr << "compare_reco_step_sizes: no step sizes provided" << std::endl;
        return;
    }

    GeometryManager geo;
    if (!geo.LoadGeometry(geom_macro)) {
        std::cerr << "Failed to load geometry macro: " << geom_macro << std::endl;
        return;
    }
    const TVector3 targetPos = geo.GetTargetPosition();
    const double targetAngleRad = geo.GetTargetAngleRad();

    MagneticField magField;
    if (!magField.LoadFieldMap(field_table)) {
        std::cerr << "Failed to load magnetic field table: " << field_table << std::endl;
        return;
    }
    magField.SetRotationAngle(field_rotation_deg);

    TFile inFile(sim_root, "READ");
    if (inFile.IsZombie()) {
        std::cerr << "Failed to open simulation file: " << sim_root << std::endl;
        return;
    }

    TTree* tree = dynamic_cast<TTree*>(inFile.Get("tree"));
    if (!tree) {
        std::cerr << "No tree named 'tree' in " << sim_root << std::endl;
        return;
    }

    TClonesArray* fragArray = nullptr;
    tree->SetBranchAddress("FragSimData", &fragArray);

    std::vector<TBeamSimData>* beam = nullptr;
    if (tree->GetBranch("beam")) {
        tree->SetBranchAddress("beam", &beam);
    }

    TFile outFile(out_root, "RECREATE");
    TTree scanTree("reco_scan", "Step-size scan for TMinuit/ThreePoint reconstruction");
    TTree summaryTree("summary", "Summary by method and step size");

    Long64_t eventId = -1;
    int beamIndex = -1;
    double stepSize = 0.0;
    char method[16] = {0};
    bool success = false;
    double truePx = 0.0, truePy = 0.0, truePz = 0.0;
    double recoPx = 0.0, recoPy = 0.0, recoPz = 0.0;
    double pdc1X = 0.0, pdc1Y = 0.0, pdc1Z = 0.0;
    double pdc2X = 0.0, pdc2Y = 0.0, pdc2Z = 0.0;
    double dPdc1 = 0.0, dPdc2 = 0.0, dPdcMean = 0.0, dTarget = 0.0;
    double finalDistance = 0.0;
    double runtimeMs = 0.0;
    int trajPoints = 0;

    scanTree.Branch("event_id", &eventId, "event_id/L");
    scanTree.Branch("beam_index", &beamIndex, "beam_index/I");
    scanTree.Branch("step_size", &stepSize, "step_size/D");
    scanTree.Branch("method", method, "method/C");
    scanTree.Branch("success", &success, "success/O");
    scanTree.Branch("true_px", &truePx, "true_px/D");
    scanTree.Branch("true_py", &truePy, "true_py/D");
    scanTree.Branch("true_pz", &truePz, "true_pz/D");
    scanTree.Branch("reco_px", &recoPx, "reco_px/D");
    scanTree.Branch("reco_py", &recoPy, "reco_py/D");
    scanTree.Branch("reco_pz", &recoPz, "reco_pz/D");
    scanTree.Branch("pdc1_x", &pdc1X, "pdc1_x/D");
    scanTree.Branch("pdc1_y", &pdc1Y, "pdc1_y/D");
    scanTree.Branch("pdc1_z", &pdc1Z, "pdc1_z/D");
    scanTree.Branch("pdc2_x", &pdc2X, "pdc2_x/D");
    scanTree.Branch("pdc2_y", &pdc2Y, "pdc2_y/D");
    scanTree.Branch("pdc2_z", &pdc2Z, "pdc2_z/D");
    scanTree.Branch("d_pdc1", &dPdc1, "d_pdc1/D");
    scanTree.Branch("d_pdc2", &dPdc2, "d_pdc2/D");
    scanTree.Branch("d_pdc_mean", &dPdcMean, "d_pdc_mean/D");
    scanTree.Branch("d_target", &dTarget, "d_target/D");
    scanTree.Branch("final_distance", &finalDistance, "final_distance/D");
    scanTree.Branch("runtime_ms", &runtimeMs, "runtime_ms/D");
    scanTree.Branch("traj_points", &trajPoints, "traj_points/I");

    char summaryMethod[16] = {0};
    double summaryStep = 0.0;
    int summaryCount = 0;
    int successCount = 0;
    double meanDPdc = 0.0;
    double rmsDPdc = 0.0;
    double maxDPdc = 0.0;
    double meanDTarget = 0.0;
    double meanRuntimeMs = 0.0;
    double successRate = 0.0;

    summaryTree.Branch("method", summaryMethod, "method/C");
    summaryTree.Branch("step_size", &summaryStep, "step_size/D");
    summaryTree.Branch("count", &summaryCount, "count/I");
    summaryTree.Branch("success_count", &successCount, "success_count/I");
    summaryTree.Branch("success_rate", &successRate, "success_rate/D");
    summaryTree.Branch("mean_d_pdc", &meanDPdc, "mean_d_pdc/D");
    summaryTree.Branch("rms_d_pdc", &rmsDPdc, "rms_d_pdc/D");
    summaryTree.Branch("max_d_pdc", &maxDPdc, "max_d_pdc/D");
    summaryTree.Branch("mean_d_target", &meanDTarget, "mean_d_target/D");
    summaryTree.Branch("mean_runtime_ms", &meanRuntimeMs, "mean_runtime_ms/D");

    std::map<std::string, std::vector<double>> pdcMetricMap;
    std::map<std::string, std::vector<double>> targetMetricMap;
    std::map<std::string, std::vector<double>> runtimeMetricMap;
    std::map<std::string, int> countMap;
    std::map<std::string, int> successMap;

    const std::vector<std::string> methods = {"minuit", "threepoint"};
    Long64_t nEntries = tree->GetEntries();
    if (max_events > 0 && nEntries > max_events) nEntries = max_events;

    for (Long64_t i = 0; i < nEntries; ++i) {
        tree->GetEntry(i);
        eventId = i;

        const PdcPair pdc = ExtractPdcPair(fragArray);
        if (!pdc.IsValid()) continue;

        TVector3 fixedMomentum(0.0, 0.0, 627.0);
        beamIndex = -1;
        if (beam) {
            ExtractProtonMomentum(beam, targetAngleRad, fixedMomentum, beamIndex);
        }

        truePx = fixedMomentum.X();
        truePy = fixedMomentum.Y();
        truePz = fixedMomentum.Z();
        pdc1X = pdc.pdc1.position.X();
        pdc1Y = pdc.pdc1.position.Y();
        pdc1Z = pdc.pdc1.position.Z();
        pdc2X = pdc.pdc2.position.X();
        pdc2Y = pdc.pdc2.position.Y();
        pdc2Z = pdc.pdc2.position.Z();

        RecoTrack track;
        track.start = pdc.pdc1.position;
        track.end = pdc.pdc2.position;

        for (double ss : stepSizes) {
            stepSize = ss;

            for (const auto& m : methods) {
                TargetReconstructor recon(&magField);
                recon.SetTrajectoryStepSize(stepSize);
                TargetReconstructionResult recoResult;

                if (m == "minuit") {
                    const auto t0 = std::chrono::steady_clock::now();
                    recoResult = recon.ReconstructAtTargetMinuit(
                        track, targetPos, true, minuit_p_init, tol, minuit_max_iterations, false
                    );
                    const auto t1 = std::chrono::steady_clock::now();
                    runtimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
                } else {
                    const auto t0 = std::chrono::steady_clock::now();
                    recoResult = recon.ReconstructAtTargetThreePointGradientDescent(
                        track, targetPos, true, fixedMomentum, threepoint_lr, tol, threepoint_max_iterations, pdc_sigma, target_sigma
                    );
                    const auto t1 = std::chrono::steady_clock::now();
                    runtimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
                }

                std::snprintf(method, sizeof(method), "%s", m.c_str());
                success = recoResult.success;
                recoPx = recoResult.bestMomentum.Px();
                recoPy = recoResult.bestMomentum.Py();
                recoPz = recoResult.bestMomentum.Pz();
                finalDistance = recoResult.finalDistance;
                trajPoints = static_cast<int>(recoResult.bestTrajectory.size());

                dPdc1 = MinDistanceToTrajectory(recoResult.bestTrajectory, pdc.pdc1.position);
                dPdc2 = MinDistanceToTrajectory(recoResult.bestTrajectory, pdc.pdc2.position);
                dPdcMean = 0.5 * (dPdc1 + dPdc2);
                dTarget = MinDistanceToTrajectory(recoResult.bestTrajectory, targetPos);
                scanTree.Fill();

                const std::string key = BuildKey(m, ss);
                pdcMetricMap[key].push_back(dPdcMean);
                targetMetricMap[key].push_back(dTarget);
                runtimeMetricMap[key].push_back(runtimeMs);
                countMap[key] += 1;
                if (success) successMap[key] += 1;
            }
        }
    }

    for (double ss : stepSizes) {
        for (const auto& m : methods) {
            const std::string key = BuildKey(m, ss);
            const auto& pdcVals = pdcMetricMap[key];
            const auto& targetVals = targetMetricMap[key];
            const auto& runtimeVals = runtimeMetricMap[key];
            summaryCount = countMap[key];
            successCount = successMap[key];
            successRate = (summaryCount > 0) ? (static_cast<double>(successCount) / summaryCount) : 0.0;
            meanDPdc = Mean(pdcVals);
            rmsDPdc = Rms(pdcVals);
            maxDPdc = Max(pdcVals);
            meanDTarget = Mean(targetVals);
            meanRuntimeMs = Mean(runtimeVals);
            summaryStep = ss;
            std::snprintf(summaryMethod, sizeof(summaryMethod), "%s", m.c_str());
            summaryTree.Fill();
        }
    }

    outFile.Write();
    outFile.Close();

    if (out_csv && std::string(out_csv).size() > 0) {
        std::ofstream fout(out_csv);
        if (fout.is_open()) {
            fout << "method,step_size,mean_d_pdc,rms_d_pdc,max_d_pdc,mean_d_target,mean_runtime_ms,success_rate,count\n";
            for (double ss : stepSizes) {
                for (const auto& m : methods) {
                    const std::string key = BuildKey(m, ss);
                    const auto& pdcVals = pdcMetricMap[key];
                    const auto& targetVals = targetMetricMap[key];
                    const auto& runtimeVals = runtimeMetricMap[key];
                    const int c = countMap[key];
                    const int succ = successMap[key];
                    const double succRate = (c > 0) ? (static_cast<double>(succ) / c) : 0.0;
                    fout << m << ","
                         << ss << ","
                         << Mean(pdcVals) << ","
                         << Rms(pdcVals) << ","
                         << Max(pdcVals) << ","
                         << Mean(targetVals) << ","
                         << Mean(runtimeVals) << ","
                         << succRate << ","
                         << c << "\n";
                }
            }
        }
    }

    std::cout << "compare_reco_step_sizes done. Output ROOT: " << out_root << std::endl;
    if (out_csv && std::string(out_csv).size() > 0) {
        std::cout << "compare_reco_step_sizes done. Output CSV: " << out_csv << std::endl;
    }
}
