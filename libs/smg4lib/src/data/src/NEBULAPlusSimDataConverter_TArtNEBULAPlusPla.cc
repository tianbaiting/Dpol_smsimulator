#include "NEBULAPlusSimDataConverter_TArtNEBULAPlusPla.hh"

//#include "TNEBULAPlusSimData.hh"
#include "TSimData.hh"
#include "TNEBULAPlusSimParameter.hh"
#include "SimDataManager.hh"
#include "SimDataInitializer.hh"
#include "SimDataConverter.hh"

#include "TArtNEBULAPlusPla.hh"

#include "TTree.h"
#include "TClonesArray.h"
#include "TRandom.h"

#include <iostream>
//____________________________________________________________________
NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::NEBULAPlusSimDataConverter_TArtNEBULAPlusPla(TString name)
    : SimDataConverter(name),
      fIncludeResolution(true)
{
}


//____________________________________________________________________
NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::~NEBULAPlusSimDataConverter_TArtNEBULAPlusPla()
{
  delete fNEBULAPlusPlaArray;
}
//____________________________________________________________________
int NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::Initialize()
{
  SimDataManager *sman = SimDataManager::GetSimDataManager();
  fNEBULAPlusSimParameter = (TNEBULAPlusSimParameter *)sman->FindParameter("NEBULAPlusParameter");
  if (fNEBULAPlusSimParameter == 0)
  {
    std::cout << "NEBULAPlusSimDataConveter_TArtNEBULAPlusPla : NEBULAPlusParameter is not found."
              << std::endl;
    return 1;
  }

  Int_t NumOfDetector = fNEBULAPlusSimParameter->fNeutNum + fNEBULAPlusSimParameter->fVetoNum;

  fNEBULAPlusSimDataArray = sman->FindSimDataArray("NEBULAPlusSimData");
  if (fNEBULAPlusSimDataArray == 0)
  {
    std::cout << "NEBULAPlusSimDataConveter_TArtNEBULAPlusPla : NEBULAPlusSimDataArray is not found."
              << std::endl;
    return 1;
  }

  fNEBULAPlusPlaArray = new TClonesArray("TArtNEBULAPlusPla", NumOfDetector);
  fNEBULAPlusPlaArray->SetOwner();
  fNEBULAPlusPlaArray->SetName("NEBULAPlusPla");
  return 0;
}
//____________________________________________________________________
int NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::DefineBranch(TTree *tree)
{
  if (fDataStore)
    tree->Branch("NEBULAPlusPla", &fNEBULAPlusPlaArray);
  return 0;
}
//____________________________________________________________________
int NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::ConvertSimData()
{
  fDataBufferMap.clear();

  std::map<int,tmp_data>::iterator it;
  for (auto&& hit: *fNEBULAPlusSimDataArray)
  {
    auto simdata = (TSimData*)hit;
    tmp_data* data = &fDataBufferMap[simdata->fID];
    data->q += simdata->fEnergyDeposit;
    data->light += MeVtoMeVee(
        simdata->fPreKineticEnergy,
        simdata->fPreKineticEnergy - simdata->fEnergyDeposit,
        simdata->fParticleName.Data(),
        simdata->fPDGCode);
    if (data->id == 0 || simdata->fPreTime < data->t)
    { // find the earliest hit in each module
      data->id = simdata->fID;
      data->t = simdata->fPreTime;
      data->pos = simdata->fPrePosition;
    }
  }

  SimDataManager *sman = SimDataManager::GetSimDataManager();
  auto NEBULAPlusParameter = (TNEBULAPlusSimParameter*)sman
                         ->FindParameter("NEBULAPlusParameter");
  if (!NEBULAPlusParameter) {
    std::cerr << "NEBULAPlusSimDataConverter_TArtNEBULAPlusPla: NEBULAPlusParameter not found at ConvertSimData(), aborting conversion." << std::endl;
    return 1;
  }

  for (auto&& [ID,data]: fDataBufferMap)
  {
    TDetectorSimParameter *prm = NEBULAPlusParameter->FindDetectorSimParameter(ID);
    if (!prm) {
      std::cerr << "NEBULAPlusSimDataConverter_TArtNEBULAPlusPla: WARNING - detector parameter for ID=" << ID
                << " not found. Skipping this ID." << std::endl;
      // do not attempt to fill pla with invalid prm; continue to next ID
      continue;
    }

    // `fNEBULAPlusPlaArray` has been filled by `npla` modules, so we create the next
    // module `pla` at the (npla)th slot, which is (*fNEBULAPlusPlaArray)[npla].
    Int_t npla = fNEBULAPlusPlaArray->GetEntries();
    TArtNEBULAPlusPla *pla = new ((*fNEBULAPlusPlaArray)[npla]) TArtNEBULAPlusPla;

    // store converted data
    pla->fID       = ID;
    pla->fLayer    = prm->fLayer;
    pla->fSubLayer = prm->fSubLayer;

    if (fIncludeResolution)
    {
      Double_t AttLen, Xsiz, Ysiz, Zsiz;
      if (prm->fDetectorType == "Neut")
      {
        Xsiz = NEBULAPlusParameter->fNeutSize.x();
        Ysiz = NEBULAPlusParameter->fNeutSize.y();
        Zsiz = NEBULAPlusParameter->fNeutSize.z();
        AttLen = NEBULAPlusParameter->fAttLen_Neut;
      }
      else if (prm->fDetectorType == "Veto")
      {
        Xsiz = NEBULAPlusParameter->fVetoSize.x();
        Ysiz = NEBULAPlusParameter->fVetoSize.y();
        Zsiz = NEBULAPlusParameter->fVetoSize.z();
        AttLen = NEBULAPlusParameter->fAttLen_Veto;
      }
      Double_t V_scinti = NEBULAPlusParameter->fV_scinti;

      Double_t y_at_detec = data.pos.y() - (prm->fPosition.y()
                            + NEBULAPlusParameter->fPosition.y());
      Double_t dy_u = 0.5 * Ysiz - y_at_detec;
      Double_t dy_d = 0.5 * Ysiz + y_at_detec;

      Double_t tu = data.t + dy_u / V_scinti;
      Double_t td = data.t + dy_d / V_scinti;
      tu += gRandom->Gaus(0, NEBULAPlusParameter->fTimeReso);
      td += gRandom->Gaus(0, NEBULAPlusParameter->fTimeReso);
      Double_t t = 0.5 * (tu + td) - 0.5 * Ysiz / V_scinti;
      Double_t dt = td - tu;

      Double_t qu = data.light * exp(-dy_u / AttLen);
      Double_t qd = data.light * exp(-dy_d / AttLen);
      // Double_t q = sqrt(qu*qd);
      Double_t q = sqrt(qu * qd) * exp(0.5 * Ysiz / AttLen);

      Double_t x = prm->fPosition.x() + NEBULAPlusParameter->fPosition.x();
      Double_t z = prm->fPosition.z() + NEBULAPlusParameter->fPosition.z();

      Double_t y = 0.5 * dt * V_scinti + prm->fPosition.y()
                   + NEBULAPlusParameter->fPosition.y();

      pla->fPos.SetXYZ(x, y, z);
      pla->fTime = t;
      pla->fQU   = qu;
      pla->fQD   = qd;
      pla->fEdep = q;
    }
    else // without resolutions
    {
      pla->fPos  = data.pos;
      pla->fTime = data.t;
      pla->fEdep = data.q;
      pla->fQU   = data.q;
      pla->fQD   = data.q;
    }
  }

  return 0;
}
//____________________________________________________________________
int NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::ClearBuffer()
{
  fNEBULAPlusPlaArray->Delete();
  return 0;
}
//____________________________________________________________________
// fit by
// D.Fox et al.    Nucl. Inst. and Meth. A374(1996)63
Double_t NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::MeVeeRelation_FOX
  (Double_t T, std::string name, Int_t pdgcode)
{
  Double_t a1, a2, a3, a4;

  //    std::cout << pdgcode << std::endl;
  //  std::cout << name << " " << pdgcode << std::endl;
  if (name == "e-" || name == "e+" || name == "mu-" || name == "mu+")
  { // leptons
    a1 = 1;
    a2 = 0;
    a3 = 0;
    a4 = 0;
  }
  else if (name == "proton")
  {
    a1 = 0.902713;
    a2 = 7.55009;
    a3 = 0.0990013;
    a4 = 0.736281;
  }
  else if (name == "deuteron")
  {
    a1 = 0.891575;
    a2 = 12.2122;
    a3 = 0.0702262;
    a4 = 0.782977;
  }
  else if (name == "triton")
  {
    a1 = 0.881489;
    a2 = 15.9064;
    a3 = 0.0564987;
    a4 = 0.811916;
  }
  else if (name == "He3")
  {
    a1 = 0.803919;
    a2 = 34.4153;
    a3 = 0.0254322;
    a4 = 0.894859;
  }
  else if (name == "alpha")
  {
    a1 = 0.781501;
    a2 = 39.3133;
    a3 = 0.0217115;
    a4 = 0.910333;
  }
  else
  {
    pdgcode -= 1000000000;
    double z = pdgcode / 10000;
    //    std::cout << z << std::endl;
    //    double a = pdgcode % 10000;
    if (z == 3)
    { // Li
      // Li7
      a1 = 0.613491;
      a2 = 57.1372;
      a3 = 0.0115948;
      a4 = 0.951875;
    }
    else if (z == 4)
    { // be
      // Be9
      a1 = 0.435772;
      a2 = 45.538;
      a3 = 0.0104221;
      a4 = 0.916373;
    }
    else if (z == 5)
    { // B
      // B11
      a1 = 0.350273;
      a2 = 34.4664;
      a3 = 0.0112395;
      a4 = 0.912711;
    }
    else if (z == 6)
    { // C
      // C12
      a1 = 0.298394;
      a2 = 25.5679;
      a3 = 0.0130345;
      a4 = 0.908512;
    }
    else
    {
      // C12
      a1 = 0.298394;
      a2 = 25.5679;
      a3 = 0.0130345;
      a4 = 0.908512;
    }
  }
  return a1 * T - a2 * (1 - exp(-a3 * pow(T, a4)));
}
//____________________________________________________________________
Double_t NEBULAPlusSimDataConverter_TArtNEBULAPlusPla::MeVtoMeVee(Double_t Tin, Double_t Tout, std::string name, Int_t pdgcode)
{
  Double_t Tein, Teout, dTe;
  Double_t Tout_chk = Tout;
  if (Tout_chk < 0)
    Tout_chk = 0;

  Tein = MeVeeRelation_FOX(Tin, name, pdgcode);
  Teout = MeVeeRelation_FOX(Tout_chk, name, pdgcode);
  dTe = Tein - Teout;

  if (dTe < 0)
    dTe = 0;

  return dTe;
}
//____________________________________________________________________
