#include "ReconstructionVisualizer.hh"
#include "ParticleTrajectory.hh"
#include "MagneticField.hh"
#include "TStyle.h"
#include "TMultiGraph.h"
#include "TLegend.h"
#include "TAxis.h"
#include "TLatex.h"
#include "TMarker.h"
#include "TArrow.h"
#include "TLine.h"
#include "TPolyLine3D.h"
#include "TPolyMarker3D.h"
#include "TView.h"
#include "TSystem.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

ReconstructionVisualizer::ReconstructionVisualizer() {
    // Set ROOT style
    gStyle->SetOptStat(0);
    gStyle->SetPalette(kRainBow);
}

ReconstructionVisualizer::~ReconstructionVisualizer() {
    // Clean up canvases
    for (auto* canvas : fCanvases) {
        if (canvas) delete canvas;
    }
}

void ReconstructionVisualizer::RecordStep(const OptimizationStep& step) {
    fSteps.push_back(step);
}

ReconstructionVisualizer::GlobalGrid 
ReconstructionVisualizer::CalculateGlobalGrid(const TargetReconstructor* reconstructor,
                                             const RecoTrack& track,
                                             const TVector3& targetPos,
                                             double pMin, double pMax, int nSamples) {
    GlobalGrid grid;
    grid.pMin = pMin;
    grid.pMax = pMax;
    grid.nSamples = nSamples;
    
    std::cout << "\n=== Calculating Global Function Grid ===" << std::endl;
    std::cout << "Momentum range: [" << pMin << ", " << pMax << "] MeV/c" << std::endl;
    std::cout << "Number of samples: " << nSamples << std::endl;
    
    double dp = (pMax - pMin) / (nSamples - 1);
    
    // Calculate direction and starting point (consistent with TargetReconstructor logic)
    double distStart = (track.start - targetPos).Mag();
    double distEnd = (track.end - targetPos).Mag();
    bool useEndPoint = (distEnd > distStart);
    TVector3 startPos = useEndPoint ? track.end : track.start;
    TVector3 otherPos = useEndPoint ? track.start : track.end;
    TVector3 backwardDirection = (otherPos - startPos);
    
    for (int i = 0; i < nSamples; i++) {
        double p = pMin + i * dp;
        
        // Use TargetReconstructor's calculation method
        double dist = reconstructor->CalculateMinimumDistance(
            p, startPos, backwardDirection, targetPos, -1.0, 938.272
        );
        
        grid.momenta.push_back(p);
        grid.distances.push_back(dist);
        
        // Print progress
        if (i % (nSamples / 10) == 0) {
            std::cout << "  Progress: " << (i * 100 / nSamples) << "% (p=" 
                      << p << " MeV/c, dist=" << dist << " mm)" << std::endl;
        }
    }
    
    // Find minimum value
    auto minIt = std::min_element(grid.distances.begin(), grid.distances.end());
    int minIdx = std::distance(grid.distances.begin(), minIt);
    std::cout << "Global minimum found at p=" << grid.momenta[minIdx] 
              << " MeV/c, distance=" << *minIt << " mm" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    return grid;
}

TCanvas* ReconstructionVisualizer::PlotOptimizationFunction1D(
    const GlobalGrid& grid, const std::string& title) {
    
    TCanvas* c = CreateCanvas("c_opt_func", title.c_str());
    c->SetGrid();
    
    // Create TGraph
    TGraph* gr = new TGraph(grid.nSamples);
    for (int i = 0; i < grid.nSamples; i++) {
        gr->SetPoint(i, grid.momenta[i], grid.distances[i]);
    }
    
    gr->SetTitle((title + ";Momentum [MeV/c];Distance to Target [mm]").c_str());
    gr->SetLineColor(kBlue);
    gr->SetLineWidth(2);
    gr->SetMarkerStyle(20);
    gr->SetMarkerSize(0.5);
    gr->SetMarkerColor(kBlue);
    
    gr->Draw("ALP");
    
    // Mark minimum value
    auto minIt = std::min_element(grid.distances.begin(), grid.distances.end());
    int minIdx = std::distance(grid.distances.begin(), minIt);
    
    TMarker* marker = new TMarker(grid.momenta[minIdx], *minIt, 29);
    marker->SetMarkerColor(kRed);
    marker->SetMarkerSize(2);
    marker->Draw();
    
    TLatex* label = new TLatex(grid.momenta[minIdx], *minIt * 1.1,
                              Form("Min: p=%.1f MeV/c, d=%.2f mm", 
                                   grid.momenta[minIdx], *minIt));
    label->SetTextColor(kRed);
    label->SetTextSize(0.03);
    label->Draw();
    
    c->Update();
    return c;
}

