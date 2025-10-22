/*
 * Visualize magnetic field with symmetry extension
 * Shows the full field including negative x and z regions
 */

void plot_extended_field(double rotationAngle = 30.0)
{
    cout << "=== Plotting Extended Magnetic Field Distribution ===" << endl;
    cout << "Rotation angle: " << rotationAngle << " degrees" << endl;
    
    // Load magnetic field
    MagneticField* field = new MagneticField();
    if (!field->LoadFromROOTFile("magnetic_field_test.root")) {
        cout << "ERROR: Could not load magnetic field file!" << endl;
        return;
    }
    
    field->SetRotationAngle(rotationAngle);
    
    // Create canvas with 2x2 layout
    TCanvas* c1 = new TCanvas("c1", "Extended Magnetic Field Distribution", 1200, 1000);
    c1->Divide(2, 2);
    
    // Parameters for plotting - extend range to negative values
    int nBins = 100;
    double xMin = -1500, xMax = 1500;  // Extended x range
    double zMin = -1500, zMax = 1500;  // Extended z range
    double y = 0;  // Y=0 plane
    
    // 1. Magnetic field magnitude in XZ plane (Y=0)
    c1->cd(1);
    TH2F* h_mag = new TH2F("h_mag", "Magnetic Field Magnitude |B| (Y=0, Extended)", 
                           nBins, xMin, xMax, nBins, zMin, zMax);
    h_mag->GetXaxis()->SetTitle("X [mm]");
    h_mag->GetYaxis()->SetTitle("Z [mm]");
    h_mag->GetZaxis()->SetTitle("|B| [T]");
    
    for (int i = 1; i <= nBins; i++) {
        for (int j = 1; j <= nBins; j++) {
            double x = h_mag->GetXaxis()->GetBinCenter(i);
            double z = h_mag->GetYaxis()->GetBinCenter(j);
            TVector3 B = field->GetField(x, y, z);
            double mag = B.Mag();
            h_mag->SetBinContent(i, j, mag);
        }
        if (i % 20 == 0) cout << "Progress: " << (i*100/nBins) << "%" << endl;
    }
    h_mag->Draw("COLZ");
    
    // Add lines to show original data boundaries
    TLine* line1 = new TLine(0, zMin, 0, zMax);
    line1->SetLineColor(kRed);
    line1->SetLineWidth(2);
    line1->Draw();
    TLine* line2 = new TLine(xMin, 0, xMax, 0);
    line2->SetLineColor(kRed);
    line2->SetLineWidth(2);
    line2->Draw();
    
    // Add text annotation
    TLatex* text1 = new TLatex(200, 1300, "Original data");
    text1->SetTextColor(kRed);
    text1->SetTextSize(0.04);
    text1->Draw();
    TLatex* text2 = new TLatex(-1300, 1300, "X-mirrored");
    text2->SetTextColor(kBlue);
    text2->SetTextSize(0.04);
    text2->Draw();
    TLatex* text3 = new TLatex(200, -1300, "Z-mirrored");
    text3->SetTextColor(kGreen+2);
    text3->SetTextSize(0.04);
    text3->Draw();
    TLatex* text4 = new TLatex(-1300, -1300, "Both mirrored");
    text4->SetTextColor(kMagenta);
    text4->SetTextSize(0.04);
    text4->Draw();
    
    // 2. Bx component
    c1->cd(2);
    TH2F* h_bx = new TH2F("h_bx", "Bx Component (Y=0, Extended)", 
                          nBins, xMin, xMax, nBins, zMin, zMax);
    h_bx->GetXaxis()->SetTitle("X [mm]");
    h_bx->GetYaxis()->SetTitle("Z [mm]");
    h_bx->GetZaxis()->SetTitle("Bx [T]");
    
    for (int i = 1; i <= nBins; i++) {
        for (int j = 1; j <= nBins; j++) {
            double x = h_bx->GetXaxis()->GetBinCenter(i);
            double z = h_bx->GetYaxis()->GetBinCenter(j);
            TVector3 B = field->GetField(x, y, z);
            h_bx->SetBinContent(i, j, B.X());
        }
    }
    h_bx->Draw("COLZ");
    line1->Clone()->Draw();
    line2->Clone()->Draw();
    
    // 3. By component
    c1->cd(3);
    TH2F* h_by = new TH2F("h_by", "By Component (Y=0, Extended)", 
                          nBins, xMin, xMax, nBins, zMin, zMax);
    h_by->GetXaxis()->SetTitle("X [mm]");
    h_by->GetYaxis()->SetTitle("Z [mm]");
    h_by->GetZaxis()->SetTitle("By [T]");
    
    for (int i = 1; i <= nBins; i++) {
        for (int j = 1; j <= nBins; j++) {
            double x = h_by->GetXaxis()->GetBinCenter(i);
            double z = h_by->GetYaxis()->GetBinCenter(j);
            TVector3 B = field->GetField(x, y, z);
            h_by->SetBinContent(i, j, B.Y());
        }
    }
    h_by->Draw("COLZ");
    line1->Clone()->Draw();
    line2->Clone()->Draw();
    
    // 4. Bz component
    c1->cd(4);
    TH2F* h_bz = new TH2F("h_bz", "Bz Component (Y=0, Extended)", 
                          nBins, xMin, xMax, nBins, zMin, zMax);
    h_bz->GetXaxis()->SetTitle("X [mm]");
    h_bz->GetYaxis()->SetTitle("Z [mm]");
    h_bz->GetZaxis()->SetTitle("Bz [T]");
    
    for (int i = 1; i <= nBins; i++) {
        for (int j = 1; j <= nBins; j++) {
            double x = h_bz->GetXaxis()->GetBinCenter(i);
            double z = h_bz->GetYaxis()->GetBinCenter(j);
            TVector3 B = field->GetField(x, y, z);
            h_bz->SetBinContent(i, j, B.Z());
        }
    }
    h_bz->Draw("COLZ");
    line1->Clone()->Draw();
    line2->Clone()->Draw();
    
    // Update canvas
    c1->Update();
    
    // Save plots
    TString filename = Form("extended_magnetic_field_%.0fdeg", rotationAngle);
    c1->SaveAs(filename + ".png");
    c1->SaveAs(filename + ".pdf");
    
    // Print summary
    cout << "\n=== Extended Field Summary ===" << endl;
    cout << "Original data coverage: X ∈ [0, 3000] mm, Z ∈ [0, 3000] mm" << endl;
    cout << "Extended coverage: X ∈ [-3000, 3000] mm, Z ∈ [-3000, 3000] mm" << endl;
    cout << "Effective coverage increase: 4x (quadrupled area)" << endl;
    
    // Test some field values to verify symmetry
    cout << "\nSymmetry verification at selected points:" << endl;
    double test_points[][2] = {{500, 500}, {-500, 500}, {500, -500}, {-500, -500}};
    for (int i = 0; i < 4; i++) {
        double x = test_points[i][0];
        double z = test_points[i][1];
        TVector3 B = field->GetField(x, 0, z);
        cout << Form("B(%+4.0f, 0, %+4.0f) = (%+.6f, %+.6f, %+.6f) T", 
                     x, z, B.X(), B.Y(), B.Z()) << endl;
    }
    
    cout << "\nPlots saved as: " << filename << ".png/.pdf" << endl;
    cout << "=== Plotting completed ===" << endl;
    
    delete field;
}