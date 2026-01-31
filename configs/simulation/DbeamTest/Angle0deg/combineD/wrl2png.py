#!/usr/bin/env python3
# =========================================================================
# [EN] Convert VRML (.wrl) files to PNG with ZX plane view (top view)
# [CN] 将 VRML (.wrl) 文件转换为 PNG，显示 ZX 平面视图（俯视图）
# =========================================================================
# Usage: python wrl2png.py [--all] [file1.wrl file2.wrl ...]
# =========================================================================

import argparse
import glob
import os
import sys

try:
    import pyvista as pv
except ImportError:
    print("[ERROR] pyvista not installed. Run: pip install pyvista")
    sys.exit(1)

def wrl_to_png(wrl_file: str, output_dir: str = None, width: int = 1920, height: int = 1080):
    """
    [EN] Convert a single VRML file to PNG with ZX plane view
    [CN] 将单个 VRML 文件转换为 PNG，ZX 平面视图
    """
    if not os.path.exists(wrl_file):
        print(f"[ERROR] File not found: {wrl_file}")
        return False
    
    base = os.path.splitext(os.path.basename(wrl_file))[0]
    if output_dir is None:
        output_dir = os.path.dirname(wrl_file) or "."
    png_file = os.path.join(output_dir, f"{base}.png")
    
    print(f"[CONVERT] {wrl_file} -> {png_file}")
    
    try:
        # [EN] Create off-screen plotter / [CN] 创建离屏渲染器
        plotter = pv.Plotter(off_screen=True, window_size=[width, height])
        plotter.set_background('white')
        
        # [EN] Import VRML file directly into plotter / [CN] 直接将 VRML 文件导入渲染器
        plotter.import_vrml(wrl_file)
        
        # [EN] Set ZX plane view (looking from +Y direction, top view)
        # [CN] 设置 ZX 平面视图（从 +Y 方向俯视）
        plotter.view_zx()
        
        # [EN] Add axes for reference / [CN] 添加坐标轴作为参考
        plotter.add_axes(xlabel='X', ylabel='Y', zlabel='Z')
        
        # [EN] Save screenshot / [CN] 保存截图
        plotter.screenshot(png_file)
        plotter.close()
        
        print(f"[OK] Saved: {png_file}")
        return True
        
    except Exception as e:
        print(f"[ERROR] Failed to convert {wrl_file}: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description='[EN] Convert VRML to PNG (ZX view) / [CN] 将 VRML 转换为 PNG（ZX 视图）'
    )
    parser.add_argument('files', nargs='*', help='VRML files to convert')
    parser.add_argument('--all', '-a', action='store_true', 
                        help='Convert all .wrl files in current directory')
    parser.add_argument('--output', '-o', type=str, default=None,
                        help='Output directory (default: same as input)')
    parser.add_argument('--width', '-W', type=int, default=1920,
                        help='Image width (default: 1920)')
    parser.add_argument('--height', '-H', type=int, default=1080,
                        help='Image height (default: 1080)')
    
    args = parser.parse_args()
    
    # [EN] Collect files to process / [CN] 收集要处理的文件
    files = []
    if args.all:
        files = glob.glob("*.wrl")
    if args.files:
        files.extend(args.files)
    
    if not files:
        print("[ERROR] No files specified. Use --all or provide file names.")
        parser.print_help()
        sys.exit(1)
    
    print(f"[INFO] Converting {len(files)} file(s) to PNG (ZX view)")
    print(f"[INFO] Resolution: {args.width}x{args.height}")
    
    success = 0
    for f in files:
        if wrl_to_png(f, args.output, args.width, args.height):
            success += 1
    
    print(f"\n[DONE] Converted {success}/{len(files)} files")


if __name__ == "__main__":
    main()
