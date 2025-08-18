#!/usr/bin/perl

@Layer;
@SubLayer;
@DetType;
@PosX;
@PosY;
@PosZ;
@SizX;
@SizY;
@SizZ;
@AngleX;
@AngleY;
@AngleZ;

$NumNeut = 240;
$NumVeto = 48;
$NumDetec = $NumNeut+$NumVeto;

# Plastic size of  Neut and Veto
$Siz_n_x = 120;
$Siz_n_y = 1800;
$Siz_n_z = 120;
$Siz_v_x = 320;
$Siz_v_y = 1900;
$Siz_v_z = 10;

$VetoNumLayer = 12;
$NeutNumSubLayer = 30;
$VetoNumLayer = 12;
$NeutIDCenter = 15.5;
$VetoIDCenter = 6.5;
$DistLayer = 850;
$DistVetoNeut = 260;
$DistNeutNeutX = 0.0;
$DistNeutNeutZ = 10;
$DistVetoVetoZ = 15;
$VetoOverWrapX = 10;

$WholeShiftX = 127;
$WholeShiftY = 0;
$WholeShiftZ = 0;
#$WholeShiftZ = 1111.7;
$NeutWholeShiftZ = 0;
$RotAngleX = 0;
$RotAngleY = 50;
$RotAngleZ = 0;

$ID=1;
$iSubLayerSeq=1;

#for Neutron Detector (ID=1:240)
while($iSubLayerSeq<9){
    for ($ineut=1;$ineut<=$NeutNumSubLayer;++$ineut){
	$Layer[$ID] = ($iSubLayerSeq+1)/2;
	$Layer[$ID] = int $Layer[$ID];
	$SubLayer[$ID] = ($iSubLayerSeq+1)%2+1;
	$DetType[$ID] = "Neut";
	$PosX[$ID] = -($ineut-$NeutIDCenter)*($Siz_n_x+$DistNeutNeutX);
	$PosY[$ID] = 0;
	$PosZ[$ID] = ($Layer[$ID]-1)*$DistLayer;
	if ($SubLayer[$ID]==2){$PosZ[$ID] += $Siz_n_z + $DistNeutNeutZ;}
	$SizX[$ID] = $Siz_n_x;
	$SizY[$ID] = $Siz_n_y;
	$SizZ[$ID] = $Siz_n_z;
	$RotAngleX[$ID] = $RotAngleX;
	$RotAngleY[$ID] = $RotAngleY;
	$RotAngleZ[$ID] = $RotAngleZ;
	$ID++;
    }
    $iSubLayerSeq++;
}

#for Veto Detector (ID=241:288)
$iLayer = 1;
while($iLayer<5){
    for ($iveto=1;$iveto<=$VetoNumLayer;++$iveto){
	$Layer[$ID]  = $iLayer;
	$Sublayer[$ID] = 0;
	$DetType[$ID] = "Veto";
	$PosX[$ID] = -($iveto-$VetoIDCenter)*($Siz_v_x-$VetoOverWrapX);
	$PosY[$ID] = 0;
	$PosZ[$ID] = ($iLayer-1)*$DistLayer - $DistVetoNeut;
	if ($ID%2==1) {$PosZ[$ID] -= $DistVetoVetoZ;}
	$SizX[$ID] = $Siz_v_x;
	$SizY[$ID] = $Siz_v_y;
	$SizZ[$ID] = $Siz_v_z;
	$RotAngleX[$ID] = $RotAngleX;
	$RotAngleY[$ID] = $RotAngleY;
	$RotAngleZ[$ID] = $RotAngleZ;
	$ID++;
    }
    $iLayer++;
}

for ($i=1;$i<$NumDetec+1;++$i){
    $PosX[$i] += $WholeShiftX;
    $PosY[$i] += $WholeShiftY;
    $PosZ[$i] += $WholeShiftZ;
}

print "ID, DetectorType, Layer, SubLayer, PositionX, PositionY, PositionZ, AngleX, AngleY, AngleZ,\n";
for ($i=1;$i<$NumDetec+1;++$i){
    printf("%3i, %s, %2i, %2i, %8.2f, %8.1f, %8.1f, %5.1f, %5.1f, %5.1f,\n",
	   $i,$DetType[$i], $Layer[$i],$SubLayer[$i],
	   $PosX[$i],$PosY[$i],$PosZ[$i],
	   $RotAngleX[$i],$RotAngleY[$i],$RotAngleZ[$i]
	);
}
