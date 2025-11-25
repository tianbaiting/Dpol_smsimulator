#ifndef TDetectorSIMPARAMETER_HH
#define TDetectorSIMPARAMETER_HH

#include "TSimParameter.hh"
#include "TNamed.h"
#include "TVector3.h"
#include "TString.h"

class TDetectorSimParameter : public TSimParameter
{
public:
  TDetectorSimParameter(TString name="");
  virtual ~TDetectorSimParameter();
  virtual void Print(Option_t* option="") const;// interited from TNamed
  friend std::ostream& operator<<(std::ostream& out,  const TDetectorSimParameter& prm);

public:
  Int_t    fID;
  Int_t    fLayer;
  Int_t    fSubLayer;
  TString  fDetectorType;
  TString  fDetectorName;
  TVector3 fPosition;// position of whole system
  TVector3 fSize;// size of Neut
  TVector3 fAngle;// 0: rot angle around X-axis (deg), 1:Y, 2:Z

  ClassDef(TDetectorSimParameter, 1)
};

#endif
