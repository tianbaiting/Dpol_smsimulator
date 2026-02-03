/**
 * @file RunQMDGeoFilter.cc
 * @brief QMD 几何筛选分析主程序
 * 
 * [EN] Usage Examples: / [CN] 使用示例:
 * 
 * [EN] Basic usage with default settings: / [CN] 使用默认设置:
 *   ./RunQMDGeoFilter
 * 
 * [EN] With specific field and angle: / [CN] 指定磁场和角度:
 *   ./RunQMDGeoFilter --field 1.0 --angle 5 --target Pb208 --pol zpol
 * 
 * [EN] With fixed PDC position (like sim_deuteron geometry): / [CN] 使用固定PDC位置:
 *   ./RunQMDGeoFilter --fixed-pdc --pdc-x -3500 --pdc-z 2500 --pdc-angle -30
 * 
 * [EN] With custom Px range for PDC acceptance: / [CN] 自定义Px范围:
 *   ./RunQMDGeoFilter --px-range 150
 * 
 * [EN] API for programmers: / [CN] 程序员API:
 * @code
 *   QMDGeoFilter filter;
 *   QMDGeoFilter::AnalysisConfig config;
 *   
 *   // [EN] Set magnetic field strength(s) / [CN] 设置磁场强度
 *   config.fieldStrengths = {1.0, 1.4};  // 1.0T and 1.4T
 *   
 *   // [EN] Set deflection angles / [CN] 设置偏转角度
 *   config.deflectionAngles = {5.0, 10.0};  // 5 and 10 degrees
 *   
 *   // [EN] Fixed PDC mode / [CN] 固定PDC模式
 *   config.useFixedPDC = true;
 *   config.fixedPDCPosition1 = TVector3(-3500, 0, 2500);  // [mm]
 *   config.fixedPDCPosition2 = TVector3(-3500, 0, 3500);  // [mm]
 *   config.fixedPDCRotationAngle = -30.0;  // [deg]
 *   
 *   // [EN] Or optimized PDC mode (default) / [CN] 或优化PDC模式（默认）
 *   config.useFixedPDC = false;
 *   config.pxRange = 150.0;  // PDC sized for Px=±150 MeV/c
 *   
 *   filter.SetConfig(config);
 *   filter.RunFullAnalysis();
 *   
 *   // [EN] Get results / [CN] 获取结果
 *   auto results = filter.GetAllResults();
 *   for (const auto& r : results) {
 *       // Access r.gammaResults, r.targetPosition, r.pdcConfig, etc.
 *   }
 * @endcode
 */

#include "QMDGeoFilter.hh"
#include "SMLogger.hh"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <getopt.h>

void PrintUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [OPTIONS]\n\n";
    std::cout << "QMD Geometry Filter Analysis Program\n\n";
    
    std::cout << "Basic Options:\n";
    std::cout << "  -f, --field <T>       Field strength in Tesla (can be specified multiple times)\n";
    std::cout << "  -a, --angle <deg>     Deflection angle in degrees (can be specified multiple times)\n";
    std::cout << "  -t, --target <name>   Target material (can be specified multiple times)\n";
    std::cout << "  -p, --pol <type>      Polarization type: zpol or ypol (can be specified multiple times)\n";
    std::cout << "  -g, --gamma <value>   Gamma value (can be specified multiple times)\n";
    std::cout << "\n";
    
    std::cout << "Path Options:\n";
    std::cout << "  -q, --qmd <path>      QMD data path\n";
    std::cout << "  -m, --fieldmap <path> Field map directory path\n";
    std::cout << "  -o, --output <path>   Output directory path\n";
    std::cout << "\n";
    
    std::cout << "PDC Configuration Options:\n";
    std::cout << "  --fixed-pdc           Use fixed PDC position instead of optimization\n";
    std::cout << "  --pdc-x <mm>          Fixed PDC X position (applies to PDC1/2)\n";
    std::cout << "  --pdc-y <mm>          Fixed PDC Y position (applies to PDC1/2)\n";
    std::cout << "  --pdc-z <mm>          Fixed PDC Z position (applies to PDC1/2)\n";
    std::cout << "  --pdc-angle <deg>     Fixed PDC rotation angle (default: 65)\n";
    std::cout << "  --pdc-macro <path>    PDC macro file (Angle/Position1/Position2)\n";
    std::cout << "  --px-range <MeV/c>    Px range for PDC acceptance (default: 100)\n";
    std::cout << "\n";
    
    std::cout << "Other Options:\n";
    std::cout << "  -h, --help            Show this help message\n";
    std::cout << "\n";
    
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " --field 1.0 --angle 5 --target Pb208 --pol zpol --pol ypol\n";
    std::cout << "  " << progName << " -f 0.8 -f 1.0 -a 0 -a 5 -a 10 -t Pb208 -p zpol\n";
    std::cout << "  " << progName << " --fixed-pdc --pdc-x -3500 --pdc-z 2500 --pdc-angle -30 --px-range 150\n";
    std::cout << "\n";
    
    std::cout << "PDC Position Note:\n";
    std::cout << "  The sim_deuteron geometry uses (SAMURAI definition):\n";
    std::cout << "    /samurai/geometry/PDC/Angle 65 deg\n";
    std::cout << "    /samurai/geometry/PDC/Position1 +0 0 400 cm\n";
    std::cout << "    /samurai/geometry/PDC/Position2 +0 0 500 cm\n";
    std::cout << "  QMDGeoFilter interprets fixed PDC Position1/2 in SAMURAI frame,\n";
    std::cout << "  then rotates around origin by -Angle to convert into lab frame.\n";
}

