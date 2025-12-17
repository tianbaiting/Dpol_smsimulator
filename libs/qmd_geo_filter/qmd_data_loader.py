"""
QMD数据加载器模块

支持读取 z_pol 和 y_pol 两种格式的数据文件
"""

import os
import numpy as np
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple
from pathlib import Path
import glob


@dataclass
class QMDEvent:
    """单个QMD事件数据"""
    event_no: int
    pxp: float  # proton x momentum [MeV/c]
    pyp: float  # proton y momentum [MeV/c]
    pzp: float  # proton z momentum [MeV/c]
    pxn: float  # neutron x momentum [MeV/c]
    pyn: float  # neutron y momentum [MeV/c]
    pzn: float  # neutron z momentum [MeV/c]
    b: float = 0.0         # 碰撞参数 [fm], 仅 y_pol 数据
    rpphi: float = 0.0     # 反应平面角度 [deg], 仅 y_pol 数据
    
    @property
    def pp(self) -> np.ndarray:
        """质子动量向量"""
        return np.array([self.pxp, self.pyp, self.pzp])
    
    @property
    def pn(self) -> np.ndarray:
        """中子动量向量"""
        return np.array([self.pxn, self.pyn, self.pzn])
    
    @property
    def p_total(self) -> np.ndarray:
        """总动量向量"""
        return self.pp + self.pn


@dataclass
class QMDDataset:
    """一组QMD数据（包含多个事件）"""
    target: str              # 靶材料, e.g., "Pb208", "Sn124"
    energy: int              # 入射能量 [MeV/u]
    gamma: str               # gamma参数, e.g., "050", "060"
    pol_type: str            # 极化类型, "znp", "zpn", "ynp", "ypn"
    events: List[QMDEvent] = field(default_factory=list)
    b_value: Optional[float] = None  # 固定b值 (仅 z_pol discrete)
    
    @property
    def is_zpol(self) -> bool:
        return self.pol_type in ["znp", "zpn"]
    
    @property
    def is_ypol(self) -> bool:
        return self.pol_type in ["ynp", "ypn"]
    
    @property
    def pol_direction(self) -> str:
        """极化方向"""
        return "z" if self.is_zpol else "y"
    
    def get_pxp_array(self) -> np.ndarray:
        return np.array([e.pxp for e in self.events])
    
    def get_pyp_array(self) -> np.ndarray:
        return np.array([e.pyp for e in self.events])
    
    def get_pzp_array(self) -> np.ndarray:
        return np.array([e.pzp for e in self.events])
    
    def get_pxn_array(self) -> np.ndarray:
        return np.array([e.pxn for e in self.events])
    
    def get_pyn_array(self) -> np.ndarray:
        return np.array([e.pyn for e in self.events])
    
    def get_pzn_array(self) -> np.ndarray:
        return np.array([e.pzn for e in self.events])
    
    def get_b_array(self) -> np.ndarray:
        return np.array([e.b for e in self.events])
    
    def get_rpphi_array(self) -> np.ndarray:
        return np.array([e.rpphi for e in self.events])
    
    def get_momentum_arrays(self) -> Tuple[np.ndarray, np.ndarray, np.ndarray,
                                           np.ndarray, np.ndarray, np.ndarray]:
        """返回所有动量分量数组"""
        return (self.get_pxp_array(), self.get_pyp_array(), self.get_pzp_array(),
                self.get_pxn_array(), self.get_pyn_array(), self.get_pzn_array())


