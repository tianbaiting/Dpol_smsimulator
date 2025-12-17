#include "VacuumDownstreamConstruction.hh"

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
VacuumDownstreamConstruction::VacuumDownstreamConstruction()
{
  fVacuumMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
}
//______________________________________________________________________________________
VacuumDownstreamConstruction::~VacuumDownstreamConstruction()
{;}
//______________________________________________________________________________________
G4VPhysicalVolume* VacuumDownstreamConstruction::Construct()
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
//  // Construct VacuumDownstream LV
//  G4LogicalVolume *LogicVacuumDownstream = ConstructSub();
//
//  // position
//  G4ThreeVector pos(0,0,0);
//  G4double phi = 0*deg;
//  G4RotationMatrix rm; rm.rotateY(phi);
//  new G4PVPlacement(G4Transform3D(rm,pos),
//		    LogicVacuumDownstream,"VacuumDownstream",LogicWorld,false,0);
//
//  //  LogicVacuumDownstream->SetVisAttributes(G4VisAttributes::Invisible);
//  //  LogicWorld->SetVisAttributes(G4VisAttributes::Invisible);
//
//  G4cout <<* (G4Material::GetMaterialTable()) << G4endl;

  return world;
}
//______________________________________________________________________________________
G4LogicalVolume* VacuumDownstreamConstruction::ConstructSub(G4VPhysicalVolume *dipole_phys,
							    G4VPhysicalVolume *flange_phys)
{
  std::cout << "Creating VacuumDownstream" << std::endl;

  G4RotationMatrix *dipole_rm = dipole_phys->GetObjectRotation();
  dipole_rm->invert();
  G4double dipole_angle = dipole_rm->theta()*rad;

  G4ThreeVector flange_pos = flange_phys->GetObjectTranslation();
  G4RotationMatrix *flange_rm = flange_phys->GetObjectRotation();
  flange_rm ->invert();
  G4double flange_angle = flange_rm->theta()*rad;

  G4ThreeVector flange_pos0 = flange_pos;
  flange_pos0.rotateY(flange_angle);

  G4RotationMatrix rm;
  rm.rotateY(dipole_angle-flange_angle);

  // size is not exact
  G4double vacuum_dx = 3000*0.5*mm;
  G4double vacuum_dy = 1000*0.5*mm;
  G4double vacuum_dz = 2000*0.5*mm;
  G4double flange_dz = 30*mm;
  G4ThreeVector PosFromFlange0 = G4ThreeVector(flange_pos0.x(),0,flange_pos0.z()-vacuum_dz-0.5*flange_dz);
  fPosition = PosFromFlange0;
  fPosition.rotateY(-flange_angle);

  G4Box* vacuum_box = new G4Box("vacuum_box",vacuum_dx,vacuum_dy,vacuum_dz);
  G4VSolid *subt = dipole_phys->GetLogicalVolume()->GetSolid();
  G4SubtractionSolid *vacuum_sub_dipole = new G4SubtractionSolid("vacuum_upstream",vacuum_box,subt,
								 &rm,-PosFromFlange0);

  // Vacuum extention, size is not exact
  G4ThreeVector subt2_size(900*mm, 600*mm, 2000*mm);
  G4VSolid *subt2 = new G4Box("subt2",subt2_size.x(), subt2_size.y(), subt2_size.z());
  G4ThreeVector subt2_pos0(vacuum_dx + 0.5*subt2_size.x(),
			   0, 0);
  G4ThreeVector subt2_pos = subt2_pos0;
  subt2_pos.rotateY(-flange_angle);
  G4ThreeVector dpos(0.5*subt2_size.x(),0,-0.5*subt2_size.z());
  G4SubtractionSolid *vacuum_solid = new  G4SubtractionSolid("vacuum_upstream",vacuum_sub_dipole,subt2,
							     &rm,subt2_pos0);
  G4LogicalVolume *vacuum_log = new G4LogicalVolume(vacuum_solid,fVacuumMaterial,"vacuum_upstream_log");

  vacuum_log->SetVisAttributes(new G4VisAttributes(G4Colour::Gray()));

  return vacuum_log;
}
//______________________________________________________________________________________
