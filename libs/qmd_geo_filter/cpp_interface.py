"""
C++ 程序调用接口

提供 Python 接口来调用 C++ 实现的 RunQMDGeoFilter 程序
"""

import os
import subprocess
import json
from pathlib import Path
from typing import List, Optional, Dict, Any
from dataclasses import dataclass, field


@dataclass
class CppAnalysisConfig:
    """C++ 分析配置"""
    field_strengths: List[float] = field(default_factory=lambda: [1.0])
    deflection_angles: List[float] = field(default_factory=lambda: [5.0])
    targets: List[str] = field(default_factory=lambda: ["Pb208"])
    pol_types: List[str] = field(default_factory=lambda: ["zpol", "ypol"])
    gamma_values: List[str] = field(default_factory=lambda: ["050", "060", "070", "080"])
    qmd_data_path: Optional[str] = None
    field_map_path: Optional[str] = None
    output_path: Optional[str] = None
    
    def __post_init__(self):
        # 设置默认路径
        smsimdir = os.environ.get('SMSIMDIR', '/home/tian/workspace/dpol/smsimulator5.5')
        
        if self.qmd_data_path is None:
            self.qmd_data_path = f"{smsimdir}/data/qmdrawdata"
        if self.field_map_path is None:
            self.field_map_path = f"{smsimdir}/configs/simulation/geometry/filed_map"
        if self.output_path is None:
            self.output_path = f"{smsimdir}/results/qmd_geo_filter"


def find_cpp_executable() -> Optional[str]:
    """查找 C++ 可执行文件"""
    smsimdir = os.environ.get('SMSIMDIR', '/home/tian/workspace/dpol/smsimulator5.5')
    
    possible_paths = [
        f"{smsimdir}/build/bin/RunQMDGeoFilter",
        f"{smsimdir}/bin/RunQMDGeoFilter",
        "RunQMDGeoFilter"
    ]
    
    for path in possible_paths:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    
    return None


def run_cpp_analysis(config: CppAnalysisConfig) -> bool:
    """
    调用 C++ 程序运行分析
    
    Args:
        config: 分析配置
        
    Returns:
        True 如果成功，否则 False
    """
    exe = find_cpp_executable()
    if exe is None:
        print("ERROR: Cannot find RunQMDGeoFilter executable.")
        print("Please build the project first: cd build && cmake .. && make RunQMDGeoFilter")
        return False
    
    # 构建命令行参数
    cmd = [exe]
    
    for field in config.field_strengths:
        cmd.extend(["--field", str(field)])
    
    for angle in config.deflection_angles:
        cmd.extend(["--angle", str(angle)])
    
    for target in config.targets:
        cmd.extend(["--target", target])
    
    for pol in config.pol_types:
        cmd.extend(["--pol", pol])
    
    for gamma in config.gamma_values:
        cmd.extend(["--gamma", gamma])
    
    cmd.extend(["--qmd", config.qmd_data_path])
    cmd.extend(["--fieldmap", config.field_map_path])
    cmd.extend(["--output", config.output_path])
    
    print(f"Running: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(cmd, check=True)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Analysis failed with return code {e.returncode}")
        return False
    except FileNotFoundError:
        print(f"ERROR: Executable not found: {exe}")
        return False


class QMDGeoFilterRunner:
    """
    QMD 几何筛选运行器
    
    提供简化的 Python 接口来调用 C++ 分析程序
    """
    
    def __init__(self, config: Optional[CppAnalysisConfig] = None):
        self.config = config or CppAnalysisConfig()
        self._results: Dict[str, Any] = {}
    
    def set_field_strengths(self, fields: List[float]):
        self.config.field_strengths = fields
    
    def set_deflection_angles(self, angles: List[float]):
        self.config.deflection_angles = angles
    
    def set_targets(self, targets: List[str]):
        self.config.targets = targets
    
    def set_pol_types(self, pol_types: List[str]):
        self.config.pol_types = pol_types
    
    def set_gamma_values(self, gammas: List[str]):
        self.config.gamma_values = gammas
    
    def set_output_path(self, path: str):
        self.config.output_path = path
    
    def run(self) -> bool:
        """运行分析"""
        return run_cpp_analysis(self.config)
    
    def get_output_path(self) -> str:
        return self.config.output_path


def run_full_analysis(
    field_strengths: List[float] = [1.0],
    deflection_angles: List[float] = [5.0],
    targets: List[str] = ["Pb208"],
    pol_types: List[str] = ["zpol", "ypol"],
    gamma_values: List[str] = ["050", "060", "070", "080"],
    output_path: Optional[str] = None
) -> bool:
    """
    快速运行完整分析
    
    Args:
        field_strengths: 磁场强度列表 [T]
        deflection_angles: 偏转角度列表 [度]
        targets: 靶材料列表
        pol_types: 极化类型列表
        gamma_values: gamma 值列表
        output_path: 输出路径
        
    Returns:
        True 如果成功
    """
    config = CppAnalysisConfig(
        field_strengths=field_strengths,
        deflection_angles=deflection_angles,
        targets=targets,
        pol_types=pol_types,
        gamma_values=gamma_values,
        output_path=output_path
    )
    
    return run_cpp_analysis(config)
