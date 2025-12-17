"""
几何接受度筛选模块

调用 C++ geo_acceptance 库进行几何接受度计算
或提供简化的 Python 实现
"""

import os
import subprocess
import numpy as np
from pathlib import Path
from typing import Optional, Tuple, Dict, List, Any
from dataclasses import dataclass, field
import json


@dataclass
class PDCConfiguration:
    """PDC 配置"""
    position: Tuple[float, float, float]  # (x, y, z) [mm]
    rotation_angle: float  # 绕Y轴旋转角度 [度]
    width: float = 1680.0   # 2 * 840 mm
    height: float = 780.0   # 2 * 390 mm
    px_min: float = -100.0  # 最小 Px [MeV/c]
    px_max: float = 100.0   # 最大 Px [MeV/c]


@dataclass
class NEBULAConfiguration:
    """NEBULA 配置"""
    position: Tuple[float, float, float]  # (x, y, z) [mm]
    width: float = 3600.0   # X 方向
    height: float = 1800.0  # Y 方向
    depth: float = 600.0    # Z 方向


@dataclass
class TargetPosition:
    """Target 位置配置"""
    deflection_angle: float  # 偏转角度 [度]
    position: Tuple[float, float, float]  # (x, y, z) [mm]
    beam_direction: Tuple[float, float, float]  # 束流方向单位矢量
    rotation_angle: float  # Target 旋转角度 [度]
    field_strength: float  # 磁场强度 [T]


@dataclass 
class GeometryAcceptanceResult:
    """几何接受度结果"""
    mask: np.ndarray  # 布尔掩码 - 通过几何接受度的事件
    n_passed: int
    n_total: int
    efficiency: float
    pdc_config: Optional[PDCConfiguration] = None
    nebula_config: Optional[NEBULAConfiguration] = None
    target_config: Optional[TargetPosition] = None


