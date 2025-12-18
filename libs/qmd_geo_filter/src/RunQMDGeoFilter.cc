/**
 * @file RunQMDGeoFilter.cc
 * @brief QMD 几何筛选分析主程序
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
    std::cout << "Options:\n";
    std::cout << "  -f, --field <T>       Field strength in Tesla (can be specified multiple times)\n";
    std::cout << "  -a, --angle <deg>     Deflection angle in degrees (can be specified multiple times)\n";
    std::cout << "  -t, --target <name>   Target material (can be specified multiple times)\n";
    std::cout << "  -p, --pol <type>      Polarization type: zpol or ypol (can be specified multiple times)\n";
    std::cout << "  -g, --gamma <value>   Gamma value (can be specified multiple times)\n";
    std::cout << "  -q, --qmd <path>      QMD data path\n";
    std::cout << "  -m, --fieldmap <path> Field map directory path\n";
    std::cout << "  -o, --output <path>   Output directory path\n";
    std::cout << "  -h, --help            Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " --field 1.0 --angle 5 --target Pb208 --pol zpol --pol ypol\n";
    std::cout << "  " << progName << " -f 0.8 -f 1.0 -a 0 -a 5 -a 10 -t Pb208 -p zpol\n";
}

int main(int argc, char* argv[]) {
    // 默认配置
    QMDGeoFilter::AnalysisConfig config;
    config.fieldStrengths.clear();
    config.deflectionAngles.clear();
    config.targets.clear();
    config.polTypes.clear();
    config.gammaValues.clear();
    
    bool hasField = false, hasAngle = false, hasTarget = false, hasPol = false, hasGamma = false;
    
    // 命令行选项
    static struct option long_options[] = {
        {"field",    required_argument, 0, 'f'},
        {"angle",    required_argument, 0, 'a'},
        {"target",   required_argument, 0, 't'},
        {"pol",      required_argument, 0, 'p'},
        {"gamma",    required_argument, 0, 'g'},
        {"qmd",      required_argument, 0, 'q'},
        {"fieldmap", required_argument, 0, 'm'},
        {"output",   required_argument, 0, 'o'},
        {"help",     no_argument,       0, 'h'},
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
            case 'h':
                PrintUsage(argv[0]);
                return 0;
            default:
                PrintUsage(argv[0]);
                return 1;
        }
    }
    
    // 使用默认值
    if (!hasField) config.fieldStrengths = {1.0};
    if (!hasAngle) config.deflectionAngles = {5.0};
    if (!hasTarget) config.targets = {"Pb208"};
    if (!hasPol) config.polTypes = {"zpol", "ypol"};
    if (!hasGamma) config.gammaValues = {"050", "060", "070", "080"};
    
    // 打印配置
    SM_INFO("QMD Geometry Filter Analysis");
    SM_INFO("============================");
    SM_INFO("Configuration:");
    SM_INFO("  Field strengths: {}", config.fieldStrengths.size());
    SM_INFO("  Deflection angles: {}", config.deflectionAngles.size());
    SM_INFO("  Targets: {}", config.targets.size());
    SM_INFO("  Polarization types: {}", config.polTypes.size());
    SM_INFO("  Gamma values: {}", config.gammaValues.size());
    SM_INFO("  QMD data path: {}", config.qmdDataPath);
    SM_INFO("  Field map path: {}", config.fieldMapPath);
    SM_INFO("  Output path: {}", config.outputPath);
    
    // 创建分析器并运行
    QMDGeoFilter filter;
    filter.SetConfig(config);
    
    if (!filter.RunFullAnalysis()) {
        SM_ERROR("Analysis failed!");
        return 1;
    }
    
    SM_INFO("Analysis completed successfully!");
    SM_INFO("Results saved to: {}", config.outputPath);
    
    return 0;
}