TCanvas* ReconstructionVisualizer::PlotOptimizationPath(
    const GlobalGrid& grid, const std::vector<OptimizationStep>& steps,
    const std::string& title, double trueMomentum,
    const TVector3* trueMomentumVec, const TVector3* recoMomentumVec) {
    
    TCanvas* c = CreateCanvas("c_opt_path", title.c_str(), 1400, 900);
    c->SetGrid();
    c->SetLeftMargin(0.12);
    c->SetRightMargin(0.05);
    c->SetTopMargin(0.08);
    c->SetBottomMargin(0.12);
    
    // Draw global function curve (background, dashed line)
    TGraph* grGlobal = new TGraph(grid.nSamples);
    for (int i = 0; i < grid.nSamples; i++) {
        grGlobal->SetPoint(i, grid.momenta[i], grid.distances[i]);
    }
    
    grGlobal->SetTitle((title + ";Momentum p [MeV/c];Loss (Distance to Target) [mm]").c_str());
    grGlobal->SetLineColor(kBlack);
    grGlobal->SetLineWidth(2);
    grGlobal->SetLineStyle(2);  // Dashed line
    grGlobal->GetXaxis()->SetTitleSize(0.045);
    grGlobal->GetYaxis()->SetTitleSize(0.045);
    grGlobal->GetXaxis()->SetLabelSize(0.04);
    grGlobal->GetYaxis()->SetLabelSize(0.04);
    grGlobal->GetXaxis()->SetTitleOffset(1.1);
    grGlobal->GetYaxis()->SetTitleOffset(1.2);
    grGlobal->Draw("AL");
    
    // If true momentum is provided, draw vertical line at true momentum
    TLine* trueLine = nullptr;
    if (trueMomentum > 0) {
        double yMin = grGlobal->GetYaxis()->GetXmin();
        double yMax = grGlobal->GetYaxis()->GetXmax();
        trueLine = new TLine(trueMomentum, yMin, trueMomentum, yMax);
        trueLine->SetLineColor(kGreen+2);
        trueLine->SetLineWidth(3);
        trueLine->SetLineStyle(7);  // Dash-dot line
        trueLine->Draw();
    }
    
    // Draw optimization path
    if (!steps.empty()) {
        std::cout << "\n=== Drawing TMinuit Optimization Path ===" << std::endl;
        std::cout << "Total steps: " << steps.size() << std::endl;
        
        // Helper function: interpolate loss value at given momentum from global grid
        auto getLossAtMomentum = [&](double p) -> double {
            // Find closest points in grid
            for (size_t i = 0; i < grid.momenta.size() - 1; i++) {
                if (p >= grid.momenta[i] && p <= grid.momenta[i+1]) {
                    // Linear interpolation
                    double t = (p - grid.momenta[i]) / (grid.momenta[i+1] - grid.momenta[i]);
                    return grid.distances[i] + t * (grid.distances[i+1] - grid.distances[i]);
                }
            }
            // If outside range, use nearest endpoint
            if (p < grid.momenta.front()) return grid.distances.front();
            return grid.distances.back();
        };
        
        // 1. Draw each step as a point ON the global loss function curve
        TGraph* grPoints = new TGraph(steps.size());
        for (size_t i = 0; i < steps.size(); i++) {
            double momentum = steps[i].momentum;
            double lossOnCurve = getLossAtMomentum(momentum);  // Use interpolated loss from global function
            grPoints->SetPoint(i, momentum, lossOnCurve);
        }
        grPoints->SetMarkerStyle(20);
        grPoints->SetMarkerSize(1.5);
        grPoints->SetMarkerColor(kRed);
        grPoints->Draw("P SAME");
        
        // 2. Draw arrows connecting each step ON the curve
        std::cout << "Drawing " << (steps.size() - 1) << " arrows on loss function..." << std::endl;
        for (size_t i = 0; i < steps.size() - 1; i++) {
            double x1 = steps[i].momentum;
            double y1 = getLossAtMomentum(x1);  // Y position on global curve
            double x2 = steps[i+1].momentum;
            double y2 = getLossAtMomentum(x2);  // Y position on global curve
            
            // Calculate arrow size based on step magnitude
            double dx = x2 - x1;
            double dy = y2 - y1;
            double stepSize = std::sqrt(dx*dx + dy*dy);
            
            // Adjust arrow head size
            double arrowSize = std::min(0.015, std::max(0.005, stepSize * 0.05));
            
            TArrow* arrow = new TArrow(x1, y1, x2, y2, arrowSize, ">");
            arrow->SetLineColor(kRed);
            arrow->SetFillColor(kRed);
            arrow->SetLineWidth(2);
            arrow->SetLineStyle(1);
            arrow->Draw();
        }
        
        // 3. Mark starting point (large green star) ON the curve
        double startLoss = getLossAtMomentum(steps.front().momentum);
        TMarker* startMarker = new TMarker(steps.front().momentum, startLoss, 29);
        startMarker->SetMarkerColor(kGreen+2);
        startMarker->SetMarkerSize(4.0);
        startMarker->Draw();
        
        // 4. Mark end point (large blue star) ON the curve
        double endLoss = getLossAtMomentum(steps.back().momentum);
        TMarker* endMarker = new TMarker(steps.back().momentum, endLoss, 29);
        endMarker->SetMarkerColor(kBlue);
        endMarker->SetMarkerSize(4.0);
        endMarker->Draw();
        
        // 5. Label key step numbers on the points
        TLatex* text = new TLatex();
        text->SetTextSize(0.028);
        text->SetTextColor(kBlack);
        text->SetTextFont(42);
        text->SetTextAlign(22); // Center alignment
        
        // Label: start, 25%, 50%, 75%, end
        std::vector<int> keySteps = {
            0, 
            (int)(steps.size() * 0.25),
            (int)(steps.size() * 0.5),
            (int)(steps.size() * 0.75),
            (int)(steps.size() - 1)
        };
        
        double yRange = grGlobal->GetYaxis()->GetXmax() - grGlobal->GetYaxis()->GetXmin();
        
        for (int idx : keySteps) {
            if (idx >= 0 && idx < (int)steps.size()) {
                double xPos = steps[idx].momentum;
                double yPosOnCurve = getLossAtMomentum(xPos);  // Use position on curve
                double yOffset = yRange * 0.06;  // 6% offset above the point
                
                text->DrawLatex(xPos, yPosOnCurve + yOffset, 
                               Form("#color[4]{#bf{#%d}}", idx));
            }
        }
        
        // 6. Legend
        TLegend* legend = new TLegend(0.65, 0.65, 0.93, 0.90);
        legend->SetBorderSize(1);
        legend->SetFillStyle(1001);
        legend->SetTextSize(0.035);
        legend->AddEntry(grGlobal, "Loss Function (Global)", "l");
        legend->AddEntry(grPoints, "TMinuit Steps", "p");
        legend->AddEntry(startMarker, "Initial Guess", "p");
        legend->AddEntry(endMarker, "Converged Point", "p");
        if (trueLine) {
            legend->AddEntry(trueLine, Form("True Momentum: %.1f MeV/c", trueMomentum), "l");
        }
        legend->Draw();
        
        // 7. Display statistics in a text box
        TLatex* info = new TLatex();
        info->SetNDC();
        info->SetTextSize(0.032);
        info->SetTextColor(kBlack);
        info->SetTextFont(42);
        
        double xInfo = 0.15;
        double yPos = 0.88;
        
        info->DrawLatex(xInfo, yPos, "#bf{Optimization Statistics}");
        yPos -= 0.045;
        info->DrawLatex(xInfo, yPos, Form("Total iterations: #bf{%zu} steps", steps.size()));
        yPos -= 0.04;
        info->DrawLatex(xInfo, yPos, Form("Start: p = %.1f MeV/c, Loss = %.2f mm", 
                                        steps.front().momentum, steps.front().distance));
        yPos -= 0.04;
        info->DrawLatex(xInfo, yPos, Form("End:   p = %.1f MeV/c, Loss = %.2f mm", 
                                        steps.back().momentum, steps.back().distance));
        yPos -= 0.04;
        
        double pChange = steps.back().momentum - steps.front().momentum;
        double distImprove = steps.front().distance - steps.back().distance;
        double pctImprove = 100.0 * distImprove / steps.front().distance;
        
        info->SetTextColor(pChange > 0 ? kRed+1 : kBlue+1);
        info->DrawLatex(xInfo, yPos, Form("#Deltap = %+.1f MeV/c (%+.1f%%)", 
                                         pChange, 100.0 * pChange / steps.front().momentum));
        yPos -= 0.04;
        info->SetTextColor(kGreen+2);
        info->DrawLatex(xInfo, yPos, Form("Loss reduction: %.2f #rightarrow %.2f mm (#bf{%.1f%%})", 
                                        steps.front().distance, steps.back().distance, pctImprove));
        
        // 8. Display momentum components if provided
        if (trueMomentumVec || recoMomentumVec) {
            yPos -= 0.05;  // Extra spacing
            info->SetTextColor(kBlack);
            info->DrawLatex(xInfo, yPos, "#bf{Momentum Components}");
            yPos -= 0.04;
            
            if (trueMomentumVec) {
                info->SetTextColor(kGreen+2);
                info->DrawLatex(xInfo, yPos, Form("True:  #vec{p} = (%.1f, %.1f, %.1f) MeV/c", 
                                                trueMomentumVec->X(), 
                                                trueMomentumVec->Y(), 
                                                trueMomentumVec->Z()));
                yPos -= 0.035;
                info->DrawLatex(xInfo, yPos, Form("       |#vec{p}| = %.1f MeV/c", 
                                                trueMomentumVec->Mag()));
                yPos -= 0.04;
            }
            
            if (recoMomentumVec) {
                info->SetTextColor(kBlue+1);
                info->DrawLatex(xInfo, yPos, Form("Reco:  #vec{p} = (%.1f, %.1f, %.1f) MeV/c", 
                                                recoMomentumVec->X(), 
                                                recoMomentumVec->Y(), 
                                                recoMomentumVec->Z()));
                yPos -= 0.035;
                info->DrawLatex(xInfo, yPos, Form("       |#vec{p}| = %.1f MeV/c", 
                                                recoMomentumVec->Mag()));
                yPos -= 0.04;
            }
            
            if (trueMomentumVec && recoMomentumVec) {
                TVector3 diff = *recoMomentumVec - *trueMomentumVec;
                info->SetTextColor(kRed+1);
                info->DrawLatex(xInfo, yPos, Form("#Delta#vec{p} = (%.1f, %.1f, %.1f) MeV/c", 
                                                diff.X(), diff.Y(), diff.Z()));
                yPos -= 0.035;
                info->DrawLatex(xInfo, yPos, Form("       |#Delta#vec{p}| = %.1f MeV/c (%.1f%%)", 
                                                diff.Mag(), 
                                                100.0 * diff.Mag() / trueMomentumVec->Mag()));
            }
        }
    }
    
    c->Update();
    return c;
}

TCanvas* ReconstructionVisualizer::PlotTrajectory3D(
    const std::vector<TrajectoryPoint>& trajectory,
    const TVector3& targetPos, const std::string& title,
    const std::vector<TrajectoryPoint>* trueTrajectory) {
    
    TCanvas* c = CreateCanvas("c_traj_3d", title.c_str(), 1000, 800);
    
    if (trajectory.empty()) {
        std::cerr << "Empty reconstructed trajectory!" << std::endl;
        return c;
    }
    
    // 1. If true trajectory exists, draw it first (green)
    TPolyLine3D* trueLine = nullptr;
    if (trueTrajectory && !trueTrajectory->empty()) {
        trueLine = new TPolyLine3D(trueTrajectory->size());
        for (size_t i = 0; i < trueTrajectory->size(); i++) {
            trueLine->SetPoint(i, (*trueTrajectory)[i].position.X(), 
                                  (*trueTrajectory)[i].position.Y(), 
                                  (*trueTrajectory)[i].position.Z());
        }
        trueLine->SetLineColor(kGreen+2);
        trueLine->SetLineWidth(3);
        trueLine->SetLineStyle(1);
        trueLine->Draw();
    }
    
    // 2. Draw reconstructed trajectory (red)
    TPolyLine3D* recoLine = new TPolyLine3D(trajectory.size());
    for (size_t i = 0; i < trajectory.size(); i++) {
        recoLine->SetPoint(i, trajectory[i].position.X(), 
                              trajectory[i].position.Y(), 
                              trajectory[i].position.Z());
    }
    recoLine->SetLineColor(kRed);
    recoLine->SetLineWidth(2);
    recoLine->SetLineStyle(2);  // Dashed line
    
    if (trueLine) {
        recoLine->Draw("SAME");
    } else {
        recoLine->Draw();
    }
    
    // 3. Mark target point (large star, magenta)
    TPolyMarker3D* targetMarker = new TPolyMarker3D(1);
    targetMarker->SetPoint(0, targetPos.X(), targetPos.Y(), targetPos.Z());
    targetMarker->SetMarkerStyle(29);
    targetMarker->SetMarkerSize(3);
    targetMarker->SetMarkerColor(kMagenta);
    targetMarker->Draw();
    
    // 4. If true trajectory exists, mark its start and end points
    if (trueTrajectory && !trueTrajectory->empty()) {
        TPolyMarker3D* trueStartMarker = new TPolyMarker3D(1);
        trueStartMarker->SetPoint(0, trueTrajectory->front().position.X(),
                                    trueTrajectory->front().position.Y(),
                                    trueTrajectory->front().position.Z());
        trueStartMarker->SetMarkerStyle(20);
        trueStartMarker->SetMarkerSize(1.5);
        trueStartMarker->SetMarkerColor(kGreen+2);
        trueStartMarker->Draw();
    }
    
    // 5. Mark reconstructed trajectory start and end points
    TPolyMarker3D* recoStartMarker = new TPolyMarker3D(1);
    recoStartMarker->SetPoint(0, trajectory.front().position.X(),
                                trajectory.front().position.Y(),
                                trajectory.front().position.Z());
    recoStartMarker->SetMarkerStyle(20);
    recoStartMarker->SetMarkerSize(1.5);
    recoStartMarker->SetMarkerColor(kRed);
    recoStartMarker->Draw();
    
    TPolyMarker3D* recoEndMarker = new TPolyMarker3D(1);
    recoEndMarker->SetPoint(0, trajectory.back().position.X(),
                              trajectory.back().position.Y(),
                              trajectory.back().position.Z());
    recoEndMarker->SetMarkerStyle(24);
    recoEndMarker->SetMarkerSize(1.5);
    recoEndMarker->SetMarkerColor(kBlue);
    recoEndMarker->Draw();
    
    // 6. Add legend
    TLegend* legend = new TLegend(0.70, 0.75, 0.88, 0.88);
    legend->SetBorderSize(1);
    legend->SetFillStyle(1001);
    if (trueLine) {
        legend->AddEntry(trueLine, "True Trajectory", "l");
    }
    legend->AddEntry(recoLine, "Reconstructed Trajectory", "l");
    legend->AddEntry(targetMarker, "Target", "p");
    legend->AddEntry(recoEndMarker, "PDC Detector", "p");
    legend->Draw();
    
    c->Update();
    return c;
}

