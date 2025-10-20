#!/bin/bash

#------> added for setting up the GEANT4
source /home/tian/software/geant4/install/bin/geant4.sh
#<------
export TARTSYS=/home/tian/software/anaroot/install


export SMSIMDIR=/home/tian/workspace/dpol/smsimulator5.5
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
    export LD_LIBRARY_PATH=$SMSIMDIR/build/sources/smg4lib/data:$SMSIMDIR/build/sources/smg4lib/action:$SMSIMDIR/build/sources/smg4lib/construction:$SMSIMDIR/build/sources/smg4lib/physics:$SMSIMDIR/build/sources/sim_deuteron:$TARTSYS/lib
else
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SMSIMDIR/build/sources/smg4lib/data:$SMSIMDIR/build/sources/smg4lib/action:$SMSIMDIR/build/sources/smg4lib/construction:$SMSIMDIR/build/sources/smg4lib/physics:$SMSIMDIR/build/sources/sim_deuteron:$TARTSYS/lib
fi

if [[ $LIBRARY_PATH == "" ]]; then
    export LIBRARY_PATH=$SMSIMDIR/build/sources/smg4lib/data:$SMSIMDIR/build/sources/smg4lib/action:$SMSIMDIR/build/sources/smg4lib/construction:$SMSIMDIR/build/sources/smg4lib/physics
else
    export LIBRARY_PATH=$LIBRARY_PATH:$SMSIMDIR/build/sources/smg4lib/data:$SMSIMDIR/build/sources/smg4lib/action:$SMSIMDIR/build/sources/smg4lib/construction:$SMSIMDIR/build/sources/smg4lib/physics
fi

if [[ $ROOT_INCLUDE_PATH == "" ]]; then
    export ROOT_INCLUDE_PATH=$SMSIMDIR/sources/smg4lib/data/include:$TARTSYS/include
else
    export ROOT_INCLUDE_PATH=$ROOT_INCLUDE_PATH:$SMSIMDIR/sources/smg4lib/data/include:$TARTSYS/include
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
