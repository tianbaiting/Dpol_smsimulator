
#include "RunActionBasic.hh"

#include "G4Run.hh"
#include "G4RunManager.hh"
#include "PrimaryGeneratorActionBasic.hh"

#include "SimDataManager.hh"
#include "TRunSimParameter.hh"
#include "TFragSimParameter.hh"

#include "TFile.h"
#include "TTree.h"

#include <fstream>
#include <iomanip>
#include <sstream>
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
