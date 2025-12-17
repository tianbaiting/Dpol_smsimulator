"""
动量 Cut 模块

实现基于 notebook 中的 cut 条件的动量筛选功能
支持 z 极化和 y 极化数据 - 它们使用不同的 cut 条件

zpol cut 条件:
    1. (pzp + pzn) > 1150
    2. (np.pi - abs(phi)) < 0.5
    3. abs(pzp - pzn) < 150
    4. (pxp + pxn) < 200
    5. (pxp+pxn)^2 + (pyp+pyn)^2 > 2500

ypol cut 条件 (分两步):
    第一步:
    1. abs(pyp - pyn) < 150
    2. (pxp+pxn)^2 + (pyp+pyn)^2 > 2500
    
    第二步 (旋转后):
    1. (pxp + pxn) < 200
    2. (np.pi - abs(phi)) < 0.2
"""

import numpy as np
from typing import Tuple, Optional, List, Dict, Any
from dataclasses import dataclass, field


@dataclass
class CutResult:
    """Cut 结果"""
    mask: np.ndarray  # 布尔掩码
    n_passed: int     # 通过 cut 的事件数
    n_total: int      # 总事件数
    efficiency: float # 效率
    
    # 旋转后的动量（如果适用）
    pxp_rotated: Optional[np.ndarray] = None
    pyp_rotated: Optional[np.ndarray] = None
    pzp_rotated: Optional[np.ndarray] = None
    pxn_rotated: Optional[np.ndarray] = None
    pyn_rotated: Optional[np.ndarray] = None
    pzn_rotated: Optional[np.ndarray] = None
    phi_rotation: Optional[np.ndarray] = None  # 用于旋转的角度


