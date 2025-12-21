#ifndef QMD_GEO_FILTER_HH
#define QMD_GEO_FILTER_HH

/**
 * @file QMDGeoFilter.hh
 * @brief QMD 数据几何接受度筛选库
 * 
 * 复用 geo_accepentce 库的功能进行几何接受度计算和可视化
 */

#include "GeoAcceptanceManager.hh"
#include "DetectorAcceptanceCalculator.hh"
#include "BeamDeflectionCalculator.hh"
#include "MagneticField.hh"

#include <TVector3.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TMultiGraph.h>
#include <TLegend.h>

#include <string>
#include <vector>
#include <map>
#include <memory>

/**
 * @struct MomentumData
 * @brief 动量数据结构
 */
struct MomentumData {
    std::vector<double> pxp, pyp, pzp;  // 质子动量 [MeV/c]
    std::vector<double> pxn, pyn, pzn;  // 中子动量 [MeV/c]
    
    size_t size() const { return pxp.size(); }
    void clear() {
        pxp.clear(); pyp.clear(); pzp.clear();
        pxn.clear(); pyn.clear(); pzn.clear();
    }
    void reserve(size_t n) {
        pxp.reserve(n); pyp.reserve(n); pzp.reserve(n);
        pxn.reserve(n); pyn.reserve(n); pzn.reserve(n);
    }
};

/**
 * @struct RatioResult
 * @brief Pxp/Pxn ratio 计算结果
 * ratio = N(pxp - pxn > 0) / N(pxp - pxn < 0)
 */
struct RatioResult {
    double ratio;           // = nPositive / nNegative
    double error;           // 二项式误差
    int nEvents;            // 总事件数
    int nPositive;          // pxp - pxn > 0 的事件数
    int nNegative;          // pxp - pxn < 0 的事件数
    
    RatioResult() : ratio(1.0), error(0.0), nEvents(0), nPositive(0), nNegative(0) {}
    RatioResult(double r, double e, int n, int np, int nn) 
        : ratio(r), error(e), nEvents(n), nPositive(np), nNegative(nn) {}
};

/**
 * @struct GeometryFilterResult
 * @brief 详细的几何筛选结果
 * 
 * 分类说明：
 * - bothAccepted: PDC 接受 proton 且 NEBULA 接受 neutron（绿色）
 * - pdcOnlyAccepted: 只有 PDC 接受 proton（蓝色 proton）
 * - nebulaOnlyAccepted: 只有 NEBULA 接受 neutron（蓝色 neutron）
 * - bothRejected: PDC 和 NEBULA 都不接受（红色）
 */
struct GeometryFilterResult {
    std::vector<int> bothAccepted;        // PDC 和 NEBULA 都接受的索引
    std::vector<int> pdcOnlyAccepted;     // 只有 PDC 接受 proton 的索引
    std::vector<int> nebulaOnlyAccepted;  // 只有 NEBULA 接受 neutron 的索引
    std::vector<int> bothRejected;        // 都不接受的索引
    
    size_t totalPassed() const { return bothAccepted.size(); }  // 只有 both 才算通过
    size_t size() const { return bothAccepted.size() + pdcOnlyAccepted.size() + 
                                 nebulaOnlyAccepted.size() + bothRejected.size(); }
};

/**
 * @struct GammaAnalysisResult
 * @brief 单个 gamma 值的分析结果
 */
struct GammaAnalysisResult {
    std::string gamma;
    std::string polType;
    
    // 三个阶段的事件数
    int nBeforeCut;
    int nAfterCut;
    int nAfterGeometry;
    
    // 三个阶段的 ratio: (pxp-pxn > 0) / (pxp-pxn < 0)
    // 注意：ratio 使用旋转到反应平面后的 pxp-pxn 计算
    RatioResult ratioBeforeCut;
    RatioResult ratioAfterCut;
    RatioResult ratioAfterGeometry;
    
    // 三个阶段的动量数据
    // 
    // 【坐标系说明】
    // - 所有动量都保存在原始靶子系（随机旋转的反应平面）
    // - 只在计算 pxp-pxn 和 ratio 时才用 atan2 临时旋转到反应平面
    // - 3D 可视化使用原始动量，真实反映探测器覆盖
    //
    MomentumData rawMomenta;              // 原始数据（靶子系）
    MomentumData afterCutMomenta;         // cut 后的靶子系动量
    MomentumData afterGeoMomenta;         // geometry 后的靶子系动量（通过的 = bothAccepted）
    MomentumData rejectedByGeoMomenta;    // geometry 拒绝的靶子系动量
    
    // 详细的 geometry 分类（用于 3D 可视化）
    MomentumData bothAcceptedMomenta;     // PDC 和 NEBULA 都接受（绿色）
    MomentumData pdcOnlyMomenta;          // 只有 PDC 接受 proton（蓝色 proton）
    MomentumData nebulaOnlyMomenta;       // 只有 NEBULA 接受 neutron（蓝色 neutron）
    MomentumData bothRejectedMomenta;     // 都不接受（红色）
    
    // 效率
    double cutEfficiency;
    double geoEfficiency;
    double totalEfficiency;
    
    GammaAnalysisResult() : nBeforeCut(0), nAfterCut(0), nAfterGeometry(0),
                            cutEfficiency(0), geoEfficiency(0), totalEfficiency(0) {}
    
    void ClearMomentumData() {
        rawMomenta.clear();
        afterCutMomenta.clear();
        afterGeoMomenta.clear();
        rejectedByGeoMomenta.clear();
        bothAcceptedMomenta.clear();
        pdcOnlyMomenta.clear();
        nebulaOnlyMomenta.clear();
        bothRejectedMomenta.clear();
    }
};

/**
 * @struct ConfigurationResult
 * @brief 单个配置（磁场+角度+靶+极化类型）的完整分析结果
 */
struct QMDConfigurationResult {
    double fieldStrength;
    double deflectionAngle;
    std::string target;
    std::string polType;
    
    // 各 gamma 值的结果
    std::map<std::string, GammaAnalysisResult> gammaResults;
    
    // 探测器配置
    DetectorAcceptanceCalculator::PDCConfiguration pdcConfig;
    DetectorAcceptanceCalculator::NEBULAConfiguration nebulaConfig;
    BeamDeflectionCalculator::TargetPosition targetPosition;
};

/**
 * @class QMDMomentumCut
 * @brief 动量 cut 类 - 区分 zpol 和 ypol 的不同 cut 条件
 */
class QMDMomentumCut {
public:
    // zpol cut 参数
    struct ZpolParams {
        double pzSumThreshold = 1150.0;    // (pzp + pzn) > threshold
        double phiThreshold = 0.5;         // |π - |φ|| < threshold [rad]
        double deltaPzThreshold = 150.0;   // |pzp - pzn| < threshold
        double pxSumThreshold = 200.0;     // (pxp + pxn) < threshold
        double ptSumSqThreshold = 2500.0;  // (pxp+pxn)² + (pyp+pyn)² > threshold
    };
    
    // ypol cut 参数
    struct YpolParams {
        double deltaPyThreshold = 150.0;   // |pyp - pyn| < threshold (step 1)
        double ptSumSqThreshold = 2500.0;  // (pxp+pxn)² + (pyp+pyn)² > threshold (step 1)
        double pxSumAfterRotation = 200.0; // |pxp + pxn| < threshold (step 2, after rotation)
        double phiThreshold = 0.2;         // |π - |φ|| < threshold (step 2) [rad]
    };

private:
    std::string fPolType;
    ZpolParams fZpolParams;
    YpolParams fYpolParams;

public:
    QMDMomentumCut(const std::string& polType = "zpol");
    
    // 设置参数
    void SetZpolParams(const ZpolParams& params) { fZpolParams = params; }
    void SetYpolParams(const YpolParams& params) { fYpolParams = params; }
    
    // 应用 cut
    // 返回通过 cut 的索引和原始动量（靶子坐标系）
    // cut 内部逻辑会临时旋转到反应平面判断条件，但输出只保存原始动量
    struct CutResult {
        std::vector<int> passedIndices;
        MomentumData momenta;  // 通过 cut 的原始动量（靶子坐标系）
    };
    
    CutResult ApplyCut(const MomentumData& data);
    
private:
    CutResult ApplyZpolCut(const MomentumData& data);
    CutResult ApplyYpolCut(const MomentumData& data);
    
    // 动量旋转 (pxp, pyp) 和 (pxn, pyn) 到 φ = 0
    void RotateMomentum(double pxp, double pyp, double pxn, double pyn,
                        double& pxp_rot, double& pyp_rot, 
                        double& pxn_rot, double& pyn_rot);
};

/**
 * @class QMDGeoFilter
 * @brief QMD 数据几何接受度筛选主类
 * 
 * 功能：
 * 1. 加载 QMD 模拟数据
 * 2. 应用动量 cut
 * 3. 利用 geo_accepentce 库计算几何接受度
 * 4. 生成分析图表
 */
class QMDGeoFilter {
public:
    struct AnalysisConfig {
        // 磁场配置
        std::vector<double> fieldStrengths;  // [T]
        
        // 偏转角度
        std::vector<double> deflectionAngles;  // [度]
        
        // 靶材料
        std::vector<std::string> targets;
        
        // 极化类型
        std::vector<std::string> polTypes;  // "zpol", "ypol"
        
        // gamma 值
        std::vector<std::string> gammaValues;
        
        // b 范围
        double bMin = 5.0;
        double bMax = 10.0;
        
        // 路径配置
        std::string qmdDataPath;
        std::string fieldMapPath;
        std::string outputPath;
        
        AnalysisConfig();
    };

private:
    AnalysisConfig fConfig;
    
    // geo_accepentce 组件
    std::unique_ptr<GeoAcceptanceManager> fGeoManager;
    std::unique_ptr<DetectorAcceptanceCalculator> fDetectorCalc;
    std::unique_ptr<BeamDeflectionCalculator> fBeamCalc;
    MagneticField* fMagField;
    