int main(int argc, char* argv[]) {
    // [EN] Default configuration / [CN] 默认配置
    QMDGeoFilter::AnalysisConfig config;
    config.fieldStrengths.clear();
    config.deflectionAngles.clear();
    config.targets.clear();
    config.polTypes.clear();
    config.gammaValues.clear();
    
    bool hasField = false, hasAngle = false, hasTarget = false, hasPol = false, hasGamma = false;
    
    // [EN] PDC configuration - default to FIXED mode / [CN] PDC配置 - 默认使用固定模式
    bool useFixedPDC = true;  // [EN] Default: fixed PDC / [CN] 默认：固定PDC位置
    double pdcX = 0.0, pdcY = 0.0, pdcZ = 4000.0;
    double pdcAngle = 65.0;
    std::string pdcMacroPath;
    double pxRange = 100.0;
    
    // [EN] Command line options / [CN] 命令行选项
    static struct option long_options[] = {
        {"field",     required_argument, 0, 'f'},
        {"angle",     required_argument, 0, 'a'},
        {"target",    required_argument, 0, 't'},
        {"pol",       required_argument, 0, 'p'},
        {"gamma",     required_argument, 0, 'g'},
        {"qmd",       required_argument, 0, 'q'},
        {"fieldmap",  required_argument, 0, 'm'},
        {"output",    required_argument, 0, 'o'},
        {"fixed-pdc", no_argument,       0, 'F'},
        {"pdc-x",     required_argument, 0, 'X'},
        {"pdc-y",     required_argument, 0, 'Y'},
        {"pdc-z",     required_argument, 0, 'Z'},
        {"pdc-angle", required_argument, 0, 'R'},
        {"pdc-macro", required_argument, 0, 'M'},
        {"px-range",  required_argument, 0, 'P'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "f:a:t:p:g:q:m:o:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'f':
                config.fieldStrengths.push_back(std::stod(optarg));
                hasField = true;
                break;
            case 'a':
                config.deflectionAngles.push_back(std::stod(optarg));
                hasAngle = true;
                break;
            case 't':
                config.targets.push_back(optarg);
                hasTarget = true;
                break;
            case 'p':
                config.polTypes.push_back(optarg);
                hasPol = true;
                break;
            case 'g':
                config.gammaValues.push_back(optarg);
                hasGamma = true;
                break;
            case 'q':
                config.qmdDataPath = optarg;
                break;
            case 'm':
                config.fieldMapPath = optarg;
                break;
            case 'o':
                config.outputPath = optarg;
                break;
            case 'F':  // --fixed-pdc
                useFixedPDC = true;
                break;
            case 'X':  // --pdc-x
                pdcX = std::stod(optarg);
                break;
            case 'Y':  // --pdc-y
                pdcY = std::stod(optarg);
                break;
            case 'Z':  // --pdc-z
                pdcZ = std::stod(optarg);
                break;
            case 'R':  // --pdc-angle
                pdcAngle = std::stod(optarg);
                break;
            case 'M':  // --pdc-macro
                pdcMacroPath = optarg;
                break;
            case 'P':  // --px-range
                pxRange = std::stod(optarg);
                break;
            case 'h':
                PrintUsage(argv[0]);
                return 0;
            default:
                PrintUsage(argv[0]);
                return 1;
        }
    }
    
    // [EN] Use default values if not specified / [CN] 使用默认值
    if (!hasField) config.fieldStrengths = {1.0};
    if (!hasAngle) config.deflectionAngles = {5.0};
    if (!hasTarget) config.targets = {"Pb208"};
    if (!hasPol) config.polTypes = {"zpol", "ypol"};
    if (!hasGamma) config.gammaValues = {"050", "060", "070", "080"};
    
    // [EN] Apply PDC configuration / [CN] 应用PDC配置
    config.useFixedPDC = useFixedPDC;
    config.fixedPDCPosition1 = TVector3(pdcX, pdcY, pdcZ);
    config.fixedPDCPosition2 = TVector3(pdcX, pdcY, pdcZ);
    config.fixedPDCRotationAngle = pdcAngle;
    config.pdcMacroPath = pdcMacroPath;
    config.pxRange = pxRange;
    
    // [EN] Print configuration / [CN] 打印配置
    SM_INFO("╔═══════════════════════════════════════════════════════════════╗");
    SM_INFO("║              QMD Geometry Filter Analysis                     ║");
    SM_INFO("╚═══════════════════════════════════════════════════════════════╝");
    SM_INFO("");
    SM_INFO("Configuration:");
    SM_INFO("  Field strengths: {} values", config.fieldStrengths.size());
    for (double f : config.fieldStrengths) SM_INFO("    - {} T", f);
    SM_INFO("  Deflection angles: {} values", config.deflectionAngles.size());
    for (double a : config.deflectionAngles) SM_INFO("    - {} deg", a);
    SM_INFO("  Targets: {}", config.targets.size());
    SM_INFO("  Polarization types: {}", config.polTypes.size());
    SM_INFO("  Gamma values: {}", config.gammaValues.size());
    SM_INFO("  QMD data path: {}", config.qmdDataPath);
    SM_INFO("  Field map path: {}", config.fieldMapPath);
    SM_INFO("  Output path: {}", config.outputPath);
    SM_INFO("");
    SM_INFO("PDC Configuration:");
    if (config.useFixedPDC) {
        SM_INFO("  Mode: FIXED position");
        SM_INFO("  PDC1 Position: ({}, {}, {}) mm", pdcX, pdcY, pdcZ);
        SM_INFO("  PDC2 Position: ({}, {}, {}) mm", pdcX, pdcY, pdcZ);
        SM_INFO("  Rotation: {} deg", pdcAngle);
        if (!pdcMacroPath.empty()) {
            SM_INFO("  Macro: {}", pdcMacroPath);
        }
    } else {
        SM_INFO("  Mode: OPTIMIZED (calculate for each target position)");
    }
    SM_INFO("  Px range: +/-{} MeV/c", pxRange);
    SM_INFO("");
    
    // [EN] Create filter and run analysis / [CN] 创建筛选器并运行分析
    QMDGeoFilter filter;
    filter.SetConfig(config);
    
    if (!filter.RunFullAnalysis()) {
        SM_ERROR("Analysis failed!");
        return 1;
    }
    
    // [EN] Generate ratio summary file / [CN] 生成ratio汇总文件
    std::string ratioFile = config.outputPath + "/ratio_summary.txt";
    std::ofstream ratioOfs(ratioFile);
    if (ratioOfs.is_open()) {
        ratioOfs << "# QMD Geometry Filter Ratio Summary\n";
        ratioOfs << "# Field[T]\tAngle[deg]\tTarget\tPol\tGamma\tRatio_BeforeCut\tRatio_AfterCut\tRatio_AfterGeo\tN_AfterGeo\n";
        
        for (const auto& result : filter.GetAllResults()) {
            for (const auto& [gamma, gr] : result.gammaResults) {
                ratioOfs << std::fixed << std::setprecision(2) << result.fieldStrength << "\t"
                         << std::fixed << std::setprecision(1) << result.deflectionAngle << "\t"
                         << result.target << "\t"
                         << result.polType << "\t"
                         << gamma << "\t"
                         << std::fixed << std::setprecision(4) << gr.ratioBeforeCut.ratio << "\t"
                         << std::fixed << std::setprecision(4) << gr.ratioAfterCut.ratio << "\t"
                         << std::fixed << std::setprecision(4) << gr.ratioAfterGeometry.ratio << "\t"
                         << gr.nAfterGeometry << "\n";
            }
        }
        ratioOfs.close();
        SM_INFO("Ratio summary saved to: {}", ratioFile);
    }
    
    SM_INFO("");
    SM_INFO("╔═══════════════════════════════════════════════════════════════╗");
    SM_INFO("║              Analysis completed successfully!                 ║");
    SM_INFO("╚═══════════════════════════════════════════════════════════════╝");
    SM_INFO("Results saved to: {}", config.outputPath);
    
    // [EN] Explicitly shutdown logger before main() returns to avoid destruction order issues
    // [CN] 在 main() 返回前显式关闭日志器，避免析构顺序问题
    SMLogger::Logger::Instance().Shutdown();
    
    return 0;
}
