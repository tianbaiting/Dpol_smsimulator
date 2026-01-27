# 

step 1:

Trajectory Calculation	4th-order Runge-Kutta integration

测试不同步长下面的轨迹计算差别, 以真实数据,真实磁场为准. libs/analysis 利用这个里面的eventdispaly.  原始数据在data/qmdrawdata/qmdrawdata/z_pol/b_discrete, 通过类似scripts/event_generator/dpol_eventgen/GenInputRoot_np_atime.cc 生成的root文件.  通过simsimulator 进行轨迹计算, 输出到root文件, 然后用eventdisplay进行可视化 scripts/visualization/run_display.C这个脚本可能需要稍微改一下, 画出不同步长的轨迹. 以及画出能量沉积点. 

测试出比较节省计算时间的步长.

step 2:

Geometry Acceptance	PDC/NEBULA position optimization	GeoAcceptanceManager::CalculateForConfiguration()

加入新功能, 加入开关,使得pdc位置固定,然后计算QMD Filtering	Event-by-event geometric filtering.(也要保持原来的功能, 在特定Px范围内扫描pdc位置, 找到最优位置. px的范围可以通过参数传入, 作图那些地方的px取值的从刚才那里传入,不能直接写个数值)

并且qmd filtering 里面加入pdc位置的参数(可以传入刚才所说的固定位置), 计算几何接受度.   

qmd filtering里面nebula接受用blue, pdc接受用red. 没接收就用灰色, 都接收就用绿色,




1,在qmd filter 的 所有关于磁铁 pdc nebula 的位置的地方, 都要画出氘核track, 以及 $px= \pm 100MeV$ track , 以及一个用参数控制的$ px = \pm 150MeV $的 track(150 设置成 确定pdc位置的默认protontrack参数).  并且磁场,beam偏转角度等参数api都要写清楚. 我该怎么调用.  这样方便我以后再用不同的磁场, beam偏转角度等参数进行测试.  

2,我的test_pdc_position.cc程序 也要满足此要求.  我要画出不同targetposition下的 target位置. scripts/analysis/pdc_position/run_test_pdc_position.sh这个脚本画出来的图都要满足这几个要求 ,清晰看得出氘核track一条, 四条proton track.

3,以及我需要再运行qmd_filter. 固定pdc位置, 计算几何接受度, (qmd filtering里面nebula接受用blue, pdc接受用red. 没接收就用灰色, 都接收就用绿色,),  这里面的总的gamma对比图得注意数据的清除,注意数据周期. 注意每个相同参数产生的对比图做出来. 以及生成一个不同参数下ratio的txt文件.  方便后续分析.pdc固定位置为, /samurai/geometry/PDC/Angle 65 deg
/samurai/geometry/PDC/Position1 +0 0 400 cm
/samurai/geometry/PDC/Position2 +0 0 500 cm

注意这个位置的定义是先放在0 0 400 旋转是绕原点. 而qmdfilter 里面旋转位置定义不一样. 


4 sim_deutron, 我需要在同样的pdc位置下, 在configs/simulation/DbeamTest/里, 不同beam偏转角度决定的靶点位置(有geo_acceptance_manager计算出来的), n p 画出来100MeV 150MeV track. 以及氘核track.  画出不同磁场下的轨迹图. 以及能量沉积点.    我希望用循环的方式在一个run里面完成. 最好是可以批量运行不同磁场 不同beam偏转角度的轨迹图.  以及能量沉积点的图, 放置在$SMSIMDIR/results目录下面mkdir目录. 命名要清楚. 方便后续查看.

你对于靶点位置的计算是错误的, 你应该调用geo_acceptance_manager 里面的计算靶点位置的api,  而不是自己计算.  你自己计算的结果和geo_acceptance_manager 计算出来的结果是有差别的.  你要改正过来. 
不可能全部都是z= -400mm


1,在qmd filter 的 所有关于磁铁 pdc nebula 的位置的地方, 都要画出氘核track, 以及 
p
x
=
±
100
M
e
V
px=±100MeV track , 以及一个用参数控制的
p
x
=
±
150
M
e
V
px=±150MeV的 track(150 设置成 确定pdc位置的默认protontrack参数). 并且磁场,beam偏转角度等参数api都要写清楚. 我该怎么调用. 这样方便我以后再用不同的磁场, beam偏转角度等参数进行测试.

2,我的test_pdc_position.cc程序 也要满足此要求. 我要画出不同targetposition下的 target位置. scripts/analysis/pdc_position/run_test_pdc_position.sh这个脚本画出来的图都要满足这几个要求 ,清晰看得出氘核track一条, 四条proton track.

