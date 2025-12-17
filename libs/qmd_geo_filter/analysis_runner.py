"""
分析运行器模块

整合数据加载、动量cut、几何筛选和绑定功能
按照配置自动生成所有结果
"""

import os
import json
import numpy as np
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass, field, asdict
from datetime import datetime

from .qmd_data_loader import QMDDataLoader, QMDDataset
from .momentum_cut import MomentumCut, CutResult
from .geometry_filter import GeometryFilter, GeometryFilterFactory
from .plot_utils import PlotUtils


@dataclass
class AnalysisConfig:
    """分析配置"""
    # 磁场和几何配置
    field_strengths: List[float] = field(default_factory=lambda: [1.0])
    deflection_angles: List[float] = field(default_factory=lambda: [5.0])
    
    # 靶材料和极化类型
    targets: List[str] = field(default_factory=lambda: ["Pb208"])
    pol_types: List[str] = field(default_factory=lambda: ["zpol", "ypol"])
    gamma_values: List[str] = field(default_factory=lambda: ["050", "060", "070", "080"])
    
    # b 值范围
    b_range: Tuple[float, float] = (5.0, 10.0)
    
    # 动量cut参数
    momentum_cut_config: Dict[str, Any] = field(default_factory=lambda: {
        "delta_py_threshold": 150.0,
        "pt_sum_sq_threshold": 2500.0,
        "px_sum_after_rotation": 200.0,
        "phi_threshold": 0.2
    })
    
    # 动量分辨率（用于模糊测试）
    momentum_resolutions: List[float] = field(default_factory=lambda: [0.0])
    
    # 输出目录
    output_base_dir: str = ""
    
    def __post_init__(self):
        if not self.output_base_dir:
            smsimdir = os.environ.get('SMSIMDIR', 
                                      '/home/tian/workspace/dpol/smsimulator5.5')
            self.output_base_dir = os.path.join(smsimdir, 'results', 'qmd_geo_filter')
    
    def to_dict(self) -> Dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, d: Dict) -> 'AnalysisConfig':
        return cls(**d)
    
    @classmethod
    def from_json(cls, filepath: str) -> 'AnalysisConfig':
        with open(filepath, 'r') as f:
            return cls.from_dict(json.load(f))
    
    def to_json(self, filepath: str):
        Path(filepath).parent.mkdir(parents=True, exist_ok=True)
        with open(filepath, 'w') as f:
            json.dump(self.to_dict(), f, indent=2)


@dataclass
class StageResult:
    """单个阶段的结果"""
    stage: str  # "before_cut", "after_cut", "after_geometry"
    pxp: np.ndarray
    pyp: np.ndarray
    pzp: np.ndarray
    pxn: np.ndarray
    pyn: np.ndarray
    pzn: np.ndarray
    n_events: int
    ratio_info: Dict[str, Any] = field(default_factory=dict)