class QMDDataLoader:
    """QMD原始数据加载器"""
    
    def __init__(self, base_path: str = None):
        """
        初始化数据加载器
        
        Args:
            base_path: qmdrawdata 目录路径，默认使用环境变量 SMSIMDIR
        """
        if base_path is None:
            smsimdir = os.environ.get('SMSIMDIR', 
                                      '/home/tian/workspace/dpol/smsimulator5.5')
            base_path = os.path.join(smsimdir, 'data', 'qmdrawdata', 'qmdrawdata')
        self.base_path = Path(base_path)
        
    def list_available_datasets(self, pol_type: str = None) -> Dict[str, List[str]]:
        """
        列出所有可用的数据集
        
        Args:
            pol_type: "z", "y" 或 None (全部)
            
        Returns:
            按类型分组的数据集路径字典
        """
        result = {}
        
        if pol_type is None or pol_type == "z":
            # z_pol/b_discrete
            zpol_path = self.base_path / "z_pol" / "b_discrete"
            if zpol_path.exists():
                result["z_pol"] = sorted([d.name for d in zpol_path.iterdir() 
                                         if d.is_dir()])
        
        if pol_type is None or pol_type == "y":
            # y_pol/phi_random
            ypol_path = self.base_path / "y_pol" / "phi_random"
            if ypol_path.exists():
                result["y_pol"] = sorted([d.name for d in ypol_path.iterdir() 
                                         if d.is_dir()])
        
        return result
    
    def parse_folder_name(self, folder_name: str) -> Dict[str, str]:
        """
        解析文件夹名称
        
        Examples:
            "d+Pb208E190g050znp" -> {"target": "Pb208", "energy": 190, 
                                     "gamma": "050", "pol_type": "znp"}
        """
        import re
        pattern = r'd\+(\w+)E(\d+)g(\d+)(znp|zpn|ynp|ypn)'
        match = re.match(pattern, folder_name)
        if match:
            return {
                "target": match.group(1),
                "energy": int(match.group(2)),
                "gamma": match.group(3),
                "pol_type": match.group(4)
            }
        return None
    
    def load_zpol_data(self, target: str, gamma: str, pol_type: str = "znp",
                       b_range: Tuple[int, int] = None) -> QMDDataset:
        """
        加载 z 极化数据（b_discrete 格式）
        
        Args:
            target: 靶材料, e.g., "Pb208"
            gamma: gamma值, e.g., "050"
            pol_type: "znp" 或 "zpn"
            b_range: b值范围 (bmin, bmax)，如 (5, 10)。None 表示全部
            
        Returns:
            QMDDataset 对象
        """
        folder_name = f"d+{target}E190g{gamma}{pol_type}"
        folder_path = self.base_path / "z_pol" / "b_discrete" / folder_name
        
        if not folder_path.exists():
            raise FileNotFoundError(f"Data folder not found: {folder_path}")
        
        dataset = QMDDataset(
            target=target,
            energy=190,
            gamma=gamma,
            pol_type=pol_type
        )
        
        # 获取所有 dbreakbXX.dat 文件
        data_files = sorted(folder_path.glob("dbreakb*.dat"))
        
        for data_file in data_files:
            # 从文件名提取 b 值
            b_str = data_file.stem.replace("dbreakb", "")
            try:
                b_value = int(b_str)
            except ValueError:
                continue
            
            # 检查 b 范围
            if b_range is not None:
                if b_value < b_range[0] or b_value > b_range[1]:
                    continue
            
            # 读取文件
            events = self._read_zpol_file(data_file, b_value)
            dataset.events.extend(events)
        
        return dataset
    
    def load_ypol_data(self, target: str, gamma: str, pol_type: str = "ynp",
                       b_range: Tuple[float, float] = None) -> QMDDataset:
        """
        加载 y 极化数据（phi_random 格式）
        
        Args:
            target: 靶材料, e.g., "Pb208"
            gamma: gamma值, e.g., "050"
            pol_type: "ynp" 或 "ypn"
            b_range: b值范围，如 (5.0, 10.0)。None 表示全部
            
        Returns:
            QMDDataset 对象
        """
        folder_name = f"d+{target}E190g{gamma}{pol_type}"
        folder_path = self.base_path / "y_pol" / "phi_random" / folder_name
        
        if not folder_path.exists():
            raise FileNotFoundError(f"Data folder not found: {folder_path}")
        
        dataset = QMDDataset(
            target=target,
            energy=190,
            gamma=gamma,
            pol_type=pol_type
        )
        
        # 读取 dbreak.dat 文件
        data_file = folder_path / "dbreak.dat"
        if not data_file.exists():
            raise FileNotFoundError(f"Data file not found: {data_file}")
        
        events = self._read_ypol_file(data_file)
        
        # 按 b 范围过滤
        if b_range is not None:
            events = [e for e in events 
                     if b_range[0] <= e.b <= b_range[1]]
        
        dataset.events = events
        return dataset
    
    def load_data(self, target: str, gamma: str, pol_direction: str,
                  pol_order: str = "np", b_range: Tuple[float, float] = None) -> QMDDataset:
        """
        通用数据加载接口
        
        Args:
            target: 靶材料, e.g., "Pb208"
            gamma: gamma值, e.g., "050"
            pol_direction: "z" 或 "y"
            pol_order: "np" (proton-neutron->z/y) 或 "pn" (neutron-proton->z/y)
            b_range: b值范围
            
        Returns:
            QMDDataset 对象
        """
        pol_type = f"{pol_direction}{pol_order}"
        
        if pol_direction == "z":
            return self.load_zpol_data(target, gamma, pol_type, b_range)
        else:
            return self.load_ypol_data(target, gamma, pol_type, b_range)
    
    def _read_zpol_file(self, filepath: Path, b_value: int) -> List[QMDEvent]:
        """读取 z_pol 格式文件"""
        events = []
        
        with open(filepath, 'r') as f:
            # 跳过头两行
            f.readline()  # info line
            f.readline()  # header line
            
            for line in f:
                parts = line.split()
                if len(parts) < 7:
                    continue
                
                try:
                    event = QMDEvent(
                        event_no=int(parts[0]),
                        pxp=float(parts[1]),
                        pyp=float(parts[2]),
                        pzp=float(parts[3]),
                        pxn=float(parts[4]),
                        pyn=float(parts[5]),
                        pzn=float(parts[6]),
                        b=float(b_value),
                        rpphi=0.0
                    )
                    events.append(event)
                except (ValueError, IndexError):
                    continue
        
        return events
    
    def _read_ypol_file(self, filepath: Path) -> List[QMDEvent]:
        """读取 y_pol 格式文件 (包含 b 和 rpphi)"""
        events = []
        
        with open(filepath, 'r') as f:
            # 跳过头两行
            f.readline()  # info line
            f.readline()  # header line
            
            for line in f:
                parts = line.split()
                if len(parts) < 9:
                    continue
                
                try:
                    event = QMDEvent(
                        event_no=int(parts[0]),
                        pxp=float(parts[1]),
                        pyp=float(parts[2]),
                        pzp=float(parts[3]),
                        pxn=float(parts[4]),
                        pyn=float(parts[5]),
                        pzn=float(parts[6]),
                        b=float(parts[7]),
                        rpphi=float(parts[8])
                    )
                    events.append(event)
                except (ValueError, IndexError):
                    continue
        
        return events
    
    def get_available_targets(self, pol_direction: str = None) -> List[str]:
        """获取所有可用的靶材料"""
        datasets = self.list_available_datasets(pol_direction)
        targets = set()
        
        for pol_type, folders in datasets.items():
            for folder in folders:
                info = self.parse_folder_name(folder)
                if info:
                    targets.add(info["target"])
        
        return sorted(list(targets))
    
    def get_available_gammas(self, target: str = None, 
                             pol_direction: str = None) -> List[str]:
        """获取所有可用的 gamma 值"""
        datasets = self.list_available_datasets(pol_direction)
        gammas = set()
        
        for pol_type, folders in datasets.items():
            for folder in folders:
                info = self.parse_folder_name(folder)
                if info:
                    if target is None or info["target"] == target:
                        gammas.add(info["gamma"])
        
        return sorted(list(gammas))
