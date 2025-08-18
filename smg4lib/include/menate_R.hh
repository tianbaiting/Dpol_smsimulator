//----------------------------------------------------------------------
//----------------------------------------------------------------------
//
// menate.hh 
//
// Description - a GEANT4 discrete process that models neutron
// elastic scattering with non-relativistic kinematics.
//
// started on 16 April 2007
//
// a class derived from G4VDiscreteProcess
// - Includes all processes (elastic and inelastic) from original
// menate program, including kinematics
//
// Written by: Brian Roeder, LPC Caen, email - roeder@lpccaen.in2p3.fr
// Translated from Fortran program menates.for by P. Desesquelles et al.
// Nucl. Instr. and Meths. A 307 Pg. 366 (1991)
// Uses cross sections from A. Del Geuerra et al., NIM 135 Pg. 337 (1976).
// except for n-p scattering total cross sections that come from MCNPx.
//----------------------------------------------------------------------
// To include in physics list :
//  #include "menate.hh"
//  pManager = G4Neutron::Neutron()->GetProcessManager();
// 	
//    G4String NeutronProcessName = "menate_neutron";
//    menate* theMenate = new menate(NeutronProcessName);
//    theMenate->SetMeanFreePathCalcMethod("ORIGINAL");
//
//  pManager->AddDiscreteProcess(theMenate);
// 
// -> Need also to set an environment variable called: MENATEG4XS
//    to point towards the total cross section data file directory.
//----------------------------------------------------------------------
// version comments
//----------------------------------------------------------------------
// version 0.1 - 18 Apr. 2008
// added elastic scattering and kinetmatics from :
// n+H1 -> n + p out (elastic)
// n+12C -> n + 12C (elastic)
//------------------------------------------------------------
// version 1.0 - 23 Apr 2008 -> working version
// modified functions and cross section read-in functions
// added all reactions considered in original MENATE including:
// n+H -> n+p elastic
// n+12C -> n+12C elastic
// n+12C -> n'+12C+gamma(4.4MeV)
// n+12C -> a+9Be
// n+12C -> n'+ 3a
// n+12C -> p + 12B
// n+12C -> n' + p + 11B (breakup)
// n+12C -> n' + n + 11C (breakup)
// All reactions considered together for MeanFreePath and
// reaction probability consideration
// MeanFreePath considers only total reaction probability
// PostStepDoIt chooses reaction
// **** This version includes fix in "Available_Eng" for
// Alpha2 in process 12C(n,n')3a (bug in FORTRAN version of MENATE)
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// version 1.1 - 24 Apr 2008 -> working version
// added function "ShareGammaEngC12()" to kinematics for 12C(n,n'g)12C*
// process as in original MENATE FORTRAN code. This change effects NE213
// efficiency in DEMON module between about 5-11 MeV, making the result
// of the GEANT+MENATE efficiency calc. closer to that obtained by original
// MENATE code. Efficiency below 5 MeV in GEANT code not changed.
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// 25 April 2008 - version 1.2 BTR
// ---- Add Normalization of Momentum ThreeVector in GenMomDir to avoid
// G4 Error Message. Noted that Norm diff from 1 is always less then 1e-8, so 
// error is probably due to slight rounding error.
//-------------------------------------------------------------------------
// 28 April 2008 - version 1.2b BTR
// ---- Added getenv command to ReadCrossSectionFile() Function
// Program looks for environment variable MENATEG4XS to find cross sections
// for use with menate.cc, otherwise throws exception.
//-------------------------------------------------------------------------
// 29 April 2008 - version 1.3 BTR
// --- Noted that MeanFreePath and reaction selection calculations are 
// different from method used in previous version. Added function
// SetMeanFreePathCalcMethod() to switch between calc in previous
// version and that method used specifically by MENATE
// Modified GetMeanFreePath() and ChooseReaction() part of PostStepDoIt to add
// MENATE original method.
//-------------------------------------------------------------------------
// 30 Apr 2008 - Final, unmodified verison of MENATE ! - BTR
//------------------------------------------------------------------------- 
// 24 August 2012 R. Tanaka (Tokyo Tech).
//                - fix G4Exception for Geant4-9.5
//                - add C inelastic only
//                - fix some inconsistency between .hh and .cc (.hh was old one)
//-------------------------------------------------------------------------

#ifndef menate_R_hh 
#define menate_R_hh

#include "globals.hh"
#include "G4VDiscreteProcess.hh"
#include "G4ios.hh"
#include "Randomize.hh" 
#include "G4Track.hh"
#include "G4Step.hh"
#include "G4ParticleTypes.hh"
#include "G4ParticleTable.hh"
#include "G4IonTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4DynamicParticle.hh"
// removed in geant4.10.05
//#include "G4NucleiPropertiesTableAME03.hh"
#include "G4ThreeVector.hh"
#include "G4LorentzVector.hh"
#include "G4VParticleChange.hh"

#include "G4Material.hh"
#include "G4UnitsTable.hh"