3,以及我需要再运行qmd_filter. 固定pdc位置, 计算几何接受度, (qmd filtering里面nebula接受用blue, pdc接受用red. 没接收就用灰色, 都接收就用绿色,), 这里面的总的gamma对比图得注意数据的清除,注意数据周期. 注意每个相同参数产生的对比图做出来. 以及生成一个不同参数下ratio的txt文件. 方便后续分析.pdc固定位置为, /samurai/geometry/PDC/Angle 65 deg
/samurai/geometry/PDC/Position1 +0 0 400 cm
/samurai/geometry/PDC/Position2 +0 0 500 cm

注意这个位置的定义是先放在0 0 400 旋转是绕原点. 而qmdfilter 里面旋转位置定义不一样.

4 sim_deutron, 我需要在同样的pdc位置下, 在configs/simulation/DbeamTest/里, 不同beam偏转角度决定的靶点位置(有geo_acceptance_manager计算出来的), n p 画出来100MeV 150MeV track. 以及氘核track. 画出不同磁场下的轨迹图. 以及能量沉积点. 我希望用循环的方式在一个run里面完成. 最好是可以批量运行不同磁场 不同beam偏转角度的轨迹图. 以及能量沉积点的图, 放置在$SMSIMDIR/results目录下面mkdir目录. 命名要清楚. 方便后续查看.    循环只是为了把氘核质子画在一张图里面. x让氘核不使用targetangle的同时让np使用targetangle. 我希望的是不同磁场不同deg出不同的图, 而同一个磁场同一个deg的氘核质子画在一起. 

2. 我希望qmd_geofilter的默认是PDC位置固定, 我可以画出固定pdc下的中子质子接受情况


pdc固定位置为, /samurai/geometry/PDC/Angle 65 deg
/samurai/geometry/PDC/Position1 +0 0 400 cm
/samurai/geometry/PDC/Position2 +0 0 500 cm

sim_deutron的geant4程序. 这里面写入不同的配置文件.

靶点位置信息在
[text](../../results/target_configs/target_summary_B200T.txt) [text](../../results/target_configs/target_summary_B160T.txt) [text](../../results/target_configs/target_summary_B120T.txt) [text](../../results/target_configs/target_summary_B100T.txt) [text](../../results/target_configs/target_summary_B80T.txt)

我需要画出某一个磁场下,某一个beamp偏转角度下的轨迹图. 最好能够氘核质子画在一张图里(得保证 氘核 质子在一个run里面,可能得用/control/foreach run_one_particle.mac ).  氘核是380MeV的动能,在(0 , 0, -4m)出射, 质子是动量(100MeV,0,  627MeV/c) 从靶点位置出射,使用/samurai/geometry/Target/Angle ** deg.     不能的话就分别画出来.   

最好批量运行不同磁场强度 不同偏转角度 ,导出png . 不行的话就把配置写出来用注释的方式写出来. 方便我以后直接运行. 写出配置,我只需要重新注释掉就行了.

质子是动量(\pm 100MeV/c, 0,627MeV/c) (\pm 150MeV,0, 627MeV/c) 从靶点位置出射,使用/samurai/geometry/Target/Angle ** deg. 你采用tree的方式把这质子输入 . 只用射入质子 不用氘核了.

在/home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/track_vis_useTree这个目录下建立相关的配置文件.  以及批量运行脚本.  方便我以后直接运行.





pdc固定位置为, /samurai/geometry/PDC/Angle 65 deg
/samurai/geometry/PDC/Position1 +0 0 400 cm
/samurai/geometry/PDC/Position2 +0 0 500 cm

sim_deutron的geant4程序. 这里面写入不同的配置文件.

靶点位置信息在
[text](../../results/target_configs/target_summary_B200T.txt) [text](../../results/target_configs/target_summary_B160T.txt) [text](../../results/target_configs/target_summary_B120T.txt) [text](../../results/target_configs/target_summary_B100T.txt) [text](../../results/target_configs/target_summary_B80T.txt)

我需要画出某一个磁场下,某一个beamp偏转角度下的轨迹图. 最好能够氘核质子画在一张图里(得保证 氘核 质子在一个run里面,得用/control/loop 实现循环在一个run里面循环不同的粒子以及动量 ).  氘核是380MeV的动能,在(0 , 0, -4m)出射, 质子是动量(100MeV,0,  627MeV/c) 从靶点位置出射,使用/samurai/geometry/Target/Angle ** deg.     不能的话就分别画出来.   

最好批量运行不同磁场强度 不同偏转角度 ,导出png . 不行的话就把配置写出来用注释的方式写出来. 方便我以后直接运行. 写出配置,我只需要重新注释掉就行了.