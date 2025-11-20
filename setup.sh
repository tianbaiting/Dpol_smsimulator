#!/bin/bash

#------> added for setting up the GEANT4
source /home/tian/software/geant4/install/bin/geant4.sh
#<------
export TARTSYS=/home/tian/software/anaroot/install


export SMSIMDIR=/home/tian/workspace/dpol/smsimulator5.5
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
export ROOT_PRELOAD_LIBS="libsmdata.so:libsmaction.so:libsmconstruction.so:libsmphysics.so:libsim_deuteron_core.so:libanalysis.so"

# Create rootlogon.C for automatic library loading in ROOT
# Only create if build directory exists
if [ -d "$SMSIMDIR/build/lib" ]; then
    cat > $SMSIMDIR/rootlogon.C << 'EOF'
{
    // Automatically load required libraries when ROOT starts
    cout << "Loading SM libraries from build directory..." << endl;
    gSystem->Load("$SMSIMDIR/build/lib/libsmdata.so");
    gSystem->Load("$SMSIMDIR/build/lib/libsmaction.so"); 
    gSystem->Load("$SMSIMDIR/build/lib/libsmconstruction.so");
    gSystem->Load("$SMSIMDIR/build/lib/libsmphysics.so");
    gSystem->Load("$SMSIMDIR/build/lib/libsim_deuteron_core.so");
    
    cout << "Loading PDC Analysis Tools..." << endl;
    gSystem->Load("$SMSIMDIR/build/lib/libanalysis.so");
    
    cout << "All libraries loaded successfully!" << endl;
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
