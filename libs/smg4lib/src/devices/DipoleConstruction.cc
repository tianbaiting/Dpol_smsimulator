#include "DipoleConstruction.hh"
#include "DipoleConstructionMessenger.hh"

#include "SimDataManager.hh"

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

#include "TFile.h"
#include "TH1.h"
#include "TH2.h"

#include "G4NistManager.hh"
#include "G4SystemOfUnits.hh"//Geant4.10
//______________________________________________________________________________________
DipoleConstruction::DipoleConstruction()
  : fAngle(0), fLogicDipole(0),
    fMagField(0), fMagFieldFactor(1.0)
{
  fWorldMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  fDipoleMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Fe");
  fVacuumMaterial = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
  new DipoleConstructionMessenger(this);
}
//______________________________________________________________________________________
DipoleConstruction::~DipoleConstruction()
{;}
//______________________________________________________________________________________
G4VPhysicalVolume* DipoleConstruction::Construct()
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
  // Construct Dipole LV
  ConstructSub();

  // dipole position
  G4ThreeVector pYoke(0,0,0);
  G4double phiYoke = 0*deg;
  G4RotationMatrix yoke_rm; yoke_rm.rotateY(phiYoke);
  new G4PVPlacement(G4Transform3D(yoke_rm,pYoke),
		    fLogicDipole,"yoke",LogicWorld,false,0);

  //  fLogicDipole->SetVisAttributes(G4VisAttributes::Invisible);
  //  LogicWorld->SetVisAttributes(G4VisAttributes::Invisible);

  G4cout <<* (G4Material::GetMaterialTable()) << G4endl;

  return world;
}
//______________________________________________________________________________________
G4LogicalVolume* DipoleConstruction::ConstructSub()
{
  std::cout << "Creating Dipole" << std::endl;

  //------------------------------ yoke
  G4double yoke_x = 6700*0.5*mm;
  G4double yoke_y = 4640*0.5*mm;
  G4double yoke_z = 3500*0.5*mm;
  G4Box* yoke_box = new G4Box("yoke_box",yoke_x,yoke_y,yoke_z);
  fLogicDipole = new G4LogicalVolume(yoke_box,fDipoleMaterial,"yoke_log");

  //------------------------------- vacuum chamber inside yoke
  G4double vchamber_x = 1.62*m;
  G4double vchamber_y = 0.4*m;
  G4double vchamber_z = 3.5*0.5*m;
  G4double vchamber_zz = 621.1537 *mm; // from center of yoke to trap
  G4Box* vchamber_box = new G4Box("vchamber_box",vchamber_x,vchamber_y,vchamber_z);

  G4double vchamber_xx = 2.57*m;
  G4double vchamber_yy = (vchamber_z - vchamber_zz) * 0.5;
  G4Trap* vchamber_trap = new G4Trap("vchamber_trap",vchamber_y,0,0,
				     vchamber_yy,vchamber_x,vchamber_xx,0,
				     vchamber_yy,vchamber_x,vchamber_xx,0);
  G4RotationMatrix *vct_rm = new G4RotationMatrix(); vct_rm->rotateX(-90.*deg);
  G4UnionSolid* vchamber_sol = new G4UnionSolid("vchamber_sol",vchamber_box,vchamber_trap,vct_rm,G4ThreeVector(0,0,vchamber_zz+(vchamber_z-vchamber_zz)*0.5));

  G4LogicalVolume *vchamber_log = new G4LogicalVolume(vchamber_sol,fVacuumMaterial,"vchamber_log");
  //vchamber_log->SetVisAttributes(new G4VisAttributes(G4Colour::Yellow()));
  G4ThreeVector pVchamber(0,0,0);
  new G4PVPlacement(0,
		    G4ThreeVector(0,0,0),
		    vchamber_log,
		    "vchamber",
		    fLogicDipole,
		    false,
		    0);
		    
  fLogicDipole->SetVisAttributes(new G4VisAttributes(G4Colour::Blue()));
  vchamber_log->SetVisAttributes(new G4VisAttributes(G4Colour::Gray()));


  return fLogicDipole;
}
//______________________________________________________________________________________
G4VPhysicalVolume* DipoleConstruction::PutSAMURAIMagnet(G4LogicalVolume* ExpHall_log, G4double angle)
{
  SetAngle(angle);// SAMURAI def -> Geant4 def
  G4VPhysicalVolume *mag_phys = PutSAMURAIMagnet(ExpHall_log);
  return mag_phys;
}
//______________________________________________________________________________________
G4VPhysicalVolume* DipoleConstruction::PutSAMURAIMagnet(G4LogicalVolume* ExpHall_log)
{
  G4ThreeVector pYoke(0,0,0);
  G4double phiYoke = -GetAngle();// SAMURAI def -> Geant def
  G4RotationMatrix yoke_rm; yoke_rm.rotateY(phiYoke);
  G4VPhysicalVolume *yoke_phys = 
    new G4PVPlacement(G4Transform3D(yoke_rm,pYoke),fLogicDipole,"yoke",ExpHall_log,false,0);
  return yoke_phys;
}
//______________________________________________________________________________________
#include "G4FieldManager.hh"
#include "G4ChordFinder.hh"
#include "G4TransportationManager.hh"
void DipoleConstruction::SetMagField(G4String filename)
{
  //apply a global uniform magnetic field along Z axis

  G4FieldManager* fieldMgr
    = G4TransportationManager::GetTransportationManager()->GetFieldManager();

  if(fMagField!=0) delete fMagField;         //delete the existing magn field

  fMagField = new MagField();
  fMagField->LoadMagneticField(filename);
  fMagField->SetMagAngle(fAngle);
  fMagField->SetFieldFactor(fMagFieldFactor);

  fieldMgr->SetDetectorField(fMagField);
  fieldMgr->CreateChordFinder(fMagField);
  // step for magnetic field integration
  fieldMgr->GetChordFinder()->SetDeltaChord(0.001*mm); // 10 micron m order accuracy

  // Header information
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  std::stringstream ss;
  ss<<"MagFile="<<filename.data();
  sman->AppendHeader(ss.str().c_str());

}
//______________________________________________________________________________________
void DipoleConstruction::SetAngle(G4double val)
{
  if (fMagField!=0) fMagField->SetMagAngle(fAngle);
  fAngle = val;

  // Header information
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  std::stringstream ss;
  ss<<"MagAngle="<<GetAngle()/deg<<"deg";
  sman->AppendHeader(ss.str().c_str());

}
//______________________________________________________________________________________
void DipoleConstruction::SetMagFieldFactor(double factor)
{
  fMagFieldFactor = factor;
  if (fMagField!=0) fMagField->SetFieldFactor(fMagFieldFactor);
  //std::cout<<"SimtraceDetectorConstruction : Set Magnet field factor to "<<factor<<std::endl;
}
//______________________________________________________________________________________
void DipoleConstruction::PlotMagField()
{
  if (fMagField==0) return;
  std::cout<<"DipoleConstruction : PlotMagField"<<std::endl;
  TFile *file = new TFile("MagField.root","recreate");
  Int_t nx=200,nz=200;
  G4double xmax=5000*mm,zmax=5000*mm;
  TH2D* hmfield_zx = new TH2D("hmfield_zx","Bz Z-X Y=0",nz,-zmax/mm,zmax/mm, nx,-xmax/mm,xmax/mm);

  for (Int_t iz=0;iz<nz;iz++){
    for (Int_t ix=0;ix<nx;ix++){
      G4double R[4]={xmax*(2.0*ix-nx)/nx,
		     0,
		     zmax*(2.0*iz-nz)/nz,
		     0};
      G4double Blab[3]={0,0,0};
      fMagField->GetFieldValue(R,Blab);
//      if (Blab[1]/tesla>0.1)
//      std::cout<<R[0]/mm<<" "
//	       <<R[1]/mm<<" "
//	       <<R[2]/mm<<" "
//	       <<Blab[0]/tesla<<" "
//	       <<Blab[1]/tesla<<" "
//	       <<Blab[2]/tesla<<" "
//	       <<std::endl;
      Double_t x = R[2]/mm;
      Double_t y = R[0]/mm;
      Double_t w = Blab[1]/tesla;
      hmfield_zx->Fill(x,y,w);
    }
  }
  file->Write();
  file->Close();
  std::cout<<"DipoleConstruction : Magnetic field is plotted in MagField.root\n"<<std::endl;

}
//______________________________________________________________________________________
