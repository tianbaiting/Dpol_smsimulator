<!-- 给aki 和 mizuki 介绍工作，以及进度的报告 -->

# optimazation

## 目的

确定 dpol_breakup 实验 的配置




## 程序 架构 data流程

qmd_rawdata geant4_input  geant4_output pdc_analysis(renconstruc geant4_input_proton_momentum)

scripts: 

qmd_rawdata trans cut sampleing()

reconstructed analysis （可视化 和 批处理）


geant 处理事件速度 大约 120events/s.

pdc重建 大约 0.05 - 1 events/s.



现在框架 搭好了。 


需要测试 优化结构


有部分event的重建效果不好。 优化PDCanalysis 重建过程中磁场中例子运动，RK method 步长，使得nebula


## detail

### qmd data

because to much data.


cut: to cut on p_x to see the what we want


sampleling:  not yet todo



### geant 4

need to generate many candidate mac to see the results. 

mac:  magnetic value, target position(together with pdc position) to see the detection effeciency.


## pdc analysis

由于pdc只能确定方向，重建靶点位置需要以重建track距离targt最近为最优化指标 优化 动量大小。 


我目前写了三种方式， 网格， 梯度下降， 调用tminiut


需要 着重优化。 
 

调整磁场中带电粒子运动，track计算步长, 精度和速度取得一个平衡。


