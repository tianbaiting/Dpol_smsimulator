
#include "PrimaryGeneratorActionBasic.hh"
#include "ActionBasicMessenger.hh"
#include "BeamSimDataMessenger.hh"

#include "G4RunManager.hh"
#include "G4Event.hh"
#include "G4ParticleTable.hh"
#include "G4IonTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleGun.hh"

#include "G4PrimaryVertex.hh"
#include "G4PrimaryParticle.hh"

#include "G4RandomDirection.hh"
#include "Randomize.hh"
#include "G4LorentzVector.hh"
#include "G4Tubs.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"

#include "TBeamSimData.hh"
#include "SimDataManager.hh"
#include "TRunSimParameter.hh"

#include "TRandom3.h"
#include "TMath.h"
#include "TVector3.h"
#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"

#include <sstream>
#include "G4SystemOfUnits.hh"//Geant4.10

//____________________________________________________________________
PrimaryGeneratorActionBasic::PrimaryGeneratorActionBasic(G4int seed)
  : fBeamA(-9999), fBeamZ(-9999), 
    fBeamEnergy(250*MeV), fBeamBrho(-1),
    fBeamPosition(0, 0, -4*m), fBeamPositionXSigma(0), fBeamPositionYSigma(0),
    fBeamAngleX(0), fBeamAngleY(0), fBeamAngleXSigma(0), fBeamAngleYSigma(0),
    fRootInputFile(0), fInputTree(0), 
    fInputTreeName("tree"), fBranchInput(0),
    fSkipNeutron(false), fSkipHeavyIon(false), fSkipGamma(false)
{
  fParticleGun = new G4ParticleGun(1);

  fRandom = new TRandom3(seed);

  // default property
  G4ThreeVector momentumDirection = G4ThreeVector(0, 0, 1);
  G4double kineticEnergy = fBeamEnergy;

  fParticleTable = G4ParticleTable::GetParticleTable();
  fParticleGun->SetParticleDefinition(fParticleTable->FindParticle("proton"));
  fParticleGun->SetParticleMomentumDirection(momentumDirection);
  fParticleGun->SetParticleEnergy(kineticEnergy);
  fParticleGun->SetParticlePosition(fBeamPosition);

  if(!SetBeamType("Pencil")) G4Exception(0, 0, FatalException, 0);

  fActionBasicMessenger = ActionBasicMessenger::GetInstance();
  fActionBasicMessenger->Register(this);
  fBeamSimDataMessenger = new BeamSimDataMessenger();
}
//____________________________________________________________________
PrimaryGeneratorActionBasic::~PrimaryGeneratorActionBasic()
{
  delete fParticleGun;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamType(G4String beamType)
{
  fBeamType = beamType;
  std::cout<<fBeamType.Data()<<std::endl;
  if       (beamType == "Pencil"){
    beamFunctionPtr = &PrimaryGeneratorActionBasic::BeamTypePencil;
  }else if (beamType == "Gaus"){
    beamFunctionPtr = &PrimaryGeneratorActionBasic::BeamTypeGaus;
  }else if (beamType == "Isotropic"){
    beamFunctionPtr = &PrimaryGeneratorActionBasic::BeamTypeIsotropic;
  }else if (beamType == "Tree"){
    beamFunctionPtr = &PrimaryGeneratorActionBasic::BeamTypeTree;
  }else if (beamType == "Geantino"){ // for crosssection
    beamFunctionPtr = &PrimaryGeneratorActionBasic::BeamTypeGeantino;
  }else{
    G4cerr << "\n !!! beamType \"" << beamType << "\" does not exist !!!" << G4endl;
    return false;
  }
  G4cout << " set beam type to \"" << beamType << "\"" << G4endl;

  return true;
}
//____________________________________________________________________
void PrimaryGeneratorActionBasic::GeneratePrimaries(G4Event* anEvent)
{
#if DEBUG
  std::cout<<"start GeneratePrimaries"<<std::endl;
#endif 
  (this->*beamFunctionPtr)(anEvent);

  // Header information
  if (anEvent->GetEventID()==0){
    SimDataManager *sman = SimDataManager::GetSimDataManager();
    std::stringstream ss;
    ss<<"BeamType="<<fBeamType.Data();

    if (fBeamType!="Tree"){
      ss<<" particle="<<fParticleGun->GetParticleDefinition()->GetParticleName();

      if      (fBeamEnergy>0) ss<<" BeamEnergy="<<fBeamEnergy/MeV<<"AMeV";
      else if (fBeamBrho>0)   ss<<" BeamBrho="<<fBeamBrho<<"Tm";

      ss<<" BeamPosition="
	<<fBeamPosition.x()/mm<<"mm "
	<<fBeamPosition.y()/mm<<"mm "
	<<fBeamPosition.z()/mm<<"mm "
	<<" BeamAngle="<<fBeamAngleX/mrad<<"mrad "
	<<fBeamAngleY/mrad<<"mrad";
    }

    if (fBeamType=="Gaus"){
      ss<<" fBeamPositionXSigma="<<fBeamPositionXSigma/mm
	<<" fBeamPositionYSigma="<<fBeamPositionYSigma/mm
	<<" fBeamAngleXSigma="<<fBeamAngleXSigma/mrad
	<<" fBeamAngleYSigma="<<fBeamAngleYSigma/mrad;
    }else if (fBeamType=="Tree"){
      ss<<" InputFile="<<fRootInputFile->GetName()
	<<" fInputTreeName="<<fInputTreeName.Data();
    }

    sman->AppendHeader(ss.str().c_str());
  }
#if DEBUG
  std::cout<<"end GeneratePrimaries"<<std::endl;
#endif 
}
//____________________________________________________________________
void PrimaryGeneratorActionBasic::SetPrimaryVertex(G4Event* anEvent)
{
  if(!gBeamSimDataArray) return;

  int n=gBeamSimDataArray->size();
  for (int i=0;i<n;++i){
    TBeamSimData beam = (*gBeamSimDataArray)[i];
    //std::cout<<"primaryID="<<i<<" "<<beam<<std::endl;

    if      (beam.fParticleName=="neutron") {if (fSkipNeutron) continue;}
    else if (beam.fParticleName=="gamma")   {if (fSkipGamma) continue;}
    else                                    {if (fSkipHeavyIon) continue;}
    
    G4ParticleDefinition *particle = 0;
    particle = fParticleTable->FindParticle(beam.fParticleName.Data());
    if (particle==0) 
      //particle = fParticleTable->GetIon(beam.fZ, beam.fA, 0.0); //Z, A, excitationE
      particle = fParticleTable->GetIonTable()->GetIon(beam.fZ, beam.fA, 0.0); //Z, A, excitationE
    if (particle==0){
      G4cout<<beam<<G4endl;
      std::cout<<"cannot find particle:"<<std::endl;
    }
    fParticleGun->SetParticleDefinition(particle);
    fParticleGun->SetParticleEnergy((beam.fMomentum.E()-beam.fMomentum.M())*MeV);
    G4ThreeVector dir(beam.fMomentum.Px(),
		      beam.fMomentum.Py(),
		      beam.fMomentum.Pz() );
    G4ThreeVector pos_lab(beam.fPosition.x()*mm,
			  beam.fPosition.y()*mm,
			  beam.fPosition.z()*mm );
    //
    if (fSkipLargeYAng){
      G4double Y0 = dir.y()/dir.z()*(0-pos_lab.z())*mm;
      if (std::abs(Y0*mm)>400) {
	G4cout<<i<<" "
	      <<dir.x()*mm<<" "
	      <<dir.y()*mm<<" "
	      <<dir.z()*mm<<" "
	      <<Y0*mm<<" "
	      <<G4endl;
	std::cout<<"skip large Y angle particle: i="<<i
		 <<", dy/dz="<<dir.y()/dir.z()<<std::endl;
	continue;
      }
    }
    //


    fParticleGun->SetParticleMomentumDirection(dir);
    fParticleGun->SetParticlePosition(pos_lab);
    fParticleGun->GeneratePrimaryVertex(anEvent);
    fParticleGun->SetParticleTime(beam.fTime*ns);
  }

}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamA(G4int A)
{
  if(A < 0) return false;
  fBeamA = A;
  std::cout<<"SetBeamA "<<fBeamA<<std::endl;
  std::cout<<"Must be followed by /action/gun/SetBeamParticle to complete definition"<<std::endl;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamZ(G4int Z)
{
  if(Z < 0) return false;
  fBeamZ = Z;
  std::cout<<"SetBeamZ "<<fBeamZ<<std::endl;
  std::cout<<"Must be followed by /action/gun/SetBeamParticle to complete definition"<<std::endl;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamParticle()
{
  if(fBeamZ < 0 || fBeamA < 0) return false;
  G4ParticleDefinition *particle = 0;
  if (fBeamZ==0 && fBeamA==1) 
    particle = fParticleTable->FindParticle("neutron");
  else      
    particle = fParticleTable->GetIonTable()->GetIon(fBeamZ, fBeamA, 0.0); //Z, A, excitationE
  //particle = fParticleTable->GetIon(fBeamZ, fBeamA, 0.0); //Z, A, excitationE

  if (particle==0){
    std::cout<<"cannot find particle:"<<std::endl;
    return false;
  }

  G4cout<<"Set Beam: "
	<<particle->GetParticleName()
	<<G4endl;
  fParticleGun->SetParticleDefinition(particle);
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamParticle(G4String particleName)
{
  G4ParticleDefinition *particle = fParticleTable->FindParticle(particleName);
  if (particle==0){
    std::cout<<"cannot find particle:"<<std::endl;
    return false;
  }
  fBeamZ = particle->GetAtomicNumber();
  fBeamA = particle->GetAtomicMass();

  G4cout<<"Set Beam: "
	<<particle->GetParticleName()
	<<G4endl;
  fParticleGun->SetParticleDefinition(particle);
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamEnergy(G4double energy)
{
  if(energy < 0) return false;
  fBeamEnergy = energy;
  fBeamBrho = -1;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamBrho(double brho)
{
  if(brho < 0) return false;
  fBeamBrho = brho;
  fBeamEnergy = -1;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamPosition(G4ThreeVector position)
{
  fBeamPosition = position;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamPositionXSigma(G4double sigma)
{
  if(sigma < 0) return false;
  fBeamPositionXSigma = sigma;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamPositionYSigma(G4double sigma)
{
  if(sigma < 0) return false;
  fBeamPositionYSigma = sigma;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamAngleX(G4double theta)
{
  if(theta > M_PI) return false;
  fBeamAngleX = theta;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamAngleY(G4double theta)
{
  if(theta > M_PI) return false;
  fBeamAngleY = theta;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamAngleXSigma(G4double sigma)
{
  if(sigma < 0) return false;
  fBeamAngleXSigma = sigma;
  return true;
}
//____________________________________________________________________
bool PrimaryGeneratorActionBasic::SetBeamAngleYSigma(G4double sigma)
{
  if(sigma < 0) return false;
  fBeamAngleYSigma = sigma;
  return true;
}
//____________________________________________________________________
void PrimaryGeneratorActionBasic::BeamTypePencil(G4Event* anEvent)
{
  G4ParticleDefinition *particle = fParticleGun->GetParticleDefinition();
  G4double P = 0;
  G4double E = 0;
  G4double T = 0;
  G4double M = particle->GetPDGMass();

  if (fBeamEnergy>0){// input by Energy

    if (particle->GetParticleName()=="gamma"){
      E = fBeamEnergy;
      P = fBeamEnergy;
    }else{
      T = fBeamEnergy*fBeamA;
      E = T + M;
      P = sqrt(T*T+2.0*T*M);
    }

    if (anEvent->GetEventID()==0)
      std::cout<<particle->GetParticleName().data()<<" "
	       <<fBeamEnergy/MeV<<"MeV "
	       <<std::endl;

  } else if (fBeamBrho>0){// input by Brho
      P = CLHEP::c_light*fBeamBrho*(G4double)fBeamZ *MeV;
      T = P*P / (sqrt(P*P+M*M)+M);
      E = T + M;

      if (anEvent->GetEventID()==0)
	std::cout<<particle->GetParticleName().data()<<" "
		 <<fBeamBrho<<"Tm "
		 <<std::endl;
  }

  TBeamSimData data;
  data.fPrimaryParticleID = 0;
  data.fZ                 = particle->GetAtomicNumber();
  data.fA                 = particle->GetAtomicMass();
  data.fPDGCode           = particle->GetPDGEncoding();
  data.fParticleName      = particle->GetParticleName();
  data.fCharge            = particle->GetPDGCharge()/eplus;
  data.fMass              = M/MeV;
  data.fTime              = 0;// not implemented
  data.fIsAccepted        = kTRUE;
  data.fMomentum.SetPxPyPzE(0,0,P/MeV,E/MeV);
  data.fMomentum.RotateY(fBeamAngleX/rad);
  data.fMomentum.RotateX(-fBeamAngleY/rad);
  data.fPosition.SetXYZ(fBeamPosition.x()/mm,
			fBeamPosition.y()/mm,
			fBeamPosition.z()/mm);

  gBeamSimDataArray->push_back(data);
  SetPrimaryVertex(anEvent);
}
//____________________________________________________________________
void PrimaryGeneratorActionBasic::BeamTypeGaus(G4Event* anEvent)
{
  G4ParticleDefinition *particle = fParticleGun->GetParticleDefinition();
  G4double P = 0;
  G4double T = 0;
  G4double E = 0;
  G4double M = particle->GetPDGMass();
  if (fBeamEnergy>0){
    if (particle->GetParticleName()=="gamma"){
      E = fBeamEnergy;
      P = fBeamEnergy;
    }else {
      T = fBeamEnergy*fBeamA;
      P = sqrt(T*T+2.0*T*M);
      E = T + M;
    }
  } else if (fBeamBrho>0){
    P = CLHEP::c_light*fBeamBrho*(G4double)fBeamZ *MeV;
    T = P*P / (sqrt(P*P+M*M)+M);
    E = T + M;
  }

  TBeamSimData data;
  data.fPrimaryParticleID = 0;
  data.fZ                 = particle->GetAtomicNumber();
  data.fA                 = particle->GetAtomicMass();
  data.fPDGCode           = particle->GetPDGEncoding();
  data.fParticleName      = particle->GetParticleName();
  data.fCharge            = particle->GetPDGCharge()/eplus;
  data.fMass              = M/MeV;
  data.fTime              = 0;// not implemented
  data.fIsAccepted        = kTRUE;
  data.fMomentum.SetPxPyPzE(0,0,P/MeV,E/MeV);

  G4double AngX = gRandom->Gaus(fBeamAngleX/rad,fBeamAngleXSigma/rad);
  G4double AngY = gRandom->Gaus(-fBeamAngleY/rad,fBeamAngleYSigma/rad);
  data.fMomentum.RotateY(AngX);
  data.fMomentum.RotateX(AngY);
  G4double PosX = gRandom->Gaus(fBeamPosition.x()/mm,fBeamPositionXSigma/mm);
  G4double PosY = gRandom->Gaus(fBeamPosition.y()/mm,fBeamPositionYSigma/mm);

  data.fPosition.SetXYZ(PosX,
			PosY,
			fBeamPosition.z()/mm);

  gBeamSimDataArray->push_back(data);
  SetPrimaryVertex(anEvent);
}
//____________________________________________________________________
void PrimaryGeneratorActionBasic::BeamTypeIsotropic(G4Event* anEvent)
{
  G4ParticleDefinition *particle = fParticleGun->GetParticleDefinition();
  G4double P = 0;
  G4double T = 0;
  G4double E = 0;
  G4double M = particle->GetPDGMass();
  if (fBeamEnergy>0){
    if (particle->GetParticleName()=="gamma"){
      E = fBeamEnergy;
      P = fBeamEnergy;
    }else {
      T = fBeamEnergy*fBeamA;
      P = sqrt(T*T+2.0*T*M);
      E = T + M;
    }
  } else if (fBeamBrho>0){
    P = CLHEP::c_light*fBeamBrho*(G4double)fBeamZ *MeV;
    T = P*P / (sqrt(P*P+M*M)+M);
    E = T + M;
  }

  TBeamSimData data;
  data.fPrimaryParticleID = 0;
  data.fZ                 = particle->GetAtomicNumber();
  data.fA                 = particle->GetAtomicMass();
  data.fPDGCode           = particle->GetPDGEncoding();
  data.fParticleName      = particle->GetParticleName();
  data.fCharge            = particle->GetPDGCharge()/eplus;
  data.fMass              = M/MeV;
  data.fTime              = 0;// not implemented
  data.fIsAccepted        = kTRUE;

  double costht = gRandom->Uniform(-1.0,1.0);
  double phi    = gRandom->Uniform(0.0, 2.0*TMath::Pi());
  double sintht = sqrt(1.0-costht*costht);

  data.fMomentum.SetPxPyPzE(sintht*cos(phi)*P/MeV,
			    sintht*sin(phi)*P/MeV,
			    costht*P/MeV,
			    E/MeV                 );
  
  data.fPosition.SetXYZ(fBeamPosition.x()/mm,
			fBeamPosition.y()/mm,
			fBeamPosition.z()/mm);

  gBeamSimDataArray->push_back(data);
  SetPrimaryVertex(anEvent);
}
//________________________________________________________________
void PrimaryGeneratorActionBasic::BeamTypeTree(G4Event *anEvent)
{
#if DEBUG
  std::cout<<"start BeamTypeTree"<<std::endl;
#endif 
  fInputTree->GetEntry(anEvent->GetEventID());
  for(int i=0;i<(int)gBeamSimDataArray->size();++i){
    TString        name = (*gBeamSimDataArray)[i].fParticleName;
    G4ParticleDefinition* particle=0;
    if (name==""){
      particle = fParticleTable->GetIonTable()->GetIon((*gBeamSimDataArray)[i].fZ,
						       (*gBeamSimDataArray)[i].fA,
						       0.0); //Z, A, excitationE
//      particle = fParticleTable->GetIon((*gBeamSimDataArray)[i].fZ,
//					(*gBeamSimDataArray)[i].fA,
//					0.0); //Z, A, excitationE

      (*gBeamSimDataArray)[i].fParticleName = particle->GetParticleName();
    }else{
      particle = fParticleTable->FindParticle(name.Data());
      (*gBeamSimDataArray)[i].fZ = particle->GetAtomicNumber();
      (*gBeamSimDataArray)[i].fA = particle->GetAtomicMass();
    }
    if (particle==0){
      G4cout<<(*gBeamSimDataArray)[i]<<G4endl;
      G4Exception(0, 0, FatalException, "cannot find particle");
    }

    // set other variables
    (*gBeamSimDataArray)[i].fPDGCode    = particle->GetPDGEncoding();
    (*gBeamSimDataArray)[i].fCharge     = particle->GetPDGCharge()/eplus;
    (*gBeamSimDataArray)[i].fMass       = particle->GetPDGMass()/MeV;
    (*gBeamSimDataArray)[i].fIsAccepted = kTRUE;
    
  }// for (int i=0;i<(int)gBeamSimDataArray->size();++i){

  SetPrimaryVertex(anEvent);
#if DEBUG
  std::cout<<"end BeamTypeTree"<<std::endl;
#endif 
}
//________________________________________________________________
void PrimaryGeneratorActionBasic::BeamTypeGeantino(G4Event* anEvent)
{
  G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
  G4ParticleDefinition* particle = particleTable->FindParticle("geantino");
  fParticleGun->SetParticleDefinition(particle);

  G4double energy(100*MeV);
  G4ThreeVector momentum = G4ThreeVector(0, 0, 1);
  G4ThreeVector position = G4ThreeVector(0, 0, 0);

  fParticleGun->SetParticleEnergy(energy);
  fParticleGun->SetParticleMomentumDirection(momentum);
  fParticleGun->SetParticlePosition(position);

  fParticleGun->GeneratePrimaryVertex(anEvent);  

//  gSimulationDataBeamArray->clear();
//  AccumulateBeamData(anEvent);
}
//________________________________________________________________
void PrimaryGeneratorActionBasic::SetInputTreeFileName(TString rootFile)
{
  fRootInputFile = new TFile(rootFile.Data(),"readonly");
  if (!fRootInputFile){
    G4cerr<< " File Open Error for BeamTypeTree. file = "
          << rootFile.Data()
          << G4endl;
    G4Exception(0,0,FatalException,0);
  }
}
//________________________________________________________________
void PrimaryGeneratorActionBasic::SetInputTreeName(TString treeName)
{
  if (!fRootInputFile){
    G4cerr<< " File Open Error for BeamTypeTree. file = "
          << fRootInputFile->GetName()
          << G4endl;
    G4Exception(0,0,FatalException,0);
  }

  fInputTreeName = treeName;
  fRootInputFile->GetObject(fInputTreeName.Data(),fInputTree);
  if (!fInputTree){
    G4cerr<< " Tree ("
	  << treeName.Data()
	  << ") cannot be found in "
          << fRootInputFile->GetName()
          << G4endl;
    G4Exception(0,0,FatalException,0);
  }
  G4cout<<"SetInputTreeName : "<<fInputTreeName.Data()<<G4endl;

}
//________________________________________________________________
void PrimaryGeneratorActionBasic::TreeBeamOn(G4int num)
{
  fBranchInput = fInputTree->GetBranch("TBeamSimData");
  fBranchInput->SetAddress(&gBeamSimDataArray);

  G4int n_event = 0;
  if ( num<1 || num>(G4int)fInputTree->GetEntries() ) {
    n_event = (G4int)fInputTree->GetEntries();
    G4cout<<" Number of events is set to Tree Entires = "
          <<fInputTree->GetEntries()
          <<G4endl;
  }else{
    n_event = num;
  }

  G4RunManager *RunManager = G4RunManager::GetRunManager();
  RunManager->BeamOn(n_event);

  fRootInputFile->Close();
}
//________________________________________________________________
