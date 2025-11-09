{
    // Automatically load required libraries when ROOT starts
    cout << "Loading SM libraries from build directory..." << endl;
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5/build/sources/smg4lib/data/libsmdata.so");
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5/build/sources/smg4lib/action/libsmaction.so"); 
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5/build/sources/smg4lib/construction/libsmconstruction.so");
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5/build/sources/smg4lib/physics/libsmphysics.so");
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5/build/sources/sim_deuteron/libsim_deuteron.so");
    
    cout << "Loading PDC Analysis Tools..." << endl;
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5/d_work/sources/build/libPDCAnalysisTools.so");
    
    cout << "All libraries loaded successfully!" << endl;
}
