// [EN] Usage: root -l -q 'scripts/analysis/analyze_synthetic_px_reco.C+("sim.root","geom.mac","field.table",30.0,"out.root","summary.csv",20000,2000,10.0,627.0)' / [CN] 用法: root -l -q 'scripts/analysis/analyze_synthetic_px_reco.C+("sim.root","geom.mac","field.table",30.0,"out.root","summary.csv",20000,2000,10.0,627.0)'

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
#include <fstream>
#include <iostream>
#include <limits>
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
    if (hit->fID == 0 || hit->fID == 1) return true;
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
    if (!geo.LoadGeometry(geom_macro)) {
        std::cerr << "Failed to load geometry macro: " << geom_macro << std::endl;
        return;
    }
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
    double recoPxMinuit = 0.0, dPxMinuit = 0.0;

    bool threeDone = false, threeSuccess = false;
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
    metrics.Branch("reco_px_minuit", &recoPxMinuit, "reco_px_minuit/D");
    metrics.Branch("dpx_minuit", &dPxMinuit, "dpx_minuit/D");
    metrics.Branch("three_done", &threeDone, "three_done/O");
    metrics.Branch("three_success", &threeSuccess, "three_success/O");
    metrics.Branch("reco_px_three", &recoPxThree, "reco_px_three/D");
    metrics.Branch("dpx_three", &dPxThree, "dpx_three/D");
    metrics.Branch("neutron_done", &neutronDone, "neutron_done/O");
    metrics.Branch("reco_px_neutron", &recoPxNeutron, "reco_px_neutron/D");
    metrics.Branch("dpx_neutron", &dPxNeutron, "dpx_neutron/D");

    TH1D* hPdcD1 = new TH1D("h_pdc_d1", "PDC1 position residual;#Delta r [mm];Events", 120, 0, 60);
    TH1D* hPdcD2 = new TH1D("h_pdc_d2", "PDC2 position residual;#Delta r [mm];Events", 120, 0, 60);

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
        recoPxMinuit = dPxMinuit = 0.0;
        threeDone = threeSuccess = false;
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
        }
        if (hasTruthN) {
            truePxN = truth.neutronMomentum.X();
            truePyN = truth.neutronMomentum.Y();
            truePzN = truth.neutronMomentum.Z();
        }

        const PdcReference pdcRef = ExtractPdcReference(fragArray);
        RecoEvent pdcEvent = pdcAna.ProcessEvent(fragArray);
        if (!pdcEvent.tracks.empty() && pdcRef.hasPDC1 && pdcRef.hasPDC2) {
            hasRecoTrack = true;
            const RecoTrack& track = pdcEvent.tracks.front();
            pdcD1 = (track.start - pdcRef.pdc1).Mag();
            pdcD2 = (track.end - pdcRef.pdc2).Mag();
            hPdcD1->Fill(pdcD1);
            hPdcD2->Fill(pdcD2);

            if (hasTruthP && protonRecoCounter < max_proton_reco_events) {
                ++protonRecoCounter;
                CoutSilencer silencer;

                TargetReconstructionResult minuitResult = reconMinuit.ReconstructAtTargetMinuit(
                    track, targetPos, false, 627.0, 5.0, 1000, false
                );
                minuitDone = true;
                minuitSuccess = minuitResult.success;
                recoPxMinuit = minuitResult.bestMomentum.Px();
                dPxMinuit = recoPxMinuit - truePxP;
                hDpxMinuit->Fill(dPxMinuit);
                hPxTrueRecoMinuit->Fill(truePxP, recoPxMinuit);
                hDpxVsTrueMinuit->Fill(truePxP, dPxMinuit);

                const TVector3 trackDir = (track.end - track.start).Unit();
                const TVector3 fixedMomentum = trackDir * threepoint_fixed_pmag;
                TargetReconstructionResult threeResult = reconThree.ReconstructAtTargetThreePointGradientDescent(
                    track, targetPos, false, fixedMomentum, 0.05, 5.0, 120, 0.5, 5.0
                );
                threeDone = true;
                threeSuccess = threeResult.success;
                recoPxThree = threeResult.bestMomentum.Px();
                dPxThree = recoPxThree - truePxP;
                hDpxThree->Fill(dPxThree);
                hPxTrueRecoThree->Fill(truePxP, recoPxThree);
                hDpxVsTrueThree->Fill(truePxP, dPxThree);
            }
        }

        if (hasTruthN && nebulaArray) {
            std::vector<RecoNeutron> neutrons = nebulaRecon.ReconstructNeutrons(nebulaArray);
            const RecoNeutron* best = SelectBestNeutron(neutrons);
            if (best) {
                neutronDone = true;
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
    hDpxMinuit->Write(); hDpxThree->Write();
    hPxTrueRecoMinuit->Write(); hPxTrueRecoThree->Write();
    hDpxVsTrueMinuit->Write(); hDpxVsTrueThree->Write();
    hDpxNeutron->Write(); hPxTrueRecoNeutron->Write(); hDpxVsTrueNeutron->Write();
    outFile.Write();
    outFile.Close();

    std::ofstream csv(out_csv);
    csv << "metric,count,mean,rms\n";
    csv << "pdc_d1," << hPdcD1->GetEntries() << "," << hPdcD1->GetMean() << "," << hPdcD1->GetRMS() << "\n";
    csv << "pdc_d2," << hPdcD2->GetEntries() << "," << hPdcD2->GetMean() << "," << hPdcD2->GetRMS() << "\n";
    csv << "proton_dpx_minuit," << hDpxMinuit->GetEntries() << "," << hDpxMinuit->GetMean() << "," << hDpxMinuit->GetRMS() << "\n";
    csv << "proton_dpx_threepoint," << hDpxThree->GetEntries() << "," << hDpxThree->GetMean() << "," << hDpxThree->GetRMS() << "\n";
    csv << "neutron_dpx_tof," << hDpxNeutron->GetEntries() << "," << hDpxNeutron->GetMean() << "," << hDpxNeutron->GetRMS() << "\n";
    csv.close();

    std::cerr << "[analyze_synthetic_px_reco] Done." << std::endl;
    std::cerr << "  Output ROOT: " << out_root << std::endl;
    std::cerr << "  Output CSV : " << out_csv << std::endl;
    std::cerr << "  Plots dir  : " << plotDir << std::endl;
}
