#!/bin/bash


# 使用命令获取主机名
# tr -d '\n' 用于删除可能存在的换行符
CURRENT_HOST=$(hostname | tr -d '\n')

echo "正在加载环境配置，检测到主机: ${CURRENT_HOST}"

case "$CURRENT_HOST" in
    "TBTdebian")
        # geant4 for debian
        #------> added for setting up the GEANT4
        source /home/tian/software/geant4/install/bin/geant4.sh
        #<------
        export TARTSYS=/home/tian/software/anaroot/install


        export SMSIMDIR=/home/tian/workspace/dpol/smsimulator5.5
        ;;

    "spana03")
        # --- spana03 服务器配置 ---

        # 定义当前项目的根目录
        # 如果你在这个脚本所在的目录下 source 它，建议使用 pwd 动态获取，方便移植
        # export SMSIMDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
        # 或者保持你原本的硬编码路径：
        export SMSIMDIR=/home/tbt/workspace/Dpol_smsimulator

        # [TODO] 确认 TARTSYS (ANAROOT 安装路径)
        # 如果你是用 install_anaroot.sh 安装到了 conda 环境里，这里应该是 $CONDA_PREFIX
        # 如果你是安装在本地某个文件夹，请指向那个文件夹
        # export TARTSYS=/home/kokubun/smsim_for_spana/anaroot/install
        export TARTSYS=/home/tbt/software/anaroot/install
        # 如果 ANAROOT 安装在 Conda 环境系统目录下，解开下面这行：
        # export TARTSYS=$CONDA_PREFIX
        ;;
    
    *)
        # --- 其他未匹配的设备 ---
        echo "  -> 未知设备，仅加载通用配置。"
        ;;
esac




# # spana
# source  /home/tbt/software/root/bin/thisroot.sh
# # source /home/common/singularity/centos7/geant4/geant4.10.04.p03-install/share/Geant4-10.4.3/geant4make/geant4make.sh
# source /home/tbt/software/geant4-install/bin/geant4.sh
# #source /home/rocky8/geant4/geant4.10.04.p03-build/geant4make.sh
# # export TARTSYS=/home/kokubun/smsim_for_spana/anaroot/install
# export TARTSYS=/home/tbt/software/anaroot/install

# export SMSIMDIR=/home/tbt/workspace/Dpol_smsimulator




export PATH=$PATH:$SMSIMDIR/bin
export G4SMLIBDIR=$SMSIMDIR/libs/smg4lib

export G4SMPHYSICSDIR=$SMSIMDIR/libs/smg4lib/src/physics
export G4SMACTIONDIR=$SMSIMDIR/libs/smg4lib/src/action
export G4SMCONSTRUCTIONDIR=$SMSIMDIR/libs/smg4lib/src/construction
export G4SMDATADIR=$SMSIMDIR/libs/smg4lib/src/data

export G4SMCPPFLAGS="-I$G4SMLIBDIR/include"
export G4SMLDLIBS="-lsmphysics -lsmaction -lsmconstruction -lsmdata"


#    echo $LD_LIBRARY_PATH
#    echo
#    echo $G4SMLIBDIR
#    echo
#    echo $SMSIMDIR


if [[ $LD_LIBRARY_PATH == "" ]]; then
    export LD_LIBRARY_PATH=$SMSIMDIR/build/lib:$TARTSYS/lib
else
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SMSIMDIR/build/lib:$TARTSYS/lib
fi

if [[ $LIBRARY_PATH == "" ]]; then
    export LIBRARY_PATH=$SMSIMDIR/build/lib
else
    export LIBRARY_PATH=$LIBRARY_PATH:$SMSIMDIR/build/lib
fi

## 得加自己的库的东西
if [[ $ROOT_INCLUDE_PATH == "" ]]; then
    export ROOT_INCLUDE_PATH=$SMSIMDIR/libs/smg4lib/src/data/include:$SMSIMDIR/libs/analysis/include:$TARTSYS/include
else
    export ROOT_INCLUDE_PATH=$ROOT_INCLUDE_PATH:$SMSIMDIR/libs/smg4lib/src/data/include:$SMSIMDIR/libs/analysis/include:$TARTSYS/include
fi




# Set ROOT library preloading for automatic library loading
export ROOT_PRELOAD_LIBS="libsmlogger.so:libsmdata.so:libsmaction.so:libsmconstruction.so:libsmphysics.so:libsim_deuteron_core.so:libanalysis.so"

# Create rootlogon.C for automatic library loading in ROOT
# Only create if build directory exists
if [ -d "$SMSIMDIR/build/lib" ]; then
    cat > $SMSIMDIR/rootlogon.C << 'EOF'
{
    // Automatically load required libraries when ROOT starts
    cout << "Loading SM libraries from build directory..." << endl;
    
    // Since LD_LIBRARY_PATH is set by setup.sh, we can use short names
    // ROOT will automatically find them in the library path
    TString libs[] = {
        "libsmdata.so",       // 包含 TBeamSimData 等数据类（必须先加载）
        "libsmlogger.so",
        "libsmaction.so",
        "libsmconstruction.so",
        "libsmphysics.so",
        "libsim_deuteron_core.so"
    };
    
    for (const auto& lib : libs) {
        if (gSystem->Load(lib) >= 0) {
            cout << "  ✓ Loaded: " << lib << endl;
        } else {
            // Not an error if optional library is missing
            cout << "  ⊘ Skipped: " << lib << " (not found or already loaded)" << endl;
        }
    }
    
    cout << "\nLoading PDC Analysis Tools..." << endl;
    // Try different possible names for the analysis library
    if (gSystem->Load("libpdcanalysis.so") >= 0) {
        cout << "  ✓ Loaded: libpdcanalysis.so" << endl;
    } else if (gSystem->Load("libanalysis.so") >= 0) {
        cout << "  ✓ Loaded: libanalysis.so" << endl;
    } else {
        cout << "  ⚠ Warning: Could not load analysis library" << endl;
    }
    
    cout << "\n✓ All available libraries loaded successfully!" << endl;
    cout << "  Library search path includes: $SMSIMDIR/build/lib\n" << endl;
}
EOF

    # Replace $SMSIMDIR with actual path in rootlogon.C
    sed -i "s|\$SMSIMDIR|$SMSIMDIR|g" $SMSIMDIR/rootlogon.C
fi

## ----> for kondo
#export TEMPDIRKONDO=/home/kondo/exp/samurai21/anaroot/mysrc
#if [ -e $TEMPDIRKONDO ];then
#    echo set kondo parameters 
#    export G4VRMLFILE_VIEWER=view3dscene
#    export PATH=$PATH:/home/kondo/exp/geant4/viewer/bin:$TARTSYS/../util/bin
#    export TANASYS=$TEMPDIRKONDO
#    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$TANASYS/lib
#    export LIBRARY_PATH=$LIBRARY_PATH:$TANASYS/lib
#fi
#unset TEMPDIRKONDO
