
#include "RunActionBasic.hh"

#include "G4Run.hh"
#include "G4RunManager.hh"
#include "PrimaryGeneratorActionBasic.hh"

#include "SimDataManager.hh"
#include "NeutronDetectorSimConfig.hh"
#include "SMLogger.hh"
#include "TNEBULAPlusSimParameter.hh"
#include "TNEBULASimParameter.hh"
#include "TRunSimParameter.hh"
#include "TFragSimParameter.hh"

#include "TFile.h"
#include "TNamed.h"
#include "TTree.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
//____________________________________________________________________
RunActionBasic::RunActionBasic()
  : fFileOut(0) 
{
  fSimDataManager = SimDataManager::GetSimDataManager();

  fRunSimParameter = (TRunSimParameter*)fSimDataManager->FindParameter("RunParameter");
  if (fRunSimParameter==0){
    fRunSimParameter = new TRunSimParameter("RunParameter");
    fSimDataManager->AddParameter(fRunSimParameter);
  }

  fFragSimParameter = (TFragSimParameter*)fSimDataManager->FindParameter("FragParameter");
  if (fFragSimParameter==0) {
    fFragSimParameter = new TFragSimParameter("FragParameter");
    fSimDataManager->AddParameter(fFragSimParameter);
  }

}
//____________________________________________________________________
RunActionBasic::~RunActionBasic()
{}
//____________________________________________________________________
void RunActionBasic::BeginOfRunAction(const G4Run* aRun)
{
#if DEBUG
  std::cout<<"start BeginOfRunAction"<<std::endl;
#endif 

  fRunSimParameter->fStartTime = TDatime();

  G4int id = aRun->GetRunID();
  G4cout << "### Run " << id << " start" << G4endl;
  std::stringstream ss;
  ss<<"Run#="<<id;
  fRunSimParameter->AppendHeader(ss.str().c_str());

  // accumulate data by ROOT
  std::ostringstream oss;
  oss << fRunSimParameter->fSaveDir
      << fRunSimParameter->fRunName
      << std::setw(4) << std::setfill('0') << id << ".root";
  std::filesystem::path log_path(oss.str());
  log_path.replace_extension(".log");
  SMLogger::LogConfig log_config;
  log_config.async = false;
  log_config.console = true;
  log_config.file = true;
  log_config.filename = log_path.string();
  log_config.level = SMLogger::LogLevel::INFO;
  SMLogger::Logger::Instance().Initialize(SMLogger::MakeLogConfigFromEnvironment(log_config));

  // over write warning
  std::ifstream ifileForCheck(oss.str().c_str());
  if(ifileForCheck){ // file exists
    if(fRunSimParameter->fOverWrite=="ask"){
      G4cout << oss.str() << " already exists. over write[y], exit[n]>" << G4endl;    
      std::string yn;
      G4cin >> yn;
      if(!(yn == "y" || yn == "yes")){
	G4cout << "STOP smsimulator." << G4endl;    
	std::abort();
      }
    }else if(fRunSimParameter->fOverWrite=="y"){
      G4cout << oss.str() << " already exists. over write." << G4endl;    
    }else{
      G4cout << oss.str() << " already exists. exit." << G4endl;    
      std::abort();
    }
  }
  fFileOut = TFile::Open(oss.str().c_str(), "recreate");
  if(!fFileOut || fFileOut->IsZombie()) G4Exception(0, 0, FatalException, 0);

  // Initialize Output Data classes
  fSimDataManager->Initialize();

  auto* nebula_prm = static_cast<TNEBULASimParameter*>(
      fSimDataManager->FindParameter("NEBULAParameter"));
  auto* nebula_plus_prm = static_cast<TNEBULAPlusSimParameter*>(
      fSimDataManager->FindParameter("NEBULAPlusParameter"));
  const bool nebula_enabled = sim_deuteron::IsNEBULAEnabled(nebula_prm);
  const bool nebula_plus_enabled = sim_deuteron::IsNEBULAPlusEnabled(nebula_plus_prm);
  SM_INFO("Simulation neutron detector configuration:");
  SM_INFO("  SimNeutronDetectorMode={}",
          sim_deuteron::SimNeutronDetectorModeName(nebula_enabled, nebula_plus_enabled));
  SM_INFO("  SimNEBULAEnabled={}", nebula_enabled ? "true" : "false");
  SM_INFO("  SimNEBULAPlusEnabled={}", nebula_plus_enabled ? "true" : "false");
  if (nebula_prm) {
    SM_INFO("  SimNEBULAParameterFile={}", nebula_prm->fParameterFileName.Data());
    SM_INFO("  SimNEBULADetectorParameterFile={}", nebula_prm->fDetectorParameterFileName.Data());
    SM_INFO("  SimNEBULADetectorCount={}", nebula_prm->fNeutNum + nebula_prm->fVetoNum);
  }
  if (nebula_plus_prm) {
    SM_INFO("  SimNEBULAPlusParameterFile={}", nebula_plus_prm->fParameterFileName.Data());
    SM_INFO("  SimNEBULAPlusDetectorParameterFile={}", nebula_plus_prm->fDetectorParameterFileName.Data());
    SM_INFO("  SimNEBULAPlusDetectorCount={}", nebula_plus_prm->fNeutNum + nebula_plus_prm->fVetoNum);
  }
  SM_INFO("  OutputROOT={}", oss.str());
  SM_INFO("  LogFile={}", log_path.string());

  // create event data branch. tree will be automatically added to current directory
  TTree* tree = new TTree(fRunSimParameter->fTreeName,
			  fRunSimParameter->fTreeTitle);
  fSimDataManager->DefineBranch(tree);

  fFileOut->Write();
#if DEBUG
  std::cout<<"end BeginOfRunAction"<<std::endl;
#endif 
}
//____________________________________________________________________
void RunActionBasic::EndOfRunAction(const G4Run* aRun)
{
  G4cout << "\n Event:" << aRun->GetNumberOfEvent() << std::endl;

  fSimDataManager = SimDataManager::GetSimDataManager();

  // stop time
  fRunSimParameter->fStopTime = TDatime();
  TDatime date;
  std::stringstream ss;
  ss<<" Start-time:"<<fRunSimParameter->fStartTime.AsSQLString();
  ss<<" Stop-time:"<<fRunSimParameter->fStopTime.AsSQLString();
  fSimDataManager->AppendHeader(ss.str());
  fRunSimParameter->AppendHeader(fSimDataManager->GetHeader());

  // print parameters
  fSimDataManager->PrintParameters();

  // write to file
  fSimDataManager->AddParameters(fFileOut);
  auto* nebula_prm = static_cast<TNEBULASimParameter*>(
      fSimDataManager->FindParameter("NEBULAParameter"));
  auto* nebula_plus_prm = static_cast<TNEBULAPlusSimParameter*>(
      fSimDataManager->FindParameter("NEBULAPlusParameter"));
  const bool nebula_enabled = sim_deuteron::IsNEBULAEnabled(nebula_prm);
  const bool nebula_plus_enabled = sim_deuteron::IsNEBULAPlusEnabled(nebula_plus_prm);
  TNamed sim_mode("SimNeutronDetectorMode",
                  sim_deuteron::SimNeutronDetectorModeName(nebula_enabled, nebula_plus_enabled));
  TNamed sim_nebula_enabled("SimNEBULAEnabled", nebula_enabled ? "true" : "false");
  TNamed sim_nebula_plus_enabled("SimNEBULAPlusEnabled", nebula_plus_enabled ? "true" : "false");
  const std::string nebula_count = std::to_string(
      nebula_prm ? (nebula_prm->fNeutNum + nebula_prm->fVetoNum) : 0);
  const std::string nebula_plus_count = std::to_string(
      nebula_plus_prm ? (nebula_plus_prm->fNeutNum + nebula_plus_prm->fVetoNum) : 0);
  TNamed sim_nebula_count("SimNEBULADetectorCount", nebula_count.c_str());
  TNamed sim_nebula_plus_count("SimNEBULAPlusDetectorCount", nebula_plus_count.c_str());
  sim_mode.Write();
  sim_nebula_enabled.Write();
  sim_nebula_plus_enabled.Write();
  sim_nebula_count.Write();
  sim_nebula_plus_count.Write();
  if (nebula_prm) {
    TNamed sim_nebula_param("SimNEBULAParameterFile", nebula_prm->fParameterFileName.Data());
    TNamed sim_nebula_bars("SimNEBULADetectorParameterFile",
                           nebula_prm->fDetectorParameterFileName.Data());
    sim_nebula_param.Write();
    sim_nebula_bars.Write();
  }
  if (nebula_plus_prm) {
    TNamed sim_nebula_plus_param("SimNEBULAPlusParameterFile",
                                 nebula_plus_prm->fParameterFileName.Data());
    TNamed sim_nebula_plus_bars("SimNEBULAPlusDetectorParameterFile",
                                nebula_plus_prm->fDetectorParameterFileName.Data());
    sim_nebula_plus_param.Write();
    sim_nebula_plus_bars.Write();
  }
  fFileOut->Write(0,TObject::kOverwrite);
  std::cout<< "written to "<<fFileOut->GetName()<<std::endl;

  // Reset tree, parameters
  TTree* tree;
  gDirectory->GetObject(fRunSimParameter->fTreeName, tree);
  tree->ResetBranchAddresses();
  fSimDataManager->RemoveParameters(fFileOut);// for storing parameters

  // reset Header of TRunSimParameter
  fRunSimParameter->fHeader="";
  fSimDataManager->ClearHeader();

  fFileOut->Close();

  G4cout << G4endl;
}
//____________________________________________________________________
