{
    // [EN] Load project libraries when ROOT starts; the path is resolved at runtime so this file stays portable across hosts. / [CN] ROOT 启动时加载项目库；路径在运行时解析，保证该文件可跨主机提交。
    cout << "Loading SM libraries from build directory..." << endl;

    const char* smsim_dir = gSystem->Getenv("SMSIMDIR");
    if (smsim_dir && smsim_dir[0] != '\0') {
        TString libPath = TString::Format("%s/build/lib", smsim_dir);
        gSystem->AddDynamicPath(libPath);
        cout << "  Library path added: " << libPath << endl;
    } else {
        cout << "  Warning: SMSIMDIR is not set; relying on LD_LIBRARY_PATH" << endl;
    }

    TString libs[] = {
        "libsmdata.so",
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
            // [EN] ROOT reports a negative code for missing or already-loaded optional libraries; continue so analysis macros can still run with partial builds. / [CN] ROOT 对缺失或已加载的可选库可能返回负值；继续执行以支持部分构建环境下的分析宏。
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
    if (smsim_dir && smsim_dir[0] != '\0') {
        cout << "  Library search path includes: " << TString::Format("%s/build/lib", smsim_dir) << "\n" << endl;
    }
}
