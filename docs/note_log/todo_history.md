# TODO（待办事项）

## 动量选择器（selector）的 isovector 效应
- 目标：评估在几何接受度限制下，np 粒子动量分布是否存在 isovector 效应。
- 步骤：
    - 计算几何接受度（acceptance）。
    - 在被接受的 np 事件中绘制粒子动量分布（分组比较不同配置/选择器）。
    - 进行统计检验（如 KS 检验、p-value）判断差异显著性。
- 输出：对比图、统计表格和结论。
- 记录字段：数据样本、选择器配置、绘图脚本路径。

## 比较：动量精度与接受度
- 目标：量化动量重建的精度（bias、resolution）随几何/仪器接受度的变化。
- 步骤：
    - 使用模拟真值计算重建动量与真值的残差分布。
    - 按接受度区间（或角度、p 范围）分组统计分辨率与效率。
    - 绘制：resolution vs p、efficiency vs acceptance 等图。
- 输出：分辨率曲线、效率曲线和总结表。

## evaluate runge-kutta step size impact 

first step. evaluate the impact of hitting places. use 1mm to be the standard. then evaluate  2mm, 3mm, 4mm, 5mm till 10mm.

## 

研究如何加入pdc的动量分辨率


磁场重新调研, 动量范围 ,线性, 以及是否击中(geant4)


pdc 尽量固定在exit window后面, 观察总的.  与qmdfilter可能再重新命名一个 或者加功能吧. 怎么加开关比较好.


geant4里面和 qmd里面对比一下区别, 特别是靶位置有没有弄对. 对比氘核轨迹 .

```bash
/control/getEnv SMSIMDIR
```

todo

dicussion with aki 

- [] target
- [] vacuum exit whindos
- [] 支架 in magnet
- [] polar mechanics


26号. 


lib : /home/tian/workspace/dpol/smsimulator5.5/libs/qmd_geo_filter, this lib is to determine whether a particle hits the pdc(nebula) or not.  the pdc's position is fiexd or determined by some proton tracks.   but there are bugs in the pdc position set. the rotation and the drawing of pdf is not same. you should check the rotation matrix and the drawing part.  And offer a better srtucture, but keep the functionality.  also,  add more comments to explain the logic.  the main problem is the pdc position setting and the rotation matrix.   

this lib uses runge-kutta to caculate the track in the magnetic field. and uses the load magenetic method. these are ohter libs function. you should use these functions directly.

use lab frame. rotate theta angle,  i think you can use the same defination of smg4lib.  they first set the pdc at (x,y,z) and the rotate it at the (0,0,0) instead of rotate at (x,y,z). the defaut pdc positoin in this defination is  

#-----------------------------------
# PDCs
# Z should be distance from MAGcenter to PDC Z=0 plane
/samurai/geometry/PDC/Angle 65 deg
/samurai/geometry/PDC/Position1 0 0 400 cm
/samurai/geometry/PDC/Position2 0 0 500 cm

# 最后一个是中心距离原点的距离。 第一个是距离原点连线的垂直线上的位移/
# PDC is not added - SetPDC command not available
# /samurai/geometry/PDC/SetPDC false


 1. Match sim_deuteron_core more literally: treat Position1 and Position2 as two PDC planes and accept a hit if the track hits both.




look at the /home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/track_vis_useTree, /home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/combainPD , you should do this in the /home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/detailMag1to1.2T for detaild magnetic filed and beam rotation angle from 0 1,2,3,..10 deg.  0 deg refers to target position in (0, 0, -4m), the other ang should caculate by the function in the geo_accepentce lib, to caculate the target postion. you should comine the 4 proton track 4 neutron track and deutron beam track.

configs/simulation/geometry/filed_map in this folder, 
/180626-1,20T-3000.zip
/180703-1,15T-3000.zip
/180703-1,10T-3000.zip
/180703-1,05T-3000.zip
/180626-1,00T-3000.zip

