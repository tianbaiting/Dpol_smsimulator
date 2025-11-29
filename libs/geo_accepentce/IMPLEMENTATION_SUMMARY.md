# 几何接受度分析库创建总结

## 已创建的文件

### 头文件 (include/)
1. **BeamDeflectionCalculator.hh** - 束流偏转计算
   - 计算氘核束流在磁场中的偏转
   - 确定不同偏转角度对应的target位置
   - 使用RK方法求解粒子运动方程

2. **DetectorAcceptanceCalculator.hh** - 探测器接受度计算
   - 计算PDC最佳位置
   - 检查粒子是否打到探测器
   - 统计几何接受度

3. **GeoAcceptanceManager.hh** - 分析管理器
   - 整合所有分析流程
   - 批量处理多个配置
   - 生成分析报告

### 源文件 (src/)
1. **BeamDeflectionCalculator.cc** - 实现束流偏转计算
2. **DetectorAcceptanceCalculator.cc** - 实现接受度计算
3. **GeoAcceptanceManager.cc** - 实现分析管理
4. **RunGeoAcceptanceAnalysis.cc** - 主程序
5. **test_geo_acceptance.cc** - 测试程序

### 配置和文档
1. **CMakeLists.txt** - 构建配置
2. **readme.md** - 完整文档
3. **QUICKSTART.md** - 快速入门指南

## 核心功能实现

### 1. 束流偏转计算
- ✅ Deuteron @ 190 MeV/u 物理参数
- ✅ 使用ParticleTrajectory进行RK4积分
- ✅ 迭代搜索target位置（0°, 5°, 10°）
- ✅ 验证计算结果

### 2. PDC最佳位置
- ✅ 追踪参考质子(Pz=600 MeV/c)
- ✅ PDC平面垂直于轨迹
- ✅ Px接受范围 ±100 MeV/c
- ✅ 自动优化位置

### 3. 接受度计算
- ✅ PDC几何接受度
- ✅ NEBULA几何接受度（固定位置）
- ✅ 符合接受度
- ⚠️ QMD数据读取接口（需要根据实际格式实现）

### 4. 报告生成
- ✅ 终端实时输出
- ✅ 文本报告
- ✅ ROOT文件输出
- ✅ 汇总表格

## 使用的物理和数学方法

### 物理常数
- 氘核质量: 1875.612928 MeV/c²
- 质子质量: 938.272 MeV/c²
- 中子质量: 939.565 MeV/c²
- 光速: 299.792458 mm/ns

### 数值方法
- Runge-Kutta 4阶积分（来自ParticleTrajectory类）
- 洛伦兹力方程求解
- 三线性插值（磁场）

### 几何算法
- 直线与平面相交
- 点到平面距离
- 向量投影

## 依赖关系

```
GeoAcceptance
    ├── Analysis库
    │   ├── MagneticField (磁场读取和插值)
    │   └── ParticleTrajectory (RK轨迹计算)
    └── ROOT
        ├── TVector3 (3维矢量)
        ├── TLorentzVector (4维动量)
        ├── TFile (文件IO)
        └── TTree (数据存储)
```

## 编译和使用

### 编译
```bash
cd smsimulator5.5/build
cmake .. -DBUILD_ANALYSIS=ON
make GeoAcceptance -j4
```

### 运行
```bash
# 默认配置
./bin/RunGeoAcceptanceAnalysis

# 自定义配置
./bin/RunGeoAcceptanceAnalysis \
  --fieldmap path/to/field.dat --field 0.8 \
  --angle 0 --angle 5 --angle 10 \
  --output ./results
```

## 待完成事项

### 必须完成
- [ ] **实现QMD数据读取**: `DetectorAcceptanceCalculator::LoadQMDData()`
  - 需要了解QMD数据格式（ROOT文件？ASCII？）
  - 读取n和p的动量和反应顶点信息
  - 填充`fProtons`和`fNeutrons`向量

