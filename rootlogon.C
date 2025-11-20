{
    // Automatically load required libraries when ROOT starts
    cout << "Loading SM libraries from build directory..." << endl;
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5_new/build/lib/libsmdata.so");
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5_new/build/lib/libsmaction.so"); 
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5_new/build/lib/libsmconstruction.so");
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5_new/build/lib/libsmphysics.so");
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5_new/build/lib/libsim_deuteron_core.so");
    
    cout << "Loading PDC Analysis Tools..." << endl;
    gSystem->Load("/home/tian/workspace/dpol/smsimulator5.5_new/build/lib/libanalysis.so");
    
    cout << "All libraries loaded successfully!" << endl;
}