#ifndef CS_Class
#define CS_Class 1

class CrossSectionClass
{
private:
  // Data Members - each data member has one part of datafile!
 
  G4String ElementName;

  G4int NumberOfLines;
  G4double KinEng;
  G4double TotalCrossSection;

public:

  // constructor
  CrossSectionClass()
  {
   NumberOfLines = 0;
   ElementName = "Nothing";
   KinEng = 0;
   TotalCrossSection = 0;
  }

  // destructor
  ~CrossSectionClass() 
  {;}

public:
  // Setting and Access Functions

  void SetNumberOfLines(G4int Lines)
  { NumberOfLines = Lines; }

  void SetElementName(G4String ElName)
  { ElementName = ElName; }

  void SetKinEng(G4double Energy)
  { KinEng = Energy; }

  void SetTotalCrossSection(G4double CrossSection)
  { TotalCrossSection = CrossSection; }

  G4int GetNumberOfLines()
  { return NumberOfLines; }

  G4String GetElementName()
  { return ElementName; }

  G4double GetKinEng()
  { return KinEng; }

  G4double GetTotalCrossSection()
  { return TotalCrossSection; } 

  void DumpData()
  {
    G4cout << "Number of Lines Total " << NumberOfLines
           << ", Element Name = " << ElementName
           << ", KinEng = " << KinEng
           << " , CrossSection = " << TotalCrossSection << G4endl;
  }

}; 
#endif 

class menate_R : public G4VDiscreteProcess
{
public:

  // constructor
  menate_R(const G4String& processName="menate_R");

  // destructor
  ~menate_R();

public:
  // These are the functions in the Process: Derived from G4VDiscreteProcess.hh

  G4bool IsApplicable(const G4ParticleDefinition& aParticle);
  // Decides if process applicable or not. Only valid for Neutrons
  // at some energy.

  G4double GetMeanFreePath(const G4Track& aTrack, G4double previousStepSize,
                           G4ForceCondition* condition); 
  // Returns MeanFreePath for a particle in a given material in mm

  G4VParticleChange* PostStepDoIt(const G4Track &aTrack, const G4Step &aStep);
  // This is the important function where you define the process
  // Returns steps, track and secondary particles

  void SetMeanFreePathCalcMethod(G4String Method);

private:

 // Hide assignment operator as private 
  menate_R& operator=(const menate_R &right);

  // Copy constructor
  menate_R(const menate_R&);

  // Chooses Reaction in PostStepDoIt

  G4String ChooseReaction();
  G4double NP_AngDist(G4double NEng);
  G4double NC12_DIFF(G4double NEng);

  // Other Pre-Defined Class Variables and Functions are below!

  G4double Absolute(G4double Num);
  G4double SIGN(G4double A1, G4double B2);

  // Returns Cross Section for a given element, energy
  G4double GetCrossSection(G4double KinEng, CrossSectionClass* theXS);

  void ReadCrossSectionFile(G4String FileName, CrossSectionClass* theXS);

  G4double GetXSInterpolation(G4double KinEng, G4double LEng, G4double HEng, 
                               G4double LXS, G4double HXS);

  // Functions translated from FORTRAN "MENATE" code 

  G4ThreeVector GenMomDir(G4ThreeVector MomIn, G4double theta, G4double phi);

  G4double ShareGammaEngC12(G4double A_Eng);

  G4double Evaporate_Eng(G4double AMass, G4double Avail_Eng);
 
private:

  // Class Variables

  G4bool H_Switch;
  G4bool C_Switch; 

  // Hold Number Density for H or C
  G4double Num_H;  
  G4double Num_C;

  // Variables used in MENATE functions above

  G4double AMass_Material;  // Atomic Mass NE213 (according to MENATE)
  G4String CalcMeth; // Change to calculate MeanFreePath with original
                     // MENATE method.

  // integers for ShareGammaEngC12 Function
  G4int Num_gamma_400k;   // Num of 400k gammas
  G4int Num_gamma_4439k;  // Num of 4.4MeV gammas

  G4double Sigma;
  G4double ProbDistPerReaction[10];
  G4double ProbTot;

  //  CrossSectionClass theHydrogenXS[173];
  CrossSectionClass theHydrogenXS[180];
  //  CrossSectionClass theCarbonXS[841];
  CrossSectionClass theCarbonXS[847];

  //  CrossSectionClass theC12NGammaXS[28];
  CrossSectionClass theC12NGammaXS[36];
  CrossSectionClass theC12ABe9XS[31];
  CrossSectionClass theC12NPB12XS[14];

  CrossSectionClass theC12NNPB11XS[9];
  CrossSectionClass theC12N2NC11XS[10];
  CrossSectionClass theC12NN3AlphaXS[34];


  // for storing current scattering element

  G4String ElementName;
  G4int A;             // Integer Mass of Recoil (from GetN())
  G4int Z;             // Integer Charge of Recoil (from GetZ())

  G4double Pi;
  G4double Two_Pi;

};
#endif
