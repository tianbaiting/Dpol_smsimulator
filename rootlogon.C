{
    // [EN] ROOT may start before setup.sh has populated the dynamic loader cache; add the build library directory from SMSIMDIR explicitly. / [CN] ROOT 启动时动态库缓存可能还没有包含项目库目录；这里显式加入 SMSIMDIR 指向的 build/lib。
    cout << "\n=== Loading SMSIM libraries ===" << endl;
    
    const char* smsim_dir = gSystem->Getenv("SMSIMDIR");
    if (!smsim_dir) {
        cout << "Error: SMSIMDIR environment variable not set!" << endl;
    } else {
        TString libPath = TString::Format("%s/build/lib", smsim_dir);
        gSystem->AddDynamicPath(libPath);
        cout << "Library path added: " << libPath << endl;
    }

    const TString libs[] = {
        "libsmdata.so",
        "libsmlogger.so",
        "libsmaction.so",
        "libsmconstruction.so",
        "libsmphysics.so",
        "libsim_deuteron_core.so"
    };
    
    for (const auto& lib : libs) {
        if (gSystem->Load(lib) >= 0) {
            cout << "  [OK] " << lib << endl;
        } else {
            cout << "  [XX] Failed to load: " << lib << endl;
        }
    }
    
    cout << "--- Loading Analysis Tools ---" << endl;
    if (gSystem->Load("libpdcanalysis.so") >= 0) {
        cout << "  [OK] libpdcanalysis.so" << endl;
    } else if (gSystem->Load("libanalysis.so") >= 0) {
        cout << "  [OK] libanalysis.so" << endl;
    } else {
        cout << "  [!!] Warning: Analysis library not found" << endl;
    }
    cout << "================================\n" << endl;
}
