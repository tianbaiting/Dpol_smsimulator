// [EN] Usage: root -l -q 'scripts/analysis/diagnose_truth_pdc_reco.C+("sim.root","geom.mac","field.table","out/truth_pdc",30.0,5000,10.0,627.0)' / [CN] 用法: root -l -q 'scripts/analysis/diagnose_truth_pdc_reco.C+("sim.root","geom.mac","field.table","out/truth_pdc",30.0,5000,10.0,627.0)'

#include "TCanvas.h"
#include "TClonesArray.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TLegend.h"
#include "TLine.h"
#include "TMath.h"
#include "TPaveText.h"
#include "TProfile.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TTree.h"

#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include "RecoEvent.hh"
#include "TBeamSimData.hh"
#include "TSimData.hh"
#include "TargetReconstructor.hh"
#include "../../libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh"
#include "../../libs/analysis_pdc_reco/include/PDCRecoRuntime.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace SMLogger {
class Logger {
public:
    static Logger& Instance();
    void Shutdown();
};
}  // namespace SMLogger

namespace {

namespace reco = analysis::pdc::anaroot_like;

using reco::PDCInputTrack;
using reco::PDCMomentumReconstructor;
using reco::RecoConfig;
using reco::RecoResult;
using reco::SolverStatus;
using reco::TargetConstraint;

constexpr double kProtonMassMeV = 938.2720813;
constexpr double kSqrt2 = 1.4142135623730951;

struct TruthBeamInfo {
    bool hasProton = false;
    TVector3 protonMomentum{0.0, 0.0, 0.0};
};

struct LocalUV {
    double u = std::numeric_limits<double>::quiet_NaN();
    double v = std::numeric_limits<double>::quiet_NaN();
};

struct TruePdcPlane {
    bool hasU = false;
    bool hasV = false;
    bool hasPoint = false;
    int stepU = std::numeric_limits<int>::max();
    int stepV = std::numeric_limits<int>::max();
    double angleRad = 0.0;
    double u = std::numeric_limits<double>::quiet_NaN();
    double v = std::numeric_limits<double>::quiet_NaN();
    TVector3 center{std::numeric_limits<double>::quiet_NaN(),
                    std::numeric_limits<double>::quiet_NaN(),
                    std::numeric_limits<double>::quiet_NaN()};
    TVector3 point{std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()};
};

struct TruePdcTrack {
    TruePdcPlane pdc1;
    TruePdcPlane pdc2;

    bool IsValid() const {
        return pdc1.hasPoint && pdc2.hasPoint;
    }
};

struct ClosestPointResult {
    bool valid = false;
    TVector3 point{std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()};
    double distance_mm = std::numeric_limits<double>::infinity();
};

struct RunningStat {
    long long count = 0;
    double sum = 0.0;
    double sumSq = 0.0;

