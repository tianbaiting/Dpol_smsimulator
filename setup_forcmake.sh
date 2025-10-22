#!/bin/bash

#------> added for setting up the GEANT4
source /home/tbt/Software/geant4/install/bin/geant4.sh
#<------
export TARTSYS=/home/tbt/Software/anaroot/install


export SMSIMDIR=/home/tbt/workspace/dpol/smsimulator5.5
export PATH=$PATH:$SMSIMDIR/bin
export G4SMLIBDIR=$SMSIMDIR/sources/smg4lib

export G4SMPHYSICSDIR=$SMSIMDIR/sources/smg4lib/physics
export G4SMACTIONDIR=$SMSIMDIR/sources/smg4lib/action
export G4SMCONSTRUCTIONDIR=$SMSIMDIR/sources/smg4lib/construction
export G4SMDATADIR=$SMSIMDIR/sources/smg4lib/data

export G4SMCPPFLAGS="-I$G4SMLIBDIR/include"
export G4SMLDLIBS="-lsmphysics -lsmaction -lsmconstruction -lsmdata"


#    echo $LD_LIBRARY_PATH
#    echo
#    echo $G4SMLIBDIR
#    echo
#    echo $SMSIMDIR


if [[ $LD_LIBRARY_PATH == "" ]]; then
    export LD_LIBRARY_PATH=$SMSIMDIR/smg4lib/lib:$SMSIMDIR/build/sources/smg4lib/data:$SMSIMDIR/build/sources/smg4lib/action:$SMSIMDIR/build/sources/smg4lib/construction:$SMSIMDIR/build/sources/smg4lib/physics:$SMSIMDIR/build/sources/sim_deuteron:$TARTSYS/lib:$SMSIMDIR/d_work/sources/build/
else
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SMSIMDIR/smg4lib/lib:$SMSIMDIR/build/sources/smg4lib/data:$SMSIMDIR/build/sources/smg4lib/action:$SMSIMDIR/build/sources/smg4lib/construction:$SMSIMDIR/build/sources/smg4lib/physics:$SMSIMDIR/build/sources/sim_deuteron:$TARTSYS/lib:$SMSIMDIR/d_work/sources/build/
fi

if [[ $LIBRARY_PATH == "" ]]; then
    export LIBRARY_PATH=$SMSIMDIR/smg4lib/lib:$SMSIMDIR/build/sources/smg4lib/data:$SMSIMDIR/build/sources/smg4lib/action:$SMSIMDIR/build/sources/smg4lib/construction:$SMSIMDIR/build/sources/smg4lib/physics:$SMSIMDIR/d_work/sources/build/
else
    export LIBRARY_PATH=$LIBRARY_PATH:$SMSIMDIR/smg4lib/lib:$SMSIMDIR/build/sources/smg4lib/data:$SMSIMDIR/build/sources/smg4lib/action:$SMSIMDIR/build/sources/smg4lib/construction:$SMSIMDIR/build/sources/smg4lib/physics:$SMSIMDIR/d_work/sources/build/
fi

## 得加自己的库的东西
if [[ $ROOT_INCLUDE_PATH == "" ]]; then
    export ROOT_INCLUDE_PATH=$SMSIMDIR/sources/smg4lib/data/include:$TARTSYS/include:$SMSIMDIR/d_work/sources/include:$SMSIMDIR/d_work
else
    export ROOT_INCLUDE_PATH=$ROOT_INCLUDE_PATH:$SMSIMDIR/sources/smg4lib/data/include:$TARTSYS/include:$SMSIMDIR/d_work/sources/include:$SMSIMDIR/d_work
fi




# Set ROOT library preloading for automatic library loading
export ROOT_PRELOAD_LIBS="libsmdata.so:libsmaction.so:libsmconstruction.so:libsmphysics.so:libsim_deuteron.so"

# Create rootlogon.C for automatic library loading in ROOT
cat > $SMSIMDIR/d_work/rootlogon.C << 'EOF'
{
    // Automatically load required libraries when ROOT starts
    cout << "Loading SM libraries from build directory..." << endl;
    gSystem->Load("$SMSIMDIR/build/sources/smg4lib/data/libsmdata.so");
    gSystem->Load("$SMSIMDIR/build/sources/smg4lib/action/libsmaction.so"); 
    gSystem->Load("$SMSIMDIR/build/sources/smg4lib/construction/libsmconstruction.so");
    gSystem->Load("$SMSIMDIR/build/sources/smg4lib/physics/libsmphysics.so");
    gSystem->Load("$SMSIMDIR/build/sources/sim_deuteron/libsim_deuteron.so");
    
    cout << "Loading PDC Analysis Tools..." << endl;
    gSystem->Load("$SMSIMDIR/d_work/sources/build/libPDCAnalysisTools.so");
    
    cout << "All libraries loaded successfully!" << endl;
}
EOF

# Replace $SMSIMDIR with actual path in rootlogon.C
sed -i "s|\$SMSIMDIR|$SMSIMDIR|g" $SMSIMDIR/d_work/rootlogon.C

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
