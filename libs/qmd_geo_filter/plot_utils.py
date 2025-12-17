"""
绘图工具模块

提供3D分布图、pxp-pxn分布图、ratio图等绑定功能
支持输出 PDF 和 HTML 格式
"""

import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from mpl_toolkits.mplot3d import Axes3D
from typing import Optional, List, Dict, Tuple, Any
from pathlib import Path
import warnings

# 尝试导入 plotly（用于交互式 HTML 图）
try:
    import plotly.graph_objects as go
    import plotly.express as px
    from plotly.subplots import make_subplots
    HAS_PLOTLY = True
except ImportError:
    HAS_PLOTLY = False
    warnings.warn("plotly not installed. HTML output will not be available.")


class PlotStyle:
    """绘图风格设置"""
    
    @staticmethod
    def set_publication_style():
        """设置出版物风格"""
        plt.rcParams.update({
            "font.family": "sans-serif",
            "font.sans-serif": ["Arial", "Helvetica", "DejaVu Sans"],
            "axes.labelsize": 12,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
            "legend.fontsize": 10,
            "axes.titlesize": 14,
            "axes.linewidth": 1.0,
            "lines.linewidth": 1.5,
            "xtick.major.size": 4,
            "xtick.major.width": 1.0,
            "ytick.major.size": 4,
            "ytick.major.width": 1.0,
            "figure.dpi": 150,
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.dpi": 300,
            "savefig.bbox": "tight",
        })
    
    @staticmethod
    def get_color_palette(n: int) -> List[str]:
        """获取颜色调色板"""
        if n <= 10:
            return plt.cm.tab10.colors[:n]
        else:
            return [plt.cm.viridis(i / n) for i in range(n)]


