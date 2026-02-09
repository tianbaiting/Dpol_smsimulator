// [EN] Usage: root -l -q 'scripts/visualization/dump_reco_track.C("input.root", 0, "threepoint", "geom.mac", "out.csv", 50, 5000, 1000, 0.05, 1.0, 200, 0.5, 5.0, 0, 0, 627)' / [CN] 用法: root -l -q 'scripts/visualization/dump_reco_track.C("input.root", 0, "threepoint", "geom.mac", "out.csv", 50, 5000, 1000, 0.05, 1.0, 200, 0.5, 5.0, 0, 0, 627)'
#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TSystem.h"
#include "TString.h"
#include "TVector3.h"
#include "TSimData.hh"
#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "TargetReconstructor.hh"
#include "RecoEvent.hh"
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {

std::string ToLower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(c));
    return out;
}

bool IsPdcName(const std::string& det, const std::string& mod) {
    std::string d = ToLower(det);
    std::string m = ToLower(mod);
    return (d.find("pdc") != std::string::npos) || (m.find("pdc") != std::string::npos);
}

std::string SelectPdcKey(const std::string& det, const std::string& mod) {
    std::string d = ToLower(det);
    std::string m = ToLower(mod);
    if (m.find("pdc1") != std::string::npos || d.find("pdc1") != std::string::npos) return "PDC1";
    if (m.find("pdc2") != std::string::npos || d.find("pdc2") != std::string::npos) return "PDC2";
    return "PDC";
}

} // namespace

