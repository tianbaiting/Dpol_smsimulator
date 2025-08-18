#!/bin/bash
#------> added for setting up the GEANT4
# source /usr/local/lib64/geant4.10.05.p01/share/Geant4-10.5.1/geant4make/geant4make.sh
source /home/tbt/Software/geant4/install/share/Geant4/geant4make/geant4make.sh
#source /usr/local/lib64/geant4/share/Geant4-10.6.1/geant4make/geant4make.sh
#export TARTSYS=/home/togano/tool/anaroot
# export TARTSYS=/home/kondo/work/geant/anaroot/anaroot_v4.5.36
export TARTSYS=/home/tbt/Software/anaroot/install

export SMSIMDIR=/home/tbt/workspace/dpol/smsimulator5.5


export PATH=$PATH:$SMSIMDIR/bin
export G4SMLIBDIR=$SMSIMDIR/smg4lib

export G4SMPHYSICSDIR=$SMSIMDIR/smg4lib/physics
export G4SMACTIONDIR=$SMSIMDIR/smg4lib/action
export G4SMCONSTRUCTIONDIR=$SMSIMDIR/smg4lib/construction
export G4SMDATADIR=$SMSIMDIR/smg4lib/data

export G4SMCPPFLAGS="-I$G4SMLIBDIR/include"
export G4SMLDLIBS="-lsmphysics -lsmaction -lsmconstruction -lsmdata"


#    echo $LD_LIBRARY_PATH
#    echo
#    echo $G4SMLIBDIR
#    echo
#    echo $SMSIMDIR


if [[ $LD_LIBRARY_PATH == "" ]]; then
    export LD_LIBRARY_PATH=$G4SMLIBDIR/lib:$SMSIMDIR/lib:$TARTSYS/lib
else
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$G4SMLIBDIR/lib:$SMSIMDIR/lib:$TARTSYS/lib
fi

if [[ $LIBRARY_PATH == "" ]]; then
    export LIBRARY_PATH=$G4SMLIBDIR/lib
else
    export LIBRARY_PATH=$LIBRARY_PATH:$G4SMLIBDIR/lib
fi

if [[ $ROOT_INCLUDE_PATH == "" ]]; then
    export ROOT_INCLUDE_PATH=$G4SMLIBDIR/include:$TARTSYS/include
else
    export ROOT_INCLUDE_PATH=$ROOT_INCLUDE_PATH:$G4SMLIBDIR/include::$TARTSYS/include
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
