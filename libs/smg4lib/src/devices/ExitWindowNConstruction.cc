// construction for the first version of the Vacuum Exit Window for charged 
// particle (Y aperture is 40cm) 
#include "ExitWindowNConstruction.hh"

#include "G4Box.hh"
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
//______________________________________________________________________________________
ExitWindowNConstruction::ExitWindowNConstruction()
  : fExitWindow_log(0), fFlange_log(0), fAngle(0),
    fPosition(1356.69*mm,0,2028.3*mm)// Dayone value
{
  fWorldMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  fNeutronWindowMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Fe");
  fVacuumMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");

  fWindow_flange_dx = 2730.25*0.5*mm;
  fWindow_flange_dy = 1150*0.5*mm;
  fWindow_flange_dz = 30*0.5*mm;
  fWindow_hole_dx = 2430*0.5*mm;
  fWindow_hole_dy = 800*0.5*mm;
  fWindow_hole_dz = 3*0.5*mm;
}
//______________________________________________________________________________________
ExitWindowNConstruction::~ExitWindowNConstruction()
{;}
//______________________________________________________________________________________
G4VPhysicalVolume* ExitWindowNConstruction::Construct()
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
  // Construct ExitWindowN LV
  ConstructSub();

  // position of Window flange at 0deg
  G4ThreeVector Pos_lab = fPosition;
  G4RotationMatrix rm; rm.rotateY(-fAngle);
  Pos_lab.rotateY(-fAngle);
  new G4PVPlacement(G4Transform3D(rm,Pos_lab),
		    fExitWindow_log,"window_flange",LogicWorld,false,0);


  //  fExitWindow_log->SetVisAttributes(G4VisAttributes::Invisible);
  //  LogicWorld->SetVisAttributes(G4VisAttributes::Invisible);

  G4cout <<* (G4Material::GetMaterialTable()) << G4endl;

  return world;
}
//______________________________________________________________________________________
G4LogicalVolume* ExitWindowNConstruction::ConstructSub()
{
  std::cout << "Creating ExitWindowN" << std::endl;

  // neutron window LV

  G4VSolid* flange = new G4Box("Flange",0.5*2730*mm,0.5*1150*mm,0.5*30*mm);
  G4VSolid* box = new G4Box("WindowBox",0.5*2490*mm,0.5*860*mm,0.5*130*mm);
  G4VSolid* hole = new G4Box("WindowHole",0.5*2430*mm,0.5*800*mm,0.5*400*mm);// needs extra volume for subtraction
  G4VSolid* flange_box = new G4UnionSolid("Flange+WindowBox",flange,box,
						 0,G4ThreeVector(-25*mm,0,80*mm));
  G4VSolid* flange_box_hole = new G4SubtractionSolid("ExitWindowN",flange_box,hole,
						0,G4ThreeVector(-25*mm,0,0));
  fFlange_log = new G4LogicalVolume(flange_box_hole,fNeutronWindowMaterial,"NeutronWindowFlange");  

  // exit window
  G4ThreeVector neutronWindowSize(fWindow_hole_dx, 
				  fWindow_hole_dy, 
				  fWindow_hole_dz);
  fExitWindowNSize = neutronWindowSize;
  G4Box* solidNeutronWindow = new G4Box("NeutronWindowBox",
                                        neutronWindowSize.x(),
                                        neutronWindowSize.y(),
                                        neutronWindowSize.z());
  fExitWindow_log = new G4LogicalVolume(solidNeutronWindow,fNeutronWindowMaterial,"NeutronWindow");
  fExitWindow_log->SetVisAttributes(new G4VisAttributes(G4Colour::Cyan()));

  return fExitWindow_log;

}
//______________________________________________________________________________________
void ExitWindowNConstruction::PutExitWindow(G4LogicalVolume* expHall_log)
{
  G4RotationMatrix mag_rm; mag_rm.rotateY(-fAngle);
  G4ThreeVector ExitWindowPos = fPosition;
  ExitWindowPos.rotateY(-fAngle);
  new G4PVPlacement(G4Transform3D(mag_rm,ExitWindowPos),
		    fExitWindow_log,"WinN",expHall_log,false,0);

  G4ThreeVector FlangePos(25*mm,0,-145*mm);
  FlangePos+=fPosition;
  FlangePos.rotateY(-fAngle);
  new G4PVPlacement(G4Transform3D(mag_rm,FlangePos),
		    fFlange_log,"WinN_flange",expHall_log,false,0);
}
//______________________________________________________________________________________
