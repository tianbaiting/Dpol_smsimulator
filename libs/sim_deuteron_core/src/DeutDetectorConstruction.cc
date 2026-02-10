#include "DeutDetectorConstruction.hh"
#include "DeutDetectorConstructionMessenger.hh"
#include "SMLogger.hh"

#include "G4Material.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4Trap.hh"
#include "G4UnionSolid.hh"
#include "G4SubtractionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4ThreeVector.hh"
#include "G4PVPlacement.hh"
#include "G4PVReplica.hh"
#include "G4UserLimits.hh"
#include "G4NistManager.hh"
#include "G4SDManager.hh"

#include "DipoleConstruction.hh"
#include "PDCConstruction.hh"
#include "NEBULAConstruction.hh"
#include "ExitWindowNConstruction.hh"
#include "ExitWindowC2Construction.hh"  


#include "MagField.hh"
#include "globals.hh"
#include "G4VSensitiveDetector.hh"
#include "FragmentSD.hh"
#include "NEBULASD.hh"

#include "SimDataManager.hh"
#include "TFragSimParameter.hh"

#include "G4SystemOfUnits.hh"//Geant4.10
#include "G4GeometryManager.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4SolidStore.hh"
#include "G4RunManager.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"

#include "G4UserLimits.hh"
#include "G4VModularPhysicsList.hh"
#include "G4StepLimiterPhysics.hh"
#include "DeutSteppingAction.hh"
#include "DeutTrackingAction.hh"
#include "DeutPrimaryGeneratorAction.hh"
#include "VacuumDownstreamConstruction.hh"


