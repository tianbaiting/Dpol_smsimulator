//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
//
// $Id: exampleN03.cc,v 1.39 2010-12-01 05:56:17 allison Exp $
// GEANT4 tag $Name: not supported by cvs2svn $
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "G4RunManager.hh"
#include "G4UImanager.hh"

#include "Randomize.hh"

#include "SimDataManager.hh"
#include "BeamSimDataInitializer.hh"
#include "FragSimDataInitializer.hh"
#include "NEBULASimDataInitializer.hh"

#include "NEBULASimDataConverter_TArtNEBULAPla.hh"
#include "FragSimDataConverter_Basic.hh"

#include "G4PhysListFactorySAMURAI.hh"
#include "G4VModularPhysicsList.hh"

#include "DeutDetectorConstruction.hh"
#include "DeutPrimaryGeneratorAction.hh"
#include "RunActionBasic.hh"
#include "EventActionBasic.hh"
#include "TrackingActionBasic.hh"

#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

int main(int argc,char** argv)
{
  // Construct the default run manager
  G4RunManager * runManager = new G4RunManager;

  // detector construction
  DeutDetectorConstruction* detector = new DeutDetectorConstruction();
  runManager->SetUserInitialization(detector);

  // set physics construction class
  G4String physName = "QGSP_INCLXX_XS"; // default
  // Physics List name defined via 3rd argument
  if(argc==3){
    physName = argv[2];
  }
  G4PhysListFactorySAMURAI physListFactorySAMURAI;
  G4VModularPhysicsList* modularPhysicsList
    = physListFactorySAMURAI.GetReferencePhysList(physName);
  if(!modularPhysicsList) return 1; // exit failure
  runManager->SetUserInitialization(modularPhysicsList);
  detector->SetModularPhysicsList(modularPhysicsList);


  // Choose the Random engine
  //
  CLHEP::HepRandom::setTheEngine(new CLHEP::RanecuEngine);
  
  G4UserTrackingAction* userTrackingAction = new TrackingActionBasic;
  runManager->SetUserAction(userTrackingAction);
     
  // Set user action classes
  //
  SimDataManager* simDataManager = SimDataManager::GetSimDataManager();
  simDataManager->RegistInitializer(new BeamSimDataInitializer);
  simDataManager->RegistInitializer(new FragSimDataInitializer);
  simDataManager->RegistInitializer(new NEBULASimDataInitializer);
  simDataManager->RegistConverter(new FragSimDataConverter_Basic);
  simDataManager->RegistConverter(new NEBULASimDataConverter_TArtNEBULAPla);

  G4UserRunAction* userRunAction = new RunActionBasic;
  runManager->SetUserAction(userRunAction);

  auto userPrimaryGeneratorAction = new DeutPrimaryGeneratorAction;
  // Put the primary generator at the position and orientation of the target
  userPrimaryGeneratorAction->SetUseTargetParameters(true);
  runManager->SetUserAction(userPrimaryGeneratorAction);

  G4UserEventAction* userEventAction = new EventActionBasic;
  runManager->SetUserAction(userEventAction);

  // Initialize G4 kernel
  runManager->Initialize();
  
  // Initialize visualization
  G4VisManager* visManager = new G4VisExecutive;
  visManager->Initialize();

  // Get the pointer to the User Interface manager
  G4UImanager* UImanager = G4UImanager::GetUIpointer();

  //gen_action->SetKinematics();
  if (argc!=1) {  // batch mode
    G4String command = "/control/execute ";
    G4String fileName = argv[1];
    detector->SetInputMacroFile(fileName);
    UImanager->ApplyCommand(command+fileName);
  }else{  // interactive mode : define UI session
    detector->SetInputMacroFile("vis.mac");
    G4UIExecutive* ui = new G4UIExecutive(argc, argv);
    UImanager->ApplyCommand("/control/execute vis.mac"); 
    ui->SessionStart();
    delete ui;
  }

  // Job termination
  // Free the store: user actions, physics_list and detector_description are
  //                 owned and deleted by the run manager, so they should not
  //                 be deleted in the main() program !
  delete visManager;
  delete runManager;

  return 0;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.....