### 可选优化
- [ ] 并行化粒子循环（OpenMP）
- [ ] RK步长自适应
- [ ] PDC尺寸优化算法
- [ ] 事例显示可视化
- [ ] 角度分布直方图
- [ ] 更多物理量输出

### 文档改进
- [ ] 添加物理背景说明
- [ ] 绘制几何示意图
- [ ] 添加示例分析结果
- [ ] API参考文档

## 关键接口

### 用户接口（GeoAcceptanceManager）
```cpp
GeoAcceptanceManager* mgr = new GeoAcceptanceManager();
mgr->AddFieldMap(filename, fieldStrength);
mgr->AddDeflectionAngle(angle);
mgr->SetQMDDataPath(path);
mgr->RunFullAnalysis();
mgr->PrintResults();
```

### 束流计算接口
```cpp
BeamDeflectionCalculator* beam = new BeamDeflectionCalculator();
beam->LoadFieldMap(filename);
auto target = beam->CalculateTargetPosition(5.0);  // 5度
beam->PrintTargetPosition(target);
```

### 探测器接受度接口
```cpp
DetectorAcceptanceCalculator* det = new DetectorAcceptanceCalculator();
det->SetMagneticField(magField);
det->LoadQMDData(dataFile);
auto pdc = det->CalculateOptimalPDCPosition(targetPos);
auto result = det->CalculateAcceptance();
```

## 输出示例

### 终端输出
```
╔═══════╦═══════╦════════════╦════════════╦════════════╗
║ Field ║ Angle ║    PDC     ║   NEBULA   ║ Coincidence║
║  (T)  ║ (deg) ║    (%)     ║    (%)     ║    (%)     ║
╠═══════╬═══════╬════════════╬════════════╬════════════╣
║  0.80 ║  0.00 ║      45.23 ║      67.89 ║      38.45 ║
...
```

### ROOT文件结构
```
acceptance_results.root
└── acceptance (TTree)
    ├── fieldStrength
    ├── deflectionAngle
    ├── targetX, targetY, targetZ
    ├── pdcX, pdcY, pdcZ
    ├── pdcAcceptance
    ├── nebulaAcceptance
    └── coincidenceAcceptance
```

## 测试建议

### 单元测试
```bash
./bin/test_geo_acceptance
```

### 物理验证
1. 检查0度时target确实在(0,0,-4000)
2. 验证束流动量计算 = 190 MeV/u × 2
3. 检查PDC位置合理性
4. 验证直线粒子（中子）传播

### 性能测试
- 测试不同步长的精度和速度
- 评估大数据量处理能力
- 内存使用监控

## 技术说明

### 坐标系
- 原点：实验室中心
- Z轴：束流方向（正方向向下游）
- X轴：水平横向
- Y轴：竖直向上

### 单位系统
- 长度: mm
- 动量: MeV/c
- 能量: MeV
- 时间: ns
- 磁场: Tesla

## 集成到主项目

已修改的文件:
- `libs/CMakeLists.txt` - 添加geo_accepentce子目录

确保在主CMakeLists.txt中设置:
```cmake
option(BUILD_ANALYSIS "Build analysis libraries" ON)
```

## 参考资料

- SAMURAI磁场数据: http://ribf.riken.jp/~hsato/fieldmaps/
- QMD数据位置: data/qmdrawdata/
- Analysis库文档: libs/analysis/
- ROOT文档: https://root.cern.ch/

## 总结

这个几何接受度分析库提供了完整的框架来:
1. 计算不同磁场和偏转角度下的target位置
2. 优化PDC探测器位置
3. 计算PDC和NEBULA的几何接受度
4. 生成详细的分析报告

主要优势:
- ✅ 模块化设计，易于扩展
- ✅ 使用现有的MagneticField和ParticleTrajectory类
- ✅ 完整的文档和示例
- ✅ ROOT集成，便于后续分析

下一步需要根据实际的QMD数据格式实现数据读取功能。
