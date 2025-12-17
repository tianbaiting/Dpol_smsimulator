#!/usr/bin/env python3
"""
QMD 几何接受度筛选主运行脚本

用法:
    python run_qmd_geo_filter.py                    # 使用默认配置
    python run_qmd_geo_filter.py --config config.json  # 使用配置文件
    python run_qmd_geo_filter.py --generate-config     # 生成配置模板

配置参数说明:
    - field_strengths: 磁场强度列表 [T]
    - deflection_angles: 束流偏转角度列表 [度]
    - targets: 靶材料列表
    - pol_types: 极化类型列表 ["zpol", "ypol"]
    - gamma_values: gamma 值列表
    - b_range: 碰撞参数范围 (bmin, bmax)
    - momentum_resolutions: 动量分辨率列表 (0.0 = 无模糊)
"""

import sys
import os
import argparse
import json
from pathlib import Path

# 添加库路径
SMSIMDIR = os.environ.get('SMSIMDIR', '/home/tian/workspace/dpol/smsimulator5.5')
sys.path.insert(0, os.path.join(SMSIMDIR, 'libs'))

from qmd_geo_filter import AnalysisRunner, MomentumCut, QMDDataLoader
from qmd_geo_filter.analysis_runner import AnalysisConfig


# ============================================================
# 在这里配置分析参数
# ============================================================

DEFAULT_CONFIG = {
    # 磁场强度 [T]
    "field_strengths": [1.0],
    
    # 束流偏转角度 [度]
    # 不同角度对应不同的 target 位置
    "deflection_angles": [5.0],
    
    # 靶材料
    "targets": ["Pb208"],
    
    # 极化类型
    # "zpol": z 方向极化
    # "ypol": y 方向极化
    "pol_types": ["zpol", "ypol"],
    
    # gamma 值列表
    # 对应 Esym 中的 gamma 参数
    "gamma_values": ["050", "060", "070", "080"],
    
    # 碰撞参数范围 [fm]
    "b_range": [5.0, 10.0],
    
    # 动量 cut 参数
    "momentum_cut_config": {
        "delta_py_threshold": 150.0,      # |pyp - pyn| 阈值 [MeV/c]
        "pt_sum_sq_threshold": 2500.0,    # (pxp+pxn)^2 + (pyp+pyn)^2 阈值
        "px_sum_after_rotation": 200.0,   # 旋转后 (pxp + pxn) 阈值 [MeV/c]
        "phi_threshold": 0.2              # |π - |φ|| 阈值 [rad]
    },
    
    # 动量分辨率列表 (用于测试不同分辨率)
    # 0.0 = 无模糊
    # 0.02 = 2% 分辨率
    # 0.05 = 5% 分辨率
    "momentum_resolutions": [0.0],
    
    # 输出目录 (留空使用默认)
    "output_base_dir": ""
}


def generate_config_template(output_file: str):
    """生成配置文件模板"""
    # 完整配置示例
    full_config = {
        # 多个磁场强度
        "field_strengths": [0.8, 1.0, 1.2, 1.4],
        
        # 多个偏转角度
        "deflection_angles": [0.0, 5.0, 10.0, 15.0],
        
        # 多个靶材料
        "targets": ["Pb208", "Sn124", "Sn112"],
        
        # 两种极化
        "pol_types": ["zpol", "ypol"],
        
        # 所有 gamma 值
        "gamma_values": ["050", "055", "060", "065", "070", "075", "080", "085"],
        
        # b 范围
        "b_range": [5.0, 10.0],
        
        # 动量 cut 参数
        "momentum_cut_config": {
            "delta_py_threshold": 150.0,
            "pt_sum_sq_threshold": 2500.0,
            "px_sum_after_rotation": 200.0,
            "phi_threshold": 0.2
        },
        
        # 测试不同分辨率
        "momentum_resolutions": [0.0, 0.02, 0.05, 0.10],
        
        # 输出目录
        "output_base_dir": ""
    }
    
    with open(output_file, 'w') as f:
        json.dump(full_config, f, indent=2)
    
    print(f"Configuration template saved to: {output_file}")


def run_quick_test():
    """运行快速测试"""
    print("Running quick test with minimal configuration...")
    
    config = AnalysisConfig(
        field_strengths=[1.0],
        deflection_angles=[5.0],
        targets=["Pb208"],
        pol_types=["zpol"],
        gamma_values=["050", "060"],
        b_range=(5.0, 10.0),
        momentum_resolutions=[0.0]
    )
    
    runner = AnalysisRunner(config)
    results = runner.run_full_analysis()
    
    return results


def main():
    parser = argparse.ArgumentParser(
        description="QMD Geometry Filter Analysis",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        '--config', '-c',
        type=str,
        help='Path to configuration JSON file'
    )
    
    parser.add_argument(
        '--generate-config', '-g',
        type=str,
        nargs='?',
        const='qmd_geo_filter_config.json',
        help='Generate a configuration template file'
    )
    
    parser.add_argument(
        '--test', '-t',
        action='store_true',
        help='Run a quick test with minimal configuration'
    )
    
    parser.add_argument(
        '--list-data', '-l',
        action='store_true',
        help='List available data files'
    )
    
    parser.add_argument(
        '--output', '-o',
        type=str,
        help='Override output directory'
    )
    
    args = parser.parse_args()
    
    # 生成配置模板
    if args.generate_config:
        generate_config_template(args.generate_config)
        return 0
    
    # 列出可用数据
    if args.list_data:
        loader = QMDDataLoader()
        datasets = loader.list_available_datasets()
        
        print("Available datasets:")
        print("=" * 50)
        
        for pol_type, folders in datasets.items():
            print(f"\n{pol_type}:")
            for folder in folders:
                info = loader.parse_folder_name(folder)
                if info:
                    print(f"  {folder}")
                    print(f"    Target: {info['target']}, Energy: {info['energy']} MeV/u, "
                          f"γ: {info['gamma']}, Pol: {info['pol_type']}")
        
        return 0
    
    # 快速测试
    if args.test:
        run_quick_test()
        return 0
    
    # 加载配置
    if args.config:
        print(f"Loading configuration from: {args.config}")
        config = AnalysisConfig.from_json(args.config)
    else:
        print("Using default configuration")
        config = AnalysisConfig(**{
            k: tuple(v) if k == 'b_range' else v 
            for k, v in DEFAULT_CONFIG.items()
        })
    
    # 覆盖输出目录
    if args.output:
        config.output_base_dir = args.output
    
    # 运行分析
    runner = AnalysisRunner(config)
    results = runner.run_full_analysis()
    
    print(f"\nResults saved to: {config.output_base_dir}")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
