#ifndef TFragSIMPARAMETER_HH
#define TFragSIMPARAMETER_HH

#include "TSimParameter.hh"
#include "TNamed.h"
#include "TVector3.h"

class TFragSimParameter : public TSimParameter
{
public:
  TFragSimParameter(TString name="FragParameter");
  virtual ~TFragSimParameter();
  virtual void Print(Option_t* option="") const;// interited from TNamed
  friend std::ostream& operator<<(std::ostream& out,  const TFragSimParameter& prm);

public:
  Double_t fTargetThickness;  // target thickness in mm
  TVector3 fTargetPosition;   // position at laboratory coordinate in mm
  Double_t fTargetAngle;      // angle (clockwise) in rad

  Double_t fPDCAngle;         // PDC angle (clockwise) in rad
  TVector3 fPDC1Position;     // position at rotated coordinate in mm
  TVector3 fPDC2Position;     // position at rotated coordinate in mm

  Double_t fDumpAngle;        // dump angle (counterclockwise) in rad
  TVector3 fDumpPosition;     // position at rotated coordinate in mm

  ClassDef(TFragSimParameter, 1)
};

#endif
