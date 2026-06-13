#include <array>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "TCanvas.h"
#include "TChain.h"
#include "TFile.h"
#include "TH1.h"
#include "TH2D.h"
#include "TLatex.h"
#include "TLine.h"
#include "TMath.h"
#include "TPad.h"
#include "TROOT.h"
#include "TStyle.h"
#include "TLorentzVector.h"
#include "TSystem.h"

namespace {

constexpr int kCutCount = 4;

struct CutDef {
    const char* name;
    const char* title;
};

const std::array<CutDef, kCutCount> kCuts = {{
    {"truth", "truth"},
    {"loose", "loose"},
    {"mid", "mid"},
    {"tight", "tight"},
}};

struct BranchState {
    bool hasProton = false;
    bool hasNeutron = false;
    TLorentzVector* proton = nullptr;
    TLorentzVector* neutron = nullptr;
};

struct Momentum2DHists {
    std::array<TH2D*, kCutCount> pxPy{};
    std::array<long long, kCutCount> counts{};
    std::array<long long, kCutCount> pxNeg{};
    std::array<long long, kCutCount> pxPos{};
};

std::string FormatLimitLabel(double pxLimit) {
    if (pxLimit <= 0.0) {
        return "allpx";
    }
    std::ostringstream os;
    os << "px";
    if (std::abs(pxLimit - std::round(pxLimit)) < 1.0e-6) {
        os << static_cast<int>(std::round(pxLimit));
    } else {
        os << pxLimit;
    }
    return os.str();
}

bool PassCut(int cutIndex, bool isYpol, double pxp, double pyp, double pzp, double pxn, double pyn, double pzn) {
    if (cutIndex == 0) {
        return true;
    }

    const double sumPx = pxp + pxn;
    const double sumPy = pyp + pyn;
    const double sumXY2 = sumPx * sumPx + sumPy * sumPy;
    const double phi = std::atan2(sumPy, sumPx);

    bool loose = false;
    bool mid = false;
    bool tight = false;
    if (isYpol) {
        loose = (std::abs(pyp - pyn) < 150.0) && (sumXY2 > 2500.0);
        mid = loose && (std::sqrt(sumXY2) < 200.0);
        tight = mid && ((TMath::Pi() - std::abs(phi)) < 0.2);
    } else {
        loose = ((pzp + pzn) > 1150.0) && (std::abs(pzp - pzn) < 150.0);
        mid = loose && (sumPx < 200.0) && (sumXY2 > 2500.0);
        tight = mid && ((TMath::Pi() - std::abs(phi)) < 0.5);
    }

    if (cutIndex == 1) {
        return loose;
    }
    if (cutIndex == 2) {
        return mid;
    }
    return tight;
}

void AttachBranches(TChain& chain, BranchState& state) {
    chain.SetBranchStatus("*", 0);
    chain.SetBranchStatus("truth_has_proton", 1);
    chain.SetBranchStatus("truth_has_neutron", 1);
    chain.SetBranchStatus("truth_proton_p4", 1);
    chain.SetBranchStatus("truth_neutron_p4", 1);
    chain.SetBranchAddress("truth_has_proton", &state.hasProton);
    chain.SetBranchAddress("truth_has_neutron", &state.hasNeutron);
    chain.SetBranchAddress("truth_proton_p4", &state.proton);
    chain.SetBranchAddress("truth_neutron_p4", &state.neutron);
}

void BookHists(const std::string& pol, double pxLimit, Momentum2DHists& hists) {
    const double pxAbs = pxLimit > 0.0 ? pxLimit : 300.0;
    const int pxBins = pxLimit > 0.0 ? 60 : 120;
    const int pyBins = 120;
    const std::string label = FormatLimitLabel(pxLimit);

    for (int i = 0; i < kCutCount; ++i) {
        const std::string name = "h2_" + pol + "_pxn_pyn_" + kCuts[i].name + "_" + label;
        const std::string title = pol + " " + kCuts[i].title + ";truth p_{x,n} [MeV/c];truth p_{y,n} [MeV/c]";
        hists.pxPy[i] = new TH2D(name.c_str(), title.c_str(), pxBins, -pxAbs, pxAbs, pyBins, -300.0, 300.0);
    }
}

void FillHists(TChain& chain, bool isYpol, double pxLimit, Momentum2DHists& hists) {
    BranchState state;
    AttachBranches(chain, state);

    const Long64_t entries = chain.GetEntries();
    for (Long64_t i = 0; i < entries; ++i) {
        chain.GetEntry(i);
        if (!state.hasProton || !state.hasNeutron || state.proton == nullptr || state.neutron == nullptr) {
            continue;
        }

        const double pxp = state.proton->Px();
        const double pyp = state.proton->Py();
        const double pzp = state.proton->Pz();
        const double pxn = state.neutron->Px();
        const double pyn = state.neutron->Py();
        const double pzn = state.neutron->Pz();

        if (pxLimit > 0.0 && std::abs(pxn) >= pxLimit) {
            continue;
        }

        for (int cutIndex = 0; cutIndex < kCutCount; ++cutIndex) {
            if (!PassCut(cutIndex, isYpol, pxp, pyp, pzp, pxn, pyn, pzn)) {
                continue;
            }
            hists.pxPy[cutIndex]->Fill(pxn, pyn);
            ++hists.counts[cutIndex];
            if (pxn < 0.0) {
                ++hists.pxNeg[cutIndex];
            } else if (pxn > 0.0) {
                ++hists.pxPos[cutIndex];
            }
        }
    }
}

void DrawZeroLines(double xMin, double xMax, double yMin, double yMax) {
    TLine xLine(0.0, yMin, 0.0, yMax);
    xLine.SetLineStyle(2);
    xLine.SetLineColor(kGray + 2);
    xLine.Draw();

    TLine yLine(xMin, 0.0, xMax, 0.0);
    yLine.SetLineStyle(2);
    yLine.SetLineColor(kGray + 2);
    yLine.Draw();
}

void DrawGrid(const std::string& outDir, const std::string& pol, const std::string& label, const Momentum2DHists& hists, bool logz) {
    const std::string scale = logz ? "logz" : "linear";
    TCanvas canvas(("c2_" + pol + "_" + label + "_" + scale).c_str(), "", 1500, 1250);
    canvas.Divide(2, 2);

    for (int cutIndex = 0; cutIndex < kCutCount; ++cutIndex) {
        canvas.cd(cutIndex + 1);
        gPad->SetRightMargin(0.15);
        gPad->SetGrid(1, 1);
        gPad->SetLogz(logz ? 1 : 0);

        TH2D* hist = hists.pxPy[cutIndex];
        if (logz) {
            hist->SetMinimum(1.0);
        } else {
            hist->SetMinimum(0.0);
        }
        hist->SetTitle(Form("%s %s: truth p_{x,n} vs p_{y,n}", pol.c_str(), kCuts[cutIndex].title));
        hist->Draw("COLZ");

        const double xMin = hist->GetXaxis()->GetXmin();
        const double xMax = hist->GetXaxis()->GetXmax();
        const double yMin = hist->GetYaxis()->GetXmin();
        const double yMax = hist->GetYaxis()->GetXmax();
        DrawZeroLines(xMin, xMax, yMin, yMax);

        TLatex latex;
        latex.SetNDC(true);
        latex.SetTextSize(0.035);
        latex.DrawLatex(0.15, 0.86, Form("N=%lld  -:%lld  +:%lld", hists.counts[cutIndex], hists.pxNeg[cutIndex], hists.pxPos[cutIndex]));
    }

    canvas.SaveAs((outDir + "/truth_neutron_pxn_pyn_by_cut_" + pol + "_" + label + "_" + scale + ".png").c_str());
    canvas.SaveAs((outDir + "/truth_neutron_pxn_pyn_by_cut_" + pol + "_" + label + "_" + scale + ".pdf").c_str());
}

void WriteHists(const std::string& outDir, const std::string& label, const Momentum2DHists& y, const Momentum2DHists& z) {
    TFile file((outDir + "/truth_neutron_pxn_pyn_cuts_" + label + ".root").c_str(), "RECREATE");
    for (const Momentum2DHists* hists : {&y, &z}) {
        for (int i = 0; i < kCutCount; ++i) {
            hists->pxPy[i]->Write();
        }
    }
    file.Close();
}

void PrintSummary(const std::string& pol, const Momentum2DHists& hists) {
    for (int i = 0; i < kCutCount; ++i) {
        std::cout << pol << " " << kCuts[i].name
                  << " N " << hists.counts[i]
                  << " px_neg " << hists.pxNeg[i]
                  << " px_pos " << hists.pxPos[i]
                  << std::endl;
    }
}

}

