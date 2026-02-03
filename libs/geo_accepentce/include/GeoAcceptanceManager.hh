#ifndef GEO_ACCEPTANCE_MANAGER_HH
#define GEO_ACCEPTANCE_MANAGER_HH

#include "BeamDeflectionCalculator.hh"
#include "DetectorAcceptanceCalculator.hh"
#include "MagneticField.hh"
#include <vector>
#include <map>
#include <string>
#include <fstream>

/**
 * @struct TrackPlotConfig
 * @brief [EN] Configuration for particle track plotting / [CN] 粒子轨迹绘图配置
 * 
 * [EN] This structure controls which tracks are drawn in geometry plots:
 * [CN] 此结构控制几何图中绘制哪些轨迹：
 * 
 * Usage example / 使用示例:
 * @code
 *   GeoAcceptanceManager mgr;
 *   TrackPlotConfig config;
 *   config.drawDeuteronTrack = true;
 *   config.pxEdgeValues = {100.0, 150.0};  // Draw Px=±100, ±150 MeV/c tracks
 *   mgr.SetTrackPlotConfig(config);
 * @endcode
 */
struct TrackPlotConfig {
    // [EN] Whether to draw deuteron (beam) trajectory / [CN] 是否绘制氘核（束流）轨迹
    bool drawDeuteronTrack = true;
    
    // [EN] Whether to draw center proton track (Px=0) / [CN] 是否绘制中心质子轨迹 (Px=0)
    bool drawCenterProtonTrack = true;
    
    // [EN] Px edge values for proton tracks [MeV/c] / [CN] 边缘质子轨迹的Px值 [MeV/c]
    // [EN] Each value generates both +Px and -Px tracks / [CN] 每个值都会生成+Px和-Px两条轨迹
    // [EN] Default: {100.0, 150.0} -> draws ±100 MeV/c and ±150 MeV/c tracks
    // [CN] 默认: {100.0, 150.0} -> 绘制 ±100 MeV/c 和 ±150 MeV/c 轨迹
    std::vector<double> pxEdgeValues = {100.0, 150.0};
    
    // [EN] Pz for proton track calculation [MeV/c] / [CN] 质子轨迹计算使用的Pz [MeV/c]
    double pzProton = 627.0;
    
    // [EN] Whether to draw neutron track (straight line, no field bending)
    // [CN] 是否绘制中子轨迹（直线，无磁场偏转）
    bool drawNeutronTrack = false;
    
    // [EN] Colors for different track types / [CN] 不同轨迹类型的颜色
    // [EN] Format: {Px_value, {color_positive, color_negative}}
    // [CN] For Px=0 (center), use value 0.0
    // [CN] Deuteron uses special value -1.0
    int deuteronTrackColor = kGreen + 2;  // [EN] Green for deuteron / [CN] 氘核用绿色
    int centerProtonColor = kRed;          // [EN] Red for center proton / [CN] 中心质子用红色
    std::vector<int> edgeProtonColors = {kBlue, kMagenta, kOrange, kCyan};  // [EN] Cycle through these
    
    TrackPlotConfig() = default;
};

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
        
        // [EN] PDC configuration / [CN] PDC配置
        bool useFixedPDC = false;           // [EN] Use fixed PDC position / [CN] 使用固定PDC位置
        TVector3 fixedPDCPosition1;         // [EN] Fixed PDC1 position (mm) / [CN] 固定PDC1位置
        TVector3 fixedPDCPosition2;         // [EN] Fixed PDC2 position (mm) / [CN] 固定PDC2位置
        double fixedPDCRotationAngle = 60.0; // [EN] Fixed PDC rotation (deg) / [CN] 固定PDC旋转角度
        double pxRange = 100.0;             // [EN] Px range for PDC sizing / [CN] PDC尺寸的Px范围
        
        AnalysisConfig();  // 构造函数在 .cc 文件中实现，以使用环境变量
    };
    
    // 单个配置的完整结果
    struct ConfigurationResult {
        double fieldStrength;
        std::string fieldMapFile;
        double deflectionAngle;
        BeamDeflectionCalculator::TargetPosition targetPos;
        DetectorAcceptanceCalculator::PDCConfiguration pdcConfig;
        DetectorAcceptanceCalculator::PDCConfiguration pdcConfig2;
        bool usePdcPair = false;
        DetectorAcceptanceCalculator::AcceptanceResult acceptance;
    };

private:
    AnalysisConfig fConfig;

    MagneticField* fCurrentMagField;
    BeamDeflectionCalculator* fBeamCalc;
    DetectorAcceptanceCalculator* fDetectorCalc;
    
    // 存储所有配置的结果
    std::vector<ConfigurationResult> fResults;
    
    // 日志文件输出流
    std::ofstream fLogFile;
    std::string fLogFilePath;
    
    // [EN] Track plotting configuration / [CN] 轨迹绘图配置
    TrackPlotConfig fTrackPlotConfig;

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
    
    // 为单个配置生成详细图（包含边缘质子轨迹）
    void GenerateSingleConfigPlot(const ConfigurationResult& result, 
                                  const std::string& filename) const;
    
    // [EN] Generate single config plot with full track visualization
    // [CN] 使用完整轨迹可视化生成单个配置图
    // [EN] This version uses the TrackPlotConfig to draw:
    //      - Deuteron track (beam trajectory through magnet)
    //      - Center proton track (Px=0)
    //      - Edge proton tracks (Px=±pxEdgeValues)
    // [CN] 此版本使用 TrackPlotConfig 来绘制：
    //      - 氘核轨迹（穿过磁铁的束流轨迹）
    //      - 中心质子轨迹 (Px=0)
    //      - 边缘质子轨迹 (Px=±pxEdgeValues)
    void GenerateSingleConfigPlotWithTracks(const ConfigurationResult& result, 
                                            const std::string& filename,
                                            const TrackPlotConfig& trackConfig) const;
    
    // [EN] Set default track plot configuration / [CN] 设置默认轨迹绘图配置
    void SetTrackPlotConfig(const TrackPlotConfig& config) { fTrackPlotConfig = config; }
    TrackPlotConfig GetTrackPlotConfig() const { return fTrackPlotConfig; }
    
    // [EN] Get current magnetic field / [CN] 获取当前磁场
    MagneticField* GetMagneticField() const { return fCurrentMagField; }
    
    // [EN] Get beam deflection calculator / [CN] 获取束流偏转计算器
    BeamDeflectionCalculator* GetBeamCalculator() const { return fBeamCalc; }
    
    // [EN] Get detector acceptance calculator / [CN] 获取探测器接受度计算器
    DetectorAcceptanceCalculator* GetDetectorCalculator() const { return fDetectorCalc; }
    
    // 打印结果
    void PrintResults() const;
    void PrintConfiguration() const;
    
    // 获取结果
    std::vector<ConfigurationResult> GetResults() const { return fResults; }
    
    // 清空结果
    void ClearResults() { fResults.clear(); }
    
    // 日志管理
    void OpenLogFile(const std::string& logPath);
    void CloseLogFile();
    std::string GetLogFilePath() const { return fLogFilePath; }
    
    // 快速配置接口
    static AnalysisConfig CreateDefaultConfig();
    static AnalysisConfig CreateConfig(
        const std::vector<std::string>& fieldMaps,
        const std::vector<double>& angles
    );
};

#endif // GEO_ACCEPTANCE_MANAGER_HH
