// construction for the first version of the Vacuum Exit Window for charged 
// particle (Y aperture is 40cm) 
#include "ExitWindowC1Construction.hh"

#include "G4Box.hh"
#include "G4Trap.hh"
#include "G4UnionSolid.hh"
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
//______________________________________________________________________________________
ExitWindowC1Construction::ExitWindowC1Construction()
  : fLogicExitWindowC1(0), 
    fAngle(0), 
    fPosition(170.51*mm,0,3187.46*mm)// measured in Dayone exp.
{
  fWorldMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  fFlangeMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Fe");
  fVacuumMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");

  fWindow_flange_dx = 3340.25* mm;
  fWindow_flange_dy = 1200   * mm;
  fWindow_flange_dz =   30   * mm;
  fWindow_hole_dx = 2800 * mm;
  fWindow_hole_dy =  400 * mm;
  fWindow_hole_dz =   30 * mm;
}
//______________________________________________________________________________________
ExitWindowC1Construction::~ExitWindowC1Construction()
{;}
//______________________________________________________________________________________
G4VPhysicalVolume* ExitWindowC1Construction::Construct()
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
  // Construct ExitWindowC1 LV
  fLogicExitWindowC1 = ConstructSub();

  // position of Window flange at 0deg
  G4ThreeVector Pos_lab = fPosition;
  G4RotationMatrix rm; rm.rotateY(-fAngle);
  Pos_lab.rotateY(-fAngle);
  new G4PVPlacement(G4Transform3D(rm,Pos_lab),
		    fLogicExitWindowC1,"window_flange",LogicWorld,false,0);


  //  LogicWorld->SetVisAttributes(G4VisAttributes::Invisible);

  G4cout <<* (G4Material::GetMaterialTable()) << G4endl;

  return world;
}
//______________________________________________________________________________________
G4LogicalVolume* ExitWindowC1Construction::ConstructSub()
{
  std::cout << "Creating ExitWindowC1" << std::endl;

  //------------------------------ exit window for charged particles (dy=40cm)
  G4Box* window_flange_box = new G4Box("window_flange_box",
                                       0.5*fWindow_flange_dx,
				       0.5*fWindow_flange_dy,
				       0.5*fWindow_flange_dz);
  G4LogicalVolume* window_flange_log = new G4LogicalVolume(window_flange_box,fVacuumMaterial,"window_flange_log");

//  window_flange_log->SetVisAttributes(new G4VisAttributes(false));// invisible

  G4Box* window_hole_box = new G4Box("window_hole_box",
                                     0.5*fWindow_hole_dx,
				     0.5*fWindow_hole_dy,
				     0.5*fWindow_hole_dz);

  fWindowHole_log = new G4LogicalVolume(window_hole_box,fWorldMaterial,"window_hole_log");
//  new G4PVPlacement(0,G4ThreeVector(0,0,0),fWindowHole_log,"WinC1_Hole_log",window_flange_log,false,0);
  new G4PVPlacement(0,G4ThreeVector(0,0,0),fWindowHole_log,"WinC1_Hole",window_flange_log,false,0);

  fWindowHole_log->SetVisAttributes(new G4VisAttributes(G4Colour::Cyan()));
  //window_flange_log->SetVisAttributes(G4VisAttributes::Invisible);

  return window_flange_log;
}
//______________________________________________________________________________________
