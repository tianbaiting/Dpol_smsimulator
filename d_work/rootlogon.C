{
    // Automatically load required libraries when ROOT starts
    cout << "Loading SM libraries from build directory..." << endl;
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir) {
        cout << "Environment variable SMSIMDIR is not set; skipping automatic library loads that depend on it." << endl;
    } else {
        std::string smsBase(smsDir);
        gSystem->Load((smsBase + "/build/sources/smg4lib/data/libsmdata.so").c_str());
        gSystem->Load((smsBase + "/build/sources/smg4lib/action/libsmaction.so").c_str()); 
        gSystem->Load((smsBase + "/build/sources/smg4lib/construction/libsmconstruction.so").c_str());
        gSystem->Load((smsBase + "/build/sources/smg4lib/physics/libsmphysics.so").c_str());
        gSystem->Load((smsBase + "/build/sources/sim_deuteron/libsim_deuteron.so").c_str());
        
        cout << "Loading PDC Analysis Tools..." << endl;
        gSystem->Load((smsBase + "/d_work/sources/build/libPDCAnalysisTools.so").c_str());
    }
    
    cout << "All libraries (attempted) loaded." << endl;
}