//______________________________________________________________________________
DeutDetectorConstruction::DeutDetectorConstruction() 
  :
  fFillAir{false}, fSetTarget{true}, fSetDump{true}, fTargetMat{"Sn"},
  fTargetPos{0,0,0}, fTargetSize{50,50,5}, fTargetAngle{0}
  // Otherwise they'd be initialized randomly
{
  SM_INFO("Constructor of DeutDetectorConstruction");
  fDetectorConstructionMessenger = new DeutDetectorConstructionMessenger(this);

  fDipoleConstruction = new DipoleConstruction();
  fPDCConstruction    = new PDCConstruction();
  fNEBULAConstruction = new NEBULAConstruction();
  fExitWindowNConstruction = new ExitWindowNConstruction();
  fNeutronWinSD = 0;
  fExitWindowC2Construction = new ExitWindowC2Construction();
  fWindowHoleSD = 0;
  fVacuumDownstreamConstruction = new VacuumDownstreamConstruction();
}
//______________________________________________________________________________
DeutDetectorConstruction::~DeutDetectorConstruction()
{
  delete fDetectorConstructionMessenger;
  delete fDipoleConstruction;
  delete fPDCConstruction;
  delete fNEBULAConstruction;
  delete fExitWindowNConstruction;
  delete fExitWindowC2Construction;
  delete fVacuumDownstreamConstruction;  
  
}
//______________________________________________________________________________
G4VPhysicalVolume* DeutDetectorConstruction::Construct()
{
  // Cleanup old geometry
  G4GeometryManager::GetInstance()->OpenGeometry();
  G4PhysicalVolumeStore::GetInstance()->Clean();
  G4LogicalVolumeStore::GetInstance()->Clean();
  G4SolidStore::GetInstance()->Clean();

  // Material List
  const auto nist = G4NistManager::Instance();
  G4Material* Air = nist->FindOrBuildMaterial("G4_AIR");
  G4Material* Water = nist->FindOrBuildMaterial("G4_WATER");
  G4Material* Vacuum = nist->FindOrBuildMaterial("G4_Galactic");
  G4Material* CsI = nist->FindOrBuildMaterial("G4_CESIUM_IODIDE");
  G4Material* Sn = nist->FindOrBuildMaterial("G4_Sn");

  G4Material* WorldMaterial;
  G4Material* TargetMaterial;
  if (fFillAir) WorldMaterial = Air;
  else          WorldMaterial = Vacuum;

  if (fTargetMat == "empty")    TargetMaterial = WorldMaterial;
  else if (fTargetMat == "Sn")  TargetMaterial = Sn;
  else if (fTargetMat == "CsI") TargetMaterial = CsI;
  else {
    TargetMaterial = WorldMaterial;
    G4ExceptionDescription ed;
    ed << "The target material \"" << fTargetMat 
       << "\" is invalid, using empty target"; 
    G4Exception(
      "DeutDetectorConstruction::Construct()", 
      "material", JustWarning, ed
    );
  }

  //--------------------------------- volumes ----------------------------------

  //------------------- experimental hall (world volume) -------------------
  //------------------ beam line along z axis

  G4double expHall_x = 7.0*m;
  G4double expHall_y = 5*0.5*m;
  G4double expHall_z = 10.0*m;

  G4Box* expHall_box
    = new G4Box{"expHall_box", expHall_x, expHall_y, expHall_z};

  G4LogicalVolume *expHall_log 
    = new G4LogicalVolume{expHall_box, WorldMaterial, "expHall_log"};

  G4VPhysicalVolume *expHall_phys
    = new G4PVPlacement{0, {0,0,0}, expHall_log, "expHall", 0, false, 0};

  // Make experimental hall invisible 
  expHall_log->SetVisAttributes(new G4VisAttributes{false});

  //---------------------------- SAMURAI Magnet ----------------------------
  //---------------------------- yoke
  fDipoleConstruction->ConstructSub();
  fDipoleConstruction->PutSAMURAIMagnet(expHall_log);

  G4VPhysicalVolume* Dipole_phys = fDipoleConstruction->PutSAMURAIMagnet(expHall_log);  // 保存返回值

  G4SDManager *SDMan = G4SDManager::GetSDMpointer();

  // prepare parameter for target and PDC positions
  auto *simDataManager = SimDataManager::GetSimDataManager();
  auto *frag_prm = (TFragSimParameter*)simDataManager \
                   ->FindParameter("FragParameter");
  if (frag_prm==0) {
    frag_prm = new TFragSimParameter("FragParameter");
    simDataManager->AddParameter(frag_prm);
  }

  //------------------------------ exit window for neutrons
  G4double magAngle = fDipoleConstruction->GetAngle();
  fExitWindowNConstruction->ConstructSub();
  fExitWindowNConstruction->SetAngle(magAngle);
  fExitWindowNConstruction->PutExitWindow(expHall_log);
  if (fNeutronWinSD==0){
    fNeutronWinSD = new FragmentSD("/NeutronWindow");
    SDMan->AddNewDetector(fNeutronWinSD);
  }
  fExitWindowNConstruction->GetWindowVolume()->SetSensitiveDetector(fNeutronWinSD);


  //-----exit window for charged particles

  //------------------------------ exit window for charged particles  
  fExitWindowC2Construction->ConstructSub();  
  //sim_samurai21: magAngle + 29.91*deg
  G4double windowAngle = -magAngle - 29.91*deg;  // 保存角度供后续使用
  fExitWindowC2Construction->SetAngle(windowAngle);  
  fExitWindowC2Construction->PutExitWindow(expHall_log);  
    
  if (fWindowHoleSD==0){  
    fWindowHoleSD = new FragmentSD("/WindowHole");  
    SDMan->AddNewDetector(fWindowHoleSD);  
  }  
  fExitWindowC2Construction->GetWindowHoleVolume()->SetSensitiveDetector(fWindowHoleSD);

  //------------------------------ Vacuum Downstream
  G4LogicalVolume *vacuum_downstream_log = 
    fVacuumDownstreamConstruction->ConstructSub(Dipole_phys, 
                                                 fExitWindowC2Construction->GetWindowHolePhys());
  G4ThreeVector vacuum_downstream_pos = fVacuumDownstreamConstruction->GetPosition();
  G4RotationMatrix vacuum_rm; 
  vacuum_rm.rotateY(windowAngle);  // 使用与exit window相同的角度
  new G4PVPlacement(G4Transform3D(vacuum_rm, vacuum_downstream_pos),
                    vacuum_downstream_log, "vacuum_downstream",
                    expHall_log, false, 0);

// ...existing code...

  //------------------------------- Beam Line ----------------------------------
  //------------------------------ Target

  if (fSetTarget) {
    G4Box* tgt_box = new G4Box {
      "tgt_box",
			fTargetSize.x()*0.5*mm,
			fTargetSize.y()*0.5*mm,
			fTargetSize.z()*0.5*mm
    };

    auto *LogicTarget = 
      new G4LogicalVolume {tgt_box, TargetMaterial, "target_log"};
    auto *LogicTargetSD = 
      new G4LogicalVolume {tgt_box, TargetMaterial, "targetSD_log"};

    new G4PVPlacement {
      nullptr, {0,0,0}, LogicTargetSD, "Target_SD",
      LogicTarget, false, 0, false
    };

    G4RotationMatrix target_rm;
    target_rm.rotateY(-fTargetAngle);
    new G4PVPlacement {
      G4Transform3D(target_rm, fTargetPos), LogicTarget, "Target",
      expHall_log, false, 0, true
    };

    if (fTargetSD==0){
      fTargetSD = new FragmentSD("/Target");
      SDMan->AddNewDetector(fTargetSD);
    }
    LogicTargetSD->SetSensitiveDetector(fTargetSD);
  }

  frag_prm->fTargetPosition.SetXYZ(
    fTargetPos.x()/mm, 
    fTargetPos.y()/mm, 
    fTargetPos.z()/mm
  );
  frag_prm->fTargetAngle = fTargetAngle;
  frag_prm->fTargetThickness = fTargetSize.z();

  //------------------------------ NEBULA
  fNEBULAConstruction->ConstructSub();
  fNEBULAConstruction->PutNEBULA(expHall_log);

  // Sensitive Detector
  if (fNEBULASD==0){
    fNEBULASD = new NEBULASD("/NEBULA");
    SDMan->AddNewDetector(fNEBULASD);
  }

  if (fNEBULAConstruction->DoesNeutExist()){
    G4LogicalVolume *neut_log = fNEBULAConstruction->GetLogicNeut();
    neut_log->SetSensitiveDetector(fNEBULASD);
  }

  if (fNEBULAConstruction->DoesVetoExist()){
    G4LogicalVolume *veto_log = fNEBULAConstruction->GetLogicVeto();
    veto_log->SetSensitiveDetector(fNEBULASD);
  }

  //------------------------------ PDCs 
  auto pdc_log = fPDCConstruction->ConstructSub();
  pdc_log->SetVisAttributes(G4Colour::Magenta());

  if (fPDCSD_U==0){
    fPDCSD_U = new FragmentSD("/PDC_U");
    SDMan->AddNewDetector(fPDCSD_U);
  }
  if (fPDCSD_X==0){
    fPDCSD_X = new FragmentSD("/PDC_X");
    SDMan->AddNewDetector(fPDCSD_X);
  }
  if (fPDCSD_V==0){
    fPDCSD_V = new FragmentSD("/PDC_V");
    SDMan->AddNewDetector(fPDCSD_V);
  }
  fPDCConstruction->fLayerU->SetSensitiveDetector(fPDCSD_U);
  fPDCConstruction->fLayerX->SetSensitiveDetector(fPDCSD_X);
  fPDCConstruction->fLayerV->SetSensitiveDetector(fPDCSD_V);
  
  // SAMURAI def. (clockwise)-> Geant def. (counterclockwise)
  G4double pdc_angle = -fPDCAngle;
  frag_prm->fPDCAngle = fPDCAngle;

  //------------------------------ PDC1
  G4RotationMatrix pdc1_rm; pdc1_rm.rotateY(pdc_angle);
  G4ThreeVector pdc1_pos_lab{fPDC1Pos}; pdc1_pos_lab.rotateY(pdc_angle);
  G4Transform3D pdc1_trans{pdc1_rm, pdc1_pos_lab};
  new G4PVPlacement{pdc1_trans, pdc_log, "PDC1", expHall_log, false, 0};

  frag_prm->fPDC1Position.SetXYZ(
    fPDC1Pos.x()/mm, 
    fPDC1Pos.y()/mm, 
    fPDC1Pos.z()/mm
  );

  //------------------------------ PDC2
  G4RotationMatrix pdc2_rm; pdc2_rm.rotateY(pdc_angle);
  G4ThreeVector pdc2_pos_lab{fPDC2Pos}; pdc2_pos_lab.rotateY(pdc_angle);
  G4Transform3D pdc2_trans{pdc2_rm, pdc2_pos_lab};
  new G4PVPlacement{pdc2_trans, pdc_log, "PDC2", expHall_log, false, 1};

  frag_prm->fPDC2Position.SetXYZ(
    fPDC2Pos.x()/mm, 
    fPDC2Pos.y()/mm, 
    fPDC2Pos.z()/mm
  );


  //------------------------------ Beam Dump
  auto box_sol = new G4Box{"Box", 2.5*m/2, 2.5*m/2, 3*m/2};
  auto opening_sol = new G4Box{"Opening", 34*cm/2, 38*cm/2, 120*cm/2};
  G4Translate3D opening_pos = {0, 25*cm, -90*cm};
  auto dump_sol = new G4SubtractionSolid{"Dump", box_sol, opening_sol, opening_pos};

  auto dump_log = new G4LogicalVolume{dump_sol, Water, "Dump"};
  G4VisAttributes dump_vis = G4Colour{0, 1, 1, 0.7};
  dump_vis.SetForceSolid(true);
  dump_log->SetVisAttributes(dump_vis);

  if (fSetDump) {
    G4RotationMatrix dump_rm; dump_rm.rotateY(-fDumpAngle);
    G4ThreeVector dump_pos_lab{fDumpPos}; dump_pos_lab.rotateY(-fDumpAngle);
    G4Transform3D dump_trans{dump_rm, dump_pos_lab};
    new G4PVPlacement{dump_trans, dump_log, "Dump", expHall_log, false, 0};
    frag_prm->fDumpAngle = -fDumpAngle;
    frag_prm->fDumpPosition.SetXYZ(
      fDumpPos.x()/mm, 
      fDumpPos.y()/mm, 
      fDumpPos.z()/mm
    );
  }

  physiWorld = expHall_phys;  // 存储世界体积

  return expHall_phys;
}
//______________________________________________________________________________
void DeutDetectorConstruction::SetTargetPos(G4ThreeVector pos)
{
  fTargetPos = pos;
  SM_INFO("DeutDetectorConstruction: Set target position at {:.2f} cm, {:.2f} cm, {:.2f} cm",
          fTargetPos.x()/cm, fTargetPos.y()/cm, fTargetPos.z()/cm);
}
//______________________________________________________________________________
void DeutDetectorConstruction::SetTargetAngle(G4double angle)
{
  fTargetAngle = angle;
  SM_INFO("DeutDetectorConstruction: Set target angle at {:.2f} deg", fTargetAngle/deg);
}

