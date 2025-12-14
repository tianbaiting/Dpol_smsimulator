#ifndef DIPOLECONSTRUCTION_HH
#define DIPOLECONSTRUCTION_HH

#include "G4VUserDetectorConstruction.hh"
#include "G4ThreeVector.hh"
#include "MagField.hh"

class G4LogicalVolume;
class G4VPhysicalVolume;
class G4Material;
class DetectorConstruction;

class DipoleConstruction : public G4VUserDetectorConstruction
{
  friend class DetectorConstruction;
  
public:
  DipoleConstruction();
  virtual ~DipoleConstruction();

  G4VPhysicalVolume* Construct();
  virtual G4LogicalVolume* ConstructSub();

  G4VPhysicalVolume* PutSAMURAIMagnet(G4LogicalVolume* ExpHall_log);
  G4VPhysicalVolume* PutSAMURAIMagnet(G4LogicalVolume* ExpHall_log, G4double angle);
  void SetMagField(G4String filename);
  void SetAngle(G4double val);
  void SetMagFieldFactor(double factor);
  void PlotMagField();

  G4double GetAngle(){return fAngle;}
  TString GetFieldFileName(){return fMagField->GetFieldFileName();}
  Double_t GetFieldFactor(){return fMagField->GetFieldFactor();}

protected:
  G4double fAngle;

  G4LogicalVolume* fLogicDipole;

  G4Material* fDipoleMaterial;
  G4Material* fWorldMaterial;
  G4Material* fVacuumMaterial;

  MagField* fMagField;      //pointer to the magnetic field
  double    fMagFieldFactor;

};

#endif

