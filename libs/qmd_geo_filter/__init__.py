"""
qmd_geo_filter - QMD 数据几何接受度筛选库

提供以下功能:
1. 动量 cut (MomentumCut) - 支持 zpol 和 ypol 不同的 cut 条件
2. QMD 数据读取 (QMDDataLoader)
3. 几何接受度筛选 (GeometryFilter)
4. 绘图工具 (PlotUtils)
"""

from .momentum_cut import MomentumCut, CutResult, create_zpol_cut, create_ypol_cut
from .qmd_data_loader import QMDDataLoader, QMDDataset
from .geometry_filter import GeometryFilter
from .plot_utils import PlotUtils
from .analysis_runner import AnalysisRunner

__all__ = [
    'MomentumCut',
    'CutResult',
    'create_zpol_cut',
    'create_ypol_cut',
    'QMDDataLoader',
    'QMDDataset',
    'GeometryFilter',
    'PlotUtils',
    'AnalysisRunner'
]