class GeometryFilter:
    """
    几何接受度筛选器
    
    根据磁场配置和 target 位置，筛选能同时被 PDC 和 NEBULA 接受的事件
    """
    
    # 预定义的磁场和 target 配置
    # 格式: (field_strength, deflection_angle) -> TargetPosition
    PREDEFINED_CONFIGS: Dict[Tuple[float, float], TargetPosition] = {}
    
    def __init__(self, 
                 field_strength: float = 1.0,
                 deflection_angle: float = 5.0,
                 use_cpp_backend: bool = False):
        """
        初始化几何筛选器
        
        Args:
            field_strength: 磁场强度 [T]
            deflection_angle: 束流偏转角度 [度]
            use_cpp_backend: 是否使用 C++ 后端
        """
        self.field_strength = field_strength
        self.deflection_angle = deflection_angle
        self.use_cpp_backend = use_cpp_backend
        
        # 探测器配置
        self.pdc_config: Optional[PDCConfiguration] = None
        self.nebula_config = NEBULAConfiguration(
            position=(0, 0, 12000)  # 默认位置
        )
        self.target_config: Optional[TargetPosition] = None
        
        # 初始化配置
        self._initialize_config()
    
    def _initialize_config(self):
        """根据磁场和偏转角度初始化配置"""
        # 简化的 target 位置计算
        # 实际应用中应调用 C++ 后端或读取预计算结果
        
        # 近似计算 target 位置
        # 偏转半径 R = p / (0.3 * B) [mm], p in MeV/c, B in T
        # 对于 190 MeV/u 氘核, p ≈ 1330 MeV/c
        p_beam = 1330.0  # MeV/c
        R = p_beam / (0.3 * self.field_strength) * 1000  # mm
        
        # target z 位置（简化估计）
        theta_rad = np.radians(self.deflection_angle)
        z_target = -4000 + R * np.sin(theta_rad)  # 从入射点 z=-4000 开始
        x_target = R * (1 - np.cos(theta_rad))
        
        self.target_config = TargetPosition(
            deflection_angle=self.deflection_angle,
            position=(x_target, 0, z_target),
            beam_direction=(np.sin(theta_rad), 0, np.cos(theta_rad)),
            rotation_angle=self.deflection_angle,
            field_strength=self.field_strength
        )
        
        # PDC 位置（简化估计）
        # PDC 应该在质子（pz ≈ 600 MeV/c）的轨迹上
        # 简化：假设在 z ≈ 4000-6000 mm 处
        z_pdc = 5000.0
        
        self.pdc_config = PDCConfiguration(
            position=(0, 0, z_pdc),
            rotation_angle=self.deflection_angle
        )
    
    def load_config_from_file(self, config_file: str):
        """从文件加载配置"""
        with open(config_file, 'r') as f:
            config = json.load(f)
        
        if 'target' in config:
            tc = config['target']
            self.target_config = TargetPosition(
                deflection_angle=tc['deflection_angle'],
                position=tuple(tc['position']),
                beam_direction=tuple(tc['beam_direction']),
                rotation_angle=tc['rotation_angle'],
                field_strength=tc['field_strength']
            )
        
        if 'pdc' in config:
            pc = config['pdc']
            self.pdc_config = PDCConfiguration(
                position=tuple(pc['position']),
                rotation_angle=pc['rotation_angle'],
                width=pc.get('width', 1680.0),
                height=pc.get('height', 780.0),
                px_min=pc.get('px_min', -100.0),
                px_max=pc.get('px_max', 100.0)
            )
        
        if 'nebula' in config:
            nc = config['nebula']
            self.nebula_config = NEBULAConfiguration(
                position=tuple(nc['position']),
                width=nc.get('width', 3600.0),
                height=nc.get('height', 1800.0),
                depth=nc.get('depth', 600.0)
            )
    
    def save_config_to_file(self, config_file: str):
        """保存配置到文件"""
        config = {
            'field_strength': self.field_strength,
            'deflection_angle': self.deflection_angle,
            'target': {
                'deflection_angle': self.target_config.deflection_angle,
                'position': list(self.target_config.position),
                'beam_direction': list(self.target_config.beam_direction),
                'rotation_angle': self.target_config.rotation_angle,
                'field_strength': self.target_config.field_strength
            },
            'pdc': {
                'position': list(self.pdc_config.position),
                'rotation_angle': self.pdc_config.rotation_angle,
                'width': self.pdc_config.width,
                'height': self.pdc_config.height,
                'px_min': self.pdc_config.px_min,
                'px_max': self.pdc_config.px_max
            },
            'nebula': {
                'position': list(self.nebula_config.position),
                'width': self.nebula_config.width,
                'height': self.nebula_config.height,
                'depth': self.nebula_config.depth
            }
        }
        
        Path(config_file).parent.mkdir(parents=True, exist_ok=True)
        with open(config_file, 'w') as f:
            json.dump(config, f, indent=2)
    
    def check_pdc_acceptance(self, pxp: np.ndarray, pyp: np.ndarray, 
                             pzp: np.ndarray) -> np.ndarray:
        """
        检查质子是否被 PDC 接受
        
        简化的几何接受度检查，基于 Px 范围
        
        Args:
            pxp, pyp, pzp: 质子动量分量
            
        Returns:
            布尔掩码
        """
        if self.pdc_config is None:
            return np.ones(len(pxp), dtype=bool)
        
        # 简化：基于 Px 范围筛选
        mask = (pxp >= self.pdc_config.px_min) & (pxp <= self.pdc_config.px_max)
        
        # 可以添加更复杂的几何检查
        # 例如基于 pz 和角度估算打到 PDC 的位置
        
        return mask
    
    def check_nebula_acceptance(self, pxn: np.ndarray, pyn: np.ndarray,
                                pzn: np.ndarray) -> np.ndarray:
        """
        检查中子是否被 NEBULA 接受
        
        简化的几何接受度检查
        
        Args:
            pxn, pyn, pzn: 中子动量分量
            
        Returns:
            布尔掩码
        """
        if self.nebula_config is None:
            return np.ones(len(pxn), dtype=bool)
        
        # 简化：基于角度范围筛选
        # NEBULA 在 z ≈ 12m 处，宽度 3.6m，高度 1.8m
        # 接受角约 ±8.5° (水平), ±4.3° (垂直)
        
        p_total = np.sqrt(pxn**2 + pyn**2 + pzn**2)
        theta_x = np.arctan2(pxn, pzn)  # 水平角
        theta_y = np.arctan2(pyn, pzn)  # 垂直角
        
        # NEBULA 角度接受度
        z_nebula = self.nebula_config.position[2]
        theta_x_max = np.arctan2(self.nebula_config.width / 2, z_nebula)
        theta_y_max = np.arctan2(self.nebula_config.height / 2, z_nebula)
        
        mask = (np.abs(theta_x) < theta_x_max) & (np.abs(theta_y) < theta_y_max)
        
        return mask
    
    def apply_geometry_filter(self, pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                              pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray
                              ) -> GeometryAcceptanceResult:
        """
        应用几何接受度筛选
        
        筛选同时被 PDC 和 NEBULA 接受的事件
        
        Args:
            pxp, pyp, pzp: 质子动量分量
            pxn, pyn, pzn: 中子动量分量
            
        Returns:
            GeometryAcceptanceResult 对象
        """
        n_total = len(pxp)
        
        # 检查 PDC 接受度
        pdc_mask = self.check_pdc_acceptance(pxp, pyp, pzp)
        
        # 检查 NEBULA 接受度
        nebula_mask = self.check_nebula_acceptance(pxn, pyn, pzn)
        
        # 符合接受度：同时被两个探测器接受
        final_mask = pdc_mask & nebula_mask
        n_passed = np.sum(final_mask)
        
        return GeometryAcceptanceResult(
            mask=final_mask,
            n_passed=n_passed,
            n_total=n_total,
            efficiency=n_passed / n_total if n_total > 0 else 0.0,
            pdc_config=self.pdc_config,
            nebula_config=self.nebula_config,
            target_config=self.target_config
        )
    
    def get_config_summary(self) -> str:
        """获取配置摘要字符串"""
        lines = [
            f"Geometry Filter Configuration",
            f"=" * 40,
            f"Field Strength: {self.field_strength} T",
            f"Deflection Angle: {self.deflection_angle}°",
            f"",
            f"Target Position:",
            f"  Position: {self.target_config.position}",
            f"  Beam Direction: {self.target_config.beam_direction}",
            f"  Rotation Angle: {self.target_config.rotation_angle}°",
            f"",
            f"PDC Configuration:",
            f"  Position: {self.pdc_config.position}",
            f"  Rotation: {self.pdc_config.rotation_angle}°",
            f"  Px Range: [{self.pdc_config.px_min}, {self.pdc_config.px_max}] MeV/c",
            f"",
            f"NEBULA Configuration:",
            f"  Position: {self.nebula_config.position}",
            f"  Size: {self.nebula_config.width} x {self.nebula_config.height} mm",
        ]
        return "\n".join(lines)


class GeometryFilterFactory:
    """几何筛选器工厂类"""
    
    @staticmethod
    def get_available_configurations() -> List[Tuple[float, float]]:
        """
        获取所有可用的 (磁场强度, 偏转角度) 配置
        
        Returns:
            配置列表
        """
        # 常用配置
        field_strengths = [0.8, 1.0, 1.2, 1.4]  # Tesla
        deflection_angles = [0.0, 5.0, 10.0, 15.0]  # degrees
        
        configs = []
        for fs in field_strengths:
            for da in deflection_angles:
                configs.append((fs, da))
        
        return configs
    
    @staticmethod
    def create_filter(field_strength: float, deflection_angle: float,
                     config_file: Optional[str] = None) -> GeometryFilter:
        """
        创建几何筛选器
        
        Args:
            field_strength: 磁场强度 [T]
            deflection_angle: 偏转角度 [度]
            config_file: 可选的配置文件路径
            
        Returns:
            GeometryFilter 实例
        """
        gf = GeometryFilter(field_strength, deflection_angle)
        
        if config_file and os.path.exists(config_file):
            gf.load_config_from_file(config_file)
        
        return gf