class AnalysisRunner:
    """
    分析运行器
    
    按照配置自动执行完整的分析流程并生成结果
    """
    
    def __init__(self, config: Optional[AnalysisConfig] = None):
        """
        初始化
        
        Args:
            config: 分析配置，None 使用默认配置
        """
        self.config = config or AnalysisConfig()
        self.data_loader = QMDDataLoader()
        self.plot_utils = PlotUtils(self.config.output_base_dir)
        
        # 存储结果
        self.results: Dict[str, Any] = {}
    
    def get_output_path(self, field_strength: float, deflection_angle: float,
                        target: str, pol_type: str) -> Path:
        """
        获取输出路径
        
        目录结构：
        results/qmd_geo_filter/
        ├── [磁场大小]T/
        │   ├── [角度]deg/
        │   │   ├── [target]/
        │   │   │   ├── [pol_type]/
        """
        path = Path(self.config.output_base_dir)
        path = path / f"{field_strength:.2f}T" / f"{deflection_angle:.0f}deg"
        path = path / target / pol_type
        path.mkdir(parents=True, exist_ok=True)
        return path
    
    def load_data(self, target: str, gamma: str, pol_type: str) -> Optional[QMDDataset]:
        """加载数据"""
        try:
            if pol_type == "zpol":
                # 加载 znp 和 zpn
                dataset_np = self.data_loader.load_zpol_data(
                    target, gamma, "znp", 
                    b_range=(int(self.config.b_range[0]), int(self.config.b_range[1]))
                )
                dataset_pn = self.data_loader.load_zpol_data(
                    target, gamma, "zpn",
                    b_range=(int(self.config.b_range[0]), int(self.config.b_range[1]))
                )
                # 合并
                dataset_np.events.extend(dataset_pn.events)
                return dataset_np
            else:  # ypol
                dataset_np = self.data_loader.load_ypol_data(
                    target, gamma, "ynp",
                    b_range=self.config.b_range
                )
                dataset_pn = self.data_loader.load_ypol_data(
                    target, gamma, "ypn",
                    b_range=self.config.b_range
                )
                dataset_np.events.extend(dataset_pn.events)
                return dataset_np
        except FileNotFoundError as e:
            print(f"Warning: Data not found - {e}")
            return None
    
    def analyze_single_configuration(self,
                                      field_strength: float,
                                      deflection_angle: float,
                                      target: str,
                                      pol_type: str,
                                      momentum_resolution: float = 0.0) -> Dict[str, Any]:
        """
        分析单个配置
        
        Args:
            field_strength: 磁场强度 [T]
            deflection_angle: 偏转角度 [度]
            target: 靶材料
            pol_type: 极化类型 ("zpol" 或 "ypol")
            momentum_resolution: 动量分辨率
            
        Returns:
            结果字典
        """
        output_path = self.get_output_path(field_strength, deflection_angle, 
                                           target, pol_type)
        
        # 创建动量cut对象 - 根据 pol_type 使用不同的 cut 条件
        momentum_cut = MomentumCut(
            pol_type=pol_type,
            enable_momentum_smearing=(momentum_resolution > 0),
            momentum_resolution=momentum_resolution
        )
        
        # 创建几何筛选器
        geo_filter = GeometryFilter(field_strength, deflection_angle)
        
        results = {
            'config': {
                'field_strength': field_strength,
                'deflection_angle': deflection_angle,
                'target': target,
                'pol_type': pol_type,
                'momentum_resolution': momentum_resolution
            },
            'gammas': {},
            'gamma_momentum_data': {}  # 保存动量数据用于跨 gamma 对比图
        }
        
        # 对每个 gamma 值进行分析
        for gamma in self.config.gamma_values:
            print(f"  Processing gamma={gamma}...")
            
            dataset = self.load_data(target, gamma, pol_type)
            if dataset is None or len(dataset.events) == 0:
                print(f"    Skipping - no data")
                continue
            
            # 获取动量数组
            pxp, pyp, pzp, pxn, pyn, pzn = dataset.get_momentum_arrays()
            
            # Stage 1: Before cuts
            stage1 = StageResult(
                stage="before_cut",
                pxp=pxp, pyp=pyp, pzp=pzp,
                pxn=pxn, pyn=pyn, pzn=pzn,
                n_events=len(pxp)
            )
            stage1.ratio_info = self.plot_utils.calculate_ratio(pxp, pxn)
            
            # Stage 2: After momentum cut
            cut_result = momentum_cut.apply_cut(pxp, pyp, pzp, pxn, pyn, pzn)
            mask = cut_result.mask
            
            # 使用旋转后的动量
            pxp_cut = cut_result.pxp_rotated[mask]
            pyp_cut = cut_result.pyp_rotated[mask]
            pzp_cut = cut_result.pzp_rotated[mask]
            pxn_cut = cut_result.pxn_rotated[mask]
            pyn_cut = cut_result.pyn_rotated[mask]
            pzn_cut = cut_result.pzn_rotated[mask]
            
            stage2 = StageResult(
                stage="after_cut",
                pxp=pxp_cut, pyp=pyp_cut, pzp=pzp_cut,
                pxn=pxn_cut, pyn=pyn_cut, pzn=pzn_cut,
                n_events=len(pxp_cut)
            )
            stage2.ratio_info = self.plot_utils.calculate_ratio(pxp_cut, pxn_cut)
            
            # Stage 3: After geometry cut
            geo_result = geo_filter.apply_geometry_filter(
                pxp_cut, pyp_cut, pzp_cut,
                pxn_cut, pyn_cut, pzn_cut
            )
            geo_mask = geo_result.mask
            
            pxp_geo = pxp_cut[geo_mask]
            pyp_geo = pyp_cut[geo_mask]
            pzp_geo = pzp_cut[geo_mask]
            pxn_geo = pxn_cut[geo_mask]
            pyn_geo = pyn_cut[geo_mask]
            pzn_geo = pzn_cut[geo_mask]
            
            stage3 = StageResult(
                stage="after_geometry",
                pxp=pxp_geo, pyp=pyp_geo, pzp=pzp_geo,
                pxn=pxn_geo, pyn=pyn_geo, pzn=pzn_geo,
                n_events=len(pxp_geo)
            )
            stage3.ratio_info = self.plot_utils.calculate_ratio(pxp_geo, pxn_geo) if len(pxp_geo) > 0 else {}
            
            # 保存图像
            self._save_plots(output_path, gamma, stage1, stage2, stage3)
            
            # 存储结果
            results['gammas'][gamma] = {
                'before_cut': {
                    'n_events': stage1.n_events,
                    'ratio': stage1.ratio_info
                },
                'after_cut': {
                    'n_events': stage2.n_events,
                    'ratio': stage2.ratio_info,
                    'cut_efficiency': cut_result.efficiency
                },
                'after_geometry': {
                    'n_events': stage3.n_events,
                    'ratio': stage3.ratio_info,
                    'geo_efficiency': geo_result.efficiency
                }
            }
            
            # 保存动量数据用于跨 gamma 对比图
            results['gamma_momentum_data'][gamma] = {
                'before_cut': (stage1.pxp, stage1.pxn),
                'after_cut': (stage2.pxp, stage2.pxn),
                'after_geometry': (stage3.pxp, stage3.pxn)
            }
        
        # 保存配置文件
        self._save_config(output_path, geo_filter, momentum_cut)
        
        # 生成 pol_type 层级的跨 gamma 对比图
        self._generate_pol_level_plots(output_path, results, pol_type, target, geo_filter)
        
        # 生成 angle 层级的几何布局图
        angle_path = output_path.parent.parent  # 回到 angle 层级
        self._generate_angle_level_plots(angle_path, geo_filter, field_strength, deflection_angle)
        
        return results
    
    def _save_plots(self, output_path: Path, gamma: str,
                    stage1: StageResult, stage2: StageResult, stage3: StageResult):
        """保存所有图像"""
        # 创建 gamma 子目录
        gamma_path = output_path / f"gamma_{gamma}"
        gamma_path.mkdir(parents=True, exist_ok=True)
        subdir = str(gamma_path.relative_to(self.config.output_base_dir))
        
        # 3D 分布图 - before cut
        if stage1.n_events > 0:
            self.plot_utils.save_3d_momentum(
                stage1.pxp, stage1.pyp, stage1.pzp,
                stage1.pxn, stage1.pyn, stage1.pzn,
                filename="3d_momentum_before_cuts",
                title=f"3D Momentum (γ={gamma}, Before Cuts)",
                output_subdir=subdir
            )
        
        # 3D 分布图 - after momentum cut
        if stage2.n_events > 0:
            self.plot_utils.save_3d_momentum(
                stage2.pxp, stage2.pyp, stage2.pzp,
                stage2.pxn, stage2.pyn, stage2.pzn,
                filename="3d_momentum_after_cuts",
                title=f"3D Momentum (γ={gamma}, After Cuts)",
                output_subdir=subdir
            )
        
        # 3D 分布图 - after geometry cut
        if stage3.n_events > 0:
            self.plot_utils.save_3d_momentum(
                stage3.pxp, stage3.pyp, stage3.pzp,
                stage3.pxn, stage3.pyn, stage3.pzn,
                filename="3d_momentum_after_geometry",
                title=f"3D Momentum (γ={gamma}, After Geometry)",
                output_subdir=subdir
            )
        
        # pxp-pxn 分布图 - 分成三张独立的图
        before_cut_data = {}
        after_cut_data = {}
        after_geometry_data = {}
        
        if stage1.n_events > 0:
            before_cut_data[gamma] = (stage1.pxp, stage1.pxn)
        if stage2.n_events > 0:
            after_cut_data[gamma] = (stage2.pxp, stage2.pxn)
        if stage3.n_events > 0:
            after_geometry_data[gamma] = (stage3.pxp, stage3.pxn)
        
        self.plot_utils.save_pxp_pxn_separate_stages(
            before_cut_data=before_cut_data,
            after_cut_data=after_cut_data,
            after_geometry_data=after_geometry_data,
            output_subdir=subdir,
            base_filename="pxp_pxn_distribution"
        )
    
    def _save_config(self, output_path: Path, geo_filter: GeometryFilter,
                     momentum_cut: MomentumCut):
        """保存配置文件"""
        config_file = output_path / "config.txt"
        
        with open(config_file, 'w') as f:
            f.write(f"Analysis Configuration\n")
            f.write(f"=" * 50 + "\n")
            f.write(f"Generated: {datetime.now().isoformat()}\n\n")
            
            f.write(f"Momentum Cut Parameters:\n")
            for key, value in momentum_cut.get_config_dict().items():
                f.write(f"  {key}: {value}\n")
            f.write("\n")
            
            f.write(geo_filter.get_config_summary())
        
        # 保存几何配置的 JSON
        geo_filter.save_config_to_file(str(output_path / "geometry_config.json"))
    
    def _generate_pol_level_plots(self, output_path: Path, results: Dict[str, Any],
                                    pol_type: str, target: str, geo_filter: GeometryFilter):
        """
        在 pol_type 层级生成跨 gamma 值的对比图
        
        Args:
            output_path: pol_type 层级的输出路径
            results: 分析结果
            pol_type: 极化类型
            target: 靶材料
            geo_filter: 几何筛选器
        """
        if not results.get('gamma_momentum_data'):
            print(f"  Warning: 没有动量数据可用于生成跨 gamma 对比图: {output_path}")
            return
            
        print(f"  Generating multi-gamma comparison plots...")
        
        # 提取所有 gamma 的动量数据
        gamma_data = results['gamma_momentum_data']
        
        # 生成跨 gamma 对比图 (3 个阶段各一张)
        output_subdir = str(output_path.relative_to(self.config.output_base_dir))
        self.plot_utils.save_multi_gamma_comparisons(
            gamma_data=gamma_data,
            pol_type=pol_type,
            target=target,
            output_subdir=output_subdir
        )
        
        print(f"  Saved multi-gamma comparison plots to: {output_path}")
    
    def _generate_angle_level_plots(self, angle_path: Path, geo_filter: GeometryFilter,
                                     field_strength: float, deflection_angle: float):
        """
        在 angle 层级生成几何布局图
        
        Args:
            angle_path: angle 层级的输出路径
            geo_filter: 几何筛选器
            field_strength: 磁场强度
            deflection_angle: 偏转角度
        """
        # 检查是否已经生成过（避免重复生成）
        geometry_plot = angle_path / "detector_geometry.pdf"
        if geometry_plot.exists():
            return
        
        print(f"  Generating detector geometry plot at angle level...")
        
        # 获取探测器配置
        target_pos = geo_filter.target_config.position if geo_filter.target_config else (0, 0, 0)
        pdc_pos = geo_filter.pdc_config.position if geo_filter.pdc_config else (0, 0, 5000)
        pdc_rotation = geo_filter.pdc_config.rotation_angle if geo_filter.pdc_config else deflection_angle
        nebula_pos = geo_filter.nebula_config.position
        
        # 保存几何布局图
        subdir = str(angle_path.relative_to(self.config.output_base_dir))
        self.plot_utils.save_detector_geometry(
            target_pos=target_pos,
            pdc_pos=pdc_pos,
            pdc_rotation=pdc_rotation,
            nebula_pos=nebula_pos,
            field_strength=field_strength,
            deflection_angle=deflection_angle,
            filename="detector_geometry",
            output_subdir=subdir
        )
    
    def run_full_analysis(self):
        """运行完整分析"""
        print("=" * 60)
        print("Starting Full D-Pol Analysis")
        print("=" * 60)
        print(f"Output directory: {self.config.output_base_dir}")
        print()
        
        all_results = {}
        
        # 遍历所有配置
        for field_strength in self.config.field_strengths:
            for deflection_angle in self.config.deflection_angles:
                for target in self.config.targets:
                    for pol_type in self.config.pol_types:
                        for resolution in self.config.momentum_resolutions:
                            config_key = f"{field_strength}T_{deflection_angle}deg_{target}_{pol_type}"
                            if resolution > 0:
                                config_key += f"_res{resolution:.3f}"
                            
                            print(f"\nAnalyzing: {config_key}")
                            print("-" * 40)
                            
                            try:
                                result = self.analyze_single_configuration(
                                    field_strength, deflection_angle,
                                    target, pol_type, resolution
                                )
                                all_results[config_key] = result
                            except Exception as e:
                                print(f"  Error: {e}")
                                continue
        
        # 生成总结报告和对比图
        self._generate_summary_report(all_results)
        self._generate_ratio_comparison_plots(all_results)
        
        print("\n" + "=" * 60)
        print("Analysis Complete!")
        print("=" * 60)
        
        return all_results
    
    def _generate_summary_report(self, all_results: Dict[str, Any]):
        """生成总结报告"""
        report_path = Path(self.config.output_base_dir) / "analysis_summary.txt"
        
        with open(report_path, 'w') as f:
            f.write("D-Pol Analysis Summary Report\n")
            f.write("=" * 60 + "\n")
            f.write(f"Generated: {datetime.now().isoformat()}\n\n")
            
            for config_key, result in all_results.items():
                f.write(f"\n{config_key}\n")
                f.write("-" * 40 + "\n")
                
                if 'gammas' in result:
                    for gamma, gamma_result in result['gammas'].items():
                        f.write(f"  γ = {gamma}:\n")
                        for stage, stage_result in gamma_result.items():
                            n = stage_result.get('n_events', 0)
                            ratio_info = stage_result.get('ratio', {})
                            ratio = ratio_info.get('ratio', 'N/A')
                            f.write(f"    {stage}: n={n}, ratio={ratio:.4f}\n" 
                                   if isinstance(ratio, float) else f"    {stage}: n={n}, ratio={ratio}\n")
    
    def _generate_ratio_comparison_plots(self, all_results: Dict[str, Any]):
        """生成 ratio 对比图（显示 before/after geometry filter）"""
        # 按磁场和角度分组
        for field_strength in self.config.field_strengths:
            for deflection_angle in self.config.deflection_angles:
                # 收集这个配置下的所有 ratio 数据（两个阶段）
                ratio_dict_after_cut = {}      # before geometry filter
                ratio_dict_after_geo = {}      # after geometry filter
                
                for config_key, result in all_results.items():
                    if (f"{field_strength}T_{deflection_angle}deg" not in config_key):
                        continue
                    
                    # 提取 target 和 pol_type
                    parts = config_key.split('_')
                    label = '_'.join(parts[2:])  # target_poltype
                    
                    if 'gammas' in result:
                        ratio_data_cut = {}
                        ratio_data_geo = {}
                        for gamma, gamma_result in result['gammas'].items():
                            # after_cut 阶段 (before geometry filter)
                            if 'after_cut' in gamma_result:
                                ratio_info = gamma_result['after_cut'].get('ratio', {})
                                if ratio_info:
                                    ratio_data_cut[gamma] = ratio_info
                            
                            # after_geometry 阶段 (after geometry filter)
                            if 'after_geometry' in gamma_result:
                                ratio_info = gamma_result['after_geometry'].get('ratio', {})
                                if ratio_info:
                                    ratio_data_geo[gamma] = ratio_info
                        
                        if ratio_data_cut:
                            ratio_dict_after_cut[label] = ratio_data_cut
                        if ratio_data_geo:
                            ratio_dict_after_geo[label] = ratio_data_geo
                
                if ratio_dict_after_cut or ratio_dict_after_geo:
                    output_path = Path(self.config.output_base_dir)
                    output_path = output_path / f"{field_strength:.2f}T" / f"{deflection_angle:.0f}deg"
                    output_path.mkdir(parents=True, exist_ok=True)
                    
                    self.plot_utils.output_dir = output_path
                    self.plot_utils.save_ratio_plot_with_stages(
                        ratio_dict_after_cut,
                        ratio_dict_after_geo,
                        filename="ratio_comparison",
                        title=f"Ratio Comparison ({field_strength}T, {deflection_angle}°)"
                    )


def create_default_config() -> AnalysisConfig:
    """创建默认配置"""
    return AnalysisConfig(
        field_strengths=[1.0],
        deflection_angles=[5.0],
        targets=["Pb208"],
        pol_types=["zpol", "ypol"],
        gamma_values=["050", "060", "070", "080"],
        b_range=(5.0, 10.0),
        momentum_resolutions=[0.0]
    )


def run_analysis_from_config(config_file: str):
    """从配置文件运行分析"""
    config = AnalysisConfig.from_json(config_file)
    runner = AnalysisRunner(config)
    return runner.run_full_analysis()
