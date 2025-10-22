/*
 * Test script for magnetic field symmetry functionality
 * Tests that negative x and z coordinates are properly handled through symmetry
 */

void test_symmetry()
{
    cout << "=== Magnetic Field Symmetry Test ===" << endl;
    
    // Create magnetic field object
    MagneticField* field = new MagneticField();
    
    // Load magnetic field from ROOT file (faster than text file)
    if (!field->LoadFromROOTFile("magnetic_field_test.root")) {
        cout << "ERROR: Could not load magnetic field file!" << endl;
        return;
    }
    
    // Test points for symmetry verification
    double test_x[] = {100, -100, 100, -100};  // positive and negative x
    double test_y[] = {0, 0, 0, 0};            // y=0 plane
    double test_z[] = {50, 50, -50, -50};      // positive and negative z
    
    cout << "\nTesting magnetic field symmetry:" << endl;
    cout << "Expected symmetry rules:" << endl;
    cout << "  x → -x: Bx → -Bx, By → By, Bz → Bz" << endl;
    cout << "  z → -z: Bx → Bx, By → By, Bz → -Bz" << endl;
    cout << endl;
    
    cout << setw(8) << "X[mm]" << setw(8) << "Y[mm]" << setw(8) << "Z[mm]" 
         << setw(12) << "Bx[T]" << setw(12) << "By[T]" << setw(12) << "Bz[T]" << endl;
    cout << string(68, '-') << endl;
    
    for (int i = 0; i < 4; i++) {
        TVector3 B = field->GetFieldRaw(test_x[i], test_y[i], test_z[i]);
        cout << setw(8) << test_x[i] << setw(8) << test_y[i] << setw(8) << test_z[i]
             << setw(12) << fixed << setprecision(6) << B.X()
             << setw(12) << fixed << setprecision(6) << B.Y() 
             << setw(12) << fixed << setprecision(6) << B.Z() << endl;
    }
    
    cout << endl;
    
    // Verify symmetry relationships
    TVector3 B_pos_pos = field->GetFieldRaw(100, 0, 50);   // (+x, +z)
    TVector3 B_neg_pos = field->GetFieldRaw(-100, 0, 50);  // (-x, +z)
    TVector3 B_pos_neg = field->GetFieldRaw(100, 0, -50);  // (+x, -z)
    TVector3 B_neg_neg = field->GetFieldRaw(-100, 0, -50); // (-x, -z)
    
    cout << "Symmetry verification:" << endl;
    
    // Test x-symmetry: B(-x,y,z) vs B(x,y,z)
    cout << "X-symmetry test (x → -x):" << endl;
    cout << "  B(+100,0,+50) = (" << B_pos_pos.X() << ", " << B_pos_pos.Y() << ", " << B_pos_pos.Z() << ")" << endl;
    cout << "  B(-100,0,+50) = (" << B_neg_pos.X() << ", " << B_neg_pos.Y() << ", " << B_neg_pos.Z() << ")" << endl;
    cout << "  Expected: Bx sign flipped, By/Bz same" << endl;
    cout << "  Check: Bx flip: " << (TMath::Abs(B_pos_pos.X() + B_neg_pos.X()) < 1e-6 ? "✓" : "✗") << endl;
    cout << "         By same: " << (TMath::Abs(B_pos_pos.Y() - B_neg_pos.Y()) < 1e-6 ? "✓" : "✗") << endl;
    cout << "         Bz same: " << (TMath::Abs(B_pos_pos.Z() - B_neg_pos.Z()) < 1e-6 ? "✓" : "✗") << endl;
    cout << endl;
    
    // Test z-symmetry: B(x,y,-z) vs B(x,y,z) 
    cout << "Z-symmetry test (z → -z):" << endl;
    cout << "  B(+100,0,+50) = (" << B_pos_pos.X() << ", " << B_pos_pos.Y() << ", " << B_pos_pos.Z() << ")" << endl;
    cout << "  B(+100,0,-50) = (" << B_pos_neg.X() << ", " << B_pos_neg.Y() << ", " << B_pos_neg.Z() << ")" << endl;
    cout << "  Expected: Bz sign flipped, Bx/By same" << endl;
    cout << "  Check: Bx same: " << (TMath::Abs(B_pos_pos.X() - B_pos_neg.X()) < 1e-6 ? "✓" : "✗") << endl;
    cout << "         By same: " << (TMath::Abs(B_pos_pos.Y() - B_pos_neg.Y()) < 1e-6 ? "✓" : "✗") << endl;
    cout << "         Bz flip: " << (TMath::Abs(B_pos_pos.Z() + B_pos_neg.Z()) < 1e-6 ? "✓" : "✗") << endl;
    cout << endl;
    
    // Test both symmetries: B(-x,y,-z) vs B(x,y,z)
    cout << "Both symmetries test (x → -x, z → -z):" << endl;
    cout << "  B(+100,0,+50) = (" << B_pos_pos.X() << ", " << B_pos_pos.Y() << ", " << B_pos_pos.Z() << ")" << endl;
    cout << "  B(-100,0,-50) = (" << B_neg_neg.X() << ", " << B_neg_neg.Y() << ", " << B_neg_neg.Z() << ")" << endl;
    cout << "  Expected: Bx flipped, By same, Bz flipped" << endl;
    cout << "  Check: Bx flip: " << (TMath::Abs(B_pos_pos.X() + B_neg_neg.X()) < 1e-6 ? "✓" : "✗") << endl;
    cout << "         By same: " << (TMath::Abs(B_pos_pos.Y() - B_neg_neg.Y()) < 1e-6 ? "✓" : "✗") << endl;
    cout << "         Bz flip: " << (TMath::Abs(B_pos_pos.Z() + B_neg_neg.Z()) < 1e-6 ? "✓" : "✗") << endl;
    cout << endl;
    
    // Test coverage expansion
    cout << "Coverage test:" << endl;
    field->PrintInfo();
    
    cout << "\nTesting field access in extended regions:" << endl;
    TVector3 B_extended1 = field->GetFieldRaw(-200, 0, 0);   // Negative x
    TVector3 B_extended2 = field->GetFieldRaw(0, 0, -100);   // Negative z
    TVector3 B_extended3 = field->GetFieldRaw(-150, 0, -200); // Both negative
    
    cout << "B(-200,0,0)   = (" << B_extended1.X() << ", " << B_extended1.Y() << ", " << B_extended1.Z() << ")" << endl;
    cout << "B(0,0,-100)   = (" << B_extended2.X() << ", " << B_extended2.Y() << ", " << B_extended2.Z() << ")" << endl;
    cout << "B(-150,0,-200) = (" << B_extended3.X() << ", " << B_extended3.Y() << ", " << B_extended3.Z() << ")" << endl;
    
    delete field;
    
    cout << "\n=== Symmetry test completed ===" << endl;
}