#include "TArtNEBULAPlusPla.hh"

ClassImp(TArtNEBULAPlusPla)

TArtNEBULAPlusPla::TArtNEBULAPlusPla()
    : TObject(),
      fID(-1), fLayer(-1), fSubLayer(-1),
      fTime(0), fEdep(0),
      fQU(0), fQD(0), fTU(0), fTD(0),
      fPos(0, 0, 0),
      fIsVeto(false)
{}

TArtNEBULAPlusPla::~TArtNEBULAPlusPla() {}

void TArtNEBULAPlusPla::Clear(Option_t*)
{
  fID = -1; fLayer = -1; fSubLayer = -1;
  fTime = 0; fEdep = 0;
  fQU = 0; fQD = 0; fTU = 0; fTD = 0;
  fPos.SetXYZ(0, 0, 0);
  fIsVeto = false;
}
