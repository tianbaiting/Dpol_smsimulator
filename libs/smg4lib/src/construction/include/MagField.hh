#ifndef MAGFIELD_HH
#define MAGFIELD_HH

#include "G4MagneticField.hh"
#include "TString.h"

class MagField : public G4MagneticField
{
public:  // with description
                       
  MagField();
  ~MagField();

  void GetFieldValue(const G4double Pos[4],
		     G4double *B     ) const;
  void LoadMagneticField(TString filename);
  void SetMagAngle(G4double val){fMagAngle = val;}
  void SetFieldFactor(double factor){fFieldFactor=factor;}
  Double_t GetFieldFactor(){return fFieldFactor;}
  TString GetFieldFileName(){return fMagFieldFile;}

private:
  G4bool IsLoaded;
  G4double fMagAngle;
  TString fMagFieldFile;
  double fFieldFactor;

  G4double Bx[301][81][301];
  G4double By[301][81][301];
  G4double Bz[301][81][301];
  
};

#endif
