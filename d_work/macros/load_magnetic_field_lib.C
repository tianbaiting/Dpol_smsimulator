// 只加载库，不包含自定义类型
{
    // 设置 LD_LIBRARY_PATH (用于 libgl2ps)
    gSystem->Setenv("LD_LIBRARY_PATH", 
        (TString("/usr/lib/x86_64-linux-gnu:") + gSystem->Getenv("LD_LIBRARY_PATH")).Data());
    
    // 添加头文件搜索路径
    gSystem->AddIncludePath("-I/home/tian/workspace/dpol/smsimulator5.5/d_work/sources/include");
    
    // 如果设置了 G4SMLIBDIR 环境变量,也添加它
    const char* g4smlibdir = gSystem->Getenv("G4SMLIBDIR");
    if (g4smlibdir) {
        gSystem->AddIncludePath(Form("-I%s/include", g4smlibdir));
    }
    
    // 添加库搜索路径
    gSystem->AddDynamicPath("/home/tian/workspace/dpol/smsimulator5.5/d_work/sources/build");
    
    // 加载库
    if (gSystem->Load("libPDCAnalysisTools.so") < 0) {
        Error("load_magnetic_field_lib", "无法加载 libPDCAnalysisTools.so 库");
        Error("load_magnetic_field_lib", "请确保已经编译了库: cd sources/build && cmake .. && make");
    } else {
        Info("load_magnetic_field_lib", "库已成功加载，包含 MagneticField 类");
    }
}