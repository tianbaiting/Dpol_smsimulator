# Presentation Script: dpol_breakup Experiment Optimization
# 报告讲稿：dpol_breakup 实验优化

**Date / 日期:** November 26, 2025  
**Presenter / 报告人:** Tian  
**Audience / 听众:** Mizuki, Aki

---

## Opening / 开场白

**English:**
Good morning everyone. Today I'd like to present our progress on optimizing the dpol_breakup experiment configuration. This work focuses on determining the optimal experimental setup through systematic simulation and analysis.

**中文:**
大家好。今天我想向大家汇报我们在 dpol_breakup 实验配置优化方面的进展。这项工作主要通过系统的模拟和分析来确定最优的实验设置。

---

## 1. Research Objectives / 研究目标

**English:**
Let me start by outlining our main objectives. We aim to answer four key questions:

First, what is the optimal target position for different magnetic field strengths? 

Second, how does the beam deflection angle affect our detection efficiency?

Third, what momentum resolution can we achieve with the current PDC setup?

And finally, how can we improve both reconstruction speed and accuracy?

**中文:**
首先让我概述一下我们的主要目标。我们要回答四个关键问题：

第一，在不同磁场强度下，靶的最优位置是什么？

第二，束流偏转角度如何影响探测效率？

第三，用现有的 PDC 装置能达到什么样的动量分辨率？

最后，我们如何提高重建速度和精度？

---

## 2. System Architecture / 系统架构

**English:**
Now let's look at our complete simulation framework. As you can see in this diagram, we have a four-stage pipeline:

Starting from QMD raw data - that's about 2 million events per configuration. After applying physical cuts and sampling, we reduce this to around 30,000 events for Geant4 input.

Then the Geant4 simulation processes these events at roughly 120 events per second, generating hit trees and energy deposit information.

Finally, the PDC reconstruction stage performs track fitting - currently this is our bottleneck, running at only 0.05 to 1 event per second.

**中文:**
现在让我们看看完整的模拟框架。如图所示，我们有一个四阶段的流程：

从 QMD 原始数据开始 - 每个配置大约有 200 万个事例。在应用物理筛选和采样后，我们将其减少到约 3 万个事例作为 Geant4 的输入。

然后 Geant4 模拟以大约每秒 120 个事例的速度处理这些数据，生成击中树和能量沉积信息。

最后，PDC 重建阶段进行径迹拟合 - 目前这是我们的瓶颈，运行速度仅为每秒 0.05 到 1 个事例。

---

## 3. Performance Status / 性能现状

**English:**
Speaking of performance, let me show you where we currently stand.

The Geant4 simulation performs quite well at 120 events per second. However, the PDC reconstruction is significantly slower - between 0.05 and 1 event per second using TMinuit.

This slow speed is a major bottleneck. Moreover, we're seeing poor performance on some events with systematic momentum bias.

Our target is to achieve over 10 events per second with momentum resolution better than 5%, while maintaining detection efficiency above 80% for the physics-relevant phase space.

**中文:**
说到性能，让我展示一下我们目前的状况。

Geant4 模拟表现相当好，达到每秒 120 个事例。然而，PDC 重建明显慢得多 - 使用 TMinuit 时速度在每秒 0.05 到 1 个事例之间。

这种缓慢的速度是一个主要瓶颈。此外，我们在某些事例上看到了系统性动量偏差导致的性能不佳。

我们的目标是达到每秒超过 10 个事例，动量分辨率优于 5%，同时在物理相关的相空间中保持 80% 以上的探测效率。

---

## 4. QMD Data Processing / QMD 数据处理

**English:**
Let me explain our data processing strategy.

The initial challenge is the massive data volume - approximately 2 million events per target and gamma configuration. This is simply too much to process efficiently.

So we applied physical cuts to focus on the region of interest. Specifically, we used momentum cuts on the proton and neutron, requiring the y-component difference to be less than 150 MeV/c, and the sum of x-components to be less than 200 MeV/c.

We also applied angular cuts based on the rotation angle phi.

As shown in this table, these cuts reduced our dataset dramatically - from 2 million to 500,000 with momentum cuts, and finally down to 30,000 events after angular selection.

This makes the analysis computationally feasible while preserving the physics we care about.

**中文:**
让我解释一下我们的数据处理策略。

最初的挑战是巨大的数据量 - 每个靶和 gamma 配置大约有 200 万个事例。这对于高效处理来说实在太多了。

因此我们应用了物理筛选来聚焦于感兴趣的区域。具体来说，我们对质子和中子使用了动量筛选，要求 y 分量的差异小于 150 MeV/c，x 分量的和小于 200 MeV/c。

我们还根据旋转角 phi 应用了角度筛选。

如表中所示，这些筛选大幅减少了数据集 - 从 200 万通过动量筛选减少到 50 万，最后经过角度选择降至 3 万个事例。

这使得分析在计算上变得可行，同时保留了我们关心的物理过程。

---

## 5. Geant4 Simulation Results / Geant4 模拟结果

**English:**
Now let's look at the Geant4 simulation results.

Our current configuration uses a target position based on 5-degree beam deflection in a 1.2 Tesla magnetic field. The detector geometry includes PDC1, PDC2, and the NEBULA array.

Figure 1 shows the visualization of 5000 accumulated events - you can clearly see the particle trajectories curving in the magnetic field.

Figure 2 presents our detection efficiency as a function of angle. An event is counted as detected if it registers any energy deposit in the Geant4 tree.

For comparison, Figures 3 and 4 show reference data from previous papers and presentations. Our results are consistent with these references.

Moving forward, we plan to scan different parameters - magnetic fields from 0.8 to 1.6 Tesla, various target positions, and beam angles from 3 to 7 degrees.

**中文:**
现在让我们看看 Geant4 模拟结果。

我们当前的配置使用基于 1.2 特斯拉磁场中 5 度束流偏转的靶位置。探测器几何结构包括 PDC1、PDC2 和 NEBULA 阵列。

图 1 显示了 5000 个累积事例的可视化 - 你可以清楚地看到粒子轨迹在磁场中的弯曲。

图 2 展示了我们的探测效率作为角度的函数。如果事例在 Geant4 树中记录了任何能量沉积，就算作被探测到。

作为对比，图 3 和图 4 显示了来自先前论文和报告的参考数据。我们的结果与这些参考数据一致。

接下来，我们计划扫描不同的参数 - 从 0.8 到 1.6 特斯拉的磁场、各种靶位置，以及从 3 到 7 度的束流角度。

---

## 6. PDC Reconstruction - Methodology / PDC 重建 - 方法论

**English:**
Let me explain our reconstruction methodology in detail.

The PDC system primarily determines the particle direction. To get the momentum magnitude, we use a clever approach - we back-propagate the track to the target position and minimize the distance.

We've implemented three algorithms:

First, grid search - this is robust and finds the global minimum, but it's computationally expensive.

Second, gradient descent - much faster, but it can get stuck in local minima and is sensitive to noise.

Third, TMinuit from ROOT - this is currently our primary method, using the MIGRAD and SIMPLEX algorithms.

**中文:**
让我详细解释一下我们的重建方法。

PDC 系统主要确定粒子的方向。为了获得动量大小，我们使用了一个巧妙的方法 - 我们将径迹反向传播到靶位置并最小化距离。

我们实现了三种算法：

首先是网格搜索 - 这种方法稳健，能找到全局最小值，但计算成本很高。

其次是梯度下降 - 快得多，但可能陷入局部最小值，并且对噪声敏感。

第三是 ROOT 的 TMinuit - 这是我们目前的主要方法，使用 MIGRAD 和 SIMPLEX 算法。

---

## 7. Current Challenges / 当前挑战

**English:**
However, we're facing some significant challenges.

First, reconstruction quality is inconsistent. While many events are reconstructed correctly, a subset shows poor results.

Second, we observe a systematic momentum bias. When we plot the momentum residuals - that's reconstructed minus true momentum - we see a double-peak structure. One peak is centered at zero, which is good. But there's a second peak around minus 200 MeV/c, indicating systematic underestimation for some events.

Third, the speed is a real bottleneck. Reconstruction takes anywhere from 1 to 20 seconds per event, which is far too slow for production analysis.

Figure 5 shows the comparison between reconstructed and input neutron momentum. Figure 6 displays the TMinuit optimization steps. Figures 7 and 8 show our 3D event display - you can see the particle trajectories and PDC hit positions clearly.

**中文:**
然而，我们面临着一些重大挑战。

首先，重建质量不稳定。虽然许多事例重建正确，但有一部分显示出较差的结果。

其次，我们观察到系统性的动量偏差。当我们绘制动量残差 - 即重建动量减去真实动量 - 时，我们看到一个双峰结构。一个峰在零点附近，这是好的。但还有第二个峰在负 200 MeV/c 左右，表明某些事例存在系统性低估。

第三，速度是真正的瓶颈。每个事例的重建需要 1 到 20 秒，这对于生产级分析来说太慢了。

图 5 显示了重建和输入中子动量的比较。图 6 显示了 TMinuit 优化步骤。图 7 和 8 展示了我们的 3D 事例显示 - 你可以清楚地看到粒子轨迹和 PDC 击中位置。

---

## 8. Example Case Analysis / 案例分析

**English:**
Let me walk you through a specific example to illustrate the problem.

For a proton with true momentum of 629 MeV/c, we start with an initial guess of 800 MeV/c.

The MIGRAD algorithm fails with error code 4, so we fall back to SIMPLEX.

The final result gives us 618 MeV/c - which is actually quite close to the true value. However, look at the uncertainty: plus or minus 2850 MeV/c! This huge uncertainty indicates the minimization is having convergence problems.

The final distance to target is about 4.8 millimeters, and the estimated distance to minimum is very small, suggesting we're near a minimum - but probably a local one, not the global minimum.

Figure 9 shows the 2D correlation between true and reconstructed momentum. Figure 10 displays the residual distribution with that characteristic double-peak I mentioned.

**中文:**
让我通过一个具体例子来说明这个问题。

对于一个真实动量为 629 MeV/c 的质子，我们从 800 MeV/c 的初始猜测开始。

MIGRAD 算法失败，错误代码为 4，所以我们转向 SIMPLEX。

最终结果给出 618 MeV/c - 实际上非常接近真实值。但是看看不确定度：正负 2850 MeV/c！这个巨大的不确定度表明最小化过程存在收敛问题。

到靶的最终距离约为 4.8 毫米，到最小值的估计距离非常小，表明我们接近一个最小值 - 但可能是局部最小值，而非全局最小值。

图 9 显示了真实和重建动量之间的 2D 相关性。图 10 显示了我提到的具有双峰特征的残差分布。

---

## 9. Technical Deep Dive / 技术细节

**English:**
For those interested in the technical details, let me briefly explain our core algorithms.

The PDC hit reconstruction uses a center-of-mass weighted energy deposition method. We process U and V layer hits independently, apply Gaussian smearing for detector resolution - about 200 micrometers - and reconstruct 3D positions.

The trajectory calculation uses 4th-order Runge-Kutta integration for solving particle motion in magnetic fields. We implement the full relativistic kinematics and Lorentz force equations.

The current step size is 1 millimeter. This is a trade-off - smaller steps give higher accuracy but slower computation. We can probably optimize this to 2-5 millimeters for better speed.

For neutrons, we use a completely different approach based on time-of-flight, since they're neutral and unaffected by the magnetic field.

**中文:**
对于感兴趣的技术细节，让我简要解释一下我们的核心算法。

PDC 击中重建使用质心加权能量沉积方法。我们独立处理 U 和 V 层的击中，应用高斯模糊来表示探测器分辨率 - 约 200 微米 - 并重建 3D 位置。

轨迹计算使用四阶龙格-库塔积分来求解磁场中的粒子运动。我们实现了完整的相对论运动学和洛伦兹力方程。

当前的步长为 1 毫米。这是一个权衡 - 更小的步长提供更高的精度但计算更慢。我们可能可以优化到 2-5 毫米以提高速度。

对于中子，我们使用基于飞行时间的完全不同的方法，因为它们是中性的，不受磁场影响。

---

## 10. Optimization Strategy / 优化策略

**English:**
So what's our plan to address these challenges?

We have a six-point strategy:

First, algorithm optimization - we're adjusting the Runge-Kutta step size and magnetic field integration precision.

Second, batch processing - implementing memory-efficient analysis by discarding unnecessary objects.

Third, parallel computing - enabling multi-threading to process multiple events simultaneously.

Fourth, I/O optimization - using asynchronous logging to prevent blocking.

Fifth, smart initial guess - using the PDC direction and typical momentum range to help TMinuit converge better.

And sixth, adaptive step size - making the RK step size depend on magnetic field strength and trajectory curvature.

**中文:**
那么我们计划如何应对这些挑战？

我们有六点策略：

第一，算法优化 - 我们正在调整龙格-库塔步长和磁场积分精度。

第二，批处理 - 通过丢弃不必要的对象来实现内存高效的分析。

第三，并行计算 - 启用多线程来同时处理多个事例。

第四，I/O 优化 - 使用异步日志记录来防止阻塞。

第五，智能初始猜测 - 使用 PDC 方向和典型动量范围来帮助 TMinuit 更好地收敛。

