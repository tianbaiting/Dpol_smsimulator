// [EN] Usage: root -l -q 'scripts/analysis/legacy_target_reco/analyze_synthetic_px_reco.C+("sim.root","geom.mac","field.table",30.0,"out.root","summary.csv",20000,2000,10.0,627.0)' / [CN] 用法: root -l -q 'scripts/analysis/legacy_target_reco/analyze_synthetic_px_reco.C+("sim.root","geom.mac","field.table",30.0,"out.root","summary.csv",20000,2000,10.0,627.0)'

#include "TCanvas.h"
#include "TClonesArray.h"
#include "TFile.h"
#include "TLegend.h"
#include "TROOT.h"
#include "TString.h"
#include "TSystem.h"
#include "TTree.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TStyle.h"
#include "TProfile.h"

#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "NEBULAReconstructor.hh"
#include "PDCSimAna.hh"
#include "RecoEvent.hh"
#include "TBeamSimData.hh"
#include "TSimData.hh"
#include "TargetReconstructor.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct TruthBeamInfo {
    bool hasProton = false;
    bool hasNeutron = false;
    TVector3 protonMomentum = TVector3(0, 0, 0);
    TVector3 neutronMomentum = TVector3(0, 0, 0);
};

struct PdcReference {
    bool hasPDC1 = false;
    bool hasPDC2 = false;
    int stepPDC1 = std::numeric_limits<int>::max();
    int stepPDC2 = std::numeric_limits<int>::max();
    TVector3 pdc1 = TVector3(0, 0, 0);
    TVector3 pdc2 = TVector3(0, 0, 0);
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

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
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
        if (!info.hasProton && (p.fZ == 1 && p.fA == 1 || pname == "proton" || p.fPDGCode == 2212)) {
            TVector3 mom = p.fMomentum.Vect();
            // [EN] Rotate to analysis frame consistent with existing reconstruction flow. / [CN] 旋转到与现有重建一致的分析坐标系。
            mom.RotateY(-targetAngleRad);
            info.protonMomentum = mom;
            info.hasProton = true;
        } else if (!info.hasNeutron && (p.fZ == 0 && p.fA == 1 || pname == "neutron" || p.fPDGCode == 2112)) {
            TVector3 mom = p.fMomentum.Vect();
            mom.RotateY(-targetAngleRad);
            info.neutronMomentum = mom;
            info.hasNeutron = true;
        }
    }
    return info;
}

PdcReference ExtractPdcReference(TClonesArray* fragArray) {
    PdcReference ref;
    if (!fragArray) return ref;

    const int n = fragArray->GetEntriesFast();
    for (int i = 0; i < n; ++i) {
        auto* hit = dynamic_cast<TSimData*>(fragArray->At(i));
        if (!hit || !IsPdcHit(hit)) continue;

        const int id = hit->fID;
        const int step = hit->fStepNo;
        if (id == 0 && step < ref.stepPDC1) {
            ref.stepPDC1 = step;
            ref.pdc1 = hit->fPrePosition;
            ref.hasPDC1 = true;
        } else if (id == 1 && step < ref.stepPDC2) {
            ref.stepPDC2 = step;
            ref.pdc2 = hit->fPrePosition;
            ref.hasPDC2 = true;
        }
    }
    return ref;
}

const RecoNeutron* SelectBestNeutron(const std::vector<RecoNeutron>& neutrons) {
    if (neutrons.empty()) return nullptr;
    return &(*std::max_element(neutrons.begin(), neutrons.end(), [](const RecoNeutron& a, const RecoNeutron& b) {
        return a.energy < b.energy;
    }));
}

void SaveCanvas(TCanvas* c, const TString& path) {
    if (!c) return;
    c->SaveAs(path);
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

bool LoadGeometryRecursive(GeometryManager& geo,
                           const std::filesystem::path& macroPath,
                           std::set<std::string>& visited) {
    const std::string key = macroPath.lexically_normal().string();
    if (visited.count(key)) {
        return true;
    }

    std::ifstream fin(macroPath);
    if (!fin.is_open()) {
        std::cerr << "[analyze_synthetic_px_reco] Cannot open geometry macro: " << macroPath << std::endl;
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
                std::cerr << "[analyze_synthetic_px_reco] Warning: cannot resolve include macro: "
                          << includePathRaw << " from " << macroPath << std::endl;
                continue;
            }
            if (!LoadGeometryRecursive(geo, includePath, visited)) {
                return false;
            }
        }
    }

    if (!geo.LoadGeometry(macroPath.string())) {
        return false;
    }
    return true;
}

bool ParsePdcAngleFromLine(const std::string& cleanLine, double* pdcAngleDeg) {
    if (!pdcAngleDeg) return false;
    if (cleanLine.rfind("/samurai/geometry/PDC/Angle", 0) != 0) return false;

    std::stringstream ss(cleanLine);
    std::string cmd;
    double angleDeg = 0.0;
    ss >> cmd >> angleDeg;
    if (!ss.fail()) {
        *pdcAngleDeg = angleDeg;
        return true;
    }
    return false;
}

