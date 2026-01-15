#!/bin/bash
# 批量运行所有磁场配置并导出俯视图
# 使用VRML格式导出（批处理模式下不依赖GUI）

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 确保 SMSIMDIR 环境变量已设置
if [ -z "$SMSIMDIR" ]; then
    export SMSIMDIR="/home/tian/workspace/dpol/smsimulator5.5"
    echo "Setting SMSIMDIR=$SMSIMDIR"
fi

SIM_BIN="$SMSIMDIR/build/bin/sim_deuteron"

echo "================================================"
echo "Running simulations for all magnetic field values"
echo "================================================"

for field in B080T B100T B120T B140T; do
    echo ""
    echo "--- Running ${field} simulation ---"
    echo ""
    
    # 创建临时宏文件用于批处理图像导出
    cat > temp_${field}.mac << EOF
# Load geometry
/control/execute /home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/nopdc/${field}.mac

# 设置氘核束流
/action/gun/Type Pencil
/action/gun/SetBeamParticleName deuteron
/action/gun/Energy 190 MeV
/action/gun/Position 0 0 0 m

/action/file/OverWrite y
/action/file/RunName DTest_${field}
/action/file/SaveDirectory /home/tian/workspace/dpol/smsimulator5.5/data/simulation/g4output

/tracking/storeTrajectory 1

# 使用VRML输出（不需要窗口）
/vis/open VRML2FILE
/vis/drawVolume
/vis/scene/add/axes 0 0 0 6000 mm
/vis/scene/endOfEventAction accumulate
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 1

/run/beamOn 10
/vis/viewer/flush
EOF

    # 运行模拟（忽略退出码，因为日志关闭时可能有问题）
    $SIM_BIN temp_${field}.mac 2>&1 || true
    
    # 移动生成的VRML文件
    if [ -f "g4_00.wrl" ]; then
        mv g4_00.wrl ${field}_geometry.wrl
        echo "  ✓ Exported: ${field}_geometry.wrl"
    elif [ -f "g4_01.wrl" ]; then
        mv g4_01.wrl ${field}_geometry.wrl
        echo "  ✓ Exported: ${field}_geometry.wrl"
    fi
    
    # 清理多余的VRML文件
    rm -f g4_*.wrl
    
    # 清理临时文件
    rm -f temp_${field}.mac
done

echo ""
echo "================================================"
echo "All simulations completed"
echo "Output files:"
ls -la *.wrl 2>/dev/null || echo "No VRML files found"
echo "================================================"
