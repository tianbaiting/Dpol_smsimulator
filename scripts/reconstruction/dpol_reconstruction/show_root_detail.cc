void show_root_detail()
{
    // Show detailed information about the ROOT file
    std::cout << "Showing ROOT file details..." << std::endl;
    
    // Open the ROOT file
    TFile* file = TFile::Open("../../../output_tree/d+Pb208E190g050xyz0000.root");
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Cannot open ROOT file!" << std::endl;
        return;
    }
    
    // Get the tree
    TTree* tree = (TTree*)file->Get("tree");
    if (!tree) {
        std::cerr << "Error: Cannot find tree in ROOT file!" << std::endl;
        file->Close();
        return;
    }
    
    std::cout << "\n=== ROOT File Information ===" << std::endl;
    std::cout << "File: " << file->GetName() << std::endl;
    std::cout << "Total entries: " << tree->GetEntries() << std::endl;
    std::cout << "Tree size: " << tree->GetTotBytes() << " bytes" << std::endl;
    
    if (tree->GetEntries() == 0) {
        std::cout << "No entries in the tree!" << std::endl;
        file->Close();
        return;
    }
    
    // Print the first event's information
    std::cout << "\n=== First Event Details (Event 0) ===" << std::endl;
    
    // Show the first event with all branch details
    tree->Show(0);
    
    std::cout << "\n=== Branch Structure ===" << std::endl;
    tree->Print();
    
    // Additional analysis for the first event
    std::cout << "\n=== Detailed Analysis of Event 0 ===" << std::endl;
    
    // Set up variables to read specific branches
    Int_t beam_;
    Int_t FragSimData_;
    
    tree->SetBranchAddress("beam", &beam_);
    tree->SetBranchAddress("FragSimData", &FragSimData_);
    
    // Read the first event
    tree->GetEntry(0);
    
    std::cout << "Beam particles in event 0: " << beam_ << std::endl;
    std::cout << "FragSimData entries in event 0: " << FragSimData_ << std::endl;
    
    // Detailed FragSimData information
    std::cout << "\n=== FragSimData Detailed Information ===" << std::endl;
    if (FragSimData_ > 0) {
        std::cout << "Number of FragSimData entries: " << FragSimData_ << std::endl;
        
        // Use tree->Scan to show specific branches
        std::cout << "\n--- Basic Particle Information ---" << std::endl;
        tree->Scan("FragSimData.fTrackID:FragSimData.fZ:FragSimData.fA:FragSimData.fPDGCode", "", "", 1, 0);
        
        std::cout << "\n--- Particle Names and Detector Info ---" << std::endl;
        tree->Scan("FragSimData.fParticleName:FragSimData.fDetectorName:FragSimData.fProcessName", "", "", 1, 0);
        
        std::cout << "\n--- Energy Information ---" << std::endl;
        tree->Scan("FragSimData.fPreKineticEnergy:FragSimData.fPostKineticEnergy:FragSimData.fEnergyDeposit", "", "", 1, 0);
        
        std::cout << "\n--- Charge and Mass ---" << std::endl;
        tree->Scan("FragSimData.fCharge:FragSimData.fMass", "", "", 1, 0);
        
        std::cout << "\n--- Step Information ---" << std::endl;
        tree->Scan("FragSimData.fParentID:FragSimData.fStepNo", "", "", 1, 0);
        
    } else {
        std::cout << "No FragSimData entries in this event." << std::endl;
    }
    
    // Check if we have NEBULA data
    TBranch* nebulaBranch = tree->GetBranch("NEBULAPla");
    if (nebulaBranch) {
        std::cout << "NEBULA detector data is present" << std::endl;
    }
    
    // Check PDC data
    TBranch* pdc1Branch = tree->GetBranch("PDC1U_x");
    if (pdc1Branch) {
        std::cout << "PDC tracking data is present" << std::endl;
    }
    
    // Print some key physics variables for event 0
    std::cout << "\n=== Key Physics Variables for Event 0 ===" << std::endl;
    
    // Target reconstruction variables
    Double_t target_x, target_y, target_z;
    tree->SetBranchAddress("target_x", &target_x);
    tree->SetBranchAddress("target_y", &target_y);
    tree->SetBranchAddress("target_z", &target_z);
    tree->GetEntry(0);
    
    std::cout << "Target position: (" << target_x << ", " << target_y << ", " << target_z << ") mm" << std::endl;
    
    std::cout << "\n=== Complete Event Display ===" << std::endl;
    std::cout << "Use tree->Show(0) output above for complete details." << std::endl;
    
    file->Close();
    std::cout << "\nAnalysis completed!" << std::endl;
}