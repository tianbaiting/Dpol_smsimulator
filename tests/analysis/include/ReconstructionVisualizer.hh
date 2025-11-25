#ifndef RECONSTRUCTION_VISUALIZER_HH
#define RECONSTRUCTION_VISUALIZER_HH

#include "TargetReconstructor.hh"
#include "TGraph.h"
#include "TGraph2D.h"
#include "TCanvas.h"
#include "TH2D.h"
#include "TVector3.h"
#include <vector>
#include <string>

/**
 * @brief 重建算法可视化工具类
 * 
 * 用于绘制优化过程的全局函数图、等高线图和优化路径
 * 仅在测试模式下启用，生产环境不编译此代码
 */
class ReconstructionVisualizer {
public:
    /**
     * @brief 优化步骤数据结构
     */
    struct OptimizationStep {
        double momentum;        ///< 当前动量值 [MeV/c]
        double distance;        ///< 到目标点的距离 [mm]
        double gradient;        ///< 梯度值 (如果可用)
        int iteration;          ///< 迭代次数
        std::vector<TrajectoryPoint> trajectory;  ///< 轨迹点 (可选)
    };

    /**
     * @brief 全局函数值网格数据
     */
    struct GlobalGrid {
        std::vector<double> momenta;      ///< 动量采样点
        std::vector<double> distances;     ///< 对应的距离值
        double pMin, pMax;                 ///< 动量范围
        int nSamples;                      ///< 采样点数
    };

    ReconstructionVisualizer();
    ~ReconstructionVisualizer();

    /**
     * @brief 记录优化步骤
     */
    void RecordStep(const OptimizationStep& step);

    /**
     * @brief 计算全局函数值网格 (暴力扫描)
     * 
     * @param reconstructor 重建器对象
     * @param track 轨迹数据
     * @param targetPos 目标位置
     * @param pMin 最小动量 [MeV/c]
     * @param pMax 最大动量 [MeV/c]
     * @param nSamples 采样点数
     */
    GlobalGrid CalculateGlobalGrid(const TargetReconstructor* reconstructor,
                                   const RecoTrack& track,
                                   const TVector3& targetPos,
                                   double pMin = 50.0,
                                   double pMax = 3000.0,
                                   int nSamples = 100);

    /**
     * @brief 绘制优化函数的1D曲线图
     */
    TCanvas* PlotOptimizationFunction1D(const GlobalGrid& grid,
                                       const std::string& title = "Optimization Function");

    /**
     * @brief 绘制优化路径 (叠加在全局函数图上)
     * @param trueMomentum 真实动量值（用于标注），如果 < 0 则不标注
     * @param trueMomentumVec 真实动量三维矢量（可选）
     * @param recoMomentumVec 重建动量三维矢量（可选）
     */
    TCanvas* PlotOptimizationPath(const GlobalGrid& grid,
                                 const std::vector<OptimizationStep>& steps,
                                 const std::string& title = "Optimization Path",
                                 double trueMomentum = -1.0,
                                 const TVector3* trueMomentumVec = nullptr,
                                 const TVector3* recoMomentumVec = nullptr);

    /**
     * @brief 绘制轨迹3D图（对比真实轨迹和重建轨迹）
     * @param trajectory 重建的轨迹
     * @param targetPos 靶点位置
     * @param title 图标题
     * @param trueTrajectory 真实轨迹（可选，用于对比）
     */
    TCanvas* PlotTrajectory3D(const std::vector<TrajectoryPoint>& trajectory,
                             const TVector3& targetPos,
                             const std::string& title = "Particle Trajectory",
                             const std::vector<TrajectoryPoint>* trueTrajectory = nullptr);

    /**
     * @brief 绘制多条试探轨迹的对比图
     */
    TCanvas* PlotMultipleTrajectories(const std::vector<std::vector<TrajectoryPoint>>& trajectories,
                                     const std::vector<double>& momenta,
                                     const TVector3& targetPos,
                                     const std::string& title = "Trial Trajectories");

    /**
     * @brief 保存所有图像到文件
     */
    void SavePlots(const std::string& outputDir = "test_output/reconstruction_viz");

    /**
     * @brief 清除所有记录的数据
     */
    void Clear();

    /**
     * @brief 获取记录的所有优化步骤
     */
    const std::vector<OptimizationStep>& GetSteps() const { return fSteps; }

    /**
     * @brief 打印优化统计信息
     */
    void PrintStatistics() const;

private:
    std::vector<OptimizationStep> fSteps;   ///< 记录的优化步骤
    std::vector<TCanvas*> fCanvases;        ///< 创建的画布 (用于管理内存)

    /**
     * @brief 创建并配置画布
     */
    TCanvas* CreateCanvas(const std::string& name, const std::string& title,
                         int width = 800, int height = 600);
};

#endif // RECONSTRUCTION_VISUALIZER_HH