TCanvas* ReconstructionVisualizer::PlotMultipleTrajectories(
    const std::vector<std::vector<TrajectoryPoint>>& trajectories,
    const std::vector<double>& momenta,
    const TVector3& targetPos, const std::string& title) {
    
    TCanvas* c = CreateCanvas("c_multi_traj", title.c_str(), 1200, 800);
    c->Divide(2, 1);
    
    if (trajectories.empty()) {
        std::cerr << "No trajectories to plot!" << std::endl;
        return c;
    }
    
    // Left plot: XZ projection
    c->cd(1);
    gPad->SetGrid();
    
    TMultiGraph* mgXZ = new TMultiGraph();
    mgXZ->SetTitle("XZ Projection;Z [mm];X [mm]");
    
    for (size_t iTraj = 0; iTraj < trajectories.size(); iTraj++) {
        const auto& traj = trajectories[iTraj];
        if (traj.empty()) continue;
        
        TGraph* gr = new TGraph(traj.size());
        for (size_t i = 0; i < traj.size(); i++) {
            gr->SetPoint(i, traj[i].position.Z(), traj[i].position.X());
        }
        
        int color = kBlue + (iTraj % 10);
        gr->SetLineColor(color);
        gr->SetLineWidth(1);
        mgXZ->Add(gr, "L");
    }
    
    mgXZ->Draw("A");
    
    // Mark target point
    TMarker* targetXZ = new TMarker(targetPos.Z(), targetPos.X(), 29);
    targetXZ->SetMarkerColor(kRed);
    targetXZ->SetMarkerSize(2);
    targetXZ->Draw();
    
    // Right plot: YZ projection
    c->cd(2);
    gPad->SetGrid();
    
    TMultiGraph* mgYZ = new TMultiGraph();
    mgYZ->SetTitle("YZ Projection;Z [mm];Y [mm]");
    
    for (size_t iTraj = 0; iTraj < trajectories.size(); iTraj++) {
        const auto& traj = trajectories[iTraj];
        if (traj.empty()) continue;
        
        TGraph* gr = new TGraph(traj.size());
        for (size_t i = 0; i < traj.size(); i++) {
            gr->SetPoint(i, traj[i].position.Z(), traj[i].position.Y());
        }
        
        int color = kBlue + (iTraj % 10);
        gr->SetLineColor(color);
        gr->SetLineWidth(1);
        mgYZ->Add(gr, "L");
    }
    
    mgYZ->Draw("A");
    
    // Mark target point
    TMarker* targetYZ = new TMarker(targetPos.Z(), targetPos.Y(), 29);
    targetYZ->SetMarkerColor(kRed);
    targetYZ->SetMarkerSize(2);
    targetYZ->Draw();
    
    c->Update();
    return c;
}

