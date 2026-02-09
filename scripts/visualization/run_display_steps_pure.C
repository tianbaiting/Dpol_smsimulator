// Usage: root -l 'scripts/visualization/run_display_steps_pure.C(0, "detector_geometry.gdml", "step_size_compare.root")'
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
#include <vector>
#include <string>
#include <cstring>

namespace {

void EnsureEve() {
    if (gEve) return;
    TEveManager::Create();
    gEnv->SetValue("OpenGL.Highlight.Select", 0);
    gEnv->SetValue("OpenGL.EventHandler.EnableMouseOver", 0);
    gEnv->SetValue("OpenGL.EventHandler.EnableSelection", 0);
    gEnv->SetValue("OpenGL.Timer.MouseOver", 0);
}

} // namespace

void run_display_steps_pure(Long64_t event_id = 0,
                            const char* gdml_file = "",
                            const char* overlay_root = "")
{
    if (!gdml_file || std::strlen(gdml_file) == 0) {
        gdml_file = "/home/tian/workspace/dpol/smsimulator5.5/d_work/detector_geometry.gdml";
    }

    EnsureEve();

    if (!gGeoManager) {
        TGeoManager::LockDefaultUnits(kFALSE);
        TGeoManager::SetDefaultUnits(TGeoManager::EDefaultUnits::kG4Units);
        TGeoManager::Import(gdml_file);
        if (gEve && gGeoManager) {
            gEve->AddGlobalElement(new TEveGeoTopNode(gGeoManager, gGeoManager->GetTopNode()));
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
                    gEve->AddElement(line);
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

                gEve->AddElement(pdcHits);
            }
        }
    }

    if (gEve) {
        gEve->EnableRedraw();
        gEve->Redraw3D(kTRUE);
        TGLViewer* viewer = gEve->GetDefaultGLViewer();
        if (viewer) {
            viewer->CurrentCamera().RotateRad(-1.2, 0.5);
            viewer->CurrentCamera().Dolly(5000, kFALSE, kFALSE);
        }
    }

    Info("run_display_steps_pure", "3D显示已准备完成。窗口将保持打开状态。");
}
