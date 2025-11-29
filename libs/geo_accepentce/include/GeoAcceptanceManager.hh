#ifndef GEO_ACCEPTANCE_MANAGER_HH
#define GEO_ACCEPTANCE_MANAGER_HH

#include "BeamDeflectionCalculator.hh"
#include "DetectorAcceptanceCalculator.hh"
#include "MagneticField.hh"
#include <vector>
#include <map>
#include <string>

/**
 * @class GeoAcceptanceManager
 * @brief 几何接受度分析的主管理类
 * 
 * 整合束流偏转计算和探测器接受度计算，生成完整的分析报告
 * 
 * 工作流程:
 * 1. 加载磁场配置
 * 2. 计算不同偏转角度下的target位置
 * 3. 对每个target位置计算PDC最佳位置
 * 4. 加载QMD反应数据
 * 5. 计算PDC和NEBULA的接受度
 * 6. 生成综合报告
 */
class GeoAcceptanceManager {
public:
    // 分析配置
    struct AnalysisConfig {
        // 磁场配置
        std::vector<std::string> fieldMapFiles;
        std::vector<double> fieldStrengths;  // Tesla
        
        // 偏转角度配置
        std::vector<double> deflectionAngles; // 度
        
        // QMD数据路径
        std::string qmdDataPath;
        
        // 输出路径
        std::string outputPath;
        
        AnalysisConfig() : qmdDataPath("/home/tian/workspace/dpol/smsimulator5.5/data/qmdrawdata"),
                          outputPath("./results") {
            // 默认配置: 0°, 5°, 10°
            deflectionAngles = {0.0, 5.0, 10.0};
        }
    };
    
    // 单个配置的完整结果
    struct ConfigurationResult {
        double fieldStrength;
        std::string fieldMapFile;
        double deflectionAngle;
        BeamDeflectionCalculator::TargetPosition targetPos;
        DetectorAcceptanceCalculator::PDCConfiguration pdcConfig;
        DetectorAcceptanceCalculator::AcceptanceResult acceptance;
    };

private:
    AnalysisConfig fConfig;
    
    MagneticField* fCurrentMagField;
    BeamDeflectionCalculator* fBeamCalc;
    DetectorAcceptanceCalculator* fDetectorCalc;
    
    // 存储所有配置的结果
    std::vector<ConfigurationResult> fResults;

public:
    GeoAcceptanceManager();
    ~GeoAcceptanceManager();
    
    // 配置分析
    void SetConfig(const AnalysisConfig& config) { fConfig = config; }
    AnalysisConfig GetConfig() const { return fConfig; }
    
    // 添加磁场配置
    void AddFieldMap(const std::string& filename, double fieldStrength);
    
    // 添加偏转角度
    void AddDeflectionAngle(double angle);
    
    // 设置QMD数据路径
    void SetQMDDataPath(const std::string& path) { fConfig.qmdDataPath = path; }
    
    // 设置输出路径
    void SetOutputPath(const std::string& path) { fConfig.outputPath = path; }
    
    // 执行完整分析
    bool RunFullAnalysis();
    
    // 分步执行
    bool AnalyzeFieldConfiguration(const std::string& fieldMapFile, double fieldStrength);
    
    // 生成报告
    void GenerateTextReport(const std::string& filename) const;
    void GenerateROOTFile(const std::string& filename) const;
    void GenerateSummaryTable() const;
    
    // 生成可视化图
    void GenerateGeometryPlot(const std::string& filename) const;
    
    // 打印结果
    void PrintResults() const;
    void PrintConfiguration() const;
    
    // 获取结果
    std::vector<ConfigurationResult> GetResults() const { return fResults; }
    
    // 清空结果
    void ClearResults() { fResults.clear(); }
    
    // 快速配置接口
    static AnalysisConfig CreateDefaultConfig();
    static AnalysisConfig CreateConfig(
        const std::vector<std::string>& fieldMaps,
        const std::vector<double>& angles
    );
};

#endif // GEO_ACCEPTANCE_MANAGER_HH
