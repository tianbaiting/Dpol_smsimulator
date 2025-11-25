#ifndef TNEBULASIMPARAMETER_HH
#define TNEBULASIMPARAMETER_HH

#include "TDetectorSimParameter.hh"
#include "TSimParameter.hh"
#include "TNamed.h"
#include "TVector3.h"
#include <map>

class TNEBULASimParameter : public TSimParameter
{
public:
  TNEBULASimParameter(TString name="NEBULAParameter");
  virtual ~TNEBULASimParameter();

  TDetectorSimParameter *FindDetectorSimParameter(Int_t id);

  virtual void Print(Option_t* option="") const;// interited from TNamed
  friend std::ostream& operator<<(std::ostream& out,  const TNEBULASimParameter& prm);


public:

  bool     fIsLoaded;
  Int_t    fNeutNum;// Total number of Neutron detctor
  Int_t    fVetoNum;// Total number of Veto detctor
  Int_t    fLayerNum;// Total number of Layers
  Int_t    fSubLayerNum; // Total number of SubLayers
  Int_t    fNeutNumPerLayer;// Total number of Neutron detectors per 1 layer
  Int_t    fVetoNumPerLayer;// Total number of Neutron detectors per 1 layer
  Int_t    fNeutNumPerSubLayer;// Total number of Neutron detectors per 1 wall
  Int_t    fVetoNumPerSubLayer;// Total number of Neutron detectors per 1 wall

  TVector3 fPosition;// position of whole system
  TVector3 fNeutSize;// size of Neut
  TVector3 fVetoSize;// size of Veto
  TVector3 fSize;// size of whole system (for Mother volume def.)
  TVector3 fAngle;// 0: rot angle around X-axis (deg), 1:Y, 2:Z

  // for Converter
  Double_t fTimeReso;// resolution of one PMT
  Double_t fV_scinti;// velocity of light in scintillator (mm/ns)
  Double_t fAttLen_Neut;// Attenuation length for Neut (mm)
  Double_t fAttLen_Veto;// Attenuation length for Neut (mm)
  Double_t fQ_factor;// correction factor for light output

  //       ID  parameter
  std::map<int,TDetectorSimParameter> fNEBULADetectorParameterMap;

  ClassDef(TNEBULASimParameter, 1)
};

#endif