class MomentumCut:
    """
    动量 Cut 类
    
    基于 notebook 中的 cut 条件实现动量筛选
    zpol 和 ypol 使用不同的 cut 条件
    """
    
    # zpol 默认参数
    ZPOL_DEFAULTS = {
        'pz_sum_threshold': 1150.0,         # (pzp + pzn) > 1150
        'phi_threshold': 0.5,               # (np.pi - abs(phi)) < 0.5
        'delta_pz_threshold': 150.0,        # abs(pzp - pzn) < 150
        'px_sum_threshold': 200.0,          # (pxp + pxn) < 200
        'pt_sum_sq_threshold': 2500.0,      # (pxp+pxn)^2 + (pyp+pyn)^2 > 2500
    }
    
    # ypol 默认参数
    YPOL_DEFAULTS = {
        'delta_py_threshold': 150.0,        # abs(pyp - pyn) < 150
        'pt_sum_sq_threshold': 2500.0,      # (pxp+pxn)^2 + (pyp+pyn)^2 > 2500
        'px_sum_after_rotation': 200.0,     # 旋转后 (pxp + pxn) < 200
        'phi_threshold': 0.2,               # (np.pi - abs(phi)) < 0.2
    }
    
    def __init__(self,
                 pol_type: str = "ypol",
                 enable_momentum_smearing: bool = False,
                 momentum_resolution: float = 0.0,
                 **kwargs):
        """
        初始化 MomentumCut
        
        Args:
            pol_type: 极化类型 "zpol" 或 "ypol"
            enable_momentum_smearing: 是否启用动量模糊
            momentum_resolution: 动量分辨率 (相对值, 如 0.02 表示 2%)
            **kwargs: 覆盖默认的 cut 参数
        """
        self.pol_type = pol_type.lower()
        self.enable_momentum_smearing = enable_momentum_smearing
        self.momentum_resolution = momentum_resolution
        
        # 根据极化类型设置默认参数
        if self.pol_type == "zpol":
            self.params = self.ZPOL_DEFAULTS.copy()
        else:  # ypol
            self.params = self.YPOL_DEFAULTS.copy()
        
        # 更新用户提供的参数
        self.params.update(kwargs)
    
    def apply_momentum_smearing(self, p: np.ndarray, 
                                 resolution: Optional[float] = None) -> np.ndarray:
        """
        应用动量模糊（高斯展宽）
        
        Args:
            p: 动量数组
            resolution: 相对分辨率，None 使用默认值
            
        Returns:
            模糊后的动量数组
        """
        if resolution is None:
            resolution = self.momentum_resolution
        
        if resolution <= 0:
            return p.copy()
        
        # 高斯模糊: sigma = resolution * |p|
        sigma = resolution * np.abs(p)
        smeared = p + np.random.normal(0, 1, size=p.shape) * sigma
        return smeared
    
    def smear_all_momenta(self, pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                          pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray,
                          resolution: Optional[float] = None) -> Tuple[np.ndarray, ...]:
        """
        对所有动量分量应用模糊
        
        Returns:
            (pxp_s, pyp_s, pzp_s, pxn_s, pyn_s, pzn_s)
        """
        return (
            self.apply_momentum_smearing(pxp, resolution),
            self.apply_momentum_smearing(pyp, resolution),
            self.apply_momentum_smearing(pzp, resolution),
            self.apply_momentum_smearing(pxn, resolution),
            self.apply_momentum_smearing(pyn, resolution),
            self.apply_momentum_smearing(pzn, resolution)
        )
    
    def apply_cut(self, pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                  pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray,
                  apply_smearing: Optional[bool] = None,
                  resolution: Optional[float] = None) -> CutResult:
        """
        应用动量 cut
        
        根据 pol_type 自动选择正确的 cut 方法
        
        Args:
            pxp, pyp, pzp: 质子动量分量数组
            pxn, pyn, pzn: 中子动量分量数组
            apply_smearing: 是否应用模糊，None 使用默认设置
            resolution: 动量分辨率，None 使用默认值
            
        Returns:
            CutResult 对象
        """
        # 应用动量模糊
        if apply_smearing is None:
            apply_smearing = self.enable_momentum_smearing
        
        if apply_smearing:
            pxp, pyp, pzp, pxn, pyn, pzn = self.smear_all_momenta(
                pxp, pyp, pzp, pxn, pyn, pzn, resolution
            )
        
        if self.pol_type == "zpol":
            return self._apply_zpol_cut(pxp, pyp, pzp, pxn, pyn, pzn)
        else:
            return self._apply_ypol_cut(pxp, pyp, pzp, pxn, pyn, pzn)
    
    def _apply_zpol_cut(self, pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                        pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray) -> CutResult:
        """
        应用 zpol 的 cut 条件
        
        zpol cut 条件 (所有条件同时满足，无旋转):
        1. (pzp + pzn) > pz_sum_threshold (1150)
        2. (np.pi - abs(phi)) < phi_threshold (0.5)
        3. abs(pzp - pzn) < delta_pz_threshold (150)
        4. (pxp + pxn) < px_sum_threshold (200)
        5. (pxp+pxn)^2 + (pyp+pyn)^2 > pt_sum_sq_threshold (2500)
        
        然后对通过 cut 的数据进行旋转
        """
        n_total = len(pxp)
        
        # 计算 phi
        vec_sum_x = pxp + pxn
        vec_sum_y = pyp + pyn
        phi_for_rotation = np.arctan2(vec_sum_y, vec_sum_x)
        
        # 所有 cut 条件
        cond1 = (pzp + pzn) > self.params['pz_sum_threshold']
        cond2 = (np.pi - np.abs(phi_for_rotation)) < self.params['phi_threshold']
        cond3 = np.abs(pzp - pzn) < self.params['delta_pz_threshold']
        cond4 = (pxp + pxn) < self.params['px_sum_threshold']
        cond5 = (vec_sum_x**2 + vec_sum_y**2) > self.params['pt_sum_sq_threshold']
        
        # 最终掩码
        final_mask = cond1 & cond2 & cond3 & cond4 & cond5
        
        # 初始化旋转后的动量数组
        pxp_rot = np.full(n_total, np.nan)
        pyp_rot = np.full(n_total, np.nan)
        pzp_rot = np.full(n_total, np.nan)
        pxn_rot = np.full(n_total, np.nan)
        pyn_rot = np.full(n_total, np.nan)
        pzn_rot = np.full(n_total, np.nan)
        phi_rotation = np.full(n_total, np.nan)
        
        # 对通过 cut 的事件进行旋转
        if np.any(final_mask):
            phi_rot = phi_for_rotation[final_mask]
            phi_rotation[final_mask] = phi_rot
            
            # 绕 Z 轴旋转 -phi
            cos_a = np.cos(-phi_rot)
            sin_a = np.sin(-phi_rot)
            
            # 旋转质子动量
            pxp_rot[final_mask] = cos_a * pxp[final_mask] - sin_a * pyp[final_mask]
            pyp_rot[final_mask] = sin_a * pxp[final_mask] + cos_a * pyp[final_mask]
            pzp_rot[final_mask] = pzp[final_mask]
            
            # 旋转中子动量
            pxn_rot[final_mask] = cos_a * pxn[final_mask] - sin_a * pyn[final_mask]
            pyn_rot[final_mask] = sin_a * pxn[final_mask] + cos_a * pyn[final_mask]
            pzn_rot[final_mask] = pzn[final_mask]
        
        n_passed = np.sum(final_mask)
        
        return CutResult(
            mask=final_mask,
            n_passed=n_passed,
            n_total=n_total,
            efficiency=n_passed / n_total if n_total > 0 else 0.0,
            pxp_rotated=pxp_rot,
            pyp_rotated=pyp_rot,
            pzp_rotated=pzp_rot,
            pxn_rotated=pxn_rot,
            pyn_rotated=pyn_rot,
            pzn_rotated=pzn_rot,
            phi_rotation=phi_rotation
        )
    
    def _apply_ypol_cut(self, pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                        pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray) -> CutResult:
        """
        应用 ypol 的 cut 条件
        
        ypol cut 条件 (分两步):
        第一步:
        1. abs(pyp - pyn) < delta_py_threshold (150)
        2. (pxp+pxn)^2 + (pyp+pyn)^2 > pt_sum_sq_threshold (2500)
        
        第二步 (旋转后):
        1. (pxp + pxn) < px_sum_after_rotation (200)
        2. (np.pi - abs(phi)) < phi_threshold (0.2)
        """
        n_total = len(pxp)
        
        # 第一步 cut
        vec_sum_x = pxp + pxn
        vec_sum_y = pyp + pyn
        
        cond1_1 = np.abs(pyp - pyn) < self.params['delta_py_threshold']
        cond1_2 = (vec_sum_x**2 + vec_sum_y**2) > self.params['pt_sum_sq_threshold']
        
        mask1 = cond1_1 & cond1_2
        
        # 初始化旋转后的动量数组
        pxp_rot = np.full(n_total, np.nan)
        pyp_rot = np.full(n_total, np.nan)
        pzp_rot = np.full(n_total, np.nan)
        pxn_rot = np.full(n_total, np.nan)
        pyn_rot = np.full(n_total, np.nan)
        pzn_rot = np.full(n_total, np.nan)
        phi_rotation = np.full(n_total, np.nan)
        
        # 对通过第一步 cut 的事件进行旋转
        if np.any(mask1):
            # 计算旋转角度
            phi_for_rotation = np.arctan2(vec_sum_y[mask1], vec_sum_x[mask1])
            phi_rotation[mask1] = phi_for_rotation
            
            # 绕 Z 轴旋转 -phi
            cos_a = np.cos(-phi_for_rotation)
            sin_a = np.sin(-phi_for_rotation)
            
            # 旋转质子动量
            pxp_rot[mask1] = cos_a * pxp[mask1] - sin_a * pyp[mask1]
            pyp_rot[mask1] = sin_a * pxp[mask1] + cos_a * pyp[mask1]
            pzp_rot[mask1] = pzp[mask1]
            
            # 旋转中子动量
            pxn_rot[mask1] = cos_a * pxn[mask1] - sin_a * pyn[mask1]
            pyn_rot[mask1] = sin_a * pxn[mask1] + cos_a * pyn[mask1]
            pzn_rot[mask1] = pzn[mask1]
        
        # 第二步 cut (旋转后)
        cond2_1 = (pxp_rot + pxn_rot) < self.params['px_sum_after_rotation']
        cond2_2 = (np.pi - np.abs(phi_rotation)) < self.params['phi_threshold']
        
        mask2 = cond2_1 & cond2_2
        
        # 最终掩码
        final_mask = mask1 & mask2
        n_passed = np.sum(final_mask)
        
        return CutResult(
            mask=final_mask,
            n_passed=n_passed,
            n_total=n_total,
            efficiency=n_passed / n_total if n_total > 0 else 0.0,
            pxp_rotated=pxp_rot,
            pyp_rotated=pyp_rot,
            pzp_rotated=pzp_rot,
            pxn_rotated=pxn_rot,
            pyn_rotated=pyn_rot,
            pzn_rotated=pzn_rot,
            phi_rotation=phi_rotation
        )
    
    def apply_simple_cut(self, pxp: np.ndarray, pyp: np.ndarray, pzp: np.ndarray,
                         pxn: np.ndarray, pyn: np.ndarray, pzn: np.ndarray,
                         px_range: Tuple[float, float] = (-300, 300),
                         py_range: Tuple[float, float] = (-300, 300),
                         pz_range: Tuple[float, float] = (200, 800)) -> np.ndarray:
        """
        应用简单的动量范围 cut（不带旋转）
        
        Args:
            pxp, pyp, pzp: 质子动量分量
            pxn, pyn, pzn: 中子动量分量
            px_range, py_range, pz_range: 动量范围
            
        Returns:
            布尔掩码
        """
        mask = (
            (pxp >= px_range[0]) & (pxp <= px_range[1]) &
            (pyp >= py_range[0]) & (pyp <= py_range[1]) &
            (pzp >= pz_range[0]) & (pzp <= pz_range[1]) &
            (pxn >= px_range[0]) & (pxn <= px_range[1]) &
            (pyn >= py_range[0]) & (pyn <= py_range[1]) &
            (pzn >= pz_range[0]) & (pzn <= pz_range[1])
        )
        return mask
    
    def get_config_dict(self) -> Dict[str, Any]:
        """获取配置字典"""
        return {
            "pol_type": self.pol_type,
            "enable_momentum_smearing": self.enable_momentum_smearing,
            "momentum_resolution": self.momentum_resolution,
            **self.params
        }
    
    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> 'MomentumCut':
        """从配置字典创建实例"""
        config = config.copy()
        pol_type = config.pop('pol_type', 'ypol')
        enable_smearing = config.pop('enable_momentum_smearing', False)
        resolution = config.pop('momentum_resolution', 0.0)
        return cls(
            pol_type=pol_type,
            enable_momentum_smearing=enable_smearing,
            momentum_resolution=resolution,
            **config
        )
    
    def __repr__(self) -> str:
        return (f"MomentumCut(pol_type={self.pol_type}, "
                f"smear={self.enable_momentum_smearing}, "
                f"resolution={self.momentum_resolution}, "
                f"params={self.params})")


# 便捷函数
def create_zpol_cut(enable_smearing: bool = False, 
                    resolution: float = 0.0, 
                    **kwargs) -> MomentumCut:
    """创建 zpol 的 MomentumCut 实例"""
    return MomentumCut(pol_type="zpol", 
                       enable_momentum_smearing=enable_smearing,
                       momentum_resolution=resolution,
                       **kwargs)


def create_ypol_cut(enable_smearing: bool = False,
                    resolution: float = 0.0,
                    **kwargs) -> MomentumCut:
    """创建 ypol 的 MomentumCut 实例"""
    return MomentumCut(pol_type="ypol",
                       enable_momentum_smearing=enable_smearing,
                       momentum_resolution=resolution,
                       **kwargs)