bool FindPdcAngleRecursive(const std::filesystem::path& macroPath,
                           std::set<std::string>& visited,
                           double* pdcAngleDeg,
                           bool* found) {
    if (!pdcAngleDeg || !found) return false;
    const std::string key = macroPath.lexically_normal().string();
    if (visited.count(key)) return true;

    std::ifstream fin(macroPath);
    if (!fin.is_open()) return false;

    visited.insert(key);

    std::string line;
    while (std::getline(fin, line)) {
        std::string clean = Trim(line);
        if (clean.empty()) continue;
        const std::size_t commentPos = clean.find('#');
        if (commentPos == 0) continue;
        if (commentPos != std::string::npos) {
            clean = Trim(clean.substr(0, commentPos));
            if (clean.empty()) continue;
        }

        if (ParsePdcAngleFromLine(clean, pdcAngleDeg)) {
            *found = true;
        }

        if (clean.rfind("/control/execute", 0) == 0) {
            std::stringstream ss(clean);
            std::string cmd;
            std::string includePathRaw;
            ss >> cmd >> includePathRaw;
            if (includePathRaw.empty()) continue;

            std::filesystem::path includePath;
            if (!ResolveMacroPath(includePathRaw, macroPath.parent_path(), &includePath)) {
                continue;
            }
            if (!FindPdcAngleRecursive(includePath, visited, pdcAngleDeg, found)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

void analyze_synthetic_px_reco(const char* sim_root,
                               const char* geom_macro,
                               const char* field_table,
                               double field_rotation_deg,
                               const char* out_root,
                               const char* out_csv,
                               int max_events = -1,
                               int max_proton_reco_events = 2000,
                               double rk_step_mm = 10.0,
                               double threepoint_fixed_pmag = 627.0) {
    gSystem->Load("libsmdata.so");
    gSystem->Load("libpdcanalysis.so");

    GeometryManager geo;
    std::set<std::string> visitedMacros;
    std::filesystem::path geomPath;
    if (!ResolveMacroPath(geom_macro, std::filesystem::current_path(), &geomPath)) {
        std::cerr << "Failed to resolve geometry macro path: " << geom_macro << std::endl;
        return;
    }
    if (!LoadGeometryRecursive(geo, geomPath, visitedMacros)) {
        std::cerr << "Failed to load geometry macro: " << geom_macro << std::endl;
        return;
    }

    // [EN] Read the effective PDC angle from macro chain for incidence-angle diagnostics.
    // [CN] 从宏链读取生效的PDC角度，用于入射角诊断。
    double pdcAngleSamuraiDeg = 65.0;
    bool hasPdcAngleFromMacro = false;
    std::set<std::string> visitedPdcAngle;
    if (!FindPdcAngleRecursive(geomPath, visitedPdcAngle, &pdcAngleSamuraiDeg, &hasPdcAngleFromMacro)) {
        std::cerr << "[analyze_synthetic_px_reco] Warning: failed to parse PDC angle from macro chain, fallback to 65 deg." << std::endl;
    }
    const double pdcAngleLabDeg = -pdcAngleSamuraiDeg;
    const double pdcAngleLabRad = pdcAngleLabDeg * TMath::DegToRad();
    const TVector3 pdcNormalLab(std::sin(pdcAngleLabRad), 0.0, std::cos(pdcAngleLabRad));

    const TVector3 targetPos = geo.GetTargetPosition();
    const double targetAngleRad = geo.GetTargetAngleRad();

    MagneticField magField;
    if (!magField.LoadFieldMap(field_table)) {
        std::cerr << "Failed to load field table: " << field_table << std::endl;
        return;
    }
    magField.SetRotationAngle(field_rotation_deg);

    PDCSimAna pdcAna(geo);
    pdcAna.SetSmearing(0.5, 0.5);

    NEBULAReconstructor nebulaRecon(geo);
    nebulaRecon.SetTargetPosition(targetPos);
    nebulaRecon.SetTimeWindow(10.0);
    nebulaRecon.SetEnergyThreshold(1.0);

    TargetReconstructor reconMinuit(&magField);
    reconMinuit.SetTrajectoryStepSize(rk_step_mm);
    TargetReconstructor reconThree(&magField);
    reconThree.SetTrajectoryStepSize(rk_step_mm);

    TFile inFile(sim_root, "READ");
    if (inFile.IsZombie()) {
        std::cerr << "Failed to open sim root: " << sim_root << std::endl;
        return;
    }

    TTree* tree = dynamic_cast<TTree*>(inFile.Get("tree"));
    if (!tree) {
        std::cerr << "No tree named 'tree' found in " << sim_root << std::endl;
        return;
    }

    TClonesArray* fragArray = nullptr;
    TClonesArray* nebulaArray = nullptr;
    std::vector<TBeamSimData>* beam = nullptr;

    tree->SetBranchAddress("FragSimData", &fragArray);
    if (tree->GetBranch("NEBULAPla")) tree->SetBranchAddress("NEBULAPla", &nebulaArray);
    if (tree->GetBranch("beam")) tree->SetBranchAddress("beam", &beam);

    TFile outFile(out_root, "RECREATE");
    TTree metrics("metrics", "Event-by-event synthetic reconstruction metrics");

    Long64_t eventId = -1;
    bool hasTruthP = false, hasTruthN = false;
    double truePxP = 0.0, truePyP = 0.0, truePzP = 0.0;
    double truePxN = 0.0, truePyN = 0.0, truePzN = 0.0;
    bool hasRecoTrack = false;
    double pdcD1 = -1.0, pdcD2 = -1.0;

    bool minuitDone = false, minuitSuccess = false;
    bool minuitUsable = false;
    double recoPxMinuit = 0.0, dPxMinuit = 0.0;

    bool threeDone = false, threeSuccess = false;
    bool threeUsable = false;
    double recoPxThree = 0.0, dPxThree = 0.0;

    bool neutronDone = false;
    double recoPxNeutron = 0.0, dPxNeutron = 0.0;

    metrics.Branch("event_id", &eventId, "event_id/L");
    metrics.Branch("has_truth_p", &hasTruthP, "has_truth_p/O");
    metrics.Branch("has_truth_n", &hasTruthN, "has_truth_n/O");
    metrics.Branch("true_px_p", &truePxP, "true_px_p/D");
    metrics.Branch("true_py_p", &truePyP, "true_py_p/D");
    metrics.Branch("true_pz_p", &truePzP, "true_pz_p/D");
    metrics.Branch("true_px_n", &truePxN, "true_px_n/D");
    metrics.Branch("true_py_n", &truePyN, "true_py_n/D");
    metrics.Branch("true_pz_n", &truePzN, "true_pz_n/D");
    metrics.Branch("has_reco_track", &hasRecoTrack, "has_reco_track/O");
    metrics.Branch("pdc_d1", &pdcD1, "pdc_d1/D");
    metrics.Branch("pdc_d2", &pdcD2, "pdc_d2/D");
    metrics.Branch("minuit_done", &minuitDone, "minuit_done/O");
    metrics.Branch("minuit_success", &minuitSuccess, "minuit_success/O");
    metrics.Branch("minuit_usable", &minuitUsable, "minuit_usable/O");
    metrics.Branch("reco_px_minuit", &recoPxMinuit, "reco_px_minuit/D");
    metrics.Branch("dpx_minuit", &dPxMinuit, "dpx_minuit/D");
    metrics.Branch("three_done", &threeDone, "three_done/O");
    metrics.Branch("three_success", &threeSuccess, "three_success/O");
    metrics.Branch("three_usable", &threeUsable, "three_usable/O");
    metrics.Branch("reco_px_three", &recoPxThree, "reco_px_three/D");
    metrics.Branch("dpx_three", &dPxThree, "dpx_three/D");
    metrics.Branch("neutron_done", &neutronDone, "neutron_done/O");
    metrics.Branch("reco_px_neutron", &recoPxNeutron, "reco_px_neutron/D");
    metrics.Branch("dpx_neutron", &dPxNeutron, "dpx_neutron/D");

    TH1D* hPdcD1 = new TH1D("h_pdc_d1", "PDC1 position residual;#Delta r [mm];Events", 200, 0, 4000);
    TH1D* hPdcD2 = new TH1D("h_pdc_d2", "PDC2 position residual;#Delta r [mm];Events", 200, 0, 4000);
    TH2D* hPdcD1VsTruePx = new TH2D("h_pdc_d1_vs_true_px",
                                     "PDC1 residual vs proton true P_{x};P_{x}^{true} [MeV/c];#Delta r_{PDC1} [mm]",
                                     30, -150, 150, 160, 0, 4000);
    TH2D* hPdcD2VsTruePx = new TH2D("h_pdc_d2_vs_true_px",
                                     "PDC2 residual vs proton true P_{x};P_{x}^{true} [MeV/c];#Delta r_{PDC2} [mm]",
                                     30, -150, 150, 160, 0, 4000);
    TH1D* hPdcIncidence = new TH1D("h_pdc_incidence_angle",
                                   "PDC incidence angle (track vs plane normal);#theta_{inc} [deg];Events",
                                   120, 0, 60);
    TH2D* hPdcIncidenceVsTruePx = new TH2D("h_pdc_incidence_vs_true_px",
                                           "PDC incidence angle vs proton true P_{x};P_{x}^{true} [MeV/c];#theta_{inc} [deg]",
                                           30, -150, 150, 120, 0, 60);
    TH1D* hTruePxPAll = new TH1D("h_true_px_p_all",
                                 "Proton true P_{x} (uniform input);P_{x}^{true} [MeV/c];Events",
                                 30, -150, 150);
    TH1D* hTruePxPRecoTrack = new TH1D("h_true_px_p_recotrack",
                                       "Proton true P_{x} with valid PDC track;P_{x}^{true} [MeV/c];Events",
                                       30, -150, 150);

    TH1D* hDpxMinuit = new TH1D("h_dpx_minuit", "Proton #DeltaP_{x} (TMinuit);#DeltaP_{x} [MeV/c];Events", 160, -320, 320);
    TH1D* hDpxThree = new TH1D("h_dpx_three", "Proton #DeltaP_{x} (Three-point);#DeltaP_{x} [MeV/c];Events", 160, -320, 320);
    TH2D* hPxTrueRecoMinuit = new TH2D("h_px_true_reco_minuit", "Proton P_{x}^{true} vs P_{x}^{reco} (TMinuit);P_{x}^{true} [MeV/c];P_{x}^{reco} [MeV/c]", 120, -180, 180, 120, -350, 350);
    TH2D* hPxTrueRecoThree = new TH2D("h_px_true_reco_three", "Proton P_{x}^{true} vs P_{x}^{reco} (Three-point);P_{x}^{true} [MeV/c];P_{x}^{reco} [MeV/c]", 120, -180, 180, 120, -350, 350);
    TH2D* hDpxVsTrueMinuit = new TH2D("h_dpx_vs_true_minuit", "Proton #DeltaP_{x} vs P_{x}^{true} (TMinuit);P_{x}^{true} [MeV/c];#DeltaP_{x} [MeV/c]", 120, -180, 180, 120, -320, 320);
    TH2D* hDpxVsTrueThree = new TH2D("h_dpx_vs_true_three", "Proton #DeltaP_{x} vs P_{x}^{true} (Three-point);P_{x}^{true} [MeV/c];#DeltaP_{x} [MeV/c]", 120, -180, 180, 120, -320, 320);

    TH1D* hDpxNeutron = new TH1D("h_dpx_neutron", "Neutron #DeltaP_{x} (NEBULA TOF);#DeltaP_{x} [MeV/c];Events", 200, -500, 500);
    TH2D* hPxTrueRecoNeutron = new TH2D("h_px_true_reco_neutron", "Neutron P_{x}^{true} vs P_{x}^{reco};P_{x}^{true} [MeV/c];P_{x}^{reco} [MeV/c]", 120, -180, 180, 120, -500, 500);
    TH2D* hDpxVsTrueNeutron = new TH2D("h_dpx_vs_true_neutron", "Neutron #DeltaP_{x} vs P_{x}^{true};P_{x}^{true} [MeV/c];#DeltaP_{x} [MeV/c]", 120, -180, 180, 160, -500, 500);

    const Long64_t nEntries = tree->GetEntries();
    const Long64_t nToProcess = (max_events > 0 && nEntries > max_events) ? max_events : nEntries;
    int protonRecoCounter = 0;
    Long64_t truthPCount = 0;
    Long64_t truthNCount = 0;
    Long64_t recoTrackCount = 0;
    Long64_t truthPAndRecoTrackCount = 0;
    Long64_t truthNAndRecoNeutronCount = 0;
    Long64_t minuitDoneCount = 0;
    Long64_t minuitSuccessCount = 0;
    Long64_t minuitUsableCount = 0;
    Long64_t threeDoneCount = 0;
    Long64_t threeSuccessCount = 0;
    Long64_t threeUsableCount = 0;

    double sumPdcD1 = 0.0;
    double sumPdcD1Sq = 0.0;
    double sumPdcD2 = 0.0;
    double sumPdcD2Sq = 0.0;
    Long64_t pdcResidualCount = 0;

    std::cerr << "[analyze_synthetic_px_reco] Processing events: " << nToProcess << std::endl;

    for (Long64_t i = 0; i < nToProcess; ++i) {
        if (i % 1000 == 0 && i > 0) {
            std::cerr << "[analyze_synthetic_px_reco] Event " << i << "/" << nToProcess << std::endl;
        }
        tree->GetEntry(i);
        eventId = i;

        hasTruthP = false;
        hasTruthN = false;
        truePxP = truePyP = truePzP = 0.0;
        truePxN = truePyN = truePzN = 0.0;
        hasRecoTrack = false;
        pdcD1 = pdcD2 = -1.0;
        minuitDone = minuitSuccess = false;
        minuitUsable = false;
        recoPxMinuit = dPxMinuit = 0.0;
        threeDone = threeSuccess = false;
        threeUsable = false;
        recoPxThree = dPxThree = 0.0;
        neutronDone = false;
        recoPxNeutron = dPxNeutron = 0.0;

        const TruthBeamInfo truth = ExtractTruthBeam(beam, targetAngleRad);
        hasTruthP = truth.hasProton;
        hasTruthN = truth.hasNeutron;
        if (hasTruthP) {
            truePxP = truth.protonMomentum.X();
            truePyP = truth.protonMomentum.Y();
            truePzP = truth.protonMomentum.Z();
            ++truthPCount;
            hTruePxPAll->Fill(truePxP);
        }
        if (hasTruthN) {
            truePxN = truth.neutronMomentum.X();
            truePyN = truth.neutronMomentum.Y();
            truePzN = truth.neutronMomentum.Z();
            ++truthNCount;
        }

        RecoEvent pdcEvent = pdcAna.ProcessEvent(fragArray);
        if (!pdcEvent.tracks.empty()) {
            const PdcReference pdcRef = ExtractPdcReference(fragArray);
            const bool hasPdcReference = pdcRef.hasPDC1 && pdcRef.hasPDC2;
            hasRecoTrack = true;
            ++recoTrackCount;
            const RecoTrack& track = pdcEvent.tracks.front();
            if (hasPdcReference) {
                pdcD1 = (track.start - pdcRef.pdc1).Mag();
                pdcD2 = (track.end - pdcRef.pdc2).Mag();
                hPdcD1->Fill(pdcD1);
                hPdcD2->Fill(pdcD2);
                sumPdcD1 += pdcD1;
                sumPdcD1Sq += pdcD1 * pdcD1;
                sumPdcD2 += pdcD2;
                sumPdcD2Sq += pdcD2 * pdcD2;
                ++pdcResidualCount;
            }
            if (hasTruthP) {
                ++truthPAndRecoTrackCount;
                hTruePxPRecoTrack->Fill(truePxP);
                if (hasPdcReference) {
                    hPdcD1VsTruePx->Fill(truePxP, pdcD1);
                    hPdcD2VsTruePx->Fill(truePxP, pdcD2);
                }
            }

            // [EN] Use reconstructed PDC segment direction as incidence-direction proxy.
            // [CN] 使用重建的PDC段方向作为入射方向近似。
            TVector3 trackDir = track.end - track.start;
            if (trackDir.Mag2() > 1e-9) {
                trackDir = trackDir.Unit();
                double cosTheta = std::abs(trackDir.Dot(pdcNormalLab));
                cosTheta = std::max(-1.0, std::min(1.0, cosTheta));
                const double incidenceDeg = std::acos(cosTheta) * TMath::RadToDeg();
                hPdcIncidence->Fill(incidenceDeg);
                if (hasTruthP) {
                    hPdcIncidenceVsTruePx->Fill(truePxP, incidenceDeg);
                }
            }

            if (hasTruthP && protonRecoCounter < max_proton_reco_events) {
                ++protonRecoCounter;
                CoutSilencer silencer;

                TargetReconstructionResult minuitResult = reconMinuit.ReconstructAtTargetMinuit(
                    track, targetPos, false, 627.0, 5.0, 1000, false
                );
                minuitDone = true;
                minuitSuccess = minuitResult.success;
                ++minuitDoneCount;
                if (minuitSuccess) ++minuitSuccessCount;
                recoPxMinuit = minuitResult.bestMomentum.Px();
                dPxMinuit = recoPxMinuit - truePxP;
                minuitUsable = std::isfinite(minuitResult.bestMomentum.Px()) &&
                               std::isfinite(minuitResult.bestMomentum.Py()) &&
                               std::isfinite(minuitResult.bestMomentum.Pz()) &&
                               (minuitResult.bestMomentum.Vect().Mag() > 0.0);
                if (minuitUsable) ++minuitUsableCount;
                if (minuitUsable) {
                    hDpxMinuit->Fill(dPxMinuit);
                    hPxTrueRecoMinuit->Fill(truePxP, recoPxMinuit);
                    hDpxVsTrueMinuit->Fill(truePxP, dPxMinuit);
                }

                // [EN] Use fixed z-axis initial guess and let optimizer update full momentum vector. / [CN] 使用固定z轴初值，再由优化器更新完整动量向量。
                const TVector3 fixedMomentum(0.0, 0.0, threepoint_fixed_pmag);
                TargetReconstructionResult threeResult = reconThree.ReconstructAtTargetThreePointFreeMinuit(
                    track, targetPos, false, targetPos, fixedMomentum, 5.0, 0.5, 1.0, 500, false
                );
                threeDone = true;
                threeSuccess = threeResult.success;
                ++threeDoneCount;
                if (threeSuccess) ++threeSuccessCount;
                recoPxThree = threeResult.bestMomentum.Px();
                dPxThree = recoPxThree - truePxP;
                threeUsable = std::isfinite(threeResult.bestMomentum.Px()) &&
                              std::isfinite(threeResult.bestMomentum.Py()) &&
                              std::isfinite(threeResult.bestMomentum.Pz()) &&
                              (threeResult.bestMomentum.Vect().Mag() > 0.0);
                if (threeUsable) ++threeUsableCount;
                if (threeUsable) {
                    hDpxThree->Fill(dPxThree);
                    hPxTrueRecoThree->Fill(truePxP, recoPxThree);
                    hDpxVsTrueThree->Fill(truePxP, dPxThree);
                }
            }
        }

        if (hasTruthN && nebulaArray) {
            std::vector<RecoNeutron> neutrons = nebulaRecon.ReconstructNeutrons(nebulaArray);
            const RecoNeutron* best = SelectBestNeutron(neutrons);
            if (best) {
                neutronDone = true;
                ++truthNAndRecoNeutronCount;
                const TVector3 pRecoN = best->GetMomentum();
                recoPxNeutron = pRecoN.X();
                dPxNeutron = recoPxNeutron - truePxN;
                hDpxNeutron->Fill(dPxNeutron);
                hPxTrueRecoNeutron->Fill(truePxN, recoPxNeutron);
                hDpxVsTrueNeutron->Fill(truePxN, dPxNeutron);
            }
        }

        metrics.Fill();
    }

    const double pdcD1Mean = (pdcResidualCount > 0) ? (sumPdcD1 / static_cast<double>(pdcResidualCount)) : 0.0;
    const double pdcD2Mean = (pdcResidualCount > 0) ? (sumPdcD2 / static_cast<double>(pdcResidualCount)) : 0.0;
    const double pdcD1Rms = (pdcResidualCount > 0)
        ? std::sqrt(std::max(0.0, sumPdcD1Sq / static_cast<double>(pdcResidualCount) - pdcD1Mean * pdcD1Mean))
        : 0.0;
    const double pdcD2Rms = (pdcResidualCount > 0)
        ? std::sqrt(std::max(0.0, sumPdcD2Sq / static_cast<double>(pdcResidualCount) - pdcD2Mean * pdcD2Mean))
        : 0.0;

    TH1D* hPdcTrackEffVsTruePx = dynamic_cast<TH1D*>(hTruePxPRecoTrack->Clone("h_pdc_track_eff_vs_true_px"));
    hPdcTrackEffVsTruePx->SetTitle("PDC track reconstruction efficiency vs proton true P_{x};P_{x}^{true} [MeV/c];Efficiency");
    hPdcTrackEffVsTruePx->Reset("ICES");
    hPdcTrackEffVsTruePx->Divide(hTruePxPRecoTrack, hTruePxPAll, 1.0, 1.0, "B");

    // [EN] Quantify Px-uniformity and edge acceptance over |Px|>=130 MeV/c.
    // [CN] 定量评估Px方向均匀性与 |Px|>=130 MeV/c 边缘接受率。
    double effMeanVsPx = 0.0;
    double effSqMeanVsPx = 0.0;
    int effBinCount = 0;
    double edgeMinEffAbs130 = 1.0;
    double edgeMeanEffAbs130 = 0.0;
    int edgeBinCountAbs130 = 0;

    for (int b = 1; b <= hPdcTrackEffVsTruePx->GetNbinsX(); ++b) {
        const double den = hTruePxPAll->GetBinContent(b);
        if (den <= 0.0) continue;
        const double eff = hPdcTrackEffVsTruePx->GetBinContent(b);
        effMeanVsPx += eff;
        effSqMeanVsPx += eff * eff;
        ++effBinCount;

        const double pxCenter = hPdcTrackEffVsTruePx->GetBinCenter(b);
        if (std::abs(pxCenter) >= 130.0) {
            edgeMinEffAbs130 = std::min(edgeMinEffAbs130, eff);
            edgeMeanEffAbs130 += eff;
            ++edgeBinCountAbs130;
        }
    }

    if (effBinCount > 0) {
        effMeanVsPx /= static_cast<double>(effBinCount);
        effSqMeanVsPx /= static_cast<double>(effBinCount);
    } else {
        effMeanVsPx = 0.0;
        effSqMeanVsPx = 0.0;
    }
    const double effStdVsPx = std::sqrt(std::max(0.0, effSqMeanVsPx - effMeanVsPx * effMeanVsPx));
    const double effCvVsPx = (effMeanVsPx > 1e-12) ? (effStdVsPx / effMeanVsPx) : 0.0;
    if (edgeBinCountAbs130 > 0) {
        edgeMeanEffAbs130 /= static_cast<double>(edgeBinCountAbs130);
    } else {
        edgeMinEffAbs130 = 0.0;
        edgeMeanEffAbs130 = 0.0;
    }

    TString outRoot = out_root;
    TString outDir = gSystem->DirName(outRoot);
    TString plotDir = outDir + "/plots";
    gSystem->mkdir(plotDir, true);

    gStyle->SetOptStat(1110);

    TCanvas* cPdc = new TCanvas("cPdc", "PDC precision", 1400, 600);
    cPdc->Divide(2, 1);
    cPdc->cd(1); hPdcD1->Draw();
    cPdc->cd(2); hPdcD2->Draw();
    SaveCanvas(cPdc, plotDir + "/pdc_position_residuals.png");

    TCanvas* cPdcUniform = new TCanvas("cPdcUniform", "PDC uniform incidence check", 1400, 600);
    cPdcUniform->Divide(2, 1);
    cPdcUniform->cd(1);
    hTruePxPAll->SetLineColor(kBlack);
    hTruePxPAll->SetLineWidth(2);
    hTruePxPRecoTrack->SetLineColor(kBlue + 1);
    hTruePxPRecoTrack->SetLineWidth(2);
    hTruePxPAll->Draw("hist");
    hTruePxPRecoTrack->Draw("hist same");
    TLegend* lgPdcUniform = new TLegend(0.54, 0.72, 0.89, 0.89);
    lgPdcUniform->AddEntry(hTruePxPAll, "Proton truth (all)", "l");
    lgPdcUniform->AddEntry(hTruePxPRecoTrack, "Proton truth (with PDC track)", "l");
    lgPdcUniform->Draw();
    cPdcUniform->cd(2);
    hPdcTrackEffVsTruePx->SetMinimum(0.0);
    hPdcTrackEffVsTruePx->SetMaximum(1.05);
    hPdcTrackEffVsTruePx->SetLineColor(kBlue + 1);
    hPdcTrackEffVsTruePx->SetLineWidth(2);
    hPdcTrackEffVsTruePx->Draw("hist");
    SaveCanvas(cPdcUniform, plotDir + "/pdc_uniform_track_efficiency.png");

    TCanvas* cPdcIncidence = new TCanvas("cPdcIncidence", "PDC incidence", 1400, 600);
    cPdcIncidence->Divide(2, 1);
    cPdcIncidence->cd(1);
    hPdcIncidence->SetLineColor(kMagenta + 1);
    hPdcIncidence->SetLineWidth(2);
    hPdcIncidence->Draw("hist");
    cPdcIncidence->cd(2);
    hPdcIncidenceVsTruePx->Draw("colz");
    SaveCanvas(cPdcIncidence, plotDir + "/pdc_incidence_angle.png");

    gSystem->Unlink((plotDir + "/pdc_residual_vs_true_px.png").Data());

    TCanvas* cProtonHist = new TCanvas("cProtonHist", "Proton dPx", 900, 700);
    hDpxMinuit->SetLineColor(kBlue + 1);
    hDpxMinuit->SetLineWidth(2);
    hDpxThree->SetLineColor(kRed + 1);
    hDpxThree->SetLineWidth(2);
    hDpxMinuit->Draw("hist");
    hDpxThree->Draw("hist same");
    TLegend* lg = new TLegend(0.60, 0.72, 0.89, 0.89);
    lg->AddEntry(hDpxMinuit, "Proton #DeltaP_{x} TMinuit", "l");
    lg->AddEntry(hDpxThree, "Proton #DeltaP_{x} Three-point", "l");
    lg->Draw();
    SaveCanvas(cProtonHist, plotDir + "/proton_dpx_comparison.png");

    TCanvas* cProtonScatter = new TCanvas("cProtonScatter", "Proton true vs reco Px", 1400, 600);
    cProtonScatter->Divide(2, 1);
    cProtonScatter->cd(1); hPxTrueRecoMinuit->Draw("colz");
    cProtonScatter->cd(2); hPxTrueRecoThree->Draw("colz");
    SaveCanvas(cProtonScatter, plotDir + "/proton_px_true_vs_reco.png");

    TCanvas* cProtonBias = new TCanvas("cProtonBias", "Proton dPx vs true", 1400, 600);
    cProtonBias->Divide(2, 1);
    cProtonBias->cd(1); hDpxVsTrueMinuit->Draw("colz");
    cProtonBias->cd(2); hDpxVsTrueThree->Draw("colz");
    SaveCanvas(cProtonBias, plotDir + "/proton_dpx_vs_true.png");

    TCanvas* cNeutron = new TCanvas("cNeutron", "Neutron dPx", 900, 700);
    hDpxNeutron->SetLineColor(kGreen + 2);
    hDpxNeutron->SetLineWidth(2);
    hDpxNeutron->Draw("hist");
    SaveCanvas(cNeutron, plotDir + "/neutron_dpx.png");

    TCanvas* cNeutronScatter = new TCanvas("cNeutronScatter", "Neutron Px", 1400, 600);
    cNeutronScatter->Divide(2, 1);
    cNeutronScatter->cd(1); hPxTrueRecoNeutron->Draw("colz");
    cNeutronScatter->cd(2); hDpxVsTrueNeutron->Draw("colz");
    SaveCanvas(cNeutronScatter, plotDir + "/neutron_px_true_vs_reco_and_dpx_vs_true.png");

    outFile.cd();
    metrics.Write();
    hPdcD1->Write(); hPdcD2->Write();
    hPdcD1VsTruePx->Write(); hPdcD2VsTruePx->Write();
    hPdcIncidence->Write(); hPdcIncidenceVsTruePx->Write();
    hTruePxPAll->Write(); hTruePxPRecoTrack->Write(); hPdcTrackEffVsTruePx->Write();
    hDpxMinuit->Write(); hDpxThree->Write();
    hPxTrueRecoMinuit->Write(); hPxTrueRecoThree->Write();
    hDpxVsTrueMinuit->Write(); hDpxVsTrueThree->Write();
    hDpxNeutron->Write(); hPxTrueRecoNeutron->Write(); hDpxVsTrueNeutron->Write();
    outFile.Write();
    outFile.Close();

    std::ofstream csv(out_csv);
    csv << "metric,count,mean,rms\n";
    const double pdcTrackEff = (truthPCount > 0) ? (static_cast<double>(truthPAndRecoTrackCount) / static_cast<double>(truthPCount)) : 0.0;
    const double neutronRecoEff = (truthNCount > 0) ? (static_cast<double>(truthNAndRecoNeutronCount) / static_cast<double>(truthNCount)) : 0.0;
    csv << "truth_proton_events," << truthPCount << ",0,0\n";
    csv << "truth_neutron_events," << truthNCount << ",0,0\n";
    csv << "pdc_track_events," << recoTrackCount << ",0,0\n";
    csv << "pdc_track_eff_vs_truth_proton," << truthPCount << "," << pdcTrackEff << ",0\n";
    csv << "pdc_eff_mean_vs_px," << effBinCount << "," << effMeanVsPx << "," << effStdVsPx << "\n";
    csv << "pdc_eff_cv_vs_px," << effBinCount << "," << effCvVsPx << ",0\n";
    csv << "pdc_eff_edge_min_abs130," << edgeBinCountAbs130 << "," << edgeMinEffAbs130 << ",0\n";
    csv << "pdc_eff_edge_mean_abs130," << edgeBinCountAbs130 << "," << edgeMeanEffAbs130 << ",0\n";
    csv << "pdc_incidence_angle_deg," << hPdcIncidence->GetEntries() << "," << hPdcIncidence->GetMean() << "," << hPdcIncidence->GetRMS() << "\n";
    csv << "pdc_angle_samurai_deg,1," << pdcAngleSamuraiDeg << ",0\n";
    csv << "pdc_angle_lab_deg,1," << pdcAngleLabDeg << ",0\n";
    csv << "pdc_angle_from_macro,1," << (hasPdcAngleFromMacro ? 1.0 : 0.0) << ",0\n";
    csv << "nebula_reco_eff_vs_truth_neutron," << truthNCount << "," << neutronRecoEff << ",0\n";
    csv << "pdc_d1," << pdcResidualCount << "," << pdcD1Mean << "," << pdcD1Rms << "\n";
    csv << "pdc_d2," << pdcResidualCount << "," << pdcD2Mean << "," << pdcD2Rms << "\n";
    csv << "proton_minuit_done," << minuitDoneCount << ",0,0\n";
    csv << "proton_minuit_success," << minuitDoneCount << ","
        << (minuitDoneCount > 0 ? static_cast<double>(minuitSuccessCount) / static_cast<double>(minuitDoneCount) : 0.0)
        << ",0\n";
    csv << "proton_minuit_usable," << minuitDoneCount << ","
        << (minuitDoneCount > 0 ? static_cast<double>(minuitUsableCount) / static_cast<double>(minuitDoneCount) : 0.0)
        << ",0\n";
    csv << "proton_three_done," << threeDoneCount << ",0,0\n";
    csv << "proton_three_success," << threeDoneCount << ","
        << (threeDoneCount > 0 ? static_cast<double>(threeSuccessCount) / static_cast<double>(threeDoneCount) : 0.0)
        << ",0\n";
    csv << "proton_three_usable," << threeDoneCount << ","
        << (threeDoneCount > 0 ? static_cast<double>(threeUsableCount) / static_cast<double>(threeDoneCount) : 0.0)
        << ",0\n";
    csv << "proton_dpx_minuit," << hDpxMinuit->GetEntries() << "," << hDpxMinuit->GetMean() << "," << hDpxMinuit->GetRMS() << "\n";
    csv << "proton_dpx_threepoint," << hDpxThree->GetEntries() << "," << hDpxThree->GetMean() << "," << hDpxThree->GetRMS() << "\n";
    csv << "neutron_dpx_tof," << hDpxNeutron->GetEntries() << "," << hDpxNeutron->GetMean() << "," << hDpxNeutron->GetRMS() << "\n";
    csv.close();

    std::cerr << "[analyze_synthetic_px_reco] PDC track efficiency (truth proton): "
              << truthPAndRecoTrackCount << "/" << truthPCount
              << " = " << pdcTrackEff << std::endl;
    std::cerr << "[analyze_synthetic_px_reco] NEBULA neutron efficiency (truth neutron): "
              << truthNAndRecoNeutronCount << "/" << truthNCount
              << " = " << neutronRecoEff << std::endl;
    std::cerr << "[analyze_synthetic_px_reco] PDC residual means: D1=" << pdcD1Mean
              << " mm, D2=" << pdcD2Mean << " mm" << std::endl;
    std::cerr << "[analyze_synthetic_px_reco] PDC edge efficiency |Px|>=130: min="
              << edgeMinEffAbs130 << ", mean=" << edgeMeanEffAbs130
              << ", CV=" << effCvVsPx << std::endl;
    std::cerr << "[analyze_synthetic_px_reco] PDC incidence angle: mean="
              << hPdcIncidence->GetMean() << " deg, rms=" << hPdcIncidence->GetRMS()
              << " deg" << std::endl;
    std::cerr << "[analyze_synthetic_px_reco] Proton Minuit usable: "
              << minuitUsableCount << "/" << minuitDoneCount << std::endl;
    std::cerr << "[analyze_synthetic_px_reco] Proton Three-point usable: "
              << threeUsableCount << "/" << threeDoneCount << std::endl;

    std::cerr << "[analyze_synthetic_px_reco] Done." << std::endl;
    std::cerr << "  Output ROOT: " << out_root << std::endl;
    std::cerr << "  Output CSV : " << out_csv << std::endl;
    std::cerr << "  Plots dir  : " << plotDir << std::endl;
}
