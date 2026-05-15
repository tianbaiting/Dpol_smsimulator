#ifndef TArtNEBULAPlusPla_HH
#define TArtNEBULAPlusPla_HH

#include "TObject.h"
#include "TVector3.h"

// Per-bar hit data for NEBULA-Plus, written to ROOT tree as TClonesArray<TArtNEBULAPlusPla>.
// Field names mirror the anaroot TArtNEBULAPla pattern so user analysis code can be cross-ported,
// but the class is independent (not derived from any anaroot type).
class TArtNEBULAPlusPla : public TObject {
public:
  TArtNEBULAPlusPla();
  virtual ~TArtNEBULAPlusPla();

  Int_t    fID;          // 101..122, 201..222 (Wall-A Neut Sub1/2);
                         // 301..323, 401..423 (Wall-B Neut Sub1/2);
                         // 501..512 (Wall-A Veto); 601..612 (Wall-B Veto)
  Int_t    fLayer;       // 1 = Wall-A, 2 = Wall-B
  Int_t    fSubLayer;    // 0 = Veto, 1 = front sub, 2 = back sub
  Double_t fTime;        // ns, mean of fTU and fTD
  Double_t fEdep;        // MeV, total energy deposit per event in this bar
  Double_t fQU;          // upper PMT integrated charge (arbitrary units)
  Double_t fQD;          // lower PMT integrated charge
  Double_t fTU;          // upper PMT time (ns)
  Double_t fTD;          // lower PMT time (ns)
  TVector3 fPos;         // reconstructed hit position (mm, target frame)
  Bool_t   fIsVeto;

  void Clear(Option_t* opt = "") override;

  ClassDef(TArtNEBULAPlusPla, 1);
};

#endif
