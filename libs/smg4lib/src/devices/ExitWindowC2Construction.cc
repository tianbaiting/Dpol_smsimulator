// construction for the second version of the Vacuum Exit Window for charged 
// particle (Y aperture is 80cm) 
#include "ExitWindowC2Construction.hh"

#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4Trap.hh"
#include "G4UnionSolid.hh"
#include "G4SubtractionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4PVReplica.hh"

#include "G4GeometryManager.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4SolidStore.hh"

#include "G4VisAttributes.hh"
#include "G4Colour.hh"

#include "G4UnitsTable.hh"
#include "G4ios.hh"
#include "G4ThreeVector.hh"

#include "G4NistManager.hh"

#include "G4SystemOfUnits.hh"//Geant4.10
#include "G4PhysicalConstants.hh"
//______________________________________________________________________________________
ExitWindowC2Construction::ExitWindowC2Construction()
  : fLogicExitWindowC2(0), 
    fAngle(0), 
    fPosition(170.51*mm,0,3187.46*mm)// pos @ mag30deg, measured in Dayone exp.
{
  fWorldMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  fFlangeMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Fe");
  fVacuumMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");

  fWindow_flange_dx = 3340 * mm;
  fWindow_flange_dy = 1200 * mm;
  fWindow_flange_dz =   30 * mm;
  fWindow_hole_dx = 2940 * mm;
  fWindow_hole_dy =  800 * mm;
  fWindow_hole_dz =   30 * mm;

  fWindow_tubeholder_dx = 3340 * mm;
  fWindow_tubeholder_dy =  130 * mm;
  fWindow_tubeholder_dz =  130 * mm;
}
//______________________________________________________________________________________
ExitWindowC2Construction::~ExitWindowC2Construction()
{;}
//______________________________________________________________________________________
G4VPhysicalVolume* ExitWindowC2Construction::Construct()
{
  // Cleanup old geometry
  G4GeometryManager::GetInstance()->OpenGeometry();
  G4PhysicalVolumeStore::GetInstance()->Clean();
  G4LogicalVolumeStore::GetInstance()->Clean();
  G4SolidStore::GetInstance()->Clean();

  //---------------------------------------------------
  // World LV
  G4ThreeVector worldSize(10*m,10*m,10*m);
  G4Box* solidWorld = new G4Box("World",
				worldSize.x()/2,
				worldSize.y()/2,
				worldSize.z()/2);
  G4LogicalVolume *LogicWorld = new G4LogicalVolume(solidWorld,fWorldMaterial,"World");
  G4ThreeVector worldPos(0, 0, 0);  
  G4VPhysicalVolume* world = new G4PVPlacement(0,
					       worldPos,
					       LogicWorld,
					       "World",
					       0,
					       false,
					       0);

  //---------------------------------------------------
  // Construct ExitWindowC2 LV
  fLogicExitWindowC2 = ConstructSub();

  // position of Window flange at 0deg
  G4ThreeVector Pos_lab = fPosition;
  G4RotationMatrix rm; rm.rotateY(-fAngle);
  Pos_lab.rotateY(-fAngle);
  new G4PVPlacement(G4Transform3D(rm,Pos_lab),
		    fLogicExitWindowC2,"window_flange",LogicWorld,false,0);


  //  LogicWorld->SetVisAttributes(G4VisAttributes::Invisible);

  G4cout <<* (G4Material::GetMaterialTable()) << G4endl;

  return world;
}
//______________________________________________________________________________________
G4LogicalVolume* ExitWindowC2Construction::ConstructSub()
{
  std::cout << "Creating ExitWindowC2" << std::endl;

  //------------------------------ exit window for charged particles (dy=80cm)
  G4Box* window_flange_box = new G4Box("windowc2_flange_box",
                                       0.5*fWindow_flange_dx,
				       0.5*fWindow_flange_dy,
				       0.5*fWindow_flange_dz);

  G4Box* window_tubeholder_box = new G4Box("windowc2_tubeholder_box",
					   0.5*fWindow_tubeholder_dx,
					   0.5*fWindow_tubeholder_dy,
					   0.5*fWindow_tubeholder_dz );
  G4VSolid* window_hole_box = new G4Box("windowc2_hole_box",
					0.5*fWindow_hole_dx,
					0.5*fWindow_hole_dy,
					0.5*fWindow_hole_dz);

  G4VSolid* window_flng_hldr = new G4UnionSolid("windowc2_flng_hldr",window_flange_box,window_tubeholder_box,0,G4ThreeVector(0,465*mm,0.5*fWindow_flange_dz+0.5*fWindow_tubeholder_dz) );
  G4VSolid* window_flng_hldr2 = new G4UnionSolid("windowc2_flng_hldr2",window_flng_hldr,window_tubeholder_box,0,G4ThreeVector(0,-465*mm,0.5*fWindow_flange_dz+0.5*fWindow_tubeholder_dz) );
  G4VSolid* window_flng_hldr2_hole = new G4SubtractionSolid("windowc2_flng_hldr2_hole",window_flng_hldr2,window_hole_box);

  fWindowFlange_log = new G4LogicalVolume(window_flng_hldr2_hole,fFlangeMaterial,"ExitWindowC2_log");
  fWindowHole_log = new G4LogicalVolume(window_hole_box,fVacuumMaterial,"ExitWindowC2Hole_log");

//  window_flange_log->SetVisAttributes(new G4VisAttributes(false));// invisible
//  //fWindowHole_log->SetVisAttributes(new G4VisAttributes(false));// invisible

  return fWindowFlange_log;
}
//______________________________________________________________________________________
void ExitWindowC2Construction::PutExitWindow(G4LogicalVolume* expHall_log)
{
  G4RotationMatrix mag_rm; mag_rm.rotateY(fAngle);
  G4ThreeVector FlangePos = fPosition;
  FlangePos.rotateY(fAngle);
  new G4PVPlacement(G4Transform3D(mag_rm,FlangePos),
				       fWindowFlange_log,"WinC2",expHall_log,
				       false,0);

  G4ThreeVector WindowHolePos = fPosition;
  WindowHolePos.rotateY(fAngle);
  fWindowHole_phys = new G4PVPlacement(G4Transform3D(mag_rm,WindowHolePos),
				       fWindowHole_log,"WinC2_Hole",expHall_log,false,0);
  fWindowHole_log->SetVisAttributes(new G4VisAttributes(G4Colour::Cyan()));

}
//______________________________________________________________________________________
