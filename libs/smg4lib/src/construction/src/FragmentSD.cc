#include "FragmentSD.hh"
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

FragmentSD::FragmentSD(G4String name) : G4VSensitiveDetector(name)
{;}

FragmentSD::~FragmentSD()
{;}

void FragmentSD::Initialize(G4HCofThisEvent* /*HCTE*/)
{;}

G4bool FragmentSD::ProcessHits(G4Step* aStep, G4TouchableHistory* /*ROhist*/)
{
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  TClonesArray *SimDataArray = sman->FindSimDataArray("FragSimData");
  if(SimDataArray==0) return true;

  G4StepPoint* preStepPoint = aStep->GetPreStepPoint();
  G4StepPoint* postStepPoint = aStep->GetPostStepPoint();
  const G4DynamicParticle* dynamicParticle = aStep->GetTrack()->GetDynamicParticle();
  const G4ParticleDefinition* particleDefinition = aStep->GetTrack()->GetParticleDefinition();
  //新加energydeposit支持
  Double_t energyDeposit_MeV = aStep->GetTotalEnergyDeposit()/MeV;  // 能量沉积

  Double_t preKineticEnergy_MeV  = preStepPoint->GetKineticEnergy()/MeV;
  Double_t postKineticEnergy_MeV = postStepPoint->GetKineticEnergy()/MeV;
  G4ThreeVector prePosition  = preStepPoint->GetPosition();
  G4ThreeVector postPosition = postStepPoint->GetPosition();
  G4ThreeVector preMomentum  = preStepPoint->GetMomentum();
  G4ThreeVector postMomentum = postStepPoint->GetMomentum();

  G4int parentid = aStep->GetTrack()->GetParentID();
  G4int trackid =  aStep->GetTrack()->GetTrackID();
  G4int stepno = aStep->GetTrack()->GetCurrentStepNumber();
  G4int pdgCode = dynamicParticle->GetPDGcode();
  TString particleName(particleDefinition->GetParticleName().data());

  G4TouchableHistory* theTouchable = (G4TouchableHistory*)(preStepPoint->GetTouchable());
  
  // GetVolume(G4int depth = 0), 0 means the sensitive area, 1 means the detector itself
  const G4String& detectorName = theTouchable->GetVolume(1)->GetLogicalVolume()->GetName();
  const G4int& detectorID = theTouchable->GetVolume(1)->GetCopyNo();
  const G4String& moduleName = theTouchable->GetVolume(0)->GetLogicalVolume()->GetName();
  const Double_t mass_MeV = particleDefinition->GetPDGMass()/MeV;

  // store only primary particle with Z!=0
  if(parentid == 0 && aStep->GetTrack()->GetDefinition()->GetPDGCharge() != 0.){

    Int_t nstep = SimDataArray->GetEntries();
    new ((*SimDataArray)[nstep]) TSimData;
    TSimData* data = (TSimData*)SimDataArray->At(nstep);

    //data->fPrimaryParticleID = trackid - 1;
    data->fParentID = parentid;
    data->fTrackID = trackid;
    data->fStepNo = stepno;
    data->fZ = particleDefinition->GetAtomicNumber();
    data->fA = particleDefinition->GetAtomicMass();
    data->fPDGCode = pdgCode;
    data->fParticleName = particleName;
    data->fDetectorName = detectorName;
    data->fID = detectorID;
    data->fModuleName = moduleName;
    data->fCharge = dynamicParticle->GetCharge()/eplus;
    data->fMass = mass_MeV;
    data->fPreMomentum.SetPxPyPzE(preMomentum.x()/MeV,
				 preMomentum.y()/MeV,
				 preMomentum.z()/MeV,
				 preKineticEnergy_MeV + mass_MeV);
    data->fPostMomentum.SetPxPyPzE(postMomentum.x()/MeV,
				  postMomentum.y()/MeV,
				  postMomentum.z()/MeV,
				  postKineticEnergy_MeV + mass_MeV);
    data->fPrePosition.SetXYZ(prePosition.x()/mm,
			     prePosition.y()/mm,
			     prePosition.z()/mm);
    data->fPostPosition.SetXYZ(postPosition.x()/mm,
			      postPosition.y()/mm,
			      postPosition.z()/mm);
    data->fPreTime  = preStepPoint->GetGlobalTime()/ns;
    data->fEnergyDeposit = energyDeposit_MeV; // 能量沉积
    data->fPostTime = postStepPoint->GetGlobalTime()/ns;
    data->fFlightLength = aStep->GetTrack()->GetTrackLength()/mm 
      - aStep->GetStepLength()/mm;// up to entrance of material
    data->fIsAccepted = kTRUE;


#ifdef DEBUG
    G4cout << "array_size = "<<SimDataArray->size() 
	   << " data : " << *data << G4endl;
#endif
  }
  
  return true;
}

void FragmentSD::EndOfEvent(G4HCofThisEvent* /*HCTE*/)
{;}