class PlotUtils:
    """绑定工具类"""
    
    def __init__(self, output_dir: str = "results"):
        """
        初始化
        
        Args:
            output_dir: 输出目录
        """
        self.output_dir = Path(output_dir)
        PlotStyle.set_publication_style()
    
    def ensure_output_dir(self, subdir: str = "") -> Path:
        """确保输出目录存在"""
        path = self.output_dir / subdir if subdir else self.output_dir
        path.mkdir(parents=True, exist_ok=True)
        return path
    
    # ========== 3D 分布图 ==========
    
    def plot_3d_momentum_matplotlib(self, 
                                     pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                                     pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray,
                                     title: str = "3D Momentum Distribution",
                                     sample_size: int = 5000) -> plt.Figure:
        """
        使用 matplotlib 绑定 3D 动量分布图
        
        Args:
            pxp, pyp, pzp: 质子动量
            pxn, pyn, pzn: 中子动量
            title: 图标题
            sample_size: 采样大小（避免点太多）
            
        Returns:
            matplotlib Figure
        """
        fig = plt.figure(figsize=(12, 5))
        
        # 采样
        n_total = len(pxp)
        if n_total > sample_size:
            idx = np.random.choice(n_total, sample_size, replace=False)
        else:
            idx = np.arange(n_total)
        
        # 质子 3D 分布
        ax1 = fig.add_subplot(121, projection='3d')
        ax1.scatter(pxp[idx], pyp[idx], pzp[idx], c='red', alpha=0.5, s=1, label='Proton')
        ax1.set_xlabel('$p_x$ (MeV/c)')
        ax1.set_ylabel('$p_y$ (MeV/c)')
        ax1.set_zlabel('$p_z$ (MeV/c)')
        ax1.set_title('Proton Momentum')
        ax1.legend()
        
        # 中子 3D 分布
        ax2 = fig.add_subplot(122, projection='3d')
        ax2.scatter(pxn[idx], pyn[idx], pzn[idx], c='blue', alpha=0.5, s=1, label='Neutron')
        ax2.set_xlabel('$p_x$ (MeV/c)')
        ax2.set_ylabel('$p_y$ (MeV/c)')
        ax2.set_zlabel('$p_z$ (MeV/c)')
        ax2.set_title('Neutron Momentum')
        ax2.legend()
        
        fig.suptitle(title, fontsize=14)
        plt.tight_layout()
        
        return fig
    
    def plot_3d_momentum_plotly(self,
                                pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                                pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray,
                                title: str = "3D Momentum Distribution",
                                sample_size: int = 10000) -> Optional['go.Figure']:
        """
        使用 plotly 绑定交互式 3D 动量分布图
        
        Returns:
            plotly Figure 或 None（如果 plotly 不可用）
        """
        if not HAS_PLOTLY:
            return None
        
        # 采样
        n_total = len(pxp)
        if n_total > sample_size:
            idx = np.random.choice(n_total, sample_size, replace=False)
        else:
            idx = np.arange(n_total)
        
        fig = make_subplots(
            rows=1, cols=2,
            specs=[[{'type': 'scatter3d'}, {'type': 'scatter3d'}]],
            subplot_titles=('Proton Momentum', 'Neutron Momentum')
        )
        
        # 质子
        fig.add_trace(
            go.Scatter3d(
                x=pxp[idx], y=pyp[idx], z=pzp[idx],
                mode='markers',
                marker=dict(size=2, color='red', opacity=0.5),
                name='Proton'
            ),
            row=1, col=1
        )
        
        # 中子
        fig.add_trace(
            go.Scatter3d(
                x=pxn[idx], y=pyn[idx], z=pzn[idx],
                mode='markers',
                marker=dict(size=2, color='blue', opacity=0.5),
                name='Neutron'
            ),
            row=1, col=2
        )
        
        fig.update_layout(
            title=title,
            scene=dict(
                xaxis_title='px (MeV/c)',
                yaxis_title='py (MeV/c)',
                zaxis_title='pz (MeV/c)'
            ),
            scene2=dict(
                xaxis_title='px (MeV/c)',
                yaxis_title='py (MeV/c)',
                zaxis_title='pz (MeV/c)'
            )
        )
        
        return fig
    
    def save_3d_momentum(self, 
                         pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                         pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray,
                         filename: str,
                         title: str = "3D Momentum Distribution",
                         output_subdir: str = ""):
        """
        保存 3D 动量分布图（PDF 和 HTML）
        
        Args:
            filename: 文件名（不含扩展名）
            output_subdir: 子目录
        """
        output_path = self.ensure_output_dir(output_subdir)
        
        # PDF
        fig_mpl = self.plot_3d_momentum_matplotlib(pxp, pyp, pzp, pxn, pyn, pzn, title)
        fig_mpl.savefig(output_path / f"{filename}.pdf")
        plt.close(fig_mpl)
        
        # HTML
        if HAS_PLOTLY:
            fig_plotly = self.plot_3d_momentum_plotly(pxp, pyp, pzp, pxn, pyn, pzn, title)
            if fig_plotly:
                fig_plotly.write_html(str(output_path / f"{filename}.html"))
    
    # ========== pxp - pxn 分布图 ==========
    
    def plot_pxp_pxn_distribution(self,
                                   pxp: np.ndarray, pxn: np.ndarray,
                                   label: str = None,
                                   bins: int = 50,
                                   range_limit: Tuple[float, float] = (-400, 400),
                                   ax: plt.Axes = None) -> plt.Axes:
        """
        绑定 pxp - pxn 分布直方图
        
        Args:
            pxp: 质子 px 数组
            pxn: 中子 px 数组
            label: 图例标签
            bins: 分箱数
            range_limit: x轴范围
            ax: 已有的 Axes，None 则创建新的
            
        Returns:
            matplotlib Axes
        """
        if ax is None:
            fig, ax = plt.subplots(figsize=(8, 6))
        
        diff = pxp - pxn
        ax.hist(diff, bins=bins, range=range_limit, histtype='step', 
                linewidth=1.5, label=label)
        
        ax.set_xlabel('$p_{x,p} - p_{x,n}$ (MeV/c)', fontsize=12)
        ax.set_ylabel('Counts', fontsize=12)
        ax.axvline(x=0, color='gray', linestyle='--', alpha=0.5)
        
        # 简洁的样式
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        if label:
            ax.legend(frameon=False)
        
        return ax
    
    def plot_pxp_pxn_multi_gamma(self,
                                  data_dict: Dict[str, Tuple[np.ndarray, np.ndarray]],
                                  title: str = "Distribution of $p_{x,p} - p_{x,n}$",
                                  bins: int = 30) -> plt.Figure:
        """
        绑定多个 gamma 值的 pxp-pxn 分布对比图
        
        Args:
            data_dict: {gamma_label: (pxp_array, pxn_array)} 字典
            title: 图标题
            bins: 分箱数
            
        Returns:
            matplotlib Figure
        """
        fig, ax = plt.subplots(figsize=(8, 6))
        
        colors = PlotStyle.get_color_palette(len(data_dict))
        
        for i, (gamma_label, (pxp, pxn)) in enumerate(data_dict.items()):
            diff = pxp - pxn
            ax.hist(diff, bins=bins, histtype='step', linewidth=1.5,
                   label=f'$\\gamma$ = {gamma_label}', color=colors[i])
        
        ax.set_title(title, fontsize=14)
        ax.set_xlabel('$p_{x,p} - p_{x,n}$ (MeV/c)', fontsize=12)
        ax.set_ylabel('Frequency', fontsize=12)
        ax.legend(frameon=False, title='Category')
        
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.axvline(x=0, color='gray', linestyle='--', alpha=0.5)
        
        plt.tight_layout()
        return fig
    
    def save_pxp_pxn_distribution(self,
                                   data_dict: Dict[str, Tuple[np.ndarray, np.ndarray]],
                                   filename: str,
                                   title: str = "Distribution of $p_{x,p} - p_{x,n}$",
                                   output_subdir: str = ""):
        """保存 pxp-pxn 分布图"""
        output_path = self.ensure_output_dir(output_subdir)
        
        # PDF
        fig = self.plot_pxp_pxn_multi_gamma(data_dict, title)
        fig.savefig(output_path / f"{filename}.pdf")
        plt.close(fig)
        
        # HTML (使用 plotly)
        if HAS_PLOTLY:
            fig_plotly = go.Figure()
            for gamma_label, (pxp, pxn) in data_dict.items():
                diff = pxp - pxn
                fig_plotly.add_trace(go.Histogram(
                    x=diff, name=f'γ = {gamma_label}',
                    opacity=0.7, nbinsx=30
                ))
            
            fig_plotly.update_layout(
                title=title,
                xaxis_title='pxp - pxn (MeV/c)',
                yaxis_title='Counts',
                barmode='overlay'
            )
            fig_plotly.write_html(str(output_path / f"{filename}.html"))
    
    def plot_pxp_pxn_single_stage(self,
                                   data_dict: Dict[str, Tuple[np.ndarray, np.ndarray]],
                                   stage: str,
                                   title: str = None,
                                   bins: int = 30) -> plt.Figure:
        """
        绑定单个阶段的 pxp-pxn 分布图
        
        Args:
            data_dict: {gamma_label: (pxp_array, pxn_array)} 字典
            stage: 阶段名称 ("before_cut", "after_cut", "after_geometry")
            title: 图标题，None 则自动生成
            bins: 分箱数
            
        Returns:
            matplotlib Figure
        """
        stage_titles = {
            "before_cut": "Before Momentum Cut",
            "after_cut": "After Momentum Cut", 
            "after_geometry": "After Geometry Filter"
        }
        
        if title is None:
            title = f"$p_{{x,p}} - p_{{x,n}}$ Distribution: {stage_titles.get(stage, stage)}"
        
        fig, ax = plt.subplots(figsize=(8, 6))
        fig.patch.set_facecolor('white')
        ax.set_facecolor('white')
        
        colors = PlotStyle.get_color_palette(len(data_dict))
        
        for i, (gamma_label, (pxp, pxn)) in enumerate(data_dict.items()):
            if len(pxp) == 0:
                continue
            diff = pxp - pxn
            # 将 gamma 标签格式化为小数 (例如 "050" -> "0.5")
            if isinstance(gamma_label, str) and len(gamma_label) == 3:
                gamma_display = f"{float(gamma_label)/100:.1f}"
            else:
                gamma_display = str(gamma_label)
            ax.hist(diff, bins=bins, histtype='step', linewidth=1.5,
                   label=f'$\\gamma$ = {gamma_display}', color=colors[i])
        
        ax.set_xlabel('$p_{x,p} - p_{x,n}$ (MeV/c)', fontsize=12)
        ax.set_ylabel('Counts', fontsize=12)
        ax.set_title(title, fontsize=14)
        ax.legend(frameon=False, loc='upper right')
        
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.axvline(x=0, color='gray', linestyle='--', alpha=0.5)
        ax.tick_params(axis='both', which='major', direction='out')
        
        plt.tight_layout()
        return fig
    
    def save_pxp_pxn_separate_stages(self,
                                      before_cut_data: Dict[str, Tuple[np.ndarray, np.ndarray]],
                                      after_cut_data: Dict[str, Tuple[np.ndarray, np.ndarray]],
                                      after_geometry_data: Dict[str, Tuple[np.ndarray, np.ndarray]],
                                      output_subdir: str = "",
                                      base_filename: str = "pxp_pxn_distribution"):
        """
        分别保存三个阶段的 pxp-pxn 分布图（三张独立的图）
        
        Args:
            before_cut_data: cut 前的数据 {gamma: (pxp, pxn)}
            after_cut_data: cut 后的数据 {gamma: (pxp, pxn)}
            after_geometry_data: 几何筛选后的数据 {gamma: (pxp, pxn)}
            output_subdir: 输出子目录
            base_filename: 基础文件名
        """
        output_path = self.ensure_output_dir(output_subdir)
        
        stages = [
            ("before_cut", before_cut_data, "Before Momentum Cut"),
            ("after_cut", after_cut_data, "After Momentum Cut"),
            ("after_geometry", after_geometry_data, "After Geometry Filter")
        ]
        
        for stage_name, data_dict, stage_title in stages:
            if not data_dict or all(len(v[0]) == 0 for v in data_dict.values()):
                continue
            
            # PDF
            fig = self.plot_pxp_pxn_single_stage(data_dict, stage_name)
            fig.savefig(output_path / f"{base_filename}_{stage_name}.pdf")
            plt.close(fig)
            
            # HTML (使用 plotly)
            if HAS_PLOTLY:
                fig_plotly = go.Figure()
                for gamma_label, (pxp, pxn) in data_dict.items():
                    if len(pxp) == 0:
                        continue
                    diff = pxp - pxn
                    # 格式化 gamma 标签
                    if isinstance(gamma_label, str) and len(gamma_label) == 3:
                        gamma_display = f"{float(gamma_label)/100:.1f}"
                    else:
                        gamma_display = str(gamma_label)
                    fig_plotly.add_trace(go.Histogram(
                        x=diff, name=f'γ = {gamma_display}',
                        opacity=0.7, nbinsx=30
                    ))
                
                fig_plotly.update_layout(
                    title=f"$p_{{x,p}} - p_{{x,n}}$ Distribution: {stage_title}",
                    xaxis_title='pxp - pxn (MeV/c)',
                    yaxis_title='Counts',
                    barmode='overlay'
                )
                fig_plotly.add_vline(x=0, line_dash="dash", line_color="gray", opacity=0.5)
                fig_plotly.write_html(str(output_path / f"{base_filename}_{stage_name}.html"))
    
    # ========== Ratio 图 ==========
    
    def calculate_ratio(self, pxp: np.ndarray, pxn: np.ndarray) -> Dict[str, Any]:
        """
        计算 ratio = (pxp-pxn>0) / (pxp-pxn<0)
        
        Args:
            pxp: 质子 px 数组
            pxn: 中子 px 数组
            
        Returns:
            包含 ratio, n_positive, n_negative, error 的字典
        """
        diff = pxp - pxn
        n_positive = np.sum(diff > 0)
        n_negative = np.sum(diff < 0)
        
        if n_negative > 0:
            ratio = n_positive / n_negative
            # 二项误差估计
            n_total = n_positive + n_negative
            error = ratio * np.sqrt(1/n_positive + 1/n_negative) if n_positive > 0 else 0
        else:
            ratio = np.inf
            error = 0
        
        return {
            'ratio': ratio,
            'n_positive': n_positive,
            'n_negative': n_negative,
            'n_total': len(diff),
            'error': error
        }
    
    def plot_ratio_vs_gamma(self,
                            ratio_data: Dict[str, Dict[str, float]],
                            title: str = "Ratio vs $\\gamma$",
                            xlabel: str = "$\\gamma$",
                            ylabel: str = "Ratio $(p_{x,p}-p_{x,n}>0)/(p_{x,p}-p_{x,n}<0)$"
                            ) -> plt.Figure:
        """
        绑定 ratio 随 gamma 变化的图
        
        Args:
            ratio_data: {gamma_str: {'ratio': value, 'error': error}} 字典
            title: 图标题
            
        Returns:
            matplotlib Figure
        """
        fig, ax = plt.subplots(figsize=(8, 6))
        
        gammas = sorted(ratio_data.keys(), key=lambda x: float(x) if x.replace('.', '').isdigit() else float(x[:3])/100)
        x_values = [float(g) / 100 if len(g) == 3 else float(g) for g in gammas]
        y_values = [ratio_data[g]['ratio'] for g in gammas]
        y_errors = [ratio_data[g].get('error', 0) for g in gammas]
        
        ax.errorbar(x_values, y_values, yerr=y_errors, 
                   fmt='o-', capsize=3, capthick=1, markersize=6)
        
        ax.set_xlabel(xlabel, fontsize=12)
        ax.set_ylabel(ylabel, fontsize=12)
        ax.set_title(title, fontsize=14)
        
        ax.axhline(y=1, color='gray', linestyle='--', alpha=0.5)
        
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        plt.tight_layout()
        return fig
    
    def plot_ratio_comparison(self,
                               ratio_dict: Dict[str, Dict[str, Dict[str, float]]],
                               title: str = "Ratio Comparison") -> plt.Figure:
        """
        绑定多个配置的 ratio 对比图
        
        Args:
            ratio_dict: {config_name: {gamma: {'ratio': value, 'error': error}}} 字典
            title: 图标题
            
        Returns:
            matplotlib Figure
        """
        fig, ax = plt.subplots(figsize=(10, 6))
        
        colors = PlotStyle.get_color_palette(len(ratio_dict))
        markers = ['o', 's', '^', 'v', 'D', 'p', '*', 'h']
        
        for i, (config_name, ratio_data) in enumerate(ratio_dict.items()):
            gammas = sorted(ratio_data.keys(), 
                          key=lambda x: float(x) if x.replace('.', '').isdigit() else float(x[:3])/100)
            x_values = [float(g) / 100 if len(g) == 3 else float(g) for g in gammas]
            y_values = [ratio_data[g]['ratio'] for g in gammas]
            y_errors = [ratio_data[g].get('error', 0) for g in gammas]
            
            ax.errorbar(x_values, y_values, yerr=y_errors,
                       fmt=f'{markers[i % len(markers)]}-',
                       capsize=3, capthick=1, markersize=6,
                       color=colors[i], label=config_name)
        
        ax.set_xlabel('$\\gamma$', fontsize=12)
        ax.set_ylabel('Ratio', fontsize=12)
        ax.set_title(title, fontsize=14)
        ax.legend(frameon=False, loc='best')
        ax.axhline(y=1, color='gray', linestyle='--', alpha=0.5)
        
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        plt.tight_layout()
        return fig
    
    def plot_ratio_comparison_with_stages(self,
                                           ratio_dict_before_geo: Dict[str, Dict[str, Dict[str, float]]],
                                           ratio_dict_after_geo: Dict[str, Dict[str, Dict[str, float]]],
                                           title: str = "Ratio Comparison") -> plt.Figure:
        """
        绘制多个配置的 ratio 对比图（显示 before/after geometry filter 两个阶段）
        
        Args:
            ratio_dict_before_geo: before geometry filter 数据
            ratio_dict_after_geo: after geometry filter 数据
            title: 图标题
            
        Returns:
            matplotlib Figure
        """
        fig, ax = plt.subplots(figsize=(12, 7))
        
        # 获取所有配置名称
        all_configs = sorted(set(list(ratio_dict_before_geo.keys()) + list(ratio_dict_after_geo.keys())))
        colors = PlotStyle.get_color_palette(len(all_configs))
        markers = ['o', 's', '^', 'v', 'D', 'p', '*', 'h']
        
        for i, config_name in enumerate(all_configs):
            color = colors[i]
            marker = markers[i % len(markers)]
            
            # Before geometry filter (实线)
            if config_name in ratio_dict_before_geo:
                ratio_data = ratio_dict_before_geo[config_name]
                gammas = sorted(ratio_data.keys(), 
                              key=lambda x: float(x) if x.replace('.', '').isdigit() else float(x[:3])/100)
                x_values = [float(g) / 100 if len(g) == 3 else float(g) for g in gammas]
                y_values = [ratio_data[g]['ratio'] for g in gammas]
                y_errors = [ratio_data[g].get('error', 0) for g in gammas]
                
                ax.errorbar(x_values, y_values, yerr=y_errors,
                           fmt=f'{marker}-',
                           capsize=3, capthick=1, markersize=6,
                           color=color, label=f"{config_name} (before geo)",
                           alpha=0.8)
            
            # After geometry filter (虚线)
            if config_name in ratio_dict_after_geo:
                ratio_data = ratio_dict_after_geo[config_name]
                gammas = sorted(ratio_data.keys(), 
                              key=lambda x: float(x) if x.replace('.', '').isdigit() else float(x[:3])/100)
                x_values = [float(g) / 100 if len(g) == 3 else float(g) for g in gammas]
                y_values = [ratio_data[g]['ratio'] for g in gammas]
                y_errors = [ratio_data[g].get('error', 0) for g in gammas]
                
                ax.errorbar(x_values, y_values, yerr=y_errors,
                           fmt=f'{marker}--',
                           capsize=3, capthick=1, markersize=6,
                           color=color, label=f"{config_name} (after geo)",
                           alpha=0.8)
        
        ax.set_xlabel('$\\gamma$', fontsize=12)
        ax.set_ylabel('Ratio', fontsize=12)
        ax.set_title(title, fontsize=14)
        ax.legend(frameon=False, loc='best', fontsize=9)
        ax.axhline(y=1, color='gray', linestyle='--', alpha=0.5)
        
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        
        plt.tight_layout()
        return fig
    
    def save_ratio_plot(self,
                        ratio_dict: Dict[str, Dict[str, Dict[str, float]]],
                        filename: str,
                        title: str = "Ratio Comparison",
                        output_subdir: str = ""):
        """保存 ratio 对比图"""
        output_path = self.ensure_output_dir(output_subdir)
        
        # PDF
        fig = self.plot_ratio_comparison(ratio_dict, title)
        fig.savefig(output_path / f"{filename}.pdf")
        plt.close(fig)
        
        # HTML
        if HAS_PLOTLY:
            fig_plotly = go.Figure()
            
            for config_name, ratio_data in ratio_dict.items():
                gammas = sorted(ratio_data.keys(),
                              key=lambda x: float(x) if x.replace('.', '').isdigit() else float(x[:3])/100)
                x_values = [float(g) / 100 if len(g) == 3 else float(g) for g in gammas]
                y_values = [ratio_data[g]['ratio'] for g in gammas]
                y_errors = [ratio_data[g].get('error', 0) for g in gammas]
                
                fig_plotly.add_trace(go.Scatter(
                    x=x_values, y=y_values,
                    error_y=dict(type='data', array=y_errors, visible=True),
                    mode='lines+markers',
                    name=config_name
                ))
            
            fig_plotly.update_layout(
                title=title,
                xaxis_title='γ',
                yaxis_title='Ratio'
            )
            fig_plotly.add_hline(y=1, line_dash="dash", line_color="gray")
            fig_plotly.write_html(str(output_path / f"{filename}.html"))
    
    def save_ratio_plot_with_stages(self,
                                     ratio_dict_before_geo: Dict[str, Dict[str, Dict[str, float]]],
                                     ratio_dict_after_geo: Dict[str, Dict[str, Dict[str, float]]],
                                     filename: str,
                                     title: str = "Ratio Comparison",
                                     output_subdir: str = ""):
        """保存 ratio 对比图（显示 before/after geometry filter 两个阶段）"""
        output_path = self.ensure_output_dir(output_subdir)
        
        # PDF
        fig = self.plot_ratio_comparison_with_stages(ratio_dict_before_geo, ratio_dict_after_geo, title)
        fig.savefig(output_path / f"{filename}.pdf")
        plt.close(fig)
        
        # HTML
        if HAS_PLOTLY:
            fig_plotly = go.Figure()
            
            # Before geometry filter (实线)
            for config_name, ratio_data in ratio_dict_before_geo.items():
                gammas = sorted(ratio_data.keys(),
                              key=lambda x: float(x) if x.replace('.', '').isdigit() else float(x[:3])/100)
                x_values = [float(g) / 100 if len(g) == 3 else float(g) for g in gammas]
                y_values = [ratio_data[g]['ratio'] for g in gammas]
                y_errors = [ratio_data[g].get('error', 0) for g in gammas]
                
                fig_plotly.add_trace(go.Scatter(
                    x=x_values, y=y_values,
                    error_y=dict(type='data', array=y_errors, visible=True),
                    mode='lines+markers',
                    name=f"{config_name} (before geo)",
                    line=dict(dash='solid')
                ))
            
            # After geometry filter (虚线)
            for config_name, ratio_data in ratio_dict_after_geo.items():
                gammas = sorted(ratio_data.keys(),
                              key=lambda x: float(x) if x.replace('.', '').isdigit() else float(x[:3])/100)
                x_values = [float(g) / 100 if len(g) == 3 else float(g) for g in gammas]
                y_values = [ratio_data[g]['ratio'] for g in gammas]
                y_errors = [ratio_data[g].get('error', 0) for g in gammas]
                
                fig_plotly.add_trace(go.Scatter(
                    x=x_values, y=y_values,
                    error_y=dict(type='data', array=y_errors, visible=True),
                    mode='lines+markers',
                    name=f"{config_name} (after geo)",
                    line=dict(dash='dash')
                ))
            
            fig_plotly.update_layout(
                title=title,
                xaxis_title='γ',
                yaxis_title='Ratio'
            )
            fig_plotly.add_hline(y=1, line_dash="dash", line_color="gray")
            fig_plotly.write_html(str(output_path / f"{filename}.html"))
    
    # ========== PDC/NEBULA 位置图 ==========
    
    def plot_detector_geometry(self,
                                target_pos: Tuple[float, float, float],
                                pdc_pos: Tuple[float, float, float],
                                pdc_rotation: float,
                                nebula_pos: Tuple[float, float, float],
                                field_strength: float,
                                deflection_angle: float,
                                title: str = "Detector Geometry Layout") -> plt.Figure:
        """
        绘制探测器几何布局图（XZ 平面俯视图）
        
        包含: Target, PDC, NEBULA, 磁铁, 束流线
        
        Args:
            target_pos: target 位置 (x, y, z) [mm]
            pdc_pos: PDC 中心位置 (x, y, z) [mm]
            pdc_rotation: PDC 旋转角度 [度]
            nebula_pos: NEBULA 位置 (x, y, z) [mm]
            field_strength: 磁场强度 [T]
            deflection_angle: 偏转角度 [度]
            title: 图标题
            
        Returns:
            matplotlib Figure
        """
        fig, ax = plt.subplots(figsize=(14, 10))
        
        # 坐标转换为米
        scale = 1000.0  # mm -> m
        
        # 绘制坐标轴
        ax.axhline(y=0, color='gray', linestyle='-', linewidth=0.5, alpha=0.5)
        ax.axvline(x=0, color='gray', linestyle='-', linewidth=0.5, alpha=0.5)
        
        # 绘制束流入射线 (从 z=-4m 到 target)
        ax.plot([-4, target_pos[2]/scale], [0, target_pos[0]/scale], 
               'g--', linewidth=2, alpha=0.7, label='Incoming Beam')
        
        # 绘制 Target
        ax.plot(target_pos[2]/scale, target_pos[0]/scale, 'ro', 
               markersize=12, label='Target', zorder=5)
        
        # 绘制磁铁区域（简化为矩形）
        magnet_angle = np.radians(30.0)  # 磁铁旋转角度
        magnet_half_z = 1.0  # m
        magnet_half_x = 1.5  # m
        
        cos_m, sin_m = np.cos(magnet_angle), np.sin(magnet_angle)
        magnet_corners_local = np.array([
            [-magnet_half_z, -magnet_half_x],
            [magnet_half_z, -magnet_half_x],
            [magnet_half_z, magnet_half_x],
            [-magnet_half_z, magnet_half_x]
        ])
        
        # 旋转矩阵
        R_magnet = np.array([[cos_m, -sin_m], [sin_m, cos_m]])
        magnet_corners = magnet_corners_local @ R_magnet.T
        
        from matplotlib.patches import Polygon
        magnet_patch = Polygon(magnet_corners, closed=True, fill=True,
                              facecolor='cyan', alpha=0.3, edgecolor='blue',
                              linewidth=2, label='SAMURAI Magnet')
        ax.add_patch(magnet_patch)
        
        # 绘制 PDC
        pdc_width = 1680.0 / scale  # m
        pdc_height = 780.0 / scale   # m
        
        theta_pdc = np.radians(pdc_rotation)
        cos_p, sin_p = np.cos(theta_pdc), np.sin(theta_pdc)
        
        # PDC 矩形角点（在 XZ 平面）
        half_w = pdc_width / 2
        pdc_corners_local = np.array([
            [-half_w, -0.05],
            [half_w, -0.05],
            [half_w, 0.05],
            [-half_w, 0.05]
        ])
        
        R_pdc = np.array([[cos_p, -sin_p], [sin_p, cos_p]])
        pdc_corners = pdc_corners_local @ R_pdc.T
        pdc_corners += np.array([pdc_pos[2]/scale, pdc_pos[0]/scale])
        
        pdc_patch = Polygon(pdc_corners, closed=True, fill=True,
                           facecolor='green', alpha=0.5, edgecolor='darkgreen',
                           linewidth=2, label='PDC')
        ax.add_patch(pdc_patch)
        
        # 绘制 NEBULA
        nebula_width = 3600.0 / scale  # m
        nebula_height = 1800.0 / scale  # m
        half_nw = nebula_width / 2
        half_nh = nebula_height / 2
        
        nebula_corners = np.array([
            [nebula_pos[2]/scale - 0.3, nebula_pos[0]/scale - half_nw],
            [nebula_pos[2]/scale + 0.3, nebula_pos[0]/scale - half_nw],
            [nebula_pos[2]/scale + 0.3, nebula_pos[0]/scale + half_nw],
            [nebula_pos[2]/scale - 0.3, nebula_pos[0]/scale + half_nw]
        ])
        
        nebula_patch = Polygon(nebula_corners, closed=True, fill=True,
                              facecolor='orange', alpha=0.5, edgecolor='darkorange',
                              linewidth=2, label='NEBULA')
        ax.add_patch(nebula_patch)
        
        # 添加文本标签
        ax.text(target_pos[2]/scale, target_pos[0]/scale + 0.3, 'Target',
               ha='center', va='bottom', fontsize=10, fontweight='bold')
        ax.text(pdc_pos[2]/scale, pdc_pos[0]/scale - 1.2, 'PDC',
               ha='center', va='top', fontsize=10, fontweight='bold', color='darkgreen')
        ax.text(nebula_pos[2]/scale, nebula_pos[0]/scale + half_nw + 0.3, 'NEBULA',
               ha='center', va='bottom', fontsize=10, fontweight='bold', color='darkorange')
        ax.text(0, -2.2, f'SAMURAI Magnet\n{field_strength} T',
               ha='center', va='top', fontsize=9, color='blue')
        
        # 设置坐标轴
        ax.set_xlabel('Z (m) - Beam Direction', fontsize=12, fontweight='bold')
        ax.set_ylabel('X (m)', fontsize=12, fontweight='bold')
        ax.set_title(f'{title}\nDeflection Angle: {deflection_angle}°', 
                    fontsize=14, fontweight='bold')
        ax.set_aspect('equal')
        ax.legend(frameon=True, loc='upper left', fontsize=10)
        ax.grid(True, alpha=0.3, linestyle='--')
        
        # 设置合适的显示范围
        ax.set_xlim(-5, 14)
        ax.set_ylim(-4, 3)
        
        plt.tight_layout()
        return fig
    
    def save_detector_geometry(self,
                                target_pos: Tuple[float, float, float],
                                pdc_pos: Tuple[float, float, float],
                                pdc_rotation: float,
                                nebula_pos: Tuple[float, float, float],
                                field_strength: float,
                                deflection_angle: float,
                                filename: str = "detector_geometry",
                                output_subdir: str = ""):
        """保存探测器几何布局图"""
        output_path = self.ensure_output_dir(output_subdir)
        
        fig = self.plot_detector_geometry(
            target_pos, pdc_pos, pdc_rotation, nebula_pos,
            field_strength, deflection_angle
        )
        fig.savefig(output_path / f"{filename}.pdf")
        plt.close(fig)
    
    # ========== 跨 gamma 值的对比图 ==========
    
    def plot_multi_gamma_comparison(self,
                                     gamma_data: Dict[str, Dict[str, Tuple[np.ndarray, np.ndarray]]],
                                     stage_name: str = "after_cut",
                                     pol_type: str = "zpol",
                                     target: str = "Pb208") -> plt.Figure:
        """
        绘制多个 gamma 值的 pxp-pxn 分布对比图
        
        Args:
            gamma_data: {gamma: {"stage": (pxp, pxn)}} 字典
            stage_name: 阶段名称
            pol_type: 极化类型
            target: 靶材料
            
        Returns:
            matplotlib Figure
        """
        stage_titles = {
            "before_cut": "Before Momentum Cut",
            "after_cut": "After Momentum Cut",
            "after_geometry": "After Geometry Filter"
        }
        
        fig, ax = plt.subplots(figsize=(10, 7))
        fig.patch.set_facecolor('white')
        ax.set_facecolor('white')
        
        colors = PlotStyle.get_color_palette(len(gamma_data))
        
        for i, (gamma, stage_data) in enumerate(sorted(gamma_data.items())):
            if stage_name not in stage_data:
                continue
            pxp, pxn = stage_data[stage_name]
            if len(pxp) == 0:
                continue
            
            diff = pxp - pxn
            gamma_display = f"{float(gamma)/100:.1f}" if len(gamma) == 3 else str(gamma)
            
            ax.hist(diff, bins=40, histtype='step', linewidth=2,
                   label=f'$\\gamma$ = {gamma_display}', color=colors[i], alpha=0.8)
        
        ax.set_xlabel('$p_{x,p} - p_{x,n}$ (MeV/c)', fontsize=13, fontweight='bold')
        ax.set_ylabel('Counts', fontsize=13, fontweight='bold')
        title = f'{target} {pol_type.upper()}: {stage_titles.get(stage_name, stage_name)}'
        ax.set_title(title, fontsize=15, fontweight='bold')
        ax.legend(frameon=False, loc='upper right', fontsize=11)
        
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.axvline(x=0, color='red', linestyle='--', alpha=0.7, linewidth=1.5)
        ax.tick_params(axis='both', which='major', direction='out', labelsize=11)
        ax.grid(True, alpha=0.2, linestyle='--')
        
        plt.tight_layout()
        return fig
    
    def save_multi_gamma_comparisons(self,
                                      gamma_data: Dict[str, Dict[str, Tuple[np.ndarray, np.ndarray]]],
                                      pol_type: str,
                                      target: str,
                                      output_subdir: str = ""):
        """
        保存三个阶段的跨 gamma 对比图
        
        Args:
            gamma_data: {gamma: {"before_cut": (pxp,pxn), "after_cut": ..., "after_geometry": ...}}
            pol_type: 极化类型
            target: 靶材料
            output_subdir: 输出子目录
        """
        output_path = self.ensure_output_dir(output_subdir)
        
        stages = ["before_cut", "after_cut", "after_geometry"]
        
        for stage in stages:
            fig = self.plot_multi_gamma_comparison(gamma_data, stage, pol_type, target)
            fig.savefig(output_path / f"gamma_comparison_{stage}.pdf", dpi=300)
            plt.close(fig)
            
            # HTML 版本
            if HAS_PLOTLY:
                fig_plotly = go.Figure()
                for gamma, stage_data in sorted(gamma_data.items()):
                    if stage not in stage_data:
                        continue
                    pxp, pxn = stage_data[stage]
                    if len(pxp) == 0:
                        continue
                    diff = pxp - pxn
                    gamma_display = f"{float(gamma)/100:.1f}" if len(gamma) == 3 else str(gamma)
                    fig_plotly.add_trace(go.Histogram(
                        x=diff, name=f'γ = {gamma_display}',
                        opacity=0.7, nbinsx=40
                    ))
                
                stage_titles = {
                    "before_cut": "Before Momentum Cut",
                    "after_cut": "After Momentum Cut",
                    "after_geometry": "After Geometry Filter"
                }
                fig_plotly.update_layout(
                    title=f'{target} {pol_type.upper()}: {stage_titles.get(stage, stage)}',
                    xaxis_title='pxp - pxn (MeV/c)',
                    yaxis_title='Counts',
                    barmode='overlay',
                    font=dict(size=12)
                )
                fig_plotly.add_vline(x=0, line_dash="dash", line_color="red", opacity=0.7)
                fig_plotly.write_html(str(output_path / f"gamma_comparison_{stage}.html"))
