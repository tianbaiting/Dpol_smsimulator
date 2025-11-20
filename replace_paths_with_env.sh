#!/bin/bash
# 批量替换绝对路径为 $SMSIMDIR 环境变量

set -e

cd /home/tian/workspace/dpol/smsimulator5.5

echo "=========================================="
echo "替换绝对路径为 \$SMSIMDIR 环境变量"
echo "=========================================="
echo ""

# 备份当前更改
git add -A 2>/dev/null || true

# 定义需要替换的路径
OLD_PATH="/home/tian/workspace/dpol/smsimulator5.5"

# 计数器
total_files=0
total_replacements=0

# 查找并替换所有相关文件
echo "正在搜索并替换文件..."
echo ""

# 1. 替换 .sh 文件 (Shell脚本)
echo "处理 Shell 脚本 (.sh)..."
find . -name "*.sh" -type f ! -path "./.git/*" ! -path "./build/*" | while read file; do
    if grep -q "$OLD_PATH" "$file" 2>/dev/null; then
        # 对于 setup.sh 中的定义行，保持不变
        if [[ "$file" == "./setup.sh" ]]; then
            # 只替换除了 export SMSIMDIR= 那一行之外的其他行
            sed -i "/export SMSIMDIR=/! s|$OLD_PATH|\$SMSIMDIR|g" "$file"
        else
            sed -i "s|$OLD_PATH|\$SMSIMDIR|g" "$file"
        fi
        count=$(grep -c "\$SMSIMDIR" "$file" 2>/dev/null || echo "0")
        echo "  ✓ $file ($count 处替换)"
        ((total_files++))
        ((total_replacements+=count))
    fi
done

# 2. 替换 .mac 文件 (Geant4 宏文件)
echo ""
echo "处理 Geant4 宏文件 (.mac)..."
find . -name "*.mac" -type f ! -path "./.git/*" ! -path "./build/*" | while read file; do
    if grep -q "$OLD_PATH" "$file" 2>/dev/null; then
        sed -i "s|$OLD_PATH|\$SMSIMDIR|g" "$file"
        count=$(grep -c "\$SMSIMDIR" "$file" 2>/dev/null || echo "0")
        echo "  ✓ $file ($count 处替换)"
        ((total_files++))
        ((total_replacements+=count))
    fi
done

# 3. 替换 .C 文件 (ROOT 宏)
echo ""
echo "处理 ROOT 宏文件 (.C)..."
find . -name "*.C" -type f ! -path "./.git/*" ! -path "./build/*" | while read file; do
    if grep -q "$OLD_PATH" "$file" 2>/dev/null; then
        # ROOT 宏中使用 gSystem->Getenv("SMSIMDIR")
        sed -i "s|\"$OLD_PATH|Form(\"%s\", gSystem->Getenv(\"SMSIMDIR\"))|g" "$file"
        sed -i "s|'$OLD_PATH|Form(\"%s\", gSystem->Getenv(\"SMSIMDIR\"))|g" "$file"
        echo "  ✓ $file"
        ((total_files++))
    fi
done

# 4. 替换 .cc 和 .cpp 文件  
echo ""
echo "处理 C++ 源文件 (.cc, .cpp)..."
find . \( -name "*.cc" -o -name "*.cpp" \) -type f ! -path "./.git/*" ! -path "./build/*" ! -path "./libs/*" | while read file; do
    if grep -q "$OLD_PATH" "$file" 2>/dev/null; then
        # C++ 中可以使用 std::getenv("SMSIMDIR")
        sed -i "s|\"$OLD_PATH|\"\$SMSIMDIR|g" "$file"
        echo "  ✓ $file"
        ((total_files++))
    fi
done

# 5. 修正 rootlogon.C - 这个文件特殊处理
echo ""
echo "特殊处理 rootlogon.C..."
if [ -f "./rootlogon.C" ]; then
    # 已经在 setup.sh 中正确生成，检查一下
    if grep -q "smsimulator5.5_new" "./rootlogon.C"; then
        echo "  ⚠️  检测到 rootlogon.C 包含 _new 路径，将重新生成"
        rm -f "./rootlogon.C"
        source ./setup.sh
        echo "  ✓ rootlogon.C 已重新生成"
    else
        echo "  ✓ rootlogon.C 已经正确"
    fi
fi

# 6. 特殊处理 d_work/rootlogon.C
echo ""
echo "处理 d_work/rootlogon.C..."
if [ -f "./d_work/rootlogon.C" ]; then
    cat > ./d_work/rootlogon.C << 'EOFROOT'
{
    // Automatically load required libraries when ROOT starts
    TString smsDir = gSystem->Getenv("SMSIMDIR");
    if (smsDir.IsNull()) {
        cerr << "Error: SMSIMDIR environment variable not set!" << endl;
        cerr << "Please run: source setup.sh" << endl;
        return;
    }
    
    cout << "Loading SM libraries from: " << smsDir << "/build/lib/" << endl;
    gSystem->Load(smsDir + "/build/lib/libsmdata.so");
    gSystem->Load(smsDir + "/build/lib/libsmaction.so"); 
    gSystem->Load(smsDir + "/build/lib/libsmconstruction.so");
    gSystem->Load(smsDir + "/build/lib/libsmphysics.so");
    gSystem->Load(smsDir + "/build/lib/libsim_deuteron_core.so");
    
    cout << "Loading PDC Analysis Tools..." << endl;
    gSystem->Load(smsDir + "/build/lib/libanalysis.so");
    
    cout << "All libraries loaded successfully!" << endl;
}
EOFROOT
    echo "  ✓ d_work/rootlogon.C 已更新"
fi

echo ""
echo "=========================================="
echo "✅ 替换完成！"
echo "=========================================="
echo ""
echo "统计："
echo "  处理文件数: $total_files+"
echo ""
echo "注意事项:"
echo "1. setup.sh 中的 export SMSIMDIR= 行保持不变"
echo "2. ROOT 宏(.C)使用 gSystem->Getenv(\"SMSIMDIR\")"
echo "3. Geant4 宏(.mac)和 Shell 脚本使用 \$SMSIMDIR"
echo "4. rootlogon.C 文件已特殊处理"
echo ""
echo "查看更改:"
echo "  git diff"
echo ""
echo "提交更改:"
echo "  git add -A"
echo "  git commit -m 'Replace absolute paths with \$SMSIMDIR environment variable'"
echo ""