void plot_truth_neutron_momentum_cuts(
    const char* baseDir = "/home/tian/workspace/sshDir/spana03/Dpol_smsimulator/data/reconstruction/nebulaplus_nn_joint_20260609_012854",
    const char* outDir = "results/nebulaplus_truth_neutron_momentum_cuts",
    double pxLimit = -1.0) {
    gROOT->SetBatch(true);
    gStyle->SetOptStat(0);
    gStyle->SetPalette(kBird);
    gSystem->mkdir(outDir, true);

    const std::string base(baseDir);
    const std::string label = FormatLimitLabel(pxLimit);

    TChain y("recoTree");
    TChain z("recoTree");
    y.Add((base + "/y_pol_nebulaplus/*/*/*_reco.root").c_str());
    z.Add((base + "/z_pol_nebulaplus/*/*/*_reco.root").c_str());

    Momentum2DHists yHists;
    Momentum2DHists zHists;
    BookHists("ypol", pxLimit, yHists);
    BookHists("zpol", pxLimit, zHists);

    FillHists(y, true, pxLimit, yHists);
    FillHists(z, false, pxLimit, zHists);

    DrawGrid(outDir, "ypol", label, yHists, false);
    DrawGrid(outDir, "ypol", label, yHists, true);
    DrawGrid(outDir, "zpol", label, zHists, false);
    DrawGrid(outDir, "zpol", label, zHists, true);
    WriteHists(outDir, label, yHists, zHists);

    std::cout << "out " << outDir << std::endl;
    PrintSummary("ypol", yHists);
    PrintSummary("zpol", zHists);
}
