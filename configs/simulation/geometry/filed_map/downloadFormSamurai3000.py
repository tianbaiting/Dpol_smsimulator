#!/usr/bin/env python3
"""
批量下载SAMURAI磁场数据文件
Download field map data files from RIBF SAMURAI
"""

import os
import urllib.request
from pathlib import Path
import zipfile
import argparse

# 定义下载链接列表
urls = [
    # "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,40T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,20T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,00T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/141114-0,8T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,60T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,80T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180627-2,00T-3000.zip"
    "http://ribf.riken.jp/~hsato/fieldmaps/180629-3,00T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180629-2,95T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180629-2,90T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180629-2,85T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180628-2,80T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180628-2,75T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180628-2,70T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180628-2,65T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180703-2,60T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180702-2,55T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180702-2,50T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180702-2,45T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180702-2,40T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180702-2,35T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180628-2,30T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180628-2,25T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180628-2,20T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180628-2,15T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180627-2,10T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180627-2,05T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180627-2,00T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180627-1,95T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180627-1,90T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180704-1,85T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,80T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180627-1,75T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,70T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180627-1,65T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,60T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,55T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,50T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,45T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,40T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,35T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,30T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,25T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,20T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,15T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,10T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180703-1,05T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/180626-1,00T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180704-0,95T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180704-0,90T-3000.zip",
    # "http://ribf.riken.jp/~hsato/fieldmaps/180626-0,85T-3000.zip",
    "http://ribf.riken.jp/~hsato/fieldmaps/141114-0,8T-3000.zip",


]

def download_file(url, save_dir="."):
    """
    下载文件
    
    Args:
        url: 文件URL
        save_dir: 保存目录
    """
    # 创建保存目录
    save_path = Path(save_dir)
    save_path.mkdir(parents=True, exist_ok=True)
    
    # 获取文件名
    filename = url.split("/")[-1]
    filepath = save_path / filename
    
    # 检查文件是否已存在
    if filepath.exists():
        print(f"文件已存在: {filename}")
        return
    
    print(f"正在下载: {filename}")
    try:
        # 下载文件并显示进度
        def reporthook(block_num, block_size, total_size):
            downloaded = block_num * block_size
            if total_size > 0:
                percent = min(downloaded * 100.0 / total_size, 100)
                print(f"\r进度: {percent:.1f}% ({downloaded}/{total_size} bytes)", end="")
        
        urllib.request.urlretrieve(url, filepath, reporthook)
        print(f"\n下载完成: {filename}")
    except Exception as e:
        print(f"\n下载失败: {filename}")
        print(f"错误信息: {e}")
        # 删除不完整的文件
        if filepath.exists():
            filepath.unlink()

def unzip_file(zip_path, extract_dir=None):
    """
    解压zip文件
    
    Args:
        zip_path: zip文件路径
        extract_dir: 解压目录，如果为None则解压到zip文件所在目录
    """
    zip_path = Path(zip_path)
    
    if not zip_path.exists():
        print(f"文件不存在: {zip_path.name}")
        return False
    
    if extract_dir is None:
        extract_dir = zip_path.parent
    else:
        extract_dir = Path(extract_dir)
    
    extract_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"正在解压: {zip_path.name}")
    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            # 获取zip文件中的文件列表
            file_list = zip_ref.namelist()
            print(f"  包含 {len(file_list)} 个文件")
            
            # 解压所有文件
            zip_ref.extractall(extract_dir)
            
        print(f"解压完成: {zip_path.name}")
        return True
    except Exception as e:
        print(f"解压失败: {zip_path.name}")
        print(f"错误信息: {e}")
        return False

def unzip_all(directory="."):
    """
    解压指定目录下所有zip文件
    
    Args:
        directory: 目录路径
    """
    directory = Path(directory)
    
    if not directory.exists():
        print(f"目录不存在: {directory}")
        return
    
    # 查找所有zip文件
    zip_files = list(directory.glob("*.zip"))
    
    if not zip_files:
        print(f"在 {directory} 中没有找到zip文件")
        return
    
    print(f"找到 {len(zip_files)} 个zip文件\n")
    
    success_count = 0
    for i, zip_file in enumerate(zip_files, 1):
        print(f"\n[{i}/{len(zip_files)}]")
        if unzip_file(zip_file):
            success_count += 1
    
    print(f"\n解压完成: {success_count}/{len(zip_files)} 个文件成功")

def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="批量下载和解压SAMURAI磁场数据文件",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 下载所有文件
  python3 downloadFormSamurai.py
  
  # 解压所有zip文件
  python3 downloadFormSamurai.py --unzip
  
  # 下载并自动解压
  python3 downloadFormSamurai.py --download --unzip
  
  # 解压指定目录的所有zip文件
  python3 downloadFormSamurai.py --unzip --dir /path/to/directory
        """
    )
    
    parser.add_argument(
        '--download', '-d',
        action='store_true',
        help='下载文件'
    )
    
    parser.add_argument(
        '--unzip', '-u',
        action='store_true',
        help='解压所有zip文件'
    )
    
    parser.add_argument(
        '--dir',
        type=str,
        default=None,
        help='指定工作目录（默认为脚本所在目录）'
    )
    
    args = parser.parse_args()
    
    # 确定工作目录
    if args.dir:
        work_dir = Path(args.dir)
    else:
        work_dir = Path(__file__).parent
    
    # 如果没有指定任何操作，默认只下载
    if not args.download and not args.unzip:
        args.download = True
    
    # 执行下载
    if args.download:
        print("开始批量下载SAMURAI磁场数据文件...")
        print(f"共 {len(urls)} 个文件\n")
        
        for i, url in enumerate(urls, 1):
            print(f"\n[{i}/{len(urls)}]")
            download_file(url, work_dir)
        
        print("\n所有下载任务完成!")
    
    # 执行解压
    if args.unzip:
        print("\n" + "="*50)
        print("开始解压zip文件...")
        print("="*50)
        unzip_all(work_dir)

if __name__ == "__main__":
    main()