    // 当前分析状态
    double fCurrentFieldStrength;
    double fCurrentDeflectionAngle;
    
    // 存储所有结果
    std::vector<QMDConfigurationResult> fAllResults;

public:
    QMDGeoFilter();
    ~QMDGeoFilter();
    
    // 配置
    void SetConfig(const AnalysisConfig& config) { fConfig = config; }
    AnalysisConfig GetConfig() const { return fConfig; }
    
    // 设置路径
    void SetQMDDataPath(const std::string& path) { fConfig.qmdDataPath = path; }
    void SetFieldMapPath(const std::string& path) { fConfig.fieldMapPath = path; }
    void SetOutputPath(const std::string& path) { fConfig.outputPath = path; }
    
    // 添加配置
    void AddFieldStrength(double field) { fConfig.fieldStrengths.push_back(field); }
    void AddDeflectionAngle(double angle) { fConfig.deflectionAngles.push_back(angle); }
    void AddTarget(const std::string& target) { fConfig.targets.push_back(target); }
    void AddPolType(const std::string& polType) { fConfig.polTypes.push_back(polType); }
    void AddGamma(const std::string& gamma) { fConfig.gammaValues.push_back(gamma); }
    
    // 运行完整分析
    bool RunFullAnalysis();
    
    // 分析单个配置
    QMDConfigurationResult AnalyzeSingleConfiguration(
        double fieldStrength,
        double deflectionAngle,
        const std::string& target,
        const std::string& polType
    );
    
    // 加载磁场
    bool LoadFieldMap(double fieldStrength);
    
    // 加载 QMD 数据
    MomentumData LoadQMDData(const std::string& target, const std::string& polType,
                             const std::string& gamma);
    
    // 计算几何接受度（返回详细分类）
    GeometryFilterResult ApplyGeometryFilterDetailed(const MomentumData& data,
                                                      const TVector3& targetPos,
                                                      double targetRotationAngle);
    
    // 计算几何接受度（返回通过的索引，兼容旧接口）
    std::vector<int> ApplyGeometryFilter(const MomentumData& data,
                                          const TVector3& targetPos,
                                          double targetRotationAngle);
    
    // 计算 ratio
    RatioResult CalculateRatio(const MomentumData& data, 
                               const std::vector<int>& indices = {});
    
    // 生成图表
    void GeneratePlots(const QMDConfigurationResult& result, 
                       const std::string& outputDir);
    
    // 生成探测器几何布局图 (复用 geo_accepentce)
    void GenerateDetectorGeometryPlot(double fieldStrength, double deflectionAngle,
                                       const std::string& outputFile);
    
    // 生成 3D 动量分布图 (proton 或 neutron)
    void Generate3DMomentumPlot(const MomentumData& data,
                                 const std::string& title,
                                 const std::string& outputFile);
    
    // 生成 geometry 前后对比的 3D 动量分布图 (通过/未通过)
    void Generate3DMomentumComparisonPlot(const GammaAnalysisResult& gammaResult,
                                           const std::string& outputFile);
    
    // 生成 2D 投影图 (更清晰地查看动量分布)
    void Generate3DMomentum2DProjections(const GammaAnalysisResult& gammaResult,
                                          const std::string& outputFile);
    
    // 生成交互式 HTML 3D 图 (可旋转)
    void GenerateInteractive3DPlot(const GammaAnalysisResult& gammaResult,
                                    const std::string& outputFile);
    
    // 生成 Pxp-Pxn 分布图
    void GeneratePxpPxnPlot(const MomentumData& data,
                             const std::string& title,
                             const std::string& outputFile);
    
    // 生成 Pxp-Pxn 综合对比图 (三个阶段)
    void GeneratePxpPxnComparisonPlot(const GammaAnalysisResult& gammaResult,
                                       const std::string& outputFile);
    
    // 生成跨 gamma 对比图
    void GenerateGammaComparisonPlot(const QMDConfigurationResult& result,
                                      const std::string& outputFile);
    
    // 生成 ratio 对比图 (before/after geometry filter)
    void GenerateRatioComparisonPlot(const std::string& outputFile);
    
    // 获取结果
    std::vector<QMDConfigurationResult> GetAllResults() const { return fAllResults; }
    
    // 生成报告
    void GenerateTextReport(const std::string& filename);
    void GenerateSummaryTable();
    
    // 静态工具函数
    static std::string GetFieldMapFile(double fieldStrength, const std::string& basePath);
    static AnalysisConfig CreateDefaultConfig();
    
private:
    // 构建 QMD 数据文件路径
    std::string BuildQMDDataPath(const std::string& target, const std::string& polType,
                                  const std::string& gamma, const std::string& polSuffix);
    
    // 创建输出目录
    void CreateOutputDirectory(const std::string& path);
    
    // 清理内存
    void ClearCurrentData();
};

#endif // QMD_GEO_FILTER_HH
