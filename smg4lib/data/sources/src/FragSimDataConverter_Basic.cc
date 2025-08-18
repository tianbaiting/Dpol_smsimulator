#include "FragSimDataConverter_Basic.hh"

#include "SimDataManager.hh"
#include "TSimData.hh"

#include "TTree.h"
#include "TClonesArray.h"
#include "TString.h"

#include <iostream>
#include <map>
#include <set>
using namespace std;

//____________________________________________________________________
FragSimDataConverter_Basic::FragSimDataConverter_Basic(TString name)
  : SimDataConverter(name)
{
  ClearBuffer();
}
//____________________________________________________________________
FragSimDataConverter_Basic::~FragSimDataConverter_Basic()
{;}
//____________________________________________________________________
int FragSimDataConverter_Basic::Initialize()
{
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  fFragSimParameter = (TFragSimParameter*)sman->FindParameter("FragParameter");
  if (fFragSimParameter==0){
    std::cout<<"FragSimDataConveter_Basic : FragmentParameter is not found."
	     <<std::endl;
    return 1;
  }

  fFragSimDataArray = sman->FindSimDataArray("FragSimData");
  if (fFragSimDataArray==0){
    std::cout<<"FragSimDataConveter_Basic : FragSimDataArray is not found."
	     <<std::endl;
    return 1;
  }

  return 0;
}
//____________________________________________________________________
TVector3 FragSimDataConverter_Basic::LabToPDC(TVector3 pos)
{
  // Convert from labortory frame to PDC frame
  Double_t ang_pdc = fFragSimParameter->fPDCAngle;
  pos.RotateY(ang_pdc);
  return pos;
}
//____________________________________________________________________
int FragSimDataConverter_Basic::DefineBranch(TTree *tree)
{
  if (fDataStore) {
    tree->Branch("target_x",&fTargetX,"target_x/D");
    tree->Branch("target_y",&fTargetY,"target_y/D");
    tree->Branch("target_theta",&fTargetTheta,"target_y/D");
    tree->Branch("target_phi",&fTargetPhi,"target_y/D");
    tree->Branch("target_energy",&fTargetEnergy,"target_energy/D");
    tree->Branch("ok_target",&fOK_Target,"ok_target/O");

    tree->Branch("PDC1U",&fPDC1U,"PDC1U/D");
    tree->Branch("PDC1X",&fPDC1X,"PDC1X/D");
    tree->Branch("PDC1V",&fPDC1V,"PDC1V/D");
    tree->Branch("PDC2U",&fPDC2U,"PDC2U/D");
    tree->Branch("PDC2X",&fPDC2X,"PDC2X/D");
    tree->Branch("PDC2V",&fPDC2V,"PDC2V/D");
    tree->Branch("PDCTrackNo",&fPDCTrackNo,"PDCTrackNo/I");
    tree->Branch("OK_PDC1",&fOK_PDC1,"OK_PDC1/O");
    tree->Branch("OK_PDC2",&fOK_PDC2,"OK_PDC2/O");
  }
  return 0;
}
//____________________________________________________________________
int FragSimDataConverter_Basic::ConvertSimData()
{
  Int_t ndata = fFragSimDataArray->GetEntries();

  TVector3 pos_target = fFragSimParameter->fTargetPosition;
  Double_t ang_target = fFragSimParameter->fTargetAngle;

  TVector3 pos_target_in{NAN, NAN, NAN};
  Int_t stepno_target_in{-1};
  TLorentzVector mom_target_in{NAN, NAN, NAN, NAN};

  // { {"U":{in,out}, "X":{in,out}, "V":{in,out}}(PDC1), ...(PDC2) }
  array< map<TString, array<TVector3, 2>>, 2> positions;

  // { {"U":{stepNo_min, stepNo_max}, "X":{...}, "V":{...}}(PDC1), ...(PDC2) }
  array< map<TString, array<Int_t,2>>, 2> stepNos;

  set<Int_t> trackIDs;

  for (int i=0;i<ndata;++i){
    auto data = (TSimData*)fFragSimDataArray->At(i);

    if (data->fDetectorName=="Target"){

      if (stepno_target_in == -1 || data->fStepNo < stepno_target_in){
        stepno_target_in = data->fStepNo;
        pos_target_in = data->fPrePosition - pos_target;
        mom_target_in = data->fPreMomentum;
        pos_target_in.RotateY(ang_target); // Change to target frame
        mom_target_in.RotateY(ang_target);
      }
    }
    else if (data->fDetectorName=="PDC"){

      trackIDs.insert(data->fTrackID);
      TString layer = data->fModuleName;        // "U", "X" or "V"
      auto& stepNo = stepNos[data->fID][layer]; // fID: 0 for PDC1, 1 for PDC2
      auto& position = positions[data->fID][layer];

      // Find the first and last step in each layer
      // Note: if `layer` was not hit, `stepNo` would be initialized as {0,0}.
      if (stepNo[0] == 0 && stepNo[1] == 0) {
        stepNo = {data->fStepNo, data->fStepNo};
        position = {data->fPrePosition, data->fPostPosition};
      }
      else if (data->fStepNo < stepNo[0]) {
        stepNo[0] = data->fStepNo;
        position[0] = data->fPrePosition;
      }
      else if (data->fStepNo > stepNo[1]) {
        stepNo[1] = data->fStepNo;
        position[1] = data->fPostPosition;
      }
    }
  }
  fPDCTrackNo = trackIDs.size();

  // OK_PDC: all the 3 layers are hit
  fOK_PDC1 = (stepNos[0].size() == 3 && positions[0].size() == 3);
  fOK_PDC2 = (stepNos[1].size() == 3 && positions[1].size() == 3);
  fOK_Target = stepno_target_in > 0;

  TVector3 pos_pdc1 = fFragSimParameter->fPDC1Position;
  TVector3 pos_pdc2 = fFragSimParameter->fPDC2Position;

  if (fOK_PDC1)
  {
    auto& pos = positions[0];
    TVector3 posU = LabToPDC((pos["U"][0] + pos["U"][1])*0.5) - pos_pdc1;
    TVector3 posX = LabToPDC((pos["X"][0] + pos["X"][1])*0.5) - pos_pdc1;
    TVector3 posV = LabToPDC((pos["V"][0] + pos["V"][1])*0.5) - pos_pdc1;

    fPDC1U = (posU.X() - posU.Y())/sqrt(2);
    fPDC1X = posX.X();
    fPDC1V = (posV.X() + posV.Y())/sqrt(2);
  }
  if (fOK_PDC2)
  {
    auto& pos = positions[1];
    TVector3 posU = LabToPDC((pos["U"][0] + pos["U"][1])*0.5) - pos_pdc2;
    TVector3 posX = LabToPDC((pos["X"][0] + pos["X"][1])*0.5) - pos_pdc2;
    TVector3 posV = LabToPDC((pos["V"][0] + pos["V"][1])*0.5) - pos_pdc2;

    fPDC2U = (posU.X() - posU.Y())/sqrt(2);
    fPDC2X = posX.X();
    fPDC2V = (posV.X() + posV.Y())/sqrt(2);
  }
  if (fOK_Target)
  {
    fTargetX = pos_target_in.X(); // Neglect the target thickness
    fTargetY = pos_target_in.Y();
    fTargetTheta = mom_target_in.Theta()/TMath::Pi()*180;
    fTargetPhi = mom_target_in.Phi()/TMath::Pi()*180;
    fTargetEnergy = mom_target_in.Energy() - mom_target_in.M();
  }

  return 0;
}
//____________________________________________________________________
int FragSimDataConverter_Basic::ClearBuffer()
{
  fTargetX = -99;
  fTargetY = -99;
  fTargetTheta = -99;
  fTargetPhi = -999;
  fTargetEnergy = -99;
  fOK_Target = false;
  fPDC1U = -999;
  fPDC1X = -999;
  fPDC1V = -999;
  fPDC2U = -999;
  fPDC2X = -999;
  fPDC2V = -999;
  fPDCTrackNo = 0;
  fOK_PDC1 = false;
  fOK_PDC2 = false;
  return 0;
}
//____________________________________________________________________