void ReconstructionVisualizer::SavePlots(const std::string& outputDir) {
    // Create output directory
    gSystem->mkdir(outputDir.c_str(), kTRUE);
    
    std::cout << "\nSaving plots to: " << outputDir << std::endl;
    
    for (size_t i = 0; i < fCanvases.size(); i++) {
        if (fCanvases[i]) {
            std::string filename = outputDir + "/" + fCanvases[i]->GetName() + ".png";
            fCanvases[i]->SaveAs(filename.c_str());
            std::cout << "  Saved: " << filename << std::endl;
            
            // Also save ROOT file
            filename = outputDir + "/" + fCanvases[i]->GetName() + ".root";
            fCanvases[i]->SaveAs(filename.c_str());
        }
    }
}

void ReconstructionVisualizer::Clear() {
    fSteps.clear();
    
    for (auto* canvas : fCanvases) {
        if (canvas) delete canvas;
    }
    fCanvases.clear();
}

void ReconstructionVisualizer::PrintStatistics() const {
    if (fSteps.empty()) {
        std::cout << "No optimization steps recorded." << std::endl;
        return;
    }
    
    std::cout << "\n=== Optimization Statistics ===" << std::endl;
    std::cout << "Total iterations: " << fSteps.size() << std::endl;
    
    double initialDist = fSteps.front().distance;
    double finalDist = fSteps.back().distance;
    double improvement = (initialDist - finalDist) / initialDist * 100.0;
    
    std::cout << "Initial distance: " << initialDist << " mm" << std::endl;
    std::cout << "Final distance: " << finalDist << " mm" << std::endl;
    std::cout << "Improvement: " << improvement << "%" << std::endl;
    
    std::cout << "\nIteration details:" << std::endl;
    std::cout << std::setw(8) << "Iter" 
              << std::setw(15) << "Momentum[MeV/c]" 
              << std::setw(15) << "Distance[mm]";
    
    // Check if gradient information exists
    bool hasGradient = !fSteps.empty() && fSteps.front().gradient != 0.0;
    if (hasGradient) {
        std::cout << std::setw(15) << "Gradient";
    }
    std::cout << std::endl;
    
    for (const auto& step : fSteps) {
        std::cout << std::setw(8) << step.iteration
                  << std::setw(15) << std::fixed << std::setprecision(2) << step.momentum
                  << std::setw(15) << std::fixed << std::setprecision(4) << step.distance;
        
        if (hasGradient) {
            std::cout << std::setw(15) << std::scientific << std::setprecision(4) << step.gradient;
        }
        std::cout << std::endl;
    }
    
    std::cout << "==============================\n" << std::endl;
}

TCanvas* ReconstructionVisualizer::CreateCanvas(const std::string& name, 
                                               const std::string& title,
                                               int width, int height) {
    TCanvas* c = new TCanvas(name.c_str(), title.c_str(), width, height);
    fCanvases.push_back(c);
    return c;
}