//______________________________________________________________________________
void DeutDetectorConstruction::SetPDCAngle(G4double angle)
{
  fPDCAngle = angle;
  SM_INFO("DeutDetectorConstruction: Set PDC angle at {:.2f} deg", fPDCAngle/deg);
}
//______________________________________________________________________________
void DeutDetectorConstruction::SetPDC1Pos(G4ThreeVector pos)
{
  fPDC1Pos = pos;
  SM_INFO("DeutDetectorConstruction: Set PDC1 position at {:.2f} cm, {:.2f} cm, {:.2f} cm",
          fPDC1Pos.x()/cm, fPDC1Pos.y()/cm, fPDC1Pos.z()/cm);
}
//______________________________________________________________________________
void DeutDetectorConstruction::SetPDC2Pos(G4ThreeVector pos)
{
  fPDC2Pos = pos;
  SM_INFO("DeutDetectorConstruction: Set PDC2 position at {:.2f} cm, {:.2f} cm, {:.2f} cm",
          fPDC2Pos.x()/cm, fPDC2Pos.y()/cm, fPDC2Pos.z()/cm);
}
//______________________________________________________________________________
void DeutDetectorConstruction::SetDumpAngle(G4double angle)
{
  fDumpAngle = angle;
  SM_INFO("DeutDetectorConstruction: Set beam dump angle at {:.2f} deg", fDumpAngle/deg);
}
//______________________________________________________________________________
void DeutDetectorConstruction::SetDumpPos(G4ThreeVector pos)
{
  fDumpPos = pos;
  SM_INFO("DeutDetectorConstruction: Set beam dump position at {:.2f} cm, {:.2f} cm, {:.2f} cm",
          fDumpPos.x()/cm, fDumpPos.y()/cm, fDumpPos.z()/cm);
}
//______________________________________________________________________________
void DeutDetectorConstruction::UpdateGeometry()
{
  G4RunManager::GetRunManager()->DefineWorldVolume(Construct());
  SM_INFO("DeutDetectorConstruction: SAMURAI Geometry is updated");
}
//______________________________________________________________________________
void DeutDetectorConstruction::AutoConfigGeometry(G4String outputMacroFile)
{
  SetTarget(true), SetFillAir(false), UpdateGeometry();
  
  G4RunManager* runManager = G4RunManager::GetRunManager();
  auto steppingAction = new DeutSteppingAction;
  auto trackingAction = new DeutTrackingAction;
  runManager->SetUserAction(steppingAction);
  runManager->SetUserAction(trackingAction);
  runManager->Initialize();

  auto logicalVolumeStore = G4LogicalVolumeStore::GetInstance();
  G4LogicalVolume* expHall_log = 
    logicalVolumeStore->GetVolume("expHall_log");

  G4UserLimits stepLimit{0.1}; // Max step length: 0.1mm
  expHall_log->SetUserLimits(&stepLimit);

  auto stepLimitPhys = new G4StepLimiterPhysics;
  fModularPhysicsList->RegisterPhysics(stepLimitPhys);

  // Do not put the particle gun at the target position
  auto action = runManager->GetUserPrimaryGeneratorAction();
  ((DeutPrimaryGeneratorAction*)action)->SetUseTargetParameters(false);

  auto primaryGen = (PrimaryGeneratorActionBasic*)runManager \
                   ->GetUserPrimaryGeneratorAction();

  primaryGen->SetBeamParticle("deuteron");
  primaryGen->SetBeamType("Pencil");
  primaryGen->SetBeamPosition(G4ThreeVector{0, 0, -5000});
  primaryGen->SetBeamAngleX(0);
  primaryGen->SetBeamAngleY(0);
  primaryGen->SetBeamEnergy(190);
  runManager->BeamOn(1);

  primaryGen->SetBeamParticle("proton");
  primaryGen->SetBeamPosition(steppingAction->GetTargetPos());
  primaryGen->SetBeamAngleX(- steppingAction->GetTargetAngle() + M_PI/36);
  runManager->BeamOn(1);

  using namespace std;
  ifstream input{fInputMacroFile.data()}, geoFile;
  ofstream output{outputMacroFile.data()};
  string line, command, geoFileName;

  while (getline(input, line))
  {
    size_t n = line.find(" ");
    if (n != line.npos) command = line.substr(0, n);
    if (command == "/control/execute"){
      geoFileName = line.substr(n+1);
      geoFile.open(geoFileName);
      input.close();
      break;
    }
  }

  while (getline(geoFile, line))
  {
    size_t n = line.find(" ");
    if (n != line.npos) command = line.substr(0, n);
    else                command = "";
    ostringstream str;
    str << fixed << setprecision(2);

    // Comment the AutoConfig command
    if (command == "/samurai/geometry/AutoConfig") line = "# " + line;

    if (command == "/samurai/geometry/Target/Position")
    {
      G4ThreeVector pos = steppingAction->GetTargetPos()/10;
      str << command << " " << pos.x() << " " << pos.y() << " "\
          << pos.z() << " " << "cm";
      line = str.str();
    }

    if (command == "/samurai/geometry/Target/Angle")
    {
      G4double angle = steppingAction->GetTargetAngle();
      str << command << " " << angle/M_PI*180 << " " << "deg";
      line = str.str();
    }
    
    if (command == "/samurai/geometry/PDC/Angle")
    {
      G4double angle = trackingAction->GetPDCAngle();
      str << command << " " << angle/M_PI*180 << " " << "deg";
      line = str.str();
    }

    if (command == "/samurai/geometry/PDC/Position1")
    {
      G4ThreeVector pos = trackingAction->GetPDC1Pos()/10;
      str << command << " " << pos.x() << " " << pos.y() << " "\
          << pos.z() << " " << "cm";
      line = str.str();
    }

    if (command == "/samurai/geometry/PDC/Position2")
    {
      G4ThreeVector pos = trackingAction->GetPDC2Pos()/10;
      str << command << " " << pos.x() << " " << pos.y() << " "\
          << pos.z() << " " << "cm";
      line = str.str();
    }

    if (command == "/samurai/geometry/Dump/Angle")
    {
      G4double angle = trackingAction->GetDumpAngle();
      str << command << " " << angle/M_PI*180 << " " << "deg";
      line = str.str();
    }

    if (command == "/samurai/geometry/Dump/Position")
    {
      G4ThreeVector pos = trackingAction->GetDumpPos()/10;
      str << command << " " << pos.x() << " " << pos.y() << " "\
          << pos.z() << " " << "cm";
      line = str.str();
    }

    output << line << endl;
  }
  geoFile.close();
  output.close();
}
//______________________________________________________________________________
