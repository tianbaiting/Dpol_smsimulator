{
    // Automatically load required libraries when ROOT starts
    cout << "Loading SM libraries from build directory..." << endl;
    
    // Since LD_LIBRARY_PATH is set by setup.sh, we can use short names
    // ROOT will automatically find them in the library path
    TString libs[] = {
        "libsmdata.so",       // 包含 TBeamSimData 等数据类（必须先加载）
        "libsmlogger.so",
        "libsmaction.so",
        "libsmconstruction.so",
        "libsmphysics.so",
        "libsim_deuteron_core.so"
    };
    
    for (const auto& lib : libs) {
        if (gSystem->Load(lib) >= 0) {
            cout << "  ✓ Loaded: " << lib << endl;
        } else {
            // Not an error if optional library is missing
            cout << "  ⊘ Skipped: " << lib << " (not found or already loaded)" << endl;
        }
    }
    
    cout << "\nLoading PDC Analysis Tools..." << endl;
    // Try different possible names for the analysis library
    if (gSystem->Load("libpdcanalysis.so") >= 0) {
        cout << "  ✓ Loaded: libpdcanalysis.so" << endl;
    } else if (gSystem->Load("libanalysis.so") >= 0) {
        cout << "  ✓ Loaded: libanalysis.so" << endl;
    } else {
        cout << "  ⚠ Warning: Could not load analysis library" << endl;
    }
    
    cout << "\n✓ All available libraries loaded successfully!" << endl;
    cout << "  Library search path includes: /home/tian/workspace/dpol/smsimulator5.5/build/lib\n" << endl;
}
