#!/usr/bin/env python3
"""
批量几何接受度分析脚本
Batch Geometry Acceptance Analysis Script

分析不同磁场和束流偏转角度下的PDC位置和几何接受度

使用方法:
    python3 run_geo_acceptance_batch.py [--output OUTPUT_DIR]
    
作者: Auto-generated
日期: 2025-11-30
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path
from datetime import datetime

# 项目根目录
PROJECT_ROOT = Path(__file__).parent.parent.parent
FIELD_MAP_DIR = PROJECT_ROOT / "configs/simulation/geometry/filed_map"
QMD_DATA_DIR = PROJECT_ROOT / "data/qmdrawdata"
BUILD_DIR = PROJECT_ROOT / "build"

# 典型磁场配置
# 注意: 磁场文件后缀是 .table 不是 .dat
FIELD_CONFIGURATIONS = [
    {
        "name": "0.8T",
        "file": "141114-0,8T-6000.table",
        "strength": 0.8
    },
    {
        "name": "1.0T", 
        "file": "180626-1,00T-6000.table",
        "strength": 1.0
    },
    {
        "name": "1.2T",
        "file": "180626-1,20T-3000.table",  # 注意: 1.2T用的是3000不是6000
        "strength": 1.2
    },
    {
        "name": "1.4T",
        "file": "180703-1,40T-6000.table",
        "strength": 1.4
    }
]

# 典型偏转角度 (度)
DEFLECTION_ANGLES = [0.0, 5.0, 10.0]

def print_banner():
    """打印横幅"""
    print("\n" + "="*70)
    print(" "*15 + "几何接受度批量分析")
    print(" "*10 + "Geometry Acceptance Batch Analysis")
    print("="*70)
    print(f"时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("="*70 + "\n")

def check_prerequisites():
    """检查前置条件"""
    print("检查前置条件...")
    
    # 检查可执行文件
    exe_path = BUILD_DIR / "bin/RunGeoAcceptanceAnalysis"
    if not exe_path.exists():
        print(f"❌ 错误: 找不到可执行文件 {exe_path}")
        print("   请先编译项目: cd build && cmake .. -DBUILD_ANALYSIS=ON && make GeoAcceptance")
        return False
    print(f"✓ 找到可执行文件: {exe_path}")
    
    # 检查QMD数据目录
    if not QMD_DATA_DIR.exists():
        print(f"⚠️  警告: QMD数据目录不存在 {QMD_DATA_DIR}")
        print("   将使用空数据运行（仅计算几何配置）")
    else:
        print(f"✓ QMD数据目录: {QMD_DATA_DIR}")
    
    # 检查磁场文件
    print(f"\n检查磁场文件:")
    missing_fields = []
    for field in FIELD_CONFIGURATIONS:
        field_path = FIELD_MAP_DIR / field["file"]
        if field_path.exists():
            print(f"  ✓ {field['name']}: {field_path}")
        else:
            print(f"  ✗ {field['name']}: {field_path} (不存在)")
            missing_fields.append(field['name'])
    
    if missing_fields:
        print(f"\n⚠️  警告: 以下磁场文件不存在: {', '.join(missing_fields)}")
        print("   请先下载磁场数据:")
        print("   cd configs/simulation/geometry/filed_map")
        print("   python3 downloadFormSamurai.py -d -u")
        return False
    
    print("\n✓ 所有前置条件满足\n")
    return True

def create_output_directory(output_dir):
    """创建输出目录"""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # 创建子目录
    (output_path / "logs").mkdir(exist_ok=True)
    (output_path / "results").mkdir(exist_ok=True)
    (output_path / "plots").mkdir(exist_ok=True)
    
    print(f"输出目录: {output_path}")
    return output_path

def run_single_analysis(field_config, angles, output_dir, qmd_path):
    """运行单个磁场配置的分析"""
    field_name = field_config['name']
    field_file = FIELD_MAP_DIR / field_config['file']
    field_strength = field_config['strength']
    
    print(f"\n{'='*70}")
    print(f"分析磁场配置: {field_name} ({field_strength} T)")
    print(f"磁场文件: {field_file}")
    print(f"偏转角度: {angles}")
    print(f"{'='*70}\n")
    
    # 构建命令
    exe_path = BUILD_DIR / "bin/RunGeoAcceptanceAnalysis"
    result_dir = output_dir / "results" / field_name
    result_dir.mkdir(parents=True, exist_ok=True)
    
    cmd = [
        str(exe_path),
        "--fieldmap", str(field_file),
        "--field", str(field_strength),
        "--qmd", str(qmd_path),
        "--output", str(result_dir)
    ]
    
    # 添加角度参数
    for angle in angles:
        cmd.extend(["--angle", str(angle)])
    
    # 运行命令
    log_file = output_dir / "logs" / f"{field_name}.log"
    print(f"运行命令: {' '.join(cmd)}")
    print(f"日志文件: {log_file}\n")
    
    try:
        with open(log_file, 'w') as log:
            log.write(f"# 几何接受度分析日志\n")
            log.write(f"# 磁场: {field_name} ({field_strength} T)\n")
            log.write(f"# 时间: {datetime.now()}\n")
            log.write(f"# 命令: {' '.join(cmd)}\n\n")
            log.flush()
            
            result = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True
            )
            
            # 写入输出
            log.write(result.stdout)
            
            # 同时打印到终端
            print(result.stdout)
            
            if result.returncode != 0:
                print(f"❌ 分析失败: {field_name}")
                return False
            
        print(f"✓ 分析成功: {field_name}")
        print(f"  结果保存在: {result_dir}")
        return True
        
    except Exception as e:
        print(f"❌ 运行出错: {e}")
        return False

def generate_summary_report(output_dir):
    """生成汇总报告"""
    print(f"\n{'='*70}")
    print("生成汇总报告...")
    print(f"{'='*70}\n")
    
    summary_file = output_dir / "SUMMARY_REPORT.txt"
    
    with open(summary_file, 'w') as f:
        f.write("="*70 + "\n")
        f.write(" "*20 + "几何接受度分析汇总报告\n")
        f.write(" "*15 + "Geometry Acceptance Summary Report\n")
        f.write("="*70 + "\n\n")
        
        f.write(f"分析时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"项目目录: {PROJECT_ROOT}\n")
        f.write(f"输出目录: {output_dir}\n\n")
        
        f.write("分析配置:\n")
        f.write("-"*70 + "\n")
        f.write(f"磁场配置: {len(FIELD_CONFIGURATIONS)} 个\n")
        for field in FIELD_CONFIGURATIONS:
            f.write(f"  - {field['name']}: {field['strength']} T\n")
        
        f.write(f"\n偏转角度: {len(DEFLECTION_ANGLES)} 个\n")
        f.write(f"  {', '.join(f'{a}°' for a in DEFLECTION_ANGLES)}\n\n")
        
        f.write("结果文件:\n")
        f.write("-"*70 + "\n")
        
        results_dir = output_dir / "results"
        if results_dir.exists():
            for field in FIELD_CONFIGURATIONS:
                field_dir = results_dir / field['name']
                if field_dir.exists():
                    f.write(f"\n{field['name']}:\n")
                    
                    report_file = field_dir / "acceptance_report.txt"
                    root_file = field_dir / "acceptance_results.root"
                    
                    if report_file.exists():
                        f.write(f"  ✓ 文本报告: {report_file}\n")
                    else:
                        f.write(f"  ✗ 文本报告: 不存在\n")
                    
                    if root_file.exists():
                        f.write(f"  ✓ ROOT文件: {root_file}\n")
                    else:
                        f.write(f"  ✗ ROOT文件: 不存在\n")
        
        f.write("\n" + "="*70 + "\n")
        f.write("分析完成!\n")
        f.write("="*70 + "\n")
    
    print(f"✓ 汇总报告已生成: {summary_file}\n")
    
    # 打印到终端
    with open(summary_file, 'r') as f:
        print(f.read())

def create_analysis_script(output_dir):
    """创建ROOT分析脚本"""
    script_file = output_dir / "analyze_results.C"
    
    script_content = '''
// ROOT脚本: 分析几何接受度结果
// 使用方法: root -l analyze_results.C

void analyze_results() {
    gStyle->SetOptStat(0);
    
    // 磁场配置
    vector<string> fields = {"0.8T", "1.0T", "1.2T", "1.4T"};
    vector<double> fieldStrengths = {0.8, 1.0, 1.2, 1.4};
    
    // 创建画布
    TCanvas *c1 = new TCanvas("c1", "PDC Acceptance vs Deflection Angle", 1200, 800);
    c1->Divide(2, 2);
    
    TLegend *leg = new TLegend(0.7, 0.7, 0.9, 0.9);
    
    // 读取所有数据
    for (int i = 0; i < fields.size(); i++) {
        string filename = Form("results/%s/acceptance_results.root", fields[i].c_str());
        TFile *f = TFile::Open(filename.c_str());
        
        if (!f || f->IsZombie()) {
            cout << "无法打开文件: " << filename << endl;
            continue;
        }
        
        TTree *tree = (TTree*)f->Get("acceptance");
        if (!tree) {
            cout << "找不到TTree: acceptance in " << filename << endl;
            continue;
        }
        
        c1->cd(i+1);
        tree->Draw("pdcAcceptance:deflectionAngle", "", "LP");
        gPad->SetTitle(Form("%s - PDC Acceptance", fields[i].c_str()));
        gPad->SetGrid();
    }
    
    c1->SaveAs("plots/acceptance_comparison.png");
    cout << "图表已保存: plots/acceptance_comparison.png" << endl;
}
'''
    
    with open(script_file, 'w') as f:
        f.write(script_content)
    
    print(f"✓ ROOT分析脚本已创建: {script_file}")
    print(f"  使用方法: cd {output_dir} && root -l analyze_results.C\n")

def main():
    parser = argparse.ArgumentParser(
        description="批量几何接受度分析",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 使用默认输出目录
  python3 run_geo_acceptance_batch.py
  
  # 指定输出目录
  python3 run_geo_acceptance_batch.py --output ./my_results
  
  # 只分析特定磁场
  python3 run_geo_acceptance_batch.py --fields 0.8T 1.0T
  
  # 只分析特定角度
  python3 run_geo_acceptance_batch.py --angles 0 5
        """
    )
    
    parser.add_argument(
        '--output', '-o',
        default='./geo_acceptance_batch_results',
        help='输出目录 (默认: ./geo_acceptance_batch_results)'
    )
    
    parser.add_argument(
        '--fields',
        nargs='+',
        choices=['0.8T', '1.0T', '1.2T', '1.4T'],
        help='指定要分析的磁场 (默认: 全部)'
    )
    
    parser.add_argument(
        '--angles',
        nargs='+',
        type=float,
        help='指定偏转角度 (默认: 0 5 10)'
    )
    
    parser.add_argument(
        '--qmd-path',
        default=str(QMD_DATA_DIR),
        help=f'QMD数据路径 (默认: {QMD_DATA_DIR})'
    )
    
    parser.add_argument(
        '--skip-checks',
        action='store_true',
        help='跳过前置条件检查'
    )
    
    args = parser.parse_args()
    
    # 打印横幅
    print_banner()
    
    # 检查前置条件
    if not args.skip_checks:
        if not check_prerequisites():
            print("\n❌ 前置条件不满足，退出")
            return 1
    
    # 创建输出目录
    output_dir = create_output_directory(args.output)
    
    # 确定要分析的磁场
    if args.fields:
        fields_to_analyze = [f for f in FIELD_CONFIGURATIONS if f['name'] in args.fields]
    else:
        fields_to_analyze = FIELD_CONFIGURATIONS
    
    # 确定偏转角度
    angles = args.angles if args.angles else DEFLECTION_ANGLES
    
    print(f"\n将分析 {len(fields_to_analyze)} 个磁场配置:")
    for field in fields_to_analyze:
        print(f"  - {field['name']}: {field['strength']} T")
    
    print(f"\n偏转角度: {angles}\n")
    
    # 运行批量分析
    success_count = 0
    total_count = len(fields_to_analyze)
    
    for i, field in enumerate(fields_to_analyze, 1):
        print(f"\n[{i}/{total_count}] 分析 {field['name']}...")
        if run_single_analysis(field, angles, output_dir, args.qmd_path):
            success_count += 1
    
    # 生成汇总报告
    generate_summary_report(output_dir)
    
    # 创建分析脚本
    create_analysis_script(output_dir)
    
    # 最终总结
    print("\n" + "="*70)
    print(f"批量分析完成!")
    print(f"成功: {success_count}/{total_count}")
    print(f"输出目录: {output_dir}")
    print("="*70 + "\n")
    
    return 0 if success_count == total_count else 1

if __name__ == "__main__":
    sys.exit(main())
