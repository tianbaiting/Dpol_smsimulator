/**
 * @file RunGeoAcceptanceAnalysis.cc
 * @brief 几何接受度分析主程序
 * 
 * 使用方法:
 *   ./RunGeoAcceptanceAnalysis [field_map_dir] [output_dir]
 * 
 * 示例:
 *   ./RunGeoAcceptanceAnalysis \
 *       /path/to/fieldmaps \
 *       ./acceptance_results
 */

#include "GeoAcceptanceManager.hh"
#include <iostream>
#include <vector>
#include <string>
#include <glob.h>

void PrintUsage(const char* progName) {
    std::cout << "\nUsage: " << progName << " [options]\n" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --fieldmap <file>      Magnetic field map file (can specify multiple times)" << std::endl;
    std::cout << "  --field <value>        Field strength in Tesla (corresponding to fieldmap)" << std::endl;
    std::cout << "  --angle <value>        Deflection angle in degrees (can specify multiple times)" << std::endl;
    std::cout << "  --qmd <path>           Path to QMD data directory" << std::endl;
    std::cout << "  --output <path>        Output directory for results" << std::endl;
    std::cout << "  --help                 Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << progName << " \\" << std::endl;
    std::cout << "    --fieldmap configs/simulation/geometry/filed_map/field_0.8T.dat --field 0.8 \\" << std::endl;
    std::cout << "    --fieldmap configs/simulation/geometry/filed_map/field_1.0T.dat --field 1.0 \\" << std::endl;
    std::cout << "    --angle 0 --angle 5 --angle 10 \\" << std::endl;
    std::cout << "    --qmd data/qmdrawdata \\" << std::endl;
    std::cout << "    --output ./results" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║       Geometry Acceptance Analysis for SAMURAI Detectors     ║" << std::endl;
    std::cout << "║                                                               ║" << std::endl;
    std::cout << "║  Analyzing PDC and NEBULA detector acceptance                ║" << std::endl;
    std::cout << "║  for different magnetic field and beam deflection angles     ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    // 解析命令行参数
    std::vector<std::string> fieldMaps;
    std::vector<double> fieldStrengths;
    std::vector<double> angles;
    std::string qmdPath = "/home/tian/workspace/dpol/smsimulator5.5/data/qmdrawdata";
    std::string outputPath = "./results";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "--fieldmap" && i + 1 < argc) {
            fieldMaps.push_back(argv[++i]);
        }
        else if (arg == "--field" && i + 1 < argc) {
            fieldStrengths.push_back(std::atof(argv[++i]));
        }
        else if (arg == "--angle" && i + 1 < argc) {
            angles.push_back(std::atof(argv[++i]));
        }
        else if (arg == "--qmd" && i + 1 < argc) {
            qmdPath = argv[++i];
        }
        else if (arg == "--output" && i + 1 < argc) {
            outputPath = argv[++i];
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }
    
    // 检查参数
    if (fieldMaps.empty()) {
        std::cout << "No field maps specified. Using default configuration..." << std::endl;
        
        // 默认配置：搜索field map目录
        std::string fieldMapDir = "configs/simulation/geometry/filed_map";
        
        // 示例默认磁场配置 - 使用.table格式文件
        std::vector<std::string> defaultMaps = {
            fieldMapDir + "/141114-0,8T-6000.table",
            fieldMapDir + "/180626-1,00T-6000.table",
            fieldMapDir + "/180626-1,20T-3000.table",
            fieldMapDir + "/180703-1,40T-6000.table"
        };
        
        std::vector<double> defaultFields = {0.8, 1.0, 1.2, 1.4};
        
        for (size_t i = 0; i < defaultMaps.size(); i++) {
            fieldMaps.push_back(defaultMaps[i]);
            fieldStrengths.push_back(defaultFields[i]);
        }
    }
    
    if (fieldMaps.size() != fieldStrengths.size()) {
        std::cerr << "ERROR: Number of field maps and field strengths must match!" << std::endl;
        std::cerr << "  Field maps: " << fieldMaps.size() << std::endl;
        std::cerr << "  Field strengths: " << fieldStrengths.size() << std::endl;
        return 1;
    }
    
    if (angles.empty()) {
        std::cout << "No angles specified. Using default: 0°, 5°, 10°" << std::endl;
        angles = {0.0, 5.0, 10.0};
    }
    
    // 创建分析管理器
    GeoAcceptanceManager manager;
    
    // 配置分析
    GeoAcceptanceManager::AnalysisConfig config;
    config.fieldMapFiles = fieldMaps;
    config.fieldStrengths = fieldStrengths;
    config.deflectionAngles = angles;
    config.qmdDataPath = qmdPath;
    config.outputPath = outputPath;
    
    manager.SetConfig(config);
    
    // 执行分析
    bool success = manager.RunFullAnalysis();
    
    if (!success) {
        std::cerr << "\nERROR: Analysis failed!" << std::endl;
        return 1;
    }
    
    // 打印最终结果
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                  Analysis Completed!                          ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Results saved in: " << outputPath << std::endl;
    std::cout << "  - acceptance_report.txt (text summary)" << std::endl;
    std::cout << "  - acceptance_results.root (ROOT file with detailed data)" << std::endl;
    std::cout << std::endl;
    
    return 0;
}