    void Fill(double value) {
        if (!std::isfinite(value)) return;
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

struct MethodStats {
    long long attempts = 0;
    long long success = 0;
    long long usable = 0;
    RunningStat dpx;
    RunningStat relDp;
    RunningStat fitHealth;
    RunningStat pdc1Du;
    RunningStat pdc1Dv;
    RunningStat pdc1Dr;
    RunningStat pdc2Du;
    RunningStat pdc2Dv;
    RunningStat pdc2Dr;
    std::map<int, long long> codeCount;
};

struct CoutSilencer {
    std::streambuf* oldBuf = nullptr;
    std::ostringstream sink;

    CoutSilencer() {
        oldBuf = std::cout.rdbuf(sink.rdbuf());
    }

    ~CoutSilencer() {
        if (oldBuf) std::cout.rdbuf(oldBuf);
    }
};

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool IsFinite(const TVector3& value) {
    return std::isfinite(value.X()) && std::isfinite(value.Y()) && std::isfinite(value.Z());
}

bool IsPdcHit(const TSimData* hit) {
    if (!hit) return false;
    const std::string det = ToLower(hit->fDetectorName.Data());
    const std::string mod = ToLower(hit->fModuleName.Data());
    return (det.find("pdc") != std::string::npos) || (mod.find("pdc") != std::string::npos);
}

TruthBeamInfo ExtractTruthBeam(const std::vector<TBeamSimData>* beam, double targetAngleRad) {
    TruthBeamInfo info;
    if (!beam) return info;

    for (const auto& p : *beam) {
        const std::string pname = ToLower(p.fParticleName.Data());
        if ((p.fZ == 1 && p.fA == 1) || pname == "proton" || p.fPDGCode == 2212) {
            TVector3 momentum = p.fMomentum.Vect();
            // [EN] Rotate truth momentum into the same analysis frame used by existing downstream plots. / [CN] 将真值动量旋转到与现有下游分析图一致的分析坐标系。
            momentum.RotateY(-targetAngleRad);
            info.hasProton = true;
            info.protonMomentum = momentum;
            break;
        }
    }
    return info;
}

TVector3 RotatedPdcCenter(const GeometryManager& geo, int pdcId) {
    TVector3 center = (pdcId == 0) ? geo.GetPDC1Position() : geo.GetPDC2Position();
    center.RotateY(-geo.GetAngleRad());
    return center;
}

LocalUV GlobalToPdcUV(const TVector3& globalPoint,
                      const TVector3& rotatedCenter,
                      double pdcAngleRad) {
    const TVector3 relative = globalPoint - rotatedCenter;
    // [EN] Mirror PDCSimAna's U/V transform exactly so solver diagnostics stay in the same coordinate convention. / [CN] 严格复用PDCSimAna的U/V变换，保证求解器诊断与现有点定义处于同一坐标约定。
    const double xRot = relative.X() * TMath::Cos(-pdcAngleRad) - relative.Z() * TMath::Sin(-pdcAngleRad);
    LocalUV uv;
    uv.u = (xRot + globalPoint.Y()) / kSqrt2;
    uv.v = (-xRot + globalPoint.Y()) / kSqrt2;
    return uv;
}

TVector3 PdcUVToGlobal(double u,
                       double v,
                       const TVector3& rotatedCenter,
                       double pdcAngleRad) {
    const double yGlobal = (u + v) / kSqrt2;
    const double xLocal = (u - v) / kSqrt2;
    const double xBack = xLocal * TMath::Cos(pdcAngleRad);
    const double zBack = xLocal * TMath::Sin(pdcAngleRad);
    return TVector3(xBack + rotatedCenter.X(), yGlobal + rotatedCenter.Y(), zBack + rotatedCenter.Z());
}

void UpdateTruePlaneHit(TruePdcPlane* plane,
                        const TSimData* hit,
                        const TVector3& rotatedCenter,
                        double pdcAngleRad) {
    if (!plane || !hit) return;
    const std::string module = ToLower(hit->fModuleName.Data());
    const LocalUV uv = GlobalToPdcUV(hit->fPrePosition, rotatedCenter, pdcAngleRad);

    if (module == "u" && hit->fStepNo < plane->stepU) {
        plane->hasU = true;
        plane->stepU = hit->fStepNo;
        plane->u = uv.u;
    } else if (module == "v" && hit->fStepNo < plane->stepV) {
        plane->hasV = true;
        plane->stepV = hit->fStepNo;
        plane->v = uv.v;
    }
}

TruePdcTrack ExtractTruePdcTrack(TClonesArray* fragArray, const GeometryManager& geo) {
    TruePdcTrack track;
    track.pdc1.angleRad = geo.GetAngleRad();
    track.pdc2.angleRad = geo.GetAngleRad();
    track.pdc1.center = RotatedPdcCenter(geo, 0);
    track.pdc2.center = RotatedPdcCenter(geo, 1);

    if (!fragArray) return track;

    const double pdcAngleRad = geo.GetAngleRad();
    const int n = fragArray->GetEntriesFast();
    for (int i = 0; i < n; ++i) {
        auto* hit = dynamic_cast<TSimData*>(fragArray->At(i));
        if (!IsPdcHit(hit)) continue;

        if (hit->fID == 0) {
            UpdateTruePlaneHit(&track.pdc1, hit, track.pdc1.center, pdcAngleRad);
        } else if (hit->fID == 1) {
            UpdateTruePlaneHit(&track.pdc2, hit, track.pdc2.center, pdcAngleRad);
        }
    }

    if (track.pdc1.hasU && track.pdc1.hasV) {
        track.pdc1.hasPoint = true;
        track.pdc1.point = PdcUVToGlobal(track.pdc1.u, track.pdc1.v, track.pdc1.center, pdcAngleRad);
    }
    if (track.pdc2.hasU && track.pdc2.hasV) {
        track.pdc2.hasPoint = true;
        track.pdc2.point = PdcUVToGlobal(track.pdc2.u, track.pdc2.v, track.pdc2.center, pdcAngleRad);
    }

    return track;
}

bool IsUsableResult(const RecoResult& result) {
    if (!(result.status == SolverStatus::kSuccess || result.status == SolverStatus::kNotConverged)) {
        return false;
    }
    return std::isfinite(result.p4_at_target.Px()) &&
           std::isfinite(result.p4_at_target.Py()) &&
           std::isfinite(result.p4_at_target.Pz()) &&
           result.p4_at_target.P() > 0.0 &&
           IsFinite(result.fit_start_position);
}

bool IsUsableThree(const TargetReconstructionResult& result) {
    return std::isfinite(result.bestMomentum.Px()) &&
           std::isfinite(result.bestMomentum.Py()) &&
           std::isfinite(result.bestMomentum.Pz()) &&
           result.bestMomentum.P() > 0.0 &&
           IsFinite(result.bestStartPos) &&
           IsFinite(result.bestInitialMomentum) &&
           result.bestInitialMomentum.Mag() > 0.0;
}

TLorentzVector MakeP4(const TVector3& momentum, double massMeV) {
    const double energy = std::sqrt(momentum.Mag2() + massMeV * massMeV);
    return TLorentzVector(momentum.X(), momentum.Y(), momentum.Z(), energy);
}

std::vector<ParticleTrajectory::TrajectoryPoint> TraceTrajectory(MagneticField* magneticField,
                                                                 const TVector3& startPos,
                                                                 const TLorentzVector& p4,
                                                                 double charge,
                                                                 double massMeV,
                                                                 double stepMm,
                                                                 const TVector3& pdc1,
                                                                 const TVector3& pdc2) {
    ParticleTrajectory tracer(magneticField);
    tracer.SetStepSize(stepMm);
    const double farDist = std::max((pdc1 - startPos).Mag(), (pdc2 - startPos).Mag());
    // [EN] Match the RK solver's generous integration window so missed plane intersections are comparable. / [CN] 复用RK求解器的较宽积分窗口，使“未打到PDC面”的诊断与求解器内部行为一致。
    tracer.SetMaxDistance(std::max(1500.0, farDist + 1500.0));
    tracer.SetMaxTime(400.0);
    tracer.SetMinMomentum(5.0);
    return tracer.CalculateTrajectory(startPos, p4, charge, massMeV);
}

ClosestPointResult FindClosestPoint(const std::vector<ParticleTrajectory::TrajectoryPoint>& trajectory,
                                    const TVector3& referencePoint) {
    ClosestPointResult result;
    for (const auto& point : trajectory) {
        const double distance = (point.position - referencePoint).Mag();
        if (distance < result.distance_mm) {
            result.distance_mm = distance;
            result.point = point.position;
            result.valid = true;
        }
    }
    return result;
}

double RelativeMomentumError(const TVector3& recoMomentum, const TVector3& truthMomentum) {
    const double truthMag = truthMomentum.Mag();
    if (!std::isfinite(truthMag) || truthMag <= 1.0e-9) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (recoMomentum.Mag() - truthMag) / truthMag;
}

double Median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    if (values.size() % 2 == 0) {
        return 0.5 * (values[mid - 1] + values[mid]);
    }
    return values[mid];
}

std::string SolverStatusToString(SolverStatus status) {
    switch (status) {
        case SolverStatus::kSuccess: return "success";
        case SolverStatus::kNotConverged: return "not_converged";
        case SolverStatus::kNotAvailable: return "not_available";
        case SolverStatus::kInvalidInput: return "invalid_input";
    }
    return "unknown";
}

void FillResidualHistograms(const TVector3& predictedPoint,
                            const TruePdcPlane& truthPlane,
                            TH2D* residualHist,
                            RunningStat* duStat,
                            RunningStat* dvStat,
                            RunningStat* drStat,
                            std::vector<double>* drSamples,
                            double* outDu,
    double* outDv) {
    if (!truthPlane.hasPoint || !IsFinite(predictedPoint)) return;
    const LocalUV predictedUV = GlobalToPdcUV(predictedPoint, truthPlane.center, truthPlane.angleRad);
    const double du = predictedUV.u - truthPlane.u;
    const double dv = predictedUV.v - truthPlane.v;
    const double dr = std::hypot(du, dv);

    if (outDu) *outDu = du;
    if (outDv) *outDv = dv;
    if (residualHist) residualHist->Fill(du, dv);
    if (duStat) duStat->Fill(du);
    if (dvStat) dvStat->Fill(dv);
    if (drStat) drStat->Fill(dr);
    if (drSamples) drSamples->push_back(dr);
}

void ConfigureLineHist(TH1D* hist, Color_t color) {
    if (!hist) return;
    hist->SetLineColor(color);
    hist->SetLineWidth(2);
}

TPaveText* DrawStatsBox(double x1,
                        double y1,
                        double x2,
                        double y2,
                        const std::vector<std::string>& lines) {
    auto* box = new TPaveText(x1, y1, x2, y2, "NDC");
    box->SetFillColor(0);
    box->SetBorderSize(1);
    box->SetTextAlign(12);
    box->SetTextSize(0.030);
    for (const auto& line : lines) {
        box->AddText(line.c_str());
    }
    box->Draw();
    return box;
}

TH1D* CloneNormalized(const TH1D* source, const char* name) {
    if (!source) return nullptr;
    auto* clone = dynamic_cast<TH1D*>(source->Clone(name));
    if (!clone) return nullptr;
    clone->SetDirectory(nullptr);
    const double integral = clone->Integral();
    if (integral > 0.0) {
        clone->Scale(1.0 / integral);
    }
    return clone;
}

template <typename THist>
THist* DetachFromDirectory(THist* hist) {
    if (hist) {
        hist->SetDirectory(nullptr);
    }
    return hist;
}

}  // namespace

void diagnose_truth_pdc_reco(const char* sim_root,
                             const char* geom_macro,
                             const char* field_table,
                             const char* out_prefix,
                             double field_rotation_deg = 30.0,
                             int max_events = -1,
                             double rk_step_mm = 10.0,
                             double initial_p_mevc = 627.0) {
    gSystem->Load("libsmdata.so");
    gSystem->Load("libpdcanalysis.so");
    gSystem->Load("libanalysis_pdc_reco.so");

    GeometryManager geo;
    if (!reco::LoadGeometryFromMacro(geo, geom_macro)) {
        std::cerr << "[diagnose_truth_pdc_reco] Failed to load geometry macro: " << geom_macro << std::endl;
        return;
    }

    MagneticField magField;
    if (!reco::LoadMagneticField(magField, field_table, field_rotation_deg)) {
        std::cerr << "[diagnose_truth_pdc_reco] Failed to load field table: " << field_table << std::endl;
        return;
    }

    const TVector3 targetPos = geo.GetTargetPosition();
    const double targetAngleRad = geo.GetTargetAngleRad();

    PDCMomentumReconstructor rkReconstructor(&magField);
    TargetReconstructor threeReconstructor(&magField);
    threeReconstructor.SetTrajectoryStepSize(rk_step_mm);

    reco::RuntimeOptions runtimeOptions;
    runtimeOptions.backend = reco::RuntimeBackend::kRungeKutta;
    runtimeOptions.initial_p_mevc = initial_p_mevc;
    runtimeOptions.max_iterations = 80;
    runtimeOptions.tolerance_mm = 5.0;
    runtimeOptions.rk_step_mm = rk_step_mm;
    runtimeOptions.pdc_sigma_mm = 0.5;
    runtimeOptions.target_sigma_xy_mm = 5.0;
    runtimeOptions.mass_mev = kProtonMassMeV;
    runtimeOptions.compute_uncertainty = false;
    runtimeOptions.compute_posterior_laplace = false;
    const RecoConfig rkConfig = reco::BuildRecoConfig(runtimeOptions, true);
    const TargetConstraint constraint = reco::BuildTargetConstraint(geo, runtimeOptions);

    TFile inFile(sim_root, "READ");
    if (inFile.IsZombie()) {
        std::cerr << "[diagnose_truth_pdc_reco] Failed to open input: " << sim_root << std::endl;
        return;
    }
    TTree* tree = dynamic_cast<TTree*>(inFile.Get("tree"));
    if (!tree) {
        std::cerr << "[diagnose_truth_pdc_reco] Missing tree 'tree' in " << sim_root << std::endl;
        return;
    }

    TClonesArray* fragArray = nullptr;
    std::vector<TBeamSimData>* beam = nullptr;
    tree->SetBranchAddress("FragSimData", &fragArray);
    if (tree->GetBranch("beam")) tree->SetBranchAddress("beam", &beam);

    const std::filesystem::path prefixPath(out_prefix);
    const std::filesystem::path outDir = prefixPath.has_parent_path() ? prefixPath.parent_path() : std::filesystem::path(".");
    std::error_code dirEc;
    std::filesystem::create_directories(outDir, dirEc);

    const std::string rootPath = prefixPath.string() + ".root";
    const std::string csvPath = prefixPath.string() + ".csv";
    const std::string rkOverviewPath = prefixPath.string() + "_rk2_truth_pdc_overview.png";
    const std::string threeOverviewPath = prefixPath.string() + "_three_truth_pdc_overview.png";
    const std::string comparePath = prefixPath.string() + "_truth_pdc_method_compare.png";

    TFile outFile(rootPath.c_str(), "RECREATE");
    TTree metrics("metrics", "Truth-PDC diagnostic metrics");
    metrics.SetDirectory(nullptr);

    Long64_t eventId = -1;
    bool hasTruthProton = false;
    bool hasTruthPdcTrack = false;
    double truePx = std::numeric_limits<double>::quiet_NaN();
    double truePy = std::numeric_limits<double>::quiet_NaN();
    double truePz = std::numeric_limits<double>::quiet_NaN();
    double trueP = std::numeric_limits<double>::quiet_NaN();
    double truePdc1U = std::numeric_limits<double>::quiet_NaN();
    double truePdc1V = std::numeric_limits<double>::quiet_NaN();
    double truePdc2U = std::numeric_limits<double>::quiet_NaN();
    double truePdc2V = std::numeric_limits<double>::quiet_NaN();

    bool rkDone = false;
    bool rkUsable = false;
    int rkStatus = -1;
    double rkPx = std::numeric_limits<double>::quiet_NaN();
    double rkPy = std::numeric_limits<double>::quiet_NaN();
    double rkPz = std::numeric_limits<double>::quiet_NaN();
    double rkP = std::numeric_limits<double>::quiet_NaN();
    double rkDpx = std::numeric_limits<double>::quiet_NaN();
    double rkRelDp = std::numeric_limits<double>::quiet_NaN();
    double rkChi2Reduced = std::numeric_limits<double>::quiet_NaN();
    double rkStartX = std::numeric_limits<double>::quiet_NaN();
    double rkStartY = std::numeric_limits<double>::quiet_NaN();
    double rkStartZ = std::numeric_limits<double>::quiet_NaN();
    double rkPdc1Du = std::numeric_limits<double>::quiet_NaN();
    double rkPdc1Dv = std::numeric_limits<double>::quiet_NaN();
    double rkPdc2Du = std::numeric_limits<double>::quiet_NaN();
    double rkPdc2Dv = std::numeric_limits<double>::quiet_NaN();

    bool threeDone = false;
    bool threeSuccess = false;
    bool threeUsable = false;
    int threeFitStatus = -1;
    double threePx = std::numeric_limits<double>::quiet_NaN();
    double threePy = std::numeric_limits<double>::quiet_NaN();
    double threePz = std::numeric_limits<double>::quiet_NaN();
    double threeP = std::numeric_limits<double>::quiet_NaN();
    double threeDpx = std::numeric_limits<double>::quiet_NaN();
    double threeRelDp = std::numeric_limits<double>::quiet_NaN();
    double threeFinalLoss = std::numeric_limits<double>::quiet_NaN();
    double threeEdm = std::numeric_limits<double>::quiet_NaN();
    double threeStartX = std::numeric_limits<double>::quiet_NaN();
    double threeStartY = std::numeric_limits<double>::quiet_NaN();
    double threeStartZ = std::numeric_limits<double>::quiet_NaN();
    double threePdc1Du = std::numeric_limits<double>::quiet_NaN();
    double threePdc1Dv = std::numeric_limits<double>::quiet_NaN();
    double threePdc2Du = std::numeric_limits<double>::quiet_NaN();
    double threePdc2Dv = std::numeric_limits<double>::quiet_NaN();

    metrics.Branch("event_id", &eventId, "event_id/L");
    metrics.Branch("has_truth_proton", &hasTruthProton, "has_truth_proton/O");
    metrics.Branch("has_truth_pdc_track", &hasTruthPdcTrack, "has_truth_pdc_track/O");
    metrics.Branch("true_px", &truePx, "true_px/D");
    metrics.Branch("true_py", &truePy, "true_py/D");
    metrics.Branch("true_pz", &truePz, "true_pz/D");
    metrics.Branch("true_p", &trueP, "true_p/D");
    metrics.Branch("true_pdc1_u", &truePdc1U, "true_pdc1_u/D");
    metrics.Branch("true_pdc1_v", &truePdc1V, "true_pdc1_v/D");
    metrics.Branch("true_pdc2_u", &truePdc2U, "true_pdc2_u/D");
    metrics.Branch("true_pdc2_v", &truePdc2V, "true_pdc2_v/D");
    metrics.Branch("rk_done", &rkDone, "rk_done/O");
    metrics.Branch("rk_usable", &rkUsable, "rk_usable/O");
    metrics.Branch("rk_status", &rkStatus, "rk_status/I");
    metrics.Branch("rk_px", &rkPx, "rk_px/D");
    metrics.Branch("rk_py", &rkPy, "rk_py/D");
    metrics.Branch("rk_pz", &rkPz, "rk_pz/D");
    metrics.Branch("rk_p", &rkP, "rk_p/D");
    metrics.Branch("rk_dpx", &rkDpx, "rk_dpx/D");
    metrics.Branch("rk_rel_dp", &rkRelDp, "rk_rel_dp/D");
    metrics.Branch("rk_chi2_reduced", &rkChi2Reduced, "rk_chi2_reduced/D");
    metrics.Branch("rk_start_x", &rkStartX, "rk_start_x/D");
    metrics.Branch("rk_start_y", &rkStartY, "rk_start_y/D");
    metrics.Branch("rk_start_z", &rkStartZ, "rk_start_z/D");
    metrics.Branch("rk_pdc1_du", &rkPdc1Du, "rk_pdc1_du/D");
    metrics.Branch("rk_pdc1_dv", &rkPdc1Dv, "rk_pdc1_dv/D");
    metrics.Branch("rk_pdc2_du", &rkPdc2Du, "rk_pdc2_du/D");
    metrics.Branch("rk_pdc2_dv", &rkPdc2Dv, "rk_pdc2_dv/D");
    metrics.Branch("three_done", &threeDone, "three_done/O");
    metrics.Branch("three_success", &threeSuccess, "three_success/O");
    metrics.Branch("three_usable", &threeUsable, "three_usable/O");
    metrics.Branch("three_fit_status", &threeFitStatus, "three_fit_status/I");
    metrics.Branch("three_px", &threePx, "three_px/D");
    metrics.Branch("three_py", &threePy, "three_py/D");
    metrics.Branch("three_pz", &threePz, "three_pz/D");
    metrics.Branch("three_p", &threeP, "three_p/D");
    metrics.Branch("three_dpx", &threeDpx, "three_dpx/D");
    metrics.Branch("three_rel_dp", &threeRelDp, "three_rel_dp/D");
    metrics.Branch("three_final_loss", &threeFinalLoss, "three_final_loss/D");
    metrics.Branch("three_edm", &threeEdm, "three_edm/D");
    metrics.Branch("three_start_x", &threeStartX, "three_start_x/D");
    metrics.Branch("three_start_y", &threeStartY, "three_start_y/D");
    metrics.Branch("three_start_z", &threeStartZ, "three_start_z/D");
    metrics.Branch("three_pdc1_du", &threePdc1Du, "three_pdc1_du/D");
    metrics.Branch("three_pdc1_dv", &threePdc1Dv, "three_pdc1_dv/D");
    metrics.Branch("three_pdc2_du", &threePdc2Du, "three_pdc2_du/D");
    metrics.Branch("three_pdc2_dv", &threePdc2Dv, "three_pdc2_dv/D");

    auto* hRkPxTrueReco = DetachFromDirectory(new TH2D("h_rk_px_true_reco",
                                                       "RK: p_{x}^{true} vs p_{x}^{reco};p_{x}^{true} [MeV/c];p_{x}^{reco} [MeV/c]",
                                                       160, -200.0, 200.0, 180, -600.0, 600.0));
    auto* hRkDpx = DetachFromDirectory(new TH1D("h_rk_dpx", "RK: #Delta p_{x};p_{x}^{reco} - p_{x}^{true} [MeV/c];Events", 180, -600.0, 600.0));
    auto* hRkRelDp = DetachFromDirectory(new TH1D("h_rk_rel_dp", "RK: #Delta p/p;(|p|^{reco} - |p|^{true}) / |p|^{true};Events", 160, -1.5, 1.5));
    auto* hRkPdc1DuDv = DetachFromDirectory(new TH2D("h_rk_pdc1_du_dv", "RK: PDC1 local residual;#Delta u [mm];#Delta v [mm]", 180, -300.0, 300.0, 180, -300.0, 300.0));
    auto* hRkPdc2DuDv = DetachFromDirectory(new TH2D("h_rk_pdc2_du_dv", "RK: PDC2 local residual;#Delta u [mm];#Delta v [mm]", 180, -300.0, 300.0, 180, -300.0, 300.0));
    auto* hRkChi2Reduced = DetachFromDirectory(new TH1D("h_rk_chi2_reduced", "RK: #chi^{2}_{reduced};#chi^{2}_{reduced};Events", 160, 0.0, 120.0));

    auto* hThreePxTrueReco = DetachFromDirectory(new TH2D("h_three_px_true_reco",
                                                          "Three-point: p_{x}^{true} vs p_{x}^{reco};p_{x}^{true} [MeV/c];p_{x}^{reco} [MeV/c]",
                                                          160, -200.0, 200.0, 180, -600.0, 600.0));
    auto* hThreeDpx = DetachFromDirectory(new TH1D("h_three_dpx", "Three-point: #Delta p_{x};p_{x}^{reco} - p_{x}^{true} [MeV/c];Events", 180, -600.0, 600.0));
    auto* hThreeRelDp = DetachFromDirectory(new TH1D("h_three_rel_dp", "Three-point: #Delta p/p;(|p|^{reco} - |p|^{true}) / |p|^{true};Events", 160, -1.5, 1.5));
    auto* hThreePdc1DuDv = DetachFromDirectory(new TH2D("h_three_pdc1_du_dv", "Three-point: PDC1 local residual;#Delta u [mm];#Delta v [mm]", 180, -300.0, 300.0, 180, -300.0, 300.0));
    auto* hThreePdc2DuDv = DetachFromDirectory(new TH2D("h_three_pdc2_du_dv", "Three-point: PDC2 local residual;#Delta u [mm];#Delta v [mm]", 180, -300.0, 300.0, 180, -300.0, 300.0));
    auto* hThreeFinalLoss = DetachFromDirectory(new TH1D("h_three_final_loss", "Three-point: final loss;normalized loss;Events", 160, 0.0, 80.0));

    MethodStats rkStats;
    MethodStats threeStats;
    std::vector<double> rkPdc1DrSamples;
    std::vector<double> rkPdc2DrSamples;
    std::vector<double> threePdc1DrSamples;
    std::vector<double> threePdc2DrSamples;

    Long64_t truthProtonCount = 0;
    Long64_t truthPdcTrackCount = 0;

    const Long64_t nEntries = tree->GetEntries();
    const Long64_t nToProcess = (max_events > 0 && nEntries > max_events) ? max_events : nEntries;

    std::cerr << "[diagnose_truth_pdc_reco] Processing events: " << nToProcess << std::endl;
    for (Long64_t i = 0; i < nToProcess; ++i) {
        if (i > 0 && i % 1000 == 0) {
            std::cerr << "[diagnose_truth_pdc_reco] Event " << i << "/" << nToProcess << std::endl;
        }
        tree->GetEntry(i);

        eventId = i;
        hasTruthProton = false;
        hasTruthPdcTrack = false;
        truePx = truePy = truePz = trueP = std::numeric_limits<double>::quiet_NaN();
        truePdc1U = truePdc1V = truePdc2U = truePdc2V = std::numeric_limits<double>::quiet_NaN();
        rkDone = false;
        rkUsable = false;
        rkStatus = -1;
        rkPx = rkPy = rkPz = rkP = rkDpx = rkRelDp = rkChi2Reduced = std::numeric_limits<double>::quiet_NaN();
        rkStartX = rkStartY = rkStartZ = std::numeric_limits<double>::quiet_NaN();
        rkPdc1Du = rkPdc1Dv = rkPdc2Du = rkPdc2Dv = std::numeric_limits<double>::quiet_NaN();
        threeDone = false;
        threeSuccess = false;
        threeUsable = false;
        threeFitStatus = -1;
        threePx = threePy = threePz = threeP = threeDpx = threeRelDp = std::numeric_limits<double>::quiet_NaN();
        threeFinalLoss = threeEdm = std::numeric_limits<double>::quiet_NaN();
        threeStartX = threeStartY = threeStartZ = std::numeric_limits<double>::quiet_NaN();
        threePdc1Du = threePdc1Dv = threePdc2Du = threePdc2Dv = std::numeric_limits<double>::quiet_NaN();

        const TruthBeamInfo truth = ExtractTruthBeam(beam, targetAngleRad);
        const TruePdcTrack truthTrack = ExtractTruePdcTrack(fragArray, geo);

        hasTruthProton = truth.hasProton;
        hasTruthPdcTrack = truthTrack.IsValid();
        if (hasTruthProton) {
            ++truthProtonCount;
            truePx = truth.protonMomentum.X();
            truePy = truth.protonMomentum.Y();
            truePz = truth.protonMomentum.Z();
            trueP = truth.protonMomentum.Mag();
        }
        if (hasTruthPdcTrack) {
            truePdc1U = truthTrack.pdc1.u;
            truePdc1V = truthTrack.pdc1.v;
            truePdc2U = truthTrack.pdc2.u;
            truePdc2V = truthTrack.pdc2.v;
        }

        if (!(hasTruthProton && hasTruthPdcTrack)) {
            metrics.Fill();
            continue;
        }
        ++truthPdcTrackCount;

        PDCInputTrack inputTrack;
        inputTrack.pdc1 = truthTrack.pdc1.point;
        inputTrack.pdc2 = truthTrack.pdc2.point;
        const RecoTrack recoTrack(truthTrack.pdc1.point, truthTrack.pdc2.point);
        TVector3 threeInitMomentum = truthTrack.pdc2.point - truthTrack.pdc1.point;
        if (threeInitMomentum.Mag2() <= 1.0e-12) {
            threeInitMomentum = TVector3(0.0, 0.0, 1.0);
        }
        threeInitMomentum = threeInitMomentum.Unit() * initial_p_mevc;

        RecoResult rkResult;
        TargetReconstructionResult threeResult;
        {
            CoutSilencer silencer;
            rkResult = rkReconstructor.ReconstructRK(inputTrack, constraint, rkConfig);
            threeResult = threeReconstructor.ReconstructAtTargetThreePointFreeMinuit(
                recoTrack,
                targetPos,
                false,
                targetPos,
                threeInitMomentum,
                5.0,
                0.5,
                1.0,
                600,
                false
            );
        }

        rkDone = true;
        rkStatus = static_cast<int>(rkResult.status);
        ++rkStats.attempts;
        ++rkStats.codeCount[rkStatus];
        if (rkResult.status == SolverStatus::kSuccess) {
            ++rkStats.success;
        }
        rkUsable = IsUsableResult(rkResult);
        if (rkUsable) {
            ++rkStats.usable;
            const TVector3 rkMomentum(rkResult.p4_at_target.Px(), rkResult.p4_at_target.Py(), rkResult.p4_at_target.Pz());
            rkPx = rkMomentum.X();
            rkPy = rkMomentum.Y();
            rkPz = rkMomentum.Z();
            rkP = rkMomentum.Mag();
            rkDpx = rkPx - truePx;
            rkRelDp = RelativeMomentumError(rkMomentum, truth.protonMomentum);
            rkStats.dpx.Fill(rkDpx);
            rkStats.relDp.Fill(rkRelDp);
            hRkPxTrueReco->Fill(truePx, rkPx);
            hRkDpx->Fill(rkDpx);
            hRkRelDp->Fill(rkRelDp);
            rkStartX = rkResult.fit_start_position.X();
            rkStartY = rkResult.fit_start_position.Y();
            rkStartZ = rkResult.fit_start_position.Z();

            const auto rkTrajectory = TraceTrajectory(&magField,
                                                      rkResult.fit_start_position,
                                                      rkResult.p4_at_target,
                                                      constraint.charge_e,
                                                      constraint.mass_mev,
                                                      rk_step_mm,
                                                      truthTrack.pdc1.point,
                                                      truthTrack.pdc2.point);
            const ClosestPointResult rkClosest1 = FindClosestPoint(rkTrajectory, truthTrack.pdc1.point);
            const ClosestPointResult rkClosest2 = FindClosestPoint(rkTrajectory, truthTrack.pdc2.point);
            if (rkClosest1.valid) {
                FillResidualHistograms(rkClosest1.point,
                                       truthTrack.pdc1,
                                       hRkPdc1DuDv,
                                       &rkStats.pdc1Du,
                                       &rkStats.pdc1Dv,
                                       &rkStats.pdc1Dr,
                                       &rkPdc1DrSamples,
                                       &rkPdc1Du,
                                       &rkPdc1Dv);
            }
            if (rkClosest2.valid) {
                FillResidualHistograms(rkClosest2.point,
                                       truthTrack.pdc2,
                                       hRkPdc2DuDv,
                                       &rkStats.pdc2Du,
                                       &rkStats.pdc2Dv,
                                       &rkStats.pdc2Dr,
                                       &rkPdc2DrSamples,
                                       &rkPdc2Du,
                                       &rkPdc2Dv);
            }
        }
        rkChi2Reduced = rkResult.chi2_reduced;
        if (std::isfinite(rkChi2Reduced)) {
            rkStats.fitHealth.Fill(rkChi2Reduced);
            hRkChi2Reduced->Fill(rkChi2Reduced);
        }

        threeDone = true;
        threeSuccess = threeResult.success;
        threeFitStatus = threeResult.fitStatus;
        ++threeStats.attempts;
        ++threeStats.codeCount[threeFitStatus];
        if (threeSuccess) {
            ++threeStats.success;
        }
        threeUsable = IsUsableThree(threeResult);
        threeFinalLoss = threeResult.finalLoss;
        threeEdm = threeResult.edm;
        if (std::isfinite(threeFinalLoss)) {
            threeStats.fitHealth.Fill(threeFinalLoss);
            hThreeFinalLoss->Fill(threeFinalLoss);
        }
        if (threeUsable) {
            ++threeStats.usable;
            const TVector3 threeMomentum(threeResult.bestMomentum.Px(),
                                         threeResult.bestMomentum.Py(),
                                         threeResult.bestMomentum.Pz());
            threePx = threeMomentum.X();
            threePy = threeMomentum.Y();
            threePz = threeMomentum.Z();
            threeP = threeMomentum.Mag();
            threeDpx = threePx - truePx;
            threeRelDp = RelativeMomentumError(threeMomentum, truth.protonMomentum);
            threeStats.dpx.Fill(threeDpx);
            threeStats.relDp.Fill(threeRelDp);
            hThreePxTrueReco->Fill(truePx, threePx);
            hThreeDpx->Fill(threeDpx);
            hThreeRelDp->Fill(threeRelDp);
            threeStartX = threeResult.bestStartPos.X();
            threeStartY = threeResult.bestStartPos.Y();
            threeStartZ = threeResult.bestStartPos.Z();

            const auto threeTrajectory = TraceTrajectory(&magField,
                                                         threeResult.bestStartPos,
                                                         MakeP4(threeResult.bestInitialMomentum, kProtonMassMeV),
                                                         constraint.charge_e,
                                                         constraint.mass_mev,
                                                         rk_step_mm,
                                                         truthTrack.pdc1.point,
                                                         truthTrack.pdc2.point);
            const ClosestPointResult threeClosest1 = FindClosestPoint(threeTrajectory, truthTrack.pdc1.point);
            const ClosestPointResult threeClosest2 = FindClosestPoint(threeTrajectory, truthTrack.pdc2.point);
            if (threeClosest1.valid) {
                FillResidualHistograms(threeClosest1.point,
                                       truthTrack.pdc1,
                                       hThreePdc1DuDv,
                                       &threeStats.pdc1Du,
                                       &threeStats.pdc1Dv,
                                       &threeStats.pdc1Dr,
                                       &threePdc1DrSamples,
                                       &threePdc1Du,
                                       &threePdc1Dv);
            }
            if (threeClosest2.valid) {
                FillResidualHistograms(threeClosest2.point,
                                       truthTrack.pdc2,
                                       hThreePdc2DuDv,
                                       &threeStats.pdc2Du,
                                       &threeStats.pdc2Dv,
                                       &threeStats.pdc2Dr,
                                       &threePdc2DrSamples,
                                       &threePdc2Du,
                                       &threePdc2Dv);
            }
        }

        metrics.Fill();
    }

    gStyle->SetOptStat(0);

    TCanvas cRk("cRkTruthPdc", "RK truth-PDC overview", 1600, 1000);
    cRk.Divide(3, 2);
    cRk.cd(1);
    hRkPxTrueReco->Draw("colz");
    {
        auto* line = new TLine(-200.0, -200.0, 200.0, 200.0);
        line->SetLineColor(kRed + 1);
        line->SetLineWidth(2);
        line->Draw("same");
        auto* profile = hRkPxTrueReco->ProfileX("p_rk_px_true_reco");
        profile->SetDirectory(nullptr);
        profile->SetLineColor(kBlack);
        profile->SetLineWidth(2);
        profile->Draw("same");
    }
    cRk.cd(2);
    ConfigureLineHist(hRkDpx, kBlue + 1);
    hRkDpx->Draw("hist");
    DrawStatsBox(0.55, 0.74, 0.88, 0.88,
                 {Form("mean = %.3f MeV/c", hRkDpx->GetMean()),
                  Form("RMS = %.3f MeV/c", hRkDpx->GetRMS())});
    cRk.cd(3);
    ConfigureLineHist(hRkRelDp, kBlue + 1);
    hRkRelDp->Draw("hist");
    DrawStatsBox(0.55, 0.74, 0.88, 0.88,
                 {Form("mean = %.4f", hRkRelDp->GetMean()),
                  Form("RMS = %.4f", hRkRelDp->GetRMS())});
    cRk.cd(4);
    hRkPdc1DuDv->Draw("colz");
    DrawStatsBox(0.15, 0.74, 0.48, 0.88,
                 {Form("RMS(#Delta u) = %.3f mm", rkStats.pdc1Du.RMS()),
                  Form("RMS(#Delta v) = %.3f mm", rkStats.pdc1Dv.RMS())});
    cRk.cd(5);
    hRkPdc2DuDv->Draw("colz");
    DrawStatsBox(0.15, 0.74, 0.48, 0.88,
                 {Form("RMS(#Delta u) = %.3f mm", rkStats.pdc2Du.RMS()),
                  Form("RMS(#Delta v) = %.3f mm", rkStats.pdc2Dv.RMS())});
    cRk.cd(6);
    ConfigureLineHist(hRkChi2Reduced, kBlue + 1);
    hRkChi2Reduced->Draw("hist");
    DrawStatsBox(0.55, 0.68, 0.88, 0.88,
                 {Form("mean = %.3f", hRkChi2Reduced->GetMean()),
                  Form("RMS = %.3f", hRkChi2Reduced->GetRMS()),
                  Form("success = %.4f", rkStats.attempts > 0 ? static_cast<double>(rkStats.success) / rkStats.attempts : 0.0),
                  Form("usable = %.4f", rkStats.attempts > 0 ? static_cast<double>(rkStats.usable) / rkStats.attempts : 0.0)});
    cRk.SaveAs(rkOverviewPath.c_str());

    TCanvas cThree("cThreeTruthPdc", "Three-point truth-PDC overview", 1600, 1000);
    cThree.Divide(3, 2);
    cThree.cd(1);
    hThreePxTrueReco->Draw("colz");
    {
        auto* line = new TLine(-200.0, -200.0, 200.0, 200.0);
        line->SetLineColor(kRed + 1);
        line->SetLineWidth(2);
        line->Draw("same");
        auto* profile = hThreePxTrueReco->ProfileX("p_three_px_true_reco");
        profile->SetDirectory(nullptr);
        profile->SetLineColor(kBlack);
        profile->SetLineWidth(2);
        profile->Draw("same");
    }
    cThree.cd(2);
    ConfigureLineHist(hThreeDpx, kRed + 1);
    hThreeDpx->Draw("hist");
    DrawStatsBox(0.55, 0.74, 0.88, 0.88,
                 {Form("mean = %.3f MeV/c", hThreeDpx->GetMean()),
                  Form("RMS = %.3f MeV/c", hThreeDpx->GetRMS())});
    cThree.cd(3);
    ConfigureLineHist(hThreeRelDp, kRed + 1);
    hThreeRelDp->Draw("hist");
    DrawStatsBox(0.55, 0.74, 0.88, 0.88,
                 {Form("mean = %.4f", hThreeRelDp->GetMean()),
                  Form("RMS = %.4f", hThreeRelDp->GetRMS())});
    cThree.cd(4);
    hThreePdc1DuDv->Draw("colz");
    DrawStatsBox(0.15, 0.74, 0.48, 0.88,
                 {Form("RMS(#Delta u) = %.3f mm", threeStats.pdc1Du.RMS()),
                  Form("RMS(#Delta v) = %.3f mm", threeStats.pdc1Dv.RMS())});
    cThree.cd(5);
    hThreePdc2DuDv->Draw("colz");
    DrawStatsBox(0.15, 0.74, 0.48, 0.88,
                 {Form("RMS(#Delta u) = %.3f mm", threeStats.pdc2Du.RMS()),
                  Form("RMS(#Delta v) = %.3f mm", threeStats.pdc2Dv.RMS())});
    cThree.cd(6);
    ConfigureLineHist(hThreeFinalLoss, kRed + 1);
    hThreeFinalLoss->Draw("hist");
    {
        std::vector<std::string> lines{
            Form("mean loss = %.3f", hThreeFinalLoss->GetMean()),
            Form("RMS loss = %.3f", hThreeFinalLoss->GetRMS()),
            Form("success = %.4f", threeStats.attempts > 0 ? static_cast<double>(threeStats.success) / threeStats.attempts : 0.0),
            Form("usable = %.4f", threeStats.attempts > 0 ? static_cast<double>(threeStats.usable) / threeStats.attempts : 0.0)
        };
        for (const auto& [code, count] : threeStats.codeCount) {
            lines.push_back(Form("fitStatus[%d] = %lld", code, count));
        }
        DrawStatsBox(0.48, 0.52, 0.88, 0.88, lines);
    }
    cThree.SaveAs(threeOverviewPath.c_str());

    auto* hRkDpxNorm = CloneNormalized(hRkDpx, "h_rk_dpx_norm");
    auto* hThreeDpxNorm = CloneNormalized(hThreeDpx, "h_three_dpx_norm");
    auto* hRkRelDpNorm = CloneNormalized(hRkRelDp, "h_rk_rel_dp_norm");
    auto* hThreeRelDpNorm = CloneNormalized(hThreeRelDp, "h_three_rel_dp_norm");

    auto* hMethodFraction = DetachFromDirectory(new TH1D("h_method_fraction", "Method fractions;category;fraction", 4, 0.5, 4.5));
    hMethodFraction->GetXaxis()->SetBinLabel(1, "RK success");
    hMethodFraction->GetXaxis()->SetBinLabel(2, "RK usable");
    hMethodFraction->GetXaxis()->SetBinLabel(3, "Three success");
    hMethodFraction->GetXaxis()->SetBinLabel(4, "Three usable");
    hMethodFraction->SetBinContent(1, rkStats.attempts > 0 ? static_cast<double>(rkStats.success) / rkStats.attempts : 0.0);
    hMethodFraction->SetBinContent(2, rkStats.attempts > 0 ? static_cast<double>(rkStats.usable) / rkStats.attempts : 0.0);
    hMethodFraction->SetBinContent(3, threeStats.attempts > 0 ? static_cast<double>(threeStats.success) / threeStats.attempts : 0.0);
    hMethodFraction->SetBinContent(4, threeStats.attempts > 0 ? static_cast<double>(threeStats.usable) / threeStats.attempts : 0.0);
    hMethodFraction->SetMinimum(0.0);
    hMethodFraction->SetMaximum(1.05);
    hMethodFraction->SetFillColor(kGray + 1);
    hMethodFraction->SetBarWidth(0.6);

    auto* hPdcResidualMedian = DetachFromDirectory(new TH1D("h_pdc_residual_median", "Median local residual radius;category;median #sqrt{(#Delta u)^{2} + (#Delta v)^{2}} [mm]", 4, 0.5, 4.5));
    hPdcResidualMedian->GetXaxis()->SetBinLabel(1, "RK PDC1");
    hPdcResidualMedian->GetXaxis()->SetBinLabel(2, "RK PDC2");
    hPdcResidualMedian->GetXaxis()->SetBinLabel(3, "Three PDC1");
    hPdcResidualMedian->GetXaxis()->SetBinLabel(4, "Three PDC2");
    hPdcResidualMedian->SetBinContent(1, Median(rkPdc1DrSamples));
    hPdcResidualMedian->SetBinContent(2, Median(rkPdc2DrSamples));
    hPdcResidualMedian->SetBinContent(3, Median(threePdc1DrSamples));
    hPdcResidualMedian->SetBinContent(4, Median(threePdc2DrSamples));
    hPdcResidualMedian->SetFillColor(kAzure - 9);
    hPdcResidualMedian->SetBarWidth(0.6);

    TCanvas cCompare("cTruthPdcCompare", "Truth-PDC method comparison", 1400, 900);
    cCompare.Divide(2, 2);
    cCompare.cd(1);
    if (hRkDpxNorm && hThreeDpxNorm) {
        ConfigureLineHist(hRkDpxNorm, kBlue + 1);
        ConfigureLineHist(hThreeDpxNorm, kRed + 1);
        const double maxY = std::max(hRkDpxNorm->GetMaximum(), hThreeDpxNorm->GetMaximum());
        hRkDpxNorm->SetMaximum(maxY * 1.15);
        hRkDpxNorm->SetTitle("Normalized #Delta p_{x} comparison;#Delta p_{x} [MeV/c];Normalized counts");
        hRkDpxNorm->Draw("hist");
        hThreeDpxNorm->Draw("hist same");
        auto* legend = new TLegend(0.64, 0.76, 0.88, 0.88);
        legend->AddEntry(hRkDpxNorm, "RK", "l");
        legend->AddEntry(hThreeDpxNorm, "Three-point", "l");
        legend->Draw();
    }
    cCompare.cd(2);
    if (hRkRelDpNorm && hThreeRelDpNorm) {
        ConfigureLineHist(hRkRelDpNorm, kBlue + 1);
        ConfigureLineHist(hThreeRelDpNorm, kRed + 1);
        const double maxY = std::max(hRkRelDpNorm->GetMaximum(), hThreeRelDpNorm->GetMaximum());
        hRkRelDpNorm->SetMaximum(maxY * 1.15);
        hRkRelDpNorm->SetTitle("Normalized #Delta p/p comparison;#Delta p/p;Normalized counts");
        hRkRelDpNorm->Draw("hist");
        hThreeRelDpNorm->Draw("hist same");
        auto* legend = new TLegend(0.64, 0.76, 0.88, 0.88);
        legend->AddEntry(hRkRelDpNorm, "RK", "l");
        legend->AddEntry(hThreeRelDpNorm, "Three-point", "l");
        legend->Draw();
    }
    cCompare.cd(3);
    hMethodFraction->Draw("hist text");
    cCompare.cd(4);
    hPdcResidualMedian->Draw("hist text");
    cCompare.SaveAs(comparePath.c_str());

    outFile.cd();
    metrics.Write();
    hRkPxTrueReco->Write();
    hRkDpx->Write();
    hRkRelDp->Write();
    hRkPdc1DuDv->Write();
    hRkPdc2DuDv->Write();
    hRkChi2Reduced->Write();
    hThreePxTrueReco->Write();
    hThreeDpx->Write();
    hThreeRelDp->Write();
    hThreePdc1DuDv->Write();
    hThreePdc2DuDv->Write();
    hThreeFinalLoss->Write();
    hMethodFraction->Write();
    hPdcResidualMedian->Write();
    if (hRkDpxNorm) hRkDpxNorm->Write();
    if (hThreeDpxNorm) hThreeDpxNorm->Write();
    if (hRkRelDpNorm) hRkRelDpNorm->Write();
    if (hThreeRelDpNorm) hThreeRelDpNorm->Write();
    outFile.Write();
    outFile.Close();

    std::ofstream csv(csvPath);
    if (!csv.is_open()) {
        std::cerr << "[diagnose_truth_pdc_reco] Failed to open CSV: " << csvPath << std::endl;
        return;
    }
    csv << "metric,count,mean,rms\n";
    csv << "truth_proton_events," << truthProtonCount << ",0,0\n";
    csv << "truth_pdc_track_events," << truthPdcTrackCount << ",0,0\n";
    csv << "rk_attempts," << rkStats.attempts << ",0,0\n";
    csv << "rk_success_rate," << rkStats.attempts << ","
        << (rkStats.attempts > 0 ? static_cast<double>(rkStats.success) / rkStats.attempts : 0.0) << ",0\n";
    csv << "rk_usable_rate," << rkStats.attempts << ","
        << (rkStats.attempts > 0 ? static_cast<double>(rkStats.usable) / rkStats.attempts : 0.0) << ",0\n";
    csv << "rk_dpx," << rkStats.dpx.count << "," << rkStats.dpx.Mean() << "," << rkStats.dpx.RMS() << "\n";
    csv << "rk_rel_dp," << rkStats.relDp.count << "," << rkStats.relDp.Mean() << "," << rkStats.relDp.RMS() << "\n";
    csv << "rk_chi2_reduced," << rkStats.fitHealth.count << "," << rkStats.fitHealth.Mean() << "," << rkStats.fitHealth.RMS() << "\n";
    csv << "rk_pdc1_du," << rkStats.pdc1Du.count << "," << rkStats.pdc1Du.Mean() << "," << rkStats.pdc1Du.RMS() << "\n";
    csv << "rk_pdc1_dv," << rkStats.pdc1Dv.count << "," << rkStats.pdc1Dv.Mean() << "," << rkStats.pdc1Dv.RMS() << "\n";
    csv << "rk_pdc1_dr_median," << rkPdc1DrSamples.size() << "," << Median(rkPdc1DrSamples) << ",0\n";
    csv << "rk_pdc2_du," << rkStats.pdc2Du.count << "," << rkStats.pdc2Du.Mean() << "," << rkStats.pdc2Du.RMS() << "\n";
    csv << "rk_pdc2_dv," << rkStats.pdc2Dv.count << "," << rkStats.pdc2Dv.Mean() << "," << rkStats.pdc2Dv.RMS() << "\n";
    csv << "rk_pdc2_dr_median," << rkPdc2DrSamples.size() << "," << Median(rkPdc2DrSamples) << ",0\n";
    csv << "three_attempts," << threeStats.attempts << ",0,0\n";
    csv << "three_success_rate," << threeStats.attempts << ","
        << (threeStats.attempts > 0 ? static_cast<double>(threeStats.success) / threeStats.attempts : 0.0) << ",0\n";
    csv << "three_usable_rate," << threeStats.attempts << ","
        << (threeStats.attempts > 0 ? static_cast<double>(threeStats.usable) / threeStats.attempts : 0.0) << ",0\n";
    csv << "three_dpx," << threeStats.dpx.count << "," << threeStats.dpx.Mean() << "," << threeStats.dpx.RMS() << "\n";
    csv << "three_rel_dp," << threeStats.relDp.count << "," << threeStats.relDp.Mean() << "," << threeStats.relDp.RMS() << "\n";
    csv << "three_final_loss," << threeStats.fitHealth.count << "," << threeStats.fitHealth.Mean() << "," << threeStats.fitHealth.RMS() << "\n";
    csv << "three_pdc1_du," << threeStats.pdc1Du.count << "," << threeStats.pdc1Du.Mean() << "," << threeStats.pdc1Du.RMS() << "\n";
    csv << "three_pdc1_dv," << threeStats.pdc1Dv.count << "," << threeStats.pdc1Dv.Mean() << "," << threeStats.pdc1Dv.RMS() << "\n";
    csv << "three_pdc1_dr_median," << threePdc1DrSamples.size() << "," << Median(threePdc1DrSamples) << ",0\n";
    csv << "three_pdc2_du," << threeStats.pdc2Du.count << "," << threeStats.pdc2Du.Mean() << "," << threeStats.pdc2Du.RMS() << "\n";
    csv << "three_pdc2_dv," << threeStats.pdc2Dv.count << "," << threeStats.pdc2Dv.Mean() << "," << threeStats.pdc2Dv.RMS() << "\n";
    csv << "three_pdc2_dr_median," << threePdc2DrSamples.size() << "," << Median(threePdc2DrSamples) << ",0\n";
    for (const auto& [statusCode, count] : rkStats.codeCount) {
        csv << "rk_status_" << SolverStatusToString(static_cast<SolverStatus>(statusCode)) << "," << count << ",0,0\n";
    }
    for (const auto& [fitCode, count] : threeStats.codeCount) {
        csv << "three_fit_status_" << fitCode << "," << count << ",0,0\n";
    }
    csv.close();

    inFile.Close();

    std::cerr << "[diagnose_truth_pdc_reco] Truth proton events: " << truthProtonCount << std::endl;
    std::cerr << "[diagnose_truth_pdc_reco] Truth PDC track events: " << truthPdcTrackCount << std::endl;
    std::cerr << "[diagnose_truth_pdc_reco] RK success/usable: " << rkStats.success << "/" << rkStats.usable
              << " / " << rkStats.attempts << std::endl;
    std::cerr << "[diagnose_truth_pdc_reco] Three success/usable: " << threeStats.success << "/" << threeStats.usable
              << " / " << threeStats.attempts << std::endl;
    std::cerr << "[diagnose_truth_pdc_reco] Output ROOT: " << rootPath << std::endl;
    std::cerr << "[diagnose_truth_pdc_reco] Output CSV : " << csvPath << std::endl;
    std::cerr << "[diagnose_truth_pdc_reco] Output PNG : " << rkOverviewPath << ", "
              << threeOverviewPath << ", " << comparePath << std::endl;

    // [EN] Shutdown the async logger before ROOT starts unloading globals to avoid destruction-order races. / [CN] 在ROOT开始卸载全局对象前先关闭异步日志器，避免析构顺序竞态。
    SMLogger::Logger::Instance().Shutdown();
}