第六，自适应步长 - 使 RK 步长依赖于磁场强度和轨迹曲率。

---

## 11. Next Steps and Priorities / 下一步工作和优先级

**English:**
Let me outline our next steps, organized by priority.

High priority tasks:
First, complete QMD data sampling and generate final ROOT files for Geant4 input.
Second, continue optimizing the simulation and reconstruction code for speed.
Third, systematically test different configurations - varying magnetic field, target position, and beam angle.

Medium priority:
Investigate isovector effects in deuteron simulations.
Evaluate systematic uncertainties in momentum reconstruction.
Compare our results with experimental data if available.

Low priority:
Complete technical documentation for all tools.
Refactor and optimize the codebase for better maintainability.

**中文:**
让我概述一下我们的下一步工作，按优先级组织。

高优先级任务：
首先，完成 QMD 数据采样并生成 Geant4 输入的最终 ROOT 文件。
其次，继续优化模拟和重建代码以提高速度。
第三，系统地测试不同配置 - 改变磁场、靶位置和束流角度。

中优先级：
研究氘核模拟中的同位旋矢量效应。
评估动量重建中的系统不确定度。
如果有实验数据的话，将我们的结果与之比较。

低优先级：
完成所有工具的技术文档。
重构和优化代码库以提高可维护性。

---

## 12. Summary and Conclusions / 总结与结论

**English:**
Let me summarize what we've accomplished and what challenges remain.

On the achievement side:
We have a complete, operational simulation framework from QMD through Geant4 to reconstruction.
We've implemented and tested multiple reconstruction algorithms.
We've identified the key bottlenecks - reconstruction speed and systematic bias.
And we've established baseline performance benchmarks.

Current challenges:
Reconstruction speed is only 0.05 to 1 Hz - we need to reach over 10 Hz.
The double-peak momentum bias needs to be understood and corrected.
We need large statistical samples for thorough configuration optimization.

Our key milestones are:
Complete QMD sampling for production-scale input.
Optimize reconstruction to achieve 10 Hz performance.
Run systematic configuration scans.
And validate against experimental data.

**中文:**
让我总结一下我们已经完成的工作以及仍然存在的挑战。

在成就方面：
我们有一个完整的、可操作的模拟框架，从 QMD 经过 Geant4 到重建。
我们已经实现并测试了多种重建算法。
我们已经识别出关键瓶颈 - 重建速度和系统偏差。
我们已经建立了基线性能基准。

当前挑战：
重建速度只有每秒 0.05 到 1 个事例 - 我们需要达到每秒 10 个以上。
双峰动量偏差需要被理解和修正。
我们需要大量统计样本来进行彻底的配置优化。

我们的关键里程碑是：
完成生产级输入的 QMD 采样。
优化重建以达到 10 Hz 性能。
运行系统的配置扫描。
并与实验数据进行验证。

---

## 13. Questions and Discussion / 问题与讨论

**English:**
That concludes my presentation. I'd be happy to take any questions or discuss any aspects in more detail.

Some specific points we could explore:
- The physics behind the momentum cuts
- Alternative reconstruction algorithms
- Computational optimization strategies
- Comparison with other experiments

Thank you for your attention.

**中文:**
我的报告到此结束。我很乐意回答任何问题或更详细地讨论任何方面。

我们可以探讨的一些具体要点：
- 动量筛选背后的物理原理
- 替代重建算法
- 计算优化策略
- 与其他实验的比较

感谢大家的关注。

---

## Backup Slides / 备份幻灯片

### Additional Technical Details / 额外技术细节

**English:**
If anyone is interested, I have additional slides with more technical details about:
- Detailed Runge-Kutta implementation
- TMinuit configuration parameters
- Memory profiling results
- Parallelization strategies

**中文:**
如果有人感兴趣，我还有额外的幻灯片，包含更多技术细节：
- 详细的龙格-库塔实现
- TMinuit 配置参数
- 内存分析结果
- 并行化策略

---

## Contact Information / 联系方式

**English:**
For further discussion or collaboration:
- Email: tbt23@mails.tsinghua.edu.cn
- Repository: github.com/tianbaiting/Dpol_smsimulator
- Branch: restructure-cmake

**中文:**
如需进一步讨论或合作：
- 邮箱：tbt23@mails.tsinghua.edu.cn
- 代码库：github.com/tianbaiting/Dpol_smsimulator
- 分支：restructure-cmake

---

**End of Presentation / 报告结束**
