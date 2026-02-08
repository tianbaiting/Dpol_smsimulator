// Usage: root -l -q 'scripts/visualization/plot_steps_2d.C("step_size_compare.root", 0, "results/step_size_scan/plots")'
//   用法：

//   root -l -q 'scripts/visualization/plot_steps_2d.C("results/step_size_scan/B115T_3deg/step_size_compare.root", 0, "results/step_size_scan/plots")'

#include "TFile.h"
#include "TTree.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TStyle.h"
#include "TSystem.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>

struct TrajData {
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> z;
};

void plot_steps_2d(const char* overlay_root,
                   Long64_t event_id = 0,
                   const char* out_dir = "results/step_size_scan/plots")
{
    if (!overlay_root || std::strlen(overlay_root) == 0) {
        std::cerr << "overlay_root is required" << std::endl;
        return;
    }

    gStyle->SetOptStat(0);

    if (out_dir && std::strlen(out_dir) > 0) {
        gSystem->mkdir(out_dir, kTRUE);
    }

    TFile f(overlay_root, "READ");
    if (f.IsZombie()) {
        std::cerr << "Failed to open overlay root" << std::endl;
        return;
    }

    TTree* trajTree = (TTree*)f.Get("traj");
    TTree* hitTree = (TTree*)f.Get("step_scan");
    if (!trajTree) {
        std::cerr << "Missing traj tree" << std::endl;
        return;
    }

    double stepSize = 0.0;
    Long64_t evt = -1;
    int pid = -1;
    std::vector<double>* x = nullptr;
    std::vector<double>* y = nullptr;
    std::vector<double>* z = nullptr;

    trajTree->SetBranchAddress("step_size", &stepSize);
    trajTree->SetBranchAddress("event_id", &evt);
    trajTree->SetBranchAddress("particle_id", &pid);
    trajTree->SetBranchAddress("x", &x);
    trajTree->SetBranchAddress("y", &y);
    trajTree->SetBranchAddress("z", &z);

    std::map<double, std::map<int, TrajData>> data;

    Long64_t n = trajTree->GetEntries();
    for (Long64_t i = 0; i < n; ++i) {
        trajTree->GetEntry(i);
        if (evt != event_id) continue;
        if (!x || !y || !z) continue;
        data[stepSize][pid] = {*x, *y, *z};
    }

    std::vector<TGraph*> pdcPoints;
    if (hitTree) {
        double hstep = 0.0;
        Long64_t hevt = -1;
        double hx = 0.0, hy = 0.0, hz = 0.0;
        hitTree->SetBranchAddress("step_size", &hstep);
        hitTree->SetBranchAddress("event_id", &hevt);
        hitTree->SetBranchAddress("hit_x", &hx);
        hitTree->SetBranchAddress("hit_y", &hy);
        hitTree->SetBranchAddress("hit_z", &hz);

        std::vector<double> px, py, pz;
        Long64_t nh = hitTree->GetEntries();
        for (Long64_t i = 0; i < nh; ++i) {
            hitTree->GetEntry(i);
            if (hevt != event_id) continue;
            px.push_back(hx);
            py.push_back(hy);
            pz.push_back(hz);
        }

        if (!px.empty()) {
            TGraph* gXZ = new TGraph(px.size());
            TGraph* gXY = new TGraph(px.size());
            TGraph* gYZ = new TGraph(px.size());
            for (size_t i = 0; i < px.size(); ++i) {
                gXZ->SetPoint(i, px[i], pz[i]);
                gXY->SetPoint(i, px[i], py[i]);
                gYZ->SetPoint(i, py[i], pz[i]);
            }
            gXZ->SetMarkerStyle(20);
            gXY->SetMarkerStyle(20);
            gYZ->SetMarkerStyle(20);
            gXZ->SetMarkerColor(kBlack);
            gXY->SetMarkerColor(kBlack);
            gYZ->SetMarkerColor(kBlack);
            pdcPoints.push_back(gXZ);
            pdcPoints.push_back(gXY);
            pdcPoints.push_back(gYZ);
        }
    }

    std::vector<int> colors = {kRed, kBlue, kGreen+2, kMagenta+2, kOrange+7, kCyan+2};

    auto drawPlane = [&](const char* name, const char* xtitle, const char* ytitle, int planeIdx) {
        TCanvas* c = new TCanvas(name, name, 1200, 800);
        TMultiGraph* mg = new TMultiGraph();
        TLegend* leg = new TLegend(0.12, 0.7, 0.45, 0.88);
        int colorIndex = 0;

        for (const auto& stepEntry : data) {
            double step = stepEntry.first;
            for (const auto& pidEntry : stepEntry.second) {
                int lstyle = (pidEntry.first % 2 == 0) ? 1 : 2;
                const TrajData& t = pidEntry.second;
                if (t.x.size() < 2) continue;
                TGraph* g = new TGraph(t.x.size());
                for (size_t i = 0; i < t.x.size(); ++i) {
                    double xv = 0.0, yv = 0.0;
                    if (planeIdx == 0) { xv = t.x[i]; yv = t.z[i]; }
                    if (planeIdx == 1) { xv = t.x[i]; yv = t.y[i]; }
                    if (planeIdx == 2) { xv = t.y[i]; yv = t.z[i]; }
                    g->SetPoint(i, xv, yv);
                }
                int color = colors[colorIndex % colors.size()];
                g->SetLineColor(color);
                g->SetLineWidth(2);
                g->SetLineStyle(lstyle);
                mg->Add(g, "L");
                if (pidEntry.first == 0) {
                    leg->AddEntry(g, Form("step %.1f mm", step), "l");
                    colorIndex++;
                }
            }
        }

        mg->Draw("A");
        mg->GetXaxis()->SetTitle(xtitle);
        mg->GetYaxis()->SetTitle(ytitle);

        if (!pdcPoints.empty()) {
            int idx = planeIdx;
            if (idx >= 0 && idx < (int)pdcPoints.size()) {
                pdcPoints[idx]->Draw("P SAME");
                leg->AddEntry(pdcPoints[idx], "PDC hits", "p");
            }
        }

        leg->Draw();
        c->Update();
        if (out_dir && std::strlen(out_dir) > 0) {
            std::string out = std::string(out_dir) + "/steps_event" + std::to_string(event_id) + "_" + name + ".png";
            c->SaveAs(out.c_str());
        }
    };

    drawPlane("xz", "X [mm]", "Z [mm]", 0);
    drawPlane("xy", "X [mm]", "Y [mm]", 1);
    drawPlane("yz", "Y [mm]", "Z [mm]", 2);

    std::cout << "Saved plots to " << out_dir << std::endl;
}
