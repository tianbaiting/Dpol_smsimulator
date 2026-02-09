// Usage: root -l 'scripts/visualization/run_display_steps.C(0, "base.root", "geom.mac", "detector_geometry.gdml", "step_size_compare.root", true)'
#include "TSystem.h"
#include "TApplication.h"
#include "TFile.h"
#include "TTree.h"
#include "TEveManager.h"
#include "TEveLine.h"
#include "TEvePointSet.h"
#include "TGeoManager.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"
#include "TEnv.h"
#include "TGLViewer.h"
#include "TVector3.h"
#include <vector>
#include <string>
#include <cstring>

#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"
#include "RecoEvent.hh"
#include "MagneticField.hh"

namespace {

void LoadAnalysisLib() {
    const char* sms = std::getenv("SMSIMDIR");
    if (!sms) return;
    std::string candidate1 = std::string(sms) + "/build/lib/libpdcanalysis.so";
    std::string candidate2 = std::string(sms) + "/lib/libpdcanalysis.so";
    if (gSystem->Load(candidate1.c_str()) >= 0) return;
    if (gSystem->Load(candidate2.c_str()) >= 0) return;
    gSystem->Load("libpdcanalysis.so");
}

void EnsureEve() {
    if (gEve) return;
    TEveManager::Create();
    gEnv->SetValue("OpenGL.Highlight.Select", 0);
    gEnv->SetValue("OpenGL.EventHandler.EnableMouseOver", 0);
}

} // namespace

void run_display_steps(Long64_t event_id = 0,
                       const char* base_root = "",
                       const char* geom_macro = "",
                       const char* gdml_file = "",
                       const char* overlay_root = "",
                       bool show_base_event = true)
{
    LoadAnalysisLib();
    gSystem->Load("libsmdata.so");

    if (!base_root || std::strlen(base_root) == 0) {
        base_root = "/home/tian/workspace/dpol/smsimulator5.5/d_work/output_tree/testry0000.root";
    }
    if (!geom_macro || std::strlen(geom_macro) == 0) {
        geom_macro = "/home/tian/workspace/dpol/smsimulator5.5/d_work/geometry/5deg_1.2T.mac";
    }
    if (!gdml_file || std::strlen(gdml_file) == 0) {
        gdml_file = "/home/tian/workspace/dpol/smsimulator5.5/d_work/detector_geometry.gdml";
    }

    GeometryManager geo;
    geo.LoadGeometry(geom_macro);

    EventDisplay display(gdml_file, geo);

    if (show_base_event) {
        PDCSimAna ana(geo);
        ana.SetSmearing(0.5, 0.5);

        EventDataReader reader(base_root);
        if (!reader.IsOpen()) {
            Error("run_display_steps", "无法打开数据文件!");
            return;
        }

        if (reader.GoToEvent(event_id)) {
            TClonesArray* hits = reader.GetHits();
            RecoEvent event = ana.ProcessEvent(hits);
            display.DisplayEvent(event);
        }
    }

    if (gEve) {
        gEve->DisableRedraw();
    }

    if (overlay_root && std::strlen(overlay_root) > 0) {
        TFile overlay(overlay_root, "READ");
        if (!overlay.IsZombie()) {
            TTree* trajTree = (TTree*)overlay.Get("traj");
            TTree* hitTree = (TTree*)overlay.Get("step_scan");

            if (trajTree) {
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

                std::vector<int> colors = {kRed, kBlue, kGreen+2, kMagenta+2, kOrange+7, kCyan+2};
                int colorIndex = 0;

                Long64_t n = trajTree->GetEntries();
                for (Long64_t i = 0; i < n; ++i) {
                    trajTree->GetEntry(i);
                    if (evt != event_id) continue;
                    if (!x || !y || !z) continue;
                    if (x->size() < 2) continue;

                    TEveLine* line = new TEveLine(Form("Step_%.1fmm_pid%d", stepSize, pid));
                    line->SetMainColor(colors[colorIndex % colors.size()]);
                    line->SetLineWidth(2);
                    colorIndex++;

                    for (size_t ip = 0; ip < x->size(); ++ip) {
                        line->SetNextPoint((*x)[ip], (*y)[ip], (*z)[ip]);
                    }
                    if (display.GetCurrentEventElements()) {
                        display.GetCurrentEventElements()->AddElement(line);
                    } else {
                        gEve->AddElement(line);
                    }
                }
            }

            if (hitTree) {
                double stepSize = 0.0;
                Long64_t evt = -1;
                double hx = 0.0, hy = 0.0, hz = 0.0;
                hitTree->SetBranchAddress("step_size", &stepSize);
                hitTree->SetBranchAddress("event_id", &evt);
                hitTree->SetBranchAddress("hit_x", &hx);
                hitTree->SetBranchAddress("hit_y", &hy);
                hitTree->SetBranchAddress("hit_z", &hz);

                TEvePointSet* pdcHits = new TEvePointSet("PDC_Hits_Overlay");
                pdcHits->SetMarkerColor(kYellow+2);
                pdcHits->SetMarkerStyle(20);
                pdcHits->SetMarkerSize(2.0);

                Long64_t n = hitTree->GetEntries();
                for (Long64_t i = 0; i < n; ++i) {
                    hitTree->GetEntry(i);
                    if (evt != event_id) continue;
                    pdcHits->SetNextPoint(hx, hy, hz);
                }

                if (display.GetCurrentEventElements()) {
                    display.GetCurrentEventElements()->AddElement(pdcHits);
                } else {
                    gEve->AddElement(pdcHits);
                }
            }
        }
    }

    if (gEve) {
        gEve->EnableRedraw();
        gEve->Redraw3D(kTRUE);
    }
    Info("run_display_steps", "3D显示已准备完成。窗口将保持打开状态。");
}
