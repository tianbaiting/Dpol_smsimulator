# 

1 画出出经过动量cut 前的 三维分布图, 以及动量cut后的 三维分布图. (pdf 和 html格式都要)

动量cut参考/home/tian/workspace/dpol/smsimulator5.5/scripts/notebooks/input_analysis这里面的cut条件. 可以把动量cut写成一个库函数, 方便以后调用. 写在smsimulator5.5/libs下, 方便之后调用 可以方便读取qmdrawdata smsimulator5.5/data/qmdrawdata . 注意qmdrawdata zpol ypol里面的文件略有差别

2 再画出用经过geometry cut后的三维分布图. (pdf 和 html格式都要)

geometry cut(filter)  需要利用/home/tian/workspace/dpol/smsimulator5.5/libs/geo_accepentce 这个库下面的.  需要按照磁场大小 和 beam 的弯曲角度确定的target位置 进行分类.   需要把不同target位置的geometry cut后的图 (即只记录能同时被pdc 和 nebula被几何接受的动量)都画出来.  而且相关配置得输出成txt文件, pdc 位置也得输出成pdf图片以供查看. 

3 并且画出来Pxp-pxn的count分布图.  (cut前,cut后,geometry cut后)

4 以及画出ratio=(pxp-pxn>0) /() 图. ratio参考

这些图都放在smsimulator5.5/results下面, 按照target gamma分类 还有按照磁场大小 和 beam 的弯曲角度确定的target位置 进行分类 , 命名体现出 是cut前后或者geometry前后.  (ratio图.随着gamma变化趋势 是把不同target的ratio图放在一起对比. 所以就不同分层级)

目录结构
```
smsimulator5.5/results/  
├── [磁场大小]/  
│   ├── [beam转过角度]/   (beam转过角度决定target位置)
|   │   ├── ratio_plots.pdf  (不同靶子 不同pol 的 ratio随着gamma变化趋势图放在一起对比)
|   │   |   ├── [不同target材料]
│   │   │   |   ├── [pol 类型(zpol or ypol)]/
│   │   │   |   | ├── 3d_momentum_before_cuts.[pdf|html]  
│   │   │   |   | ├── 3d_momentum_after_cuts.[pdf|html]  
│   │   │   |   | ├── 3d_momentum_after_geometry.[pdf|html]  
│   │   │   |   | ├── pxp_pxn_distribution_[stage].[pdf|html]  
│   │   │   |   | └── config.txt   ()
```

5 留下接口,使得我可以加入动量的模糊函数,  让我可以测试不同分辨率下 画出来Pxp-pxn的count分布图.  (cut前,cut后,geometry cut后) 画出ratio=(pxp-pxn>0) /(pxp-pxn<0) 

把运行脚本 写在scripts/下面, 运行时候的参数就再写这个脚本里面 让我可以配置 有哪些磁场大小, beam转过角度, target材料, pol类型(zpol or ypol) 以及不同分辨率下的动量模糊函数.  方便我以后直接运行这个脚本就可以生成所有结果.. 
