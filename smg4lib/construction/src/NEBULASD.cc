#include "NEBULASD.hh"
#include "SimDataManager.hh"
#include "TSimData.hh"

#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VTouchable.hh"
#include "G4TouchableHistory.hh"
#include "G4VProcess.hh"
#include "G4ParticleDefinition.hh"
#include "G4PrimaryParticle.hh"

#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"
#include "TClonesArray.h"
#include "G4SystemOfUnits.hh"//Geant4.10
//____________________________________________________________________
NEBULASD::NEBULASD(G4String name) : G4VSensitiveDetector(name)
{;}
//____________________________________________________________________
NEBULASD::~NEBULASD()
{;}
//____________________________________________________________________
void NEBULASD::Initialize(G4HCofThisEvent* /*HCTE*/)
{;}
//____________________________________________________________________
G4bool NEBULASD::ProcessHits(G4Step* aStep, G4TouchableHistory* /*ROhist*/)
{
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  TClonesArray *SimDataArray = sman->FindSimDataArray("NEBULASimData");
  if(SimDataArray==0) return true;


  G4StepPoint* preStepPoint = aStep->GetPreStepPoint();
  G4StepPoint* postStepPoint = aStep->GetPostStepPoint();
  const G4DynamicParticle* dynamicParticle = aStep->GetTrack()->GetDynamicParticle();
  const G4ParticleDefinition* particleDefinition = aStep->GetTrack()->GetParticleDefinition();
  const G4VProcess* postProcess = postStepPoint->GetProcessDefinedStep();

  // charged particleのenergy lossした時間としてはpresteppointがよい。
  // しかし、反応自体はpoststeppointの方がよい...
  Double_t energyDeposit_MeV = aStep->GetTotalEnergyDeposit()/MeV;
  G4ThreeVector prePosition  = preStepPoint->GetPosition();
  G4ThreeVector postPosition = postStepPoint->GetPosition();
  G4ThreeVector preMomentum  = preStepPoint->GetMomentum();
  G4ThreeVector postMomentum = postStepPoint->GetMomentum();
  const Double_t mass_MeV = particleDefinition->GetPDGMass()/MeV;

  G4int parentid = aStep->GetTrack()->GetParentID();
  G4int trackid = aStep->GetTrack()->GetTrackID();
  G4int stepno = aStep->GetTrack()->GetCurrentStepNumber();
  TString particleName(particleDefinition->GetParticleName().data());
  TString processName;
  if(postProcess) processName = postProcess->GetProcessName().data();

  G4TouchableHistory* theTouchable = (G4TouchableHistory*)(preStepPoint->GetTouchable());
  std::vector<Int_t> moduleID;
  for(G4int i=0; i<theTouchable->GetHistoryDepth(); ++i){
    G4int depth = i;//0:self, 1:mother, 2:grandmother, ....
    moduleID.push_back(theTouchable->GetCopyNumber(depth));
  }
  G4VPhysicalVolume* physicalVolume = theTouchable->GetVolume(0); // 0: depth
  const G4String& moduleName = physicalVolume->GetName();

  if((energyDeposit_MeV>0.) && (aStep->GetTrack()->GetDefinition()->GetPDGCharge() != 0.)){

    Int_t nstep = SimDataArray->GetEntriesFast();
    new ((*SimDataArray)[nstep]) TSimData;
    TSimData* data = (TSimData*)SimDataArray->At(nstep);

    //data->fPrimaryParticleID = parentid - 1;
    data->fParentID = parentid;
    data->fTrackID = trackid;
    data->fStepNo = stepno;
    data->fZ = particleDefinition->GetAtomicNumber();
    data->fA = particleDefinition->GetAtomicMass();
    data->fPDGCode = dynamicParticle->GetPDGcode();
    data->fModuleID = moduleID;
    data->fID = theTouchable->GetCopyNumber(0);
    data->fParticleName = particleName;
    data->fDetectorName = SensitiveDetectorName;
    data->fModuleName = moduleName;
    data->fProcessName = processName;
    data->fCharge = particleDefinition->GetPDGCharge()/eplus;
    data->fMass = particleDefinition->GetPDGMass()/MeV;
    data->fPreKineticEnergy  = preStepPoint->GetKineticEnergy()/MeV;
    data->fPostKineticEnergy = postStepPoint->GetKineticEnergy()/MeV;
    data->fPrePosition.SetXYZ(prePosition.x()/mm,
			     prePosition.y()/mm,
			     prePosition.z()/mm);
    data->fPostPosition.SetXYZ(postPosition.x()/mm,
			      postPosition.y()/mm,
			      postPosition.z()/mm);
    data->fPreMomentum.SetPxPyPzE(preMomentum.x()/MeV,
				 preMomentum.y()/MeV,
				 preMomentum.z()/MeV,
				 data->fPreKineticEnergy + mass_MeV);
    data->fPostMomentum.SetPxPyPzE(postMomentum.x()/MeV,
				  postMomentum.y()/MeV,
				  postMomentum.z()/MeV,
				  data->fPostKineticEnergy + mass_MeV);
    data->fEnergyDeposit = energyDeposit_MeV;
    data->fPreTime  = preStepPoint->GetGlobalTime()/ns;
    data->fPostTime = postStepPoint->GetGlobalTime()/ns;
    data->fIsAccepted = kTRUE;

#ifdef DEBUG
    if(SimDataArray){
      G4cout << "NEBULA_array size="<<SimDataArray->GetEntries()
	     << " data :"  << *data << G4endl;
    }
#endif
  }
  
  return true;
}
//____________________________________________________________________
void NEBULASD::EndOfEvent(G4HCofThisEvent* /*HCTE*/)
{;}
//____________________________________________________________________

