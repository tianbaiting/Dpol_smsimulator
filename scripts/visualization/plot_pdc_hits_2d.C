// Usage: root -l -q 'scripts/visualization/plot_pdc_hits_2d.C("/path/to/output.root", "results/g4input_test")'
#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TSimData.hh"
#include "TGraph.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TROOT.h"
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>

namespace {
std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return std::tolower(c); });
    return out;
}

bool IsPdcName(const std::string& detector, const std::string& module) {
    std::string d = ToLower(detector);
    std::string m = ToLower(module);
    return (d.find("pdc") != std::string::npos) || (m.find("pdc") != std::string::npos);
}
}

void plot_pdc_hits_2d(const char* root_file, const char* out_dir = "results/g4input_test") {
    if (!root_file) return;
    gROOT->SetBatch(kTRUE);
    gStyle->SetOptStat(0);

    if (out_dir) {
        gSystem->mkdir(out_dir, kTRUE);
    }

    TFile f(root_file, "READ");
    if (f.IsZombie()) return;

    TTree* tree = (TTree*)f.Get("tree");
    if (!tree) return;

    TClonesArray* fragArray = nullptr;
    tree->SetBranchAddress("FragSimData", &fragArray);

    std::vector<double> xs, ys, zs;

    Long64_t nEntries = tree->GetEntries();
    for (Long64_t i = 0; i < nEntries; ++i) {
        tree->GetEntry(i);
        if (!fragArray) continue;
        int n = fragArray->GetEntriesFast();
        for (int j = 0; j < n; ++j) {
            auto* hit = dynamic_cast<TSimData*>(fragArray->At(j));
            if (!hit) continue;
            if (!IsPdcName(hit->fDetectorName.Data(), hit->fModuleName.Data())) continue;
            xs.push_back(hit->fPostPosition.X());
            ys.push_back(hit->fPostPosition.Y());
            zs.push_back(hit->fPostPosition.Z());
        }
    }

    if (xs.empty()) return;

    TGraph gXZ(xs.size());
    TGraph gXY(xs.size());

    for (size_t i = 0; i < xs.size(); ++i) {
        gXZ.SetPoint(i, xs[i], zs[i]);
        gXY.SetPoint(i, xs[i], ys[i]);
    }

    gXZ.SetTitle("PDC Hits XZ;X [mm];Z [mm]");
    gXY.SetTitle("PDC Hits XY;X [mm];Y [mm]");
    gXZ.SetMarkerStyle(20);
    gXY.SetMarkerStyle(20);
    gXZ.SetMarkerSize(0.6);
    gXY.SetMarkerSize(0.6);

    TCanvas c1("c1", "XZ", 1200, 800);
    gXZ.Draw("AP");
    if (out_dir) {
        std::string out = std::string(out_dir) + "/pdc_hits_xz.png";
        c1.SaveAs(out.c_str());
    }

    TCanvas c2("c2", "XY", 1200, 800);
    gXY.Draw("AP");
    if (out_dir) {
        std::string out = std::string(out_dir) + "/pdc_hits_xy.png";
        c2.SaveAs(out.c_str());
    }
}
