#include "VacuumUpstreamConstruction.hh"

#include "G4Box.hh"
#include "G4Trap.hh"
#include "G4UnionSolid.hh"
#include "G4SubtractionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4PVReplica.hh"
#include "G4RotationMatrix.hh"

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
VacuumUpstreamConstruction::VacuumUpstreamConstruction()
{
  fVacuumMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
}
//______________________________________________________________________________________
VacuumUpstreamConstruction::~VacuumUpstreamConstruction()
{;}
//______________________________________________________________________________________
G4VPhysicalVolume* VacuumUpstreamConstruction::Construct()
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
  G4LogicalVolume *LogicWorld = new G4LogicalVolume(solidWorld,fVacuumMaterial,"World");
  G4ThreeVector worldPos(0, 0, 0);  
  G4VPhysicalVolume* world = new G4PVPlacement(0,
					       worldPos,
					       LogicWorld,
					       "World",
					       0,
					       false,
					       0);

//  //---------------------------------------------------
//  // Construct VacuumUpstream LV
//  G4LogicalVolume *LogicVacuumUpstream = ConstructSub();
//
//  // position
//  G4ThreeVector pos(0,0,0);
//  G4double phi = 0*deg;
//  G4RotationMatrix rm; rm.rotateY(phi);
//  new G4PVPlacement(G4Transform3D(rm,pos),
//		    LogicVacuumUpstream,"VacuumUpstream",LogicWorld,false,0);
//
//  //  LogicVacuumUpstream->SetVisAttributes(G4VisAttributes::Invisible);
//  //  LogicWorld->SetVisAttributes(G4VisAttributes::Invisible);
//
//  G4cout <<* (G4Material::GetMaterialTable()) << G4endl;

  return world;
}
//______________________________________________________________________________________
G4LogicalVolume* VacuumUpstreamConstruction::ConstructSub(G4VPhysicalVolume *dipole_phys)
{
  std::cout << "Creating VacuumUpstream" << std::endl;

  G4double vacuum_dx = 1200*0.5*mm;
  G4double vacuum_dy = 1000*0.5*mm;
  G4double vacuum_dz = 5000*0.5*mm;
  fPosition = G4ThreeVector(0,0,-vacuum_dz);

  G4Box* vacuum_box = new G4Box("vacuum_box",vacuum_dx,vacuum_dy,vacuum_dz);
  G4VSolid *subt = dipole_phys->GetLogicalVolume()->GetSolid();
  G4RotationMatrix *rm = dipole_phys->GetObjectRotation();
  rm->invert();
  G4SubtractionSolid *vacuum = new G4SubtractionSolid("vacuum_upstream",vacuum_box,subt,
						      rm,-fPosition);
  G4LogicalVolume *vacuum_log = new G4LogicalVolume(vacuum,fVacuumMaterial,"vacuum_upstream_log");
  vacuum_log->SetVisAttributes(new G4VisAttributes(G4Colour::Gray()));

  return vacuum_log;
}
//______________________________________________________________________________________