void dump_reco_track(const char* input_root,
                     int event_id,
                     const char* method,
                     const char* geom_macro,
                     const char* out_csv,
                     double pMin,
                     double pMax,
                     double pInit,
                     double learningRate,
                     double tol,
                     int maxIterations,
                     double pdcSigma,
                     double targetSigma,
                     double fx,
                     double fy,
                     double fz) {
    // [EN] Load required shared libraries. / [CN] 加载必需的共享库。
    gSystem->Load("libsmdata.so");
    gSystem->Load("libpdcanalysis.so");

    GeometryManager geo;
    if (!geo.LoadGeometry(geom_macro)) {
        std::cerr << "Failed to load geometry: " << geom_macro << std::endl;
        return;
    }

    TVector3 targetPos = geo.GetTargetPosition();

    MagneticField field;
    TString smsDir = gSystem->Getenv("SMSIMDIR");
    TString rootField = smsDir + "/configs/simulation/geometry/filed_map/180626-1,20T-3000.root";
    TString tableField = smsDir + "/configs/simulation/geometry/filed_map/180626-1,20T-3000.table";

    bool loaded = false;
    if (gSystem->AccessPathName(rootField.Data()) == 0) {
        loaded = field.LoadFromROOTFile(rootField.Data());
    }
    if (!loaded && gSystem->AccessPathName(tableField.Data()) == 0) {
        loaded = field.LoadFieldMap(tableField.Data());
    }
    field.SetRotationAngle(30.0);

    if (!loaded) {
        std::cerr << "Failed to load magnetic field" << std::endl;
        return;
    }

    TFile f(input_root, "READ");
    if (f.IsZombie()) {
        std::cerr << "Failed to open input: " << input_root << std::endl;
        return;
    }

    TTree* tree = (TTree*)f.Get("tree");
    if (!tree) {
        std::cerr << "Tree 'tree' not found" << std::endl;
        return;
    }

    TClonesArray* fragArray = nullptr;
    tree->SetBranchAddress("FragSimData", &fragArray);
    Long64_t nEntries = tree->GetEntries();
    if (event_id < 0) {
        // [EN] Auto-select first event with valid PDC hits. / [CN] 自动选择第一个包含PDC击中事件。
        for (Long64_t i = 0; i < nEntries; ++i) {
            tree->GetEntry(i);
            std::vector<TVector3> tmpHits;
            std::map<std::string, std::pair<int, TVector3>> tmpBest;

            int n = fragArray ? fragArray->GetEntriesFast() : 0;
            for (int j = 0; j < n; ++j) {
                auto* hit = dynamic_cast<TSimData*>(fragArray->At(j));
                if (!hit) continue;
                std::string det = hit->fDetectorName.Data();
                std::string mod = hit->fModuleName.Data();
                if (!IsPdcName(det, mod)) continue;
                TVector3 pos = hit->fPostPosition;
                tmpHits.push_back(pos);

                std::string key = SelectPdcKey(det, mod);
                int step = hit->fStepNo;
                if (tmpBest.find(key) == tmpBest.end() || step < tmpBest[key].first) {
                    tmpBest[key] = std::make_pair(step, pos);
                }
            }

            if (tmpHits.size() >= 2) {
                event_id = static_cast<int>(i);
                break;
            }
        }
        if (event_id < 0) {
            std::cerr << "No event with PDC hits found" << std::endl;
            return;
        }
    }

    tree->GetEntry(event_id);

    std::vector<TVector3> pdcHits;
    std::map<std::string, std::pair<int, TVector3>> best;

    int n = fragArray ? fragArray->GetEntriesFast() : 0;
    for (int i = 0; i < n; ++i) {
        auto* hit = dynamic_cast<TSimData*>(fragArray->At(i));
        if (!hit) continue;
        std::string det = hit->fDetectorName.Data();
        std::string mod = hit->fModuleName.Data();
        if (!IsPdcName(det, mod)) continue;
        TVector3 pos = hit->fPostPosition;
        pdcHits.push_back(pos);

        std::string key = SelectPdcKey(det, mod);
        int step = hit->fStepNo;
        if (best.find(key) == best.end() || step < best[key].first) {
            best[key] = std::make_pair(step, pos);
        }
    }

    if (best.find("PDC1") == best.end() || best.find("PDC2") == best.end()) {
        // [EN] Fallback: pick min/max Z as PDC1/PDC2 if names are missing. / [CN] 若名称缺失则按Z最小/最大作为PDC1/PDC2。
        if (pdcHits.size() < 2) {
            std::cerr << "Missing PDC1/PDC2 hits" << std::endl;
            return;
        }
        auto minmaxZ = std::minmax_element(
            pdcHits.begin(),
            pdcHits.end(),
            [](const TVector3& a, const TVector3& b) {
                return a.Z() < b.Z();
            });
        best["PDC1"] = std::make_pair(0, *minmaxZ.first);
        best["PDC2"] = std::make_pair(0, *minmaxZ.second);
    }

    RecoTrack track;
    track.start = best["PDC1"].second;
    track.end = best["PDC2"].second;

    TargetReconstructor recon(&field);
    TargetReconstructionResult result;

    std::string m(method ? method : "threepoint");

    if (m == "grid") {
        result = recon.ReconstructAtTargetWithDetails(track, targetPos, true, pMin, pMax, tol, 4);
    } else if (m == "gd") {
        result = recon.ReconstructAtTargetGradientDescent(track, targetPos, true, pInit, learningRate, tol, maxIterations);
    } else if (m == "minuit") {
        result = recon.ReconstructAtTargetMinuit(track, targetPos, true, pInit, tol, maxIterations, false);
    } else {
        TVector3 fixedMom(fx, fy, fz);
        result = recon.ReconstructAtTargetThreePointGradientDescent(track, targetPos, true, fixedMom,
                                                                    learningRate, tol, maxIterations,
                                                                    pdcSigma, targetSigma);
    }

    std::ofstream out(out_csv);
    if (!out.is_open()) {
        std::cerr << "Failed to open output: " << out_csv << std::endl;
        return;
    }

    out << "# method=" << m << "\n";
    out << "# momentum=" << result.bestMomentum.Px() << "," << result.bestMomentum.Py() << "," << result.bestMomentum.Pz() << "\n";
    out << "# final_distance=" << result.finalDistance << "\n";
    out << "# target=" << targetPos.X() << "," << targetPos.Y() << "," << targetPos.Z() << "\n";
    out << "kind,x,y,z\n";

    for (const auto& p : pdcHits) {
        out << "pdc," << p.X() << "," << p.Y() << "," << p.Z() << "\n";
    }

    for (const auto& tp : result.bestTrajectory) {
        out << "track," << tp.position.X() << "," << tp.position.Y() << "," << tp.position.Z() << "\n";
    }

    out << "target," << targetPos.X() << "," << targetPos.Y() << "," << targetPos.Z() << "\n";
    out.close();
}
