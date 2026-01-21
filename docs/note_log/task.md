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