//--------------------------------------------------------------------
//--------------------------------------------------------------------
//
// menate_R.cc 
//
// Description - contains member functions declared in menate_R.hh
//
// started on 6 May 2008
//
// Written by: Brian Roeder, LPC Caen, email - roeder@lpccaen.in2p3.fr
//
// -Kinematics processes in PostStepDoIt translated from FORTRAN code
//  "MENATE" by P. Desesquelles et al., NIM A 307 (1991) 366.
// -MeanFreePath and Reaction Selection based on discussions with JL Lecouey
// -Uses cross sections from MENATE, but these can be modified by changing
//  the data files.
// -Modifications : Uses non-isotropic angular distributions for
//                  other processes.
//---------------------------------------------------------------------
// Version Comments - (other changes/fixes noted in menate_R.hh)
//  
// 23 April 2008 - version 1.0 -> working version -> by Brian Roeder
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
// **** This version includes fix in "Available_Eng" for
// Alpha2 in process 12C(n,n')3a (bug in FORTRAN version of MENATE)
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// 19 May 2008 - new version with modifications - BTR
//             - Modified angular distributions for n+p, n+12C elastic
// other changes listed in menate_R.hh.
//
//------------------------------------------------------------------
// 24 August 2012 R. Tanaka (Tokyo Tech).
//                - fix G4Exception for Geant4-9.5
//                - add C inelastic only
//                - fix some inconsistency between .hh and .cc (.hh was old one)
//------------------------------------------------------------------
 
#include "menate_R.hh"
#include <cmath>
#include <iomanip>
#include "G4SystemOfUnits.hh"

menate_R::menate_R(const G4String& processName) : G4VDiscreteProcess(processName)
{
  Pi = CLHEP::pi;
  Two_Pi = 2.*Pi;

  AMass_Material = 12.; // Atomic Mass of C12 for certain inelastic reactions
                        // If Used with materials other than C12, need to
                        // give atomic Mass here or in PostStepDoIt Method (as for NE213)

  CalcMeth="ORIGINAL";

  G4cout << "Constructor for menate_R process was called! " << G4endl;
  G4cout << "A non-relativistic model for n - scattering ";
  G4cout << "on carbon and hydrogen or materials composed of it (e.g. NE213)" << G4endl;
  G4cout << "You are using MENATE_R version 1.4, finished 19 May 2008 - BTR" << G4endl;
  
  const G4MaterialTable* theMaterialList = G4Material::GetMaterialTable();
  G4int NumberOfMaterials = G4Material::GetNumberOfMaterials(); 

  // Read Each Material to get List of Elements
  H_Switch = false;
  C_Switch = false;

  for(G4int i=0; i<NumberOfMaterials; i++)
    {
      // Searches to see if Carbon and Hydrogen defined.
      // Can add other elements to this process later if needed.  

      const G4Material* theMaterial = (*theMaterialList)[i];
      G4String MaterialName = theMaterial->GetName();
      const G4ElementVector* theElementList = theMaterial->GetElementVector();
      G4int NumberOfElements = theMaterial->GetNumberOfElements();

      for(G4int j=0;j<NumberOfElements; j++)
	{
	  if(j == 0)
	    {G4cout << "Reading Elements in Material : " << MaterialName << G4endl;}

	  const G4Element* theElement = (*theElementList)[j];
	  G4String theElementName = theElement->GetName();
	  
	  if(theElementName == "Hydrogen" || theElementName == "H")
	    { H_Switch = true; } // Hydrogen defined 
	  else if(theElementName == "Carbon" || theElementName == "C")
	    { C_Switch = true; } // Carbon defined
	}
    }

  // Load Cross Sections if Element is Found

  if(H_Switch == true)
    { ReadCrossSectionFile("Hydrogen1_el.dat",theHydrogenXS);}
	  
  if(C_Switch == true)
    { 
      // Load all Carbon Elastic and Inelastic Cross Sections
      ReadCrossSectionFile("Carbon12_el.dat",theCarbonXS); 
      ReadCrossSectionFile("Carbon12_nng4_4.dat",theC12NGammaXS);
      ReadCrossSectionFile("Carbon12_na9Be.dat",theC12ABe9XS);
      ReadCrossSectionFile("Carbon12_np12B.dat",theC12NPB12XS);
      ReadCrossSectionFile("Carbon12_nnp11B.dat",theC12NNPB11XS);
      ReadCrossSectionFile("Carbon12_2n11C.dat",theC12N2NC11XS);
      ReadCrossSectionFile("Carbon12_nn3a.dat",theC12NN3AlphaXS);
    }
	    
  G4cout << "Finished Building Cross Section Table! " << G4endl;
}


menate_R::~menate_R()
{;}


G4double menate_R::Absolute(G4double Num)
{
  if(Num < 0.)
    { Num *= -1.;}

  return Num;
}

G4double menate_R::SIGN(G4double A1, G4double B2)
{
  // Does FORTRAN sign command
  // Return "A1" with the sign of B2

  if(B2 >= 0.)
    {A1 = Absolute(A1);}
  else
    {A1 = -1.*Absolute(A1);}
  return A1;
}

void menate_R::ReadCrossSectionFile(G4String FileName, CrossSectionClass* theReactionXS)
{

 system("echo $MENATEG4XS");
 if(!getenv("MENATEG4XS")) 
   {
     G4cerr << "Please setenv MENATEG4XS to point to MENATE cross-section files!" << G4endl;
     //     G4Exception("Program aborted in menate::ReadCrossSectionFile()"); // ryuki tanaka
     G4Exception(0,0,FatalException,"Program aborted in menate::ReadCrossSectionFile()");
   }
 G4String  DirName = getenv("MENATEG4XS");

 FileName = DirName+"/"+FileName;

  G4String ElementName;
  G4int NumberOfLines = 0;

  std::fstream theFile;
  theFile.open(FileName, std::fstream::in );
  theFile >> NumberOfLines; 

  if(theFile.good())
    {
      theFile >> ElementName;

      G4cout << "Loading Data For : " << ElementName << " , FileName = " << FileName << G4endl;
      for(G4int k=0; k<NumberOfLines; k++)
	{
	  G4double theEnergy;
	  G4double theCrossSection;
	  theFile >> theEnergy >> theCrossSection;

	  theReactionXS[k].SetNumberOfLines(NumberOfLines);
	  theReactionXS[k].SetElementName(ElementName);
          theReactionXS[k].SetKinEng(theEnergy*MeV);
	  theReactionXS[k].SetTotalCrossSection(theCrossSection*barn);
	  //theReactionXS[k].DumpData();
	}
      G4cout << "Successfully Loaded !" << G4endl;
      theFile.close();
    }
  else
    {
      G4cerr << "File = " << FileName << " not found or in improper format." << G4endl;
     //      G4Exception("Program aborted in menate::ReadCrossSectionFile() method!"); // ryuki tanaka
     G4Exception(0,0,FatalException,"Program aborted in menate::ReadCrossSectionFile() method!");
    }
} 


G4double menate_R::GetCrossSection(G4double KinEng, CrossSectionClass* theReactionXS)
{
  G4double CrossSection=0.;
  G4int NumberOfLines;

      NumberOfLines = theReactionXS[0].GetNumberOfLines();	
      
      if(KinEng > (theReactionXS[NumberOfLines-1].GetKinEng()))
	{ 
	  //        G4cout << "The Neutron Energy is higher than the Total Cross Section DataFile!" << G4endl;
	  //	G4cerr << "Setting Cross Section = 0 !!!!!!!!!!!!!!!!!!!!!!!" << G4endl;
	return 0.;
	}
  
      for(G4int k=0; k<NumberOfLines; k++)
	{
	  if((theReactionXS[k].GetKinEng()) == KinEng)
	  { 
	    CrossSection = theReactionXS[k].GetTotalCrossSection();
	    //G4cout << CrossSection/barn << " barn" << G4endl;
	    return CrossSection;
	  }
	  else if ((theReactionXS[k].GetKinEng()) > KinEng)
	  {
	    //G4cout << "Calculating average Cross Section (no exact match)" << G4endl;
	    //G4cout << KinEng/MeV << G4endl;
	    G4double LowEng = (theReactionXS[k-1].GetKinEng());
	    G4double HighEng = (theReactionXS[k].GetKinEng());
	    G4double LowerXS = (theReactionXS[k-1].GetTotalCrossSection());
	    G4double UpperXS = (theReactionXS[k].GetTotalCrossSection()); 
	    CrossSection = GetXSInterpolation(KinEng,LowEng,HighEng,LowerXS,UpperXS);
	    //G4cout << CrossSection/barn << " barn" << G4endl;
	    return CrossSection;
	  }
	}
	 
  return CrossSection;
}

G4double menate_R::GetXSInterpolation(G4double KinEng, G4double LowEng, G4double HighEng, 
                                         G4double LowXS, G4double HighXS)
{
  G4double slope = (HighXS-LowXS)/(HighEng-LowEng);
  G4double y_inter = HighXS-(slope*HighEng);

  G4double Interpol_XS = y_inter + (slope*KinEng);

  return Interpol_XS;
}

G4ThreeVector menate_R::GenMomDir(G4ThreeVector MomDirIn, G4double theta, G4double phi)
{
  // Generates final momentum direction in frame of initial particle
  // follows routine of "MEN_COMPUTE_DIRECTION"
  // Accounts for initial direction of neutron not parallel to z-axis

  // Variables used in calculation

  G4double SinTh = sin(theta);
  G4double CosTh = cos(theta);
  G4double SinPh = sin(phi);
  G4double CosPh = cos(phi);
  G4double CONS = sqrt( Absolute((1. - pow(MomDirIn[2],2))) );

  G4ThreeVector MomDirOut;
  MomDirOut[0] = SinTh*CosPh;
  MomDirOut[1] = SinTh*SinPh;
  MomDirOut[2] = CosTh;

  if(CONS > 1e-8)
    {
      // If CONS > 1e-8, not on z-axis, so need to change frames as below:
      MomDirOut[0] = (SinTh*CosPh*MomDirIn[0]*MomDirIn[2]/CONS) +
	             (SinTh*SinPh*MomDirIn[1]/CONS) +
	             (CosTh*MomDirIn[0]);
      MomDirOut[1] = (SinTh*CosPh*MomDirIn[1]*MomDirIn[2]/CONS) -
	             (SinTh*SinPh*MomDirIn[0]/CONS) +
	             (CosTh*MomDirIn[1]);
      MomDirOut[2] = -(SinTh*CosPh*CONS) + CosTh*MomDirIn[2];
    }

  G4double Norm = sqrt(pow(MomDirOut[0],2)+pow(MomDirOut[1],2)+pow(MomDirOut[2],2));
  //G4double DiffNorm = Absolute(1-Norm);// not used

  if(Norm != 1.)
    {
//Commented on 23 July 2012 by FD to avoid error...
/*
      if(DiffNorm >= 1e-9)
	{G4Exception(" Momentum Direction not a unit vector in menate_R::GenMomDir(): Aborted!");}
*/
      // Fixes slight norm diff due to rounding to avoid G4 error

      MomDirOut[0] /= Norm;
      MomDirOut[1] /= Norm;
      MomDirOut[2] /= Norm;
    }

  return MomDirOut;
}

G4double menate_R::ShareGammaEngC12(G4double Available_Eng)
{
  // 24 Apr. 2008 - This function translated from original FORTRAN "MENATE_R"
  // Bleeds excitation energy of C12* into 400 keV gamma rays unless there
  // is enough energy to create the 4.439 MeV (2+->g.s.) gamma. 
  // Addition of this function effects neutron det. efficiency in region
  // between 5-11 MeV (lowers eff. by a few percent).

  // 19 May 2008 - Removed counter for 400 keV gamma rays - BTR

  Num_gamma_4439k = 0;

  G4double Remaining_Eng = Available_Eng;

  while(Remaining_Eng > 4.439*MeV)
    {
      Num_gamma_4439k++;
      Remaining_Eng -= 4.439*MeV;
    }

 G4double Tot_Gamma_Eng = static_cast<G4double>(Num_gamma_4439k)*4.439*MeV;

 return Tot_Gamma_Eng;
}


G4double menate_R::Evaporate_Eng(G4double AMass,G4double Available_Eng)
{
  // Calculate an energy included between 0 and Available_Eng following a probability
  // density dp/de = e/n*exp(-e/t) --- copied from MENATE_R fortran code
  
  G4double Evap_Eng = 0.;

  G4double Temperature = sqrt(8.*Available_Eng/AMass);

  if(Temperature == 0.)
    { 
      Evap_Eng = 0.;
      return Evap_Eng; 
    }

  G4double ALEA = G4UniformRand();
  G4double XEV = Available_Eng/Temperature;

  if(XEV < 1e-4)
    { Evap_Eng = 0.; }
  else
    {
      G4double ZNORM = 1. - (1.-(1.+XEV)*exp(-XEV))*ALEA;
      if(ZNORM == 0.)
	{
	  Evap_Eng = 0.;
	  return Evap_Eng;
	} 
      G4double YEV = XEV*ALEA;
      G4double XEV;

      do{
	  XEV = YEV;
	  YEV = log((1.+XEV)/ZNORM);
	  if( XEV == 0.)
	    {
	      Evap_Eng = 0.;
	      return Evap_Eng;
	    }
	}
      while(Absolute((YEV/XEV)-1.) > 1e-6);

	Evap_Eng = YEV*Temperature;
    }

  if(Evap_Eng >= Available_Eng)
    { Evap_Eng = Available_Eng; }

  return Evap_Eng;
}


void menate_R::SetMeanFreePathCalcMethod(G4String Method)
{
  CalcMeth = Method;
  G4cout << "The MeanFreePath and Reaction Calculations method is set to : " << CalcMeth << G4endl;
}


G4String menate_R::ChooseReaction()
{
  // Chooses Reaction that is used in PostStepDoIt 
  G4String theReaction = "NoReaction";

  if(H_Switch == true && C_Switch == false)
    { theReaction = "N_P_elastic"; }
  else if(C_Switch == true)
    {
     // Get number of probabilities considered
     // Only include if Cross Section > 0 !
     G4double ProbSigma[8];
     G4double ProbLimit[8];
     if(H_Switch == false)
       {
	 ProbSigma[0] = 0.;
	 ProbLimit[0] = 0.;
       }

     for(G4int i=0; i<8; i++)
       {
	 ProbSigma[i] = ProbDistPerReaction[i]/ProbTot;
	 
	 if( i == 0 )
	   { ProbLimit[i] = ProbSigma[i]; }
	 else
	   { ProbLimit[i] = ProbLimit[i-1] + ProbSigma[i]; }
	 //G4cout << "ProbSigma " << i << " is : " << ProbSigma[i] << " ProbLimit = " << ProbLimit[i] << G4endl;
       }
    // Set Reaction Type
    // H(n,np) - N_P_elastic
    // 12C(n,n) - N_C12_elastic
    // 12C(n,n'gamma)12C - N_C12_NGamma
    // 12C(n,alpha)9Be - N_C12_A_Be9
    // 12C(n,p)12B     - N_C12_P_B12
    // 12C(n,np)11B    - N_C12_NNP_B11
    // 12C(n,2n)11C    - N_C12_N2N_C11
    // 12C(n,n')3alpha - N_C12_NN3Alpha

     while(theReaction == "NoReaction")
       {
	 // Loop through reactions until one is chosen.
	 // If Prob = 0. for a Reaction, disregarded
	 G4double RecRandnum = G4UniformRand();
	 if(RecRandnum < ProbLimit[0] && ProbSigma[0] > 0.)
	   { theReaction = "N_P_elastic"; }
	 if(RecRandnum >= ProbLimit[0] && RecRandnum < ProbLimit[1] && ProbSigma[1] > 0.)
	   { theReaction = "N_C12_elastic"; }
	 if(RecRandnum >= ProbLimit[1] && RecRandnum < ProbLimit[2] && ProbSigma[2] > 0.)
	   { theReaction = "N_C12_NGamma"; }
	 if(RecRandnum >= ProbLimit[2] && RecRandnum < ProbLimit[3] && ProbSigma[3] > 0.)
	   { theReaction = "N_C12_A_Be9"; } 
	 if(RecRandnum >= ProbLimit[3] && RecRandnum < ProbLimit[4] && ProbSigma[4] > 0.)
	   { theReaction = "N_C12_P_B12"; }
	 if(RecRandnum >= ProbLimit[4] && RecRandnum < ProbLimit[5] && ProbSigma[5] > 0.)
	   { theReaction = "N_C12_NNP_B11"; } 
	 if(RecRandnum >= ProbLimit[5] && RecRandnum < ProbLimit[6] && ProbSigma[6] > 0.)
	   { theReaction = "N_C12_N2N_C11"; } 
	 if(RecRandnum >= ProbLimit[6] && RecRandnum < ProbLimit[7] && ProbSigma[7] > 0.)
	   { theReaction = "N_C12_NN3Alpha"; }
       }

     //G4cout << "Reaction Chosen was : " << theReaction << G4endl;
   }
 return theReaction;
}

//-----------------------------------------------------------
// Angular Distribution generators - Added 6 May 2008 BTR
//-----------------------------------------------------------
G4double menate_R::NP_AngDist(G4double NEng)
{
  //NP scattering from DEMONS by Reese, Yariv and Sailor et al.
  //originally written by Stanton
  //based on equation - a + 3b cos(theta)
  // adjusted "30." in original routine with "29.", including
  // minimum energy for "dip" to 29 MeV - BTR 7 May 2008.

  G4double CosCM = 0.;

  if(NEng < 29.*MeV)
    {
      CosCM = 2.*G4UniformRand()-1.;
      return CosCM;
    }

 //      a = 3.0/(3.0+rMax)
 //      b = rMax*a/3.0
 //      RAT = a/(a+b)

  G4double rMax = NEng/29.-1.;
  G4double RAT = 1./(1.+(rMax/3.));
  G4double ran1, ran2, absRan2;

  ran1 = G4UniformRand();
  ran2 = 2.*G4UniformRand()-1.;
  absRan2 = Absolute(ran2);

   if(absRan2 < RAT)
    {CosCM = 2.0*ran1-1.0;}
  else
    {CosCM = SIGN(pow(ran1,(1./3.)),ran2);}

 return CosCM;
}

G4double menate_R::NC12_DIFF(G4double NEng)
{
  // Generates Angular Distribution for n+12C scattering
  // in center of mass system. Kinematics in PostStepDoIt
  // convert result to lab angle. 

  // Uses parameterization in DEMONS by Reese, Yariv and Sailor et al.

  // n+12C cross section is considered isotropic for NEng < 7.35 MeV.
  // The orignal "FitParam" was tweaked to 0.5 to provide a better fit
  // to the C12 diffractive peak at forward angles between 7.3 and 70 MeV.
  // For NEng>70MeV, FitParam is returned to original value 
  // (1.17 fits 96 MeV) with a linear increase (shown below).
  // Fits ang dist data up to 150 MeV.
  // Assume a "global" split between diffractive and non-diffractive
  // cross sections at 0.9.

  // References for n+12C angular distributions (elastic) include :
  // Z.P Chen et al. - J. Phys. G: Nucl. Part. Phys. 31, 1249 (2005).
  // T. Kaneko et al. - Phys. Rev. C 46, 298 (1992).   
  // Z.M. Chen et al. - J. Phys. G: Nucl. Part. Phys. 19, 877 (1993).
  // J.S. Petler et al. - Phys. Rev. C 32, 673 (1985).
  // P. Mermod et al. - Phys. Rev. C 74, 054002 (2006).
  // J.H. Osborne et al. - Phys. Rev. C 70, 054613 (2004).

  // Modifications added by Brian Roeder, LPC Caen, 19 May 2008.

  G4double CosCM = 0.;
  G4double DiffSigma;
  G4double FitParam = 0.5;
  if(NEng >= 70.*MeV)
    {FitParam = 0.021613*NEng-0.90484;} // Fit to AngDist Data

  if(NEng < 7.35*MeV)
    {
      CosCM = 1.-2.*G4UniformRand();
      return CosCM;
    }
  else
    {
      G4double rand = G4UniformRand();
      if(rand > 0.9)
	{CosCM = 1.-2.*G4UniformRand();}
      else
	{
	  DiffSigma = GetCrossSection(NEng,theCarbonXS)/barn;	  
	  do
	    {
	      G4double rand2 = 0.;
	      rand2 = G4UniformRand();
	      //CosCM = 1.0+log(rand2)/1.17/DiffSigma/NEng;
	      CosCM = 1.0+log(rand2)/FitParam/DiffSigma/NEng;
	    }
	  while(CosCM < -1. || CosCM > 1.);	 	  
	}
    }

  return CosCM;
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
// The Below are the GEANT4:: G4VDiscreteProcess class methods that must
// be overridden to run this process in G4.
//----------------------------------------------------------------------
//----------------------------------------------------------------------

 
G4bool menate_R::IsApplicable(const G4ParticleDefinition& particle)
{ 
// returns "true" if this model can be applied to the particle-type
  //G4cout << "IsApplicable loaded!" << G4endl;

  if (particle == *(       G4Neutron::Neutron()       )) 
    {
      //G4cout << "Found the Neutron ! " << G4endl;
      return true;
    }
  else
    //    {G4Exception("Model \"menate_R\" can only be used for neutron scattering!");} // ryuki tanaka
    {
      G4cerr << "Model \"menate_R\" can only be used for neutron scattering!" << G4endl;
      G4Exception(0,0,FatalException,"Model \"menate_R\" can only be used for neutron scattering!");
    }
  return false;
}


G4double menate_R::GetMeanFreePath(const G4Track& aTrack, G4double previousStepSize, 
                                      G4ForceCondition* condition) 
{
  //G4cout << "menate_R GetMeanFreePath called!" << G4endl;
  
  const G4Step* theStep = aTrack.GetStep();
  G4StepPoint* thePoint = theStep->GetPreStepPoint();
  G4String theVolume =  thePoint->GetPhysicalVolume()->GetName();
 
  /*
  if(theVolume == "target")  
    {*condition = Forced;}
  else
    {*condition = NotForced;}
  */  

  *condition = NotForced;
  
  previousStepSize = 0.1*mm;

  const G4DynamicParticle* projectile = aTrack.GetDynamicParticle();
  G4double theKinEng = projectile->GetKineticEnergy();

  if(theKinEng < 1e-4*MeV)
  {return DBL_MAX;} // Energy too low for this model!

  // Now read the medium to get mean free path at this energy!

 const G4Material* theMaterial = aTrack.GetMaterial();

 // GetVecNbOfAtomsPerVolume() returns (NumberOfAtoms/mm3)! 
 const G4double* N_Per_Volume = theMaterial->GetVecNbOfAtomsPerVolume();

 // Get List of Elements in Material, also get number!
 const G4ElementVector* theElementList = theMaterial->GetElementVector();
 G4int NumberOfElements = theMaterial->GetNumberOfElements();

 // Only do MeanFreePath calculation if in a material
 // that has Hydrogen or Carbon !
 H_Switch = false;
 C_Switch = false;

 for(G4int ne=0;ne<NumberOfElements;ne++)
   {
     const G4Element* theElement = (*theElementList)[ne];

     //G4cout << "The Element is : " << theElement->GetName() << G4endl;
     Z = static_cast<int>(theElement->GetZ());
     A = static_cast<int>(theElement->GetN());  // returns Atomic Weight

     if(Z == 1 && A == 1)
       { 
	 H_Switch = true; 
	 Num_H = N_Per_Volume[ne];
       }
     else if(Z == 6 && A == 12)
       { 
	 C_Switch = true; 
	 Num_C = N_Per_Volume[ne];
       }
   }

 if(H_Switch == false && C_Switch == false)
   { return DBL_MAX; }

 // Calculate Total Mean Free Path (From Total Cross Section and Density)
 // Does not decide reaction --- PostStepDoIt decides reaction if n-scattering occurs

 if(CalcMeth == "ORIGINAL")
   {
     G4int k_tot = 0;

     if(H_Switch == true)
       {
	 // Returns Cross Section in Barns, pre-loaded in constructor
	 Sigma = GetCrossSection(theKinEng,theHydrogenXS);
	 //G4cout << "Sigma =  " << Sigma/barn << G4endl; 
	 ProbDistPerReaction[0] = Sigma*Num_H;
	 k_tot = 1;
       }
     else
       {
	 // Clears array element
	 ProbDistPerReaction[0] = 0.;
       }

     if(C_Switch == true)
       {
	 Sigma = GetCrossSection(theKinEng,theCarbonXS);
	 ProbDistPerReaction[1] = Sigma*Num_C;	
	 Sigma = GetCrossSection(theKinEng,theC12NGammaXS);
	 ProbDistPerReaction[2] = Sigma*Num_C;
	 Sigma = GetCrossSection(theKinEng,theC12ABe9XS);
	 ProbDistPerReaction[3] = Sigma*Num_C;
	 //	 Sigma = GetCrossSection(theKinEng,theC12NPB12XS);
	 //	 ProbDistPerReaction[4] = Sigma*Num_C;
	 ProbDistPerReaction[4] = 0;
	 Sigma = GetCrossSection(theKinEng,theC12NNPB11XS);
	 ProbDistPerReaction[5] = Sigma*Num_C;
	 Sigma = GetCrossSection(theKinEng,theC12N2NC11XS);
	 ProbDistPerReaction[6] = Sigma*Num_C;
	 Sigma = GetCrossSection(theKinEng,theC12NN3AlphaXS);
	 ProbDistPerReaction[7] = Sigma*Num_C;

	 k_tot = 8;
       }
     else
       {
	 // Clears array element
	 for(G4int i=1;i<8;i++)
	   { ProbDistPerReaction[i] = 0.; }
       }
 
     ProbTot = 0.;  // Reset Probtot

     for(G4int k=0; k<k_tot; k++)
       {ProbTot += ProbDistPerReaction[k];}
    
     if(ProbTot > 1.e-6)
       {
	 G4double total_mean_free_path = 1/(ProbTot);
	 //G4cout << "The mean free path was : " << total_mean_free_path/cm << " cm" << G4endl; 
	 return total_mean_free_path;
       }
   }
 else if(CalcMeth == "MENATE")
   {
     // MENATE MeanFreePath method is as follows:
     // Adds up total cross section for hydrogen and carbon and then uses molecular weight,
     // density of NE213 to calculate MeanFreePath

     // 30 April 2008 - BTR - After comparison with program DECOI, determined the difference
     // between the ORIGINAL mean free path calculation above and the MENATE calculation
     // is that the MENATE calculation lacks a factor 1.213 that is multiplied to the n-p
     // total cross section. The 1.213 factor comes from the ratio of Carbon to Hydrogen in 
     // NE213. This disagreement is "fixed" in the original MENATE FORTRAN code by mulitiplying
     // the Hydrogren cross sections in the data file by a factor of 1.2. 
     // I have left here the original method in the FORTRAN MENATE code for mean free path
     // calculation for comparison with the ORIGINAL (standard) method. Note that the methods
     // do not agree unless the n-p total cross sections in the data file are muliplied by 1.2
     // as in the FORTRAN version data files. 

     G4String MaterialName = theMaterial->GetName();
     if(MaterialName == "NE213")
       { AMass_Material = 13.213; }
     else
       { AMass_Material = theMaterial->GetA(); }

     // Get Cross Section Sum at this energy
     ProbDistPerReaction[0] = (GetCrossSection(theKinEng,theHydrogenXS))/barn;
     ProbDistPerReaction[1] = (GetCrossSection(theKinEng,theCarbonXS))/barn;	
     ProbDistPerReaction[2] = (GetCrossSection(theKinEng,theC12NGammaXS))/barn;
     ProbDistPerReaction[3] = (GetCrossSection(theKinEng,theC12ABe9XS))/barn;
     ProbDistPerReaction[4] = (GetCrossSection(theKinEng,theC12NPB12XS))/barn;
     ProbDistPerReaction[5] = (GetCrossSection(theKinEng,theC12NNPB11XS))/barn;
     ProbDistPerReaction[6] = (GetCrossSection(theKinEng,theC12N2NC11XS))/barn;
     ProbDistPerReaction[7] = (GetCrossSection(theKinEng,theC12NN3AlphaXS))/barn;

     // Add up Cross Sections
     ProbTot = 0.;  // Reset Probtot

     for(G4int k=0; k<8; k++)
       {ProbTot += ProbDistPerReaction[k];}

     G4double NE213dens = 0.874;
     G4double NumAvagadro = 0.60221367;   // Num Avagadro*conversion to barns
     G4double Constant = 10.*AMass_Material/NumAvagadro/NE213dens;    
     G4double total_mean_free_path = Constant/ProbTot;
     //G4cout << "The mean free path was : " << total_mean_free_path/cm << " cm" << G4endl; 
	
     if(total_mean_free_path < DBL_MAX)
       {return total_mean_free_path;}
      
   }
 else if(CalcMeth == "carbon_el_only" && C_Switch == true)
   {
     // Added 7 May 2007 - For carbon elastic scattering only
      // Clears array elements
	 for(G4int i=0;i<8;i++)
	   { ProbDistPerReaction[i] = 0.; }
	 Sigma = GetCrossSection(theKinEng,theCarbonXS);
	 ProbDistPerReaction[1] = Sigma*Num_C;
	 ProbTot = ProbDistPerReaction[1];
	 if(ProbTot > 1.e-6)
	   {
	     G4double total_mean_free_path = 1/(ProbTot);
	     //G4cout << "The mean free path was : " << total_mean_free_path/cm << " cm" << G4endl; 
	     return total_mean_free_path;
	   }
   }
 else if(CalcMeth == "carbon_inel_only" && C_Switch == true)
   {
     for(G4int i=0;i<8;i++)
       { ProbDistPerReaction[i] = 0.; }
     Sigma = GetCrossSection(theKinEng,theC12NGammaXS);
     ProbDistPerReaction[2] = Sigma*Num_C;
     Sigma = GetCrossSection(theKinEng,theC12ABe9XS);
     ProbDistPerReaction[3] = Sigma*Num_C;
     Sigma = GetCrossSection(theKinEng,theC12NPB12XS); // too large, but leave this channel // ryuki tanaka
     ProbDistPerReaction[4] = Sigma*Num_C;
     Sigma = GetCrossSection(theKinEng,theC12NNPB11XS);
     ProbDistPerReaction[5] = Sigma*Num_C;
     Sigma = GetCrossSection(theKinEng,theC12N2NC11XS);
     ProbDistPerReaction[6] = Sigma*Num_C;
     Sigma = GetCrossSection(theKinEng,theC12NN3AlphaXS);
     ProbDistPerReaction[7] = Sigma*Num_C;

     ProbTot = 0.;  // Reset Probtot
     for(G4int k=0; k<8; k++)
       {ProbTot += ProbDistPerReaction[k];}

     if(ProbTot > 1.e-6)
       {
	 G4double total_mean_free_path = 1/(ProbTot);
	 //G4cout << "The mean free path was : " << total_mean_free_path/cm << " cm" << G4endl; 
	 return total_mean_free_path;
       }
   }

 return DBL_MAX;
}


G4VParticleChange* menate_R::PostStepDoIt(const G4Track& aTrack, const G4Step& aStep)
{
  // Now we tell GEANT what to do if MeanFreePath condition is satisfied!
  // Overrides PostStepDoIt function in G4VDiscreteProcess

  const G4DynamicParticle* projectile = aTrack.GetDynamicParticle();
  //const G4ParticleDefinition* particle = projectile->GetDefinition();

  const G4Material* theMaterial = aTrack.GetMaterial();
  G4String MaterialName = theMaterial->GetName();

  G4double KinEng_Int = projectile->GetKineticEnergy();
  G4ThreeVector MomDir_Int = projectile->GetMomentumDirection();
  G4double GlobalTime = aTrack.GetGlobalTime(); 
  G4ThreeVector thePosition = aTrack.GetPosition();
  G4TouchableHandle theTouchable = aTrack.GetTouchableHandle();

  /*
  // These variables not used...
  G4double LocalTime = aTrack.GetLocalTime();
  G4double ProperTime = aTrack.GetProperTime();
  G4double theProjMass = projectile->GetMass();
  */

 if(KinEng_Int < 1e-5*MeV)
   {
    G4cout << "Neutron Energy = " << KinEng_Int << G4endl;
    G4cout << "Do Nothing, Kill Event! " << G4endl;
    aParticleChange.ProposeTrackStatus(fStopAndKill);
    aParticleChange.ProposeEnergy(0.);
    aParticleChange.ProposePosition(thePosition);
    return pParticleChange;
   } 

 if(MaterialName == "NE213")
   { AMass_Material = 13.213; }

  // Define Reaction -
  // Chooses Reaction based on ProbDistPerReaction Vector defined in GetMeanFreePath()

 G4String ReactionName = ChooseReaction();

  //
  //*******MENATE Reaction Kinematics*************
  //
    
if(ReactionName == "N_P_elastic")
  {
    G4double mass_ratio = 939.6/938.3;  // Mass ratio N/P

    G4double cos_theta = 0.;
    G4double theta_cm = 0.;
    G4double theta_N  = 0.;
    G4double phi_N = 0.;
    G4double T_N = 0.;
    G4ThreeVector MomDir_N;

    G4double cos_theta_P = 0.;
    G4double theta_P = 0.;
    G4double phi_P = 0.;
    G4double T_P = 0.;
    G4ThreeVector MomDir_P;

    // *** 6 May 2008 - Added n-p angular distribution as in DEMONS - BTR
    // Made slight adjustment to routine (changed "30" to "29") to improve 
    // agreement with data.
    // Calculate recoil energy as in Knoll
       
    cos_theta = NP_AngDist(KinEng_Int); // cosine of cm_angle
    theta_cm = acos(cos_theta);

    // Following Knoll, calculate energy for proton first
    cos_theta_P = sqrt((1.-cos_theta)/2.);
    theta_P = acos(cos_theta_P);
    phi_P = 2.*Pi*G4UniformRand();

    T_P = KinEng_Int*pow(cos_theta_P,2);

    MomDir_P = GenMomDir(MomDir_Int,theta_P,phi_P);

    // scattered neutron ----------------
    // angles changed from cm to lab as in Marian and Young
    theta_N = atan(sin(theta_cm)/(cos_theta+mass_ratio));
    phi_N = phi_P+Pi;

    T_N = KinEng_Int - T_P;    

    MomDir_N = GenMomDir(MomDir_Int,theta_N,phi_N);
  
    // Generate a Secondary Neutron
    G4DynamicParticle* theSecNeutron = new G4DynamicParticle;
    G4ParticleDefinition* theSecNeutronDefinition = G4Neutron::Neutron();
    theSecNeutron->SetDefinition(theSecNeutronDefinition);
    theSecNeutron->SetKineticEnergy(T_N);
    theSecNeutron->SetMomentumDirection(MomDir_N);

    // Generate a Secondary Proton
    G4DynamicParticle* theSecProton = new G4DynamicParticle;
    G4ParticleDefinition* theSecProtonDefinition = G4Proton::Proton();
    theSecProton->SetDefinition(theSecProtonDefinition);
    theSecProton->SetKineticEnergy(T_P);
    theSecProton->SetMomentumDirection(MomDir_P);
   
    // Kill the Parent Neutron -----------------------
    aParticleChange.ProposeTrackStatus(fStopAndKill);
    aParticleChange.ProposeEnergy(0.);
    aParticleChange.ProposePosition(thePosition);
  
 // Set final tracks in motion!
    G4Track* theNTrack = new G4Track(theSecNeutron,GlobalTime,thePosition);
    theNTrack->SetTouchableHandle(theTouchable);
    G4Track* thePTrack = new G4Track(theSecProton,GlobalTime,thePosition);
    thePTrack->SetTouchableHandle(theTouchable);

    aParticleChange.SetNumberOfSecondaries(2);
    aParticleChange.AddSecondary(theNTrack);
    aParticleChange.AddSecondary(thePTrack);
  
    // G4cout << "Made it to the end ! " << G4endl;
  
  }
 else if(ReactionName == "N_C12_elastic")
   {
     G4double theta_N  = 0.;
     G4double phi_N = 0.;

     G4double T_N = 0.;
     G4double T_C12el = 0.;
     G4ThreeVector MomDir_N;
     G4ThreeVector MomDir_C12;

     // For n,12C elastic scattering, follows model of Knoll
     // Original MENATE distribution was isotropic in CM
     // G4double cos_sq_theta = G4UniformRand(); 

     // Calls diffractive angular distribution as in DEMONS
     G4double cos_theta_cm = NC12_DIFF(KinEng_Int);

     // Convert according to Knoll
     G4double cos_theta_lab = sqrt((1.-cos_theta_cm)/2.);

     G4double cos_sq_theta = pow(cos_theta_lab,2);
    
     // .2840237              =4*Mc12*Mn/(Mc12+Mn)^2
     T_C12el = 0.2840237*KinEng_Int*cos_sq_theta;

     T_N = KinEng_Int - T_C12el;

     if(T_N > 1.e-5)
       {
	 G4double arg = (sqrt(KinEng_Int)-sqrt(12.*T_C12el*cos_sq_theta))/sqrt(T_N);
	 if(arg > 1.)
	   {arg = 1.;}
	 theta_N = acos(arg);
         phi_N = 2.*Pi*G4UniformRand();
       }

     MomDir_N = GenMomDir(MomDir_Int,theta_N,phi_N);

     // Carbon angles
     G4double theta_C12 = acos(sqrt(cos_sq_theta));
     G4double phi_C12 = phi_N+Pi;

     MomDir_C12 = GenMomDir(MomDir_Int,theta_C12,phi_C12);
    
     // Generate a Secondary Neutron
     G4DynamicParticle* theSecNeutron = new G4DynamicParticle;
     G4ParticleDefinition* theSecNeutronDefinition = G4Neutron::Neutron();
     theSecNeutron->SetDefinition(theSecNeutronDefinition);
     theSecNeutron->SetKineticEnergy(T_N);
     theSecNeutron->SetMomentumDirection(MomDir_N);

     // Generate a Secondary C12
     G4DynamicParticle* theSecC12 = new G4DynamicParticle;
     // GetIon() Method works whether particle exists in physicslist or not. 
     // Arguements are GetIon(Charge,Mass,ExcitationEng)
//     G4ParticleDefinition* theSecC12Definition = G4ParticleTable::GetParticleTable()->GetIon(6,12,0.);
     G4ParticleDefinition* theSecC12Definition = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(6,12,0.);
     theSecC12->SetDefinition(theSecC12Definition);
     theSecC12->SetKineticEnergy(T_C12el);
     theSecC12->SetMomentumDirection(MomDir_C12);
   
     // Kill the Parent Neutron -----------------------
     aParticleChange.ProposeTrackStatus(fStopAndKill);
     aParticleChange.ProposeEnergy(0.);
     aParticleChange.ProposePosition(thePosition);
  
     // Set final tracks in motion!
     G4Track* theNTrack = new G4Track(theSecNeutron,GlobalTime,thePosition);
     theNTrack->SetTouchableHandle(theTouchable);
     G4Track* theC12Track = new G4Track(theSecC12,GlobalTime,thePosition);
     theC12Track->SetTouchableHandle(theTouchable);

     aParticleChange.SetNumberOfSecondaries(2);
     aParticleChange.AddSecondary(theNTrack);
     aParticleChange.AddSecondary(theC12Track);
  
    // G4cout << "Made it to the end ! " << G4endl;
   }
 else if(ReactionName == "N_C12_NGamma")
   {
     // 24 Apr. 2008 - BTR - This version replaces version in previous
     // menate.cc to 12C(n,n'gamma) reaction in orig FORTRAN MENATE

     G4double Available_Eng;

     if(KinEng_Int >= 8.5*MeV)
       { Available_Eng = 8.5*MeV*(G4UniformRand()); } // Max available Eng = 8.5 MeV ?
     else
       { Available_Eng = KinEng_Int*G4UniformRand(); }

     G4double Total_Gamma_Eng = ShareGammaEngC12(Available_Eng);

     G4ThreeVector MomDir_N;
     G4ThreeVector MomDir_C12;

     // N'

     Available_Eng = 0.9230769*KinEng_Int-Total_Gamma_Eng;
     if(Available_Eng <= 0.)
       {Available_Eng = 0.;} // = 0 in Fortran code - BTR 

     G4double cos_theta_iso = 1.-(2.*G4UniformRand());

     G4double T_N = KinEng_Int/169.+0.9230769*Available_Eng + 
                    (0.1478106*sqrt(KinEng_Int*Available_Eng))*cos_theta_iso;
     G4double theta_N = acos(cos_theta_iso);
     G4double phi_N = 2.*Pi*G4UniformRand();

     MomDir_N = GenMomDir(MomDir_Int,theta_N,phi_N);
 
     // C12*

     G4double T_C12 = KinEng_Int - T_N - Total_Gamma_Eng;
     if(T_C12 <= 0.)
       {T_C12 = 0.;}
     G4double theta_C12 = Pi - theta_N;
     G4double phi_C12 = Pi + phi_N;

     MomDir_C12 = GenMomDir(MomDir_Int,theta_C12,phi_C12);

     // Generate a Secondary Neutron
     G4DynamicParticle* theSecNeutron = new G4DynamicParticle;
     G4ParticleDefinition* theSecNeutronDefinition = G4Neutron::Neutron();
     theSecNeutron->SetDefinition(theSecNeutronDefinition);
     theSecNeutron->SetKineticEnergy(T_N);
     theSecNeutron->SetMomentumDirection(MomDir_N);

     // Generate a Secondary C12*
     G4DynamicParticle* theSecC12 = new G4DynamicParticle;
     // GetIon() Method works whether particle exists in physicslist or not. 
     // Arguements are GetIon(Charge,Mass,ExcitationEng)
     //G4ParticleDefinition* theSecC12Definition = G4ParticleTable::GetParticleTable()->GetIon(6,12,0.);
     G4ParticleDefinition* theSecC12Definition = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(6,12,0.);
     theSecC12->SetDefinition(theSecC12Definition);
     theSecC12->SetKineticEnergy(T_C12);
     theSecC12->SetMomentumDirection(MomDir_C12);

     // 4.439 MeV Gamma - emitted isotropically from reaction point..
     // -- 24 Apr 2008 - BTR - With 8.5 MeV limit given at start of 
     //    MENATE function for this reaction, can not produce more
     //    than one 4.439 MeV gamma. 
     // - 19 May 2008 - BTR
     // --- Removed 400 keV gamma ray counter from original MENATE code

     G4int NumberOfSec = 2+Num_gamma_4439k;
     aParticleChange.SetNumberOfSecondaries(NumberOfSec);

	 if(Num_gamma_4439k > 0)
	   {
	     G4double T_Gamma = 4.439*MeV;
	     G4double theta_Gamma = acos(1.-(2.*G4UniformRand()));
	     G4double phi_Gamma = 2.*Pi*G4UniformRand();
	     
	     G4ThreeVector MomDir_Gamma;
	     MomDir_Gamma = GenMomDir(MomDir_Int,theta_Gamma,phi_Gamma);

	     // Generate a Secondary Gamma (4.439 MeV)
	     G4DynamicParticle* theSecGamma = new G4DynamicParticle;
	     G4ParticleDefinition* theGammaDefinition = G4Gamma::Gamma();
	     theSecGamma->SetDefinition(theGammaDefinition);
	     theSecGamma->SetKineticEnergy(T_Gamma);
	     theSecGamma->SetMomentumDirection(MomDir_Gamma);

	     aParticleChange.SetNumberOfSecondaries(2+Num_gamma_4439k);

	     G4Track* theGammaTrack = new G4Track(theSecGamma,GlobalTime,thePosition);
	     theGammaTrack->SetTouchableHandle(theTouchable);
	     aParticleChange.AddSecondary(theGammaTrack);
	   }
    
     // Kill the Parent Neutron -----------------------
     aParticleChange.ProposeTrackStatus(fStopAndKill);
     aParticleChange.ProposeEnergy(0.);
     aParticleChange.ProposePosition(thePosition);
  
     // Set final tracks in motion!
     G4Track* theNTrack = new G4Track(theSecNeutron,GlobalTime,thePosition);
     theNTrack->SetTouchableHandle(theTouchable);
     G4Track* theC12Track = new G4Track(theSecC12,GlobalTime,thePosition);
     theC12Track->SetTouchableHandle(theTouchable);
     
     aParticleChange.AddSecondary(theNTrack);
     aParticleChange.AddSecondary(theC12Track);
     
    // G4cout << "Made it to the end ! " << G4endl;
 
   }
 else if(ReactionName == "N_C12_A_Be9")
   {
     // Copied from MENATE
     // Reaction can not occur if incoming neutron energy is below 6.176 MeV
     // Q Value = - 5.701 MeV

     // Uses formula for C12 recoil to calculate direction of 9Be

     G4ThreeVector MomDir_Alpha;
     G4ThreeVector MomDir_Be9;

     G4double Q_value = -5.701*MeV;
     // 0.9230769 = Mass_C12/(Mass_N+Mass_C12) - converts to CM frame energy
     G4double Available_Eng = 0.9230769*KinEng_Int+Q_value;

     if(Available_Eng <= 0.)
       { Available_Eng = 0.; }

     // the alpha

     G4double cos_theta_iso = 1.-(2.*G4UniformRand());

     // 0.0236686 = Malpha*MassN/(MassC12+MassN)^2
     // 0.6923077 = MBe9/(MassN+MassC12)
     // 0.2560155 = 2.*sqrt(Mn*MBe9*Malpha/(MassC12+MassN)^3)

     G4double T_Alpha = 0.0236686*KinEng_Int+0.6923077*Available_Eng+
                        (0.2560155*sqrt(KinEng_Int*Available_Eng))*cos_theta_iso;

     G4double theta_Alpha = 0.;
     G4double phi_Alpha = 0.;

     if(T_Alpha > 1.e-5)
       {
	 // 0.1538462 = sqrt(Malpha*Mn)/(Mn+Mc12)
	 // 0.8320503 = sqrt(MBe9/(MassC12+MassN))

	 G4double Arg = (0.1538462*sqrt(KinEng_Int)+0.8320503*sqrt(Available_Eng)*cos_theta_iso)/(sqrt(T_Alpha));

	 if(Arg > 1.)
	   { Arg = 1.; }

	 theta_Alpha = acos(Arg);
	 phi_Alpha = 2.*Pi*G4UniformRand();
       }
  
     MomDir_Alpha = GenMomDir(MomDir_Int,theta_Alpha,phi_Alpha);
 
     // Be9 - Need to check kinematics - maybe not right - 18 Apr 2008, BTR

     G4double T_Be9 = KinEng_Int + Q_value - T_Alpha;
     G4double theta_Be9 = Pi - theta_Alpha;
     G4double phi_Be9 = Pi + phi_Alpha;

     MomDir_Be9 = GenMomDir(MomDir_Int,theta_Be9,phi_Be9);

     // Generate a Secondary Alpha
     G4DynamicParticle* theSecAlpha = new G4DynamicParticle;
     G4ParticleDefinition* theSecAlphaDefinition = G4Alpha::Alpha();
     theSecAlpha->SetDefinition(theSecAlphaDefinition);
     theSecAlpha->SetKineticEnergy(T_Alpha);
     theSecAlpha->SetMomentumDirection(MomDir_Alpha);

     // Generate a Secondary Be9
     G4DynamicParticle* theSecBe9 = new G4DynamicParticle;
     // GetIon() Method works whether particle exists in physicslist or not. 
     // Arguements are GetIon(Charge,Mass,ExcitationEng)
     //G4ParticleDefinition* theSecBe9Definition = G4ParticleTable::GetParticleTable()->GetIon(4,9,0.);
     G4ParticleDefinition* theSecBe9Definition = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(4,9,0.);
     theSecBe9->SetDefinition(theSecBe9Definition);
     theSecBe9->SetKineticEnergy(T_Be9);
     theSecBe9->SetMomentumDirection(MomDir_Be9);

    // Kill the Parent Neutron -----------------------
     aParticleChange.ProposeTrackStatus(fStopAndKill);
     aParticleChange.ProposeEnergy(0.);
     aParticleChange.ProposePosition(thePosition);
  
     // Set final tracks in motion!
     G4Track* theAlphaTrack = new G4Track(theSecAlpha,GlobalTime,thePosition);
     theAlphaTrack->SetTouchableHandle(theTouchable);
     G4Track* theBe9Track = new G4Track(theSecBe9,GlobalTime,thePosition);
     theBe9Track->SetTouchableHandle(theTouchable);

     aParticleChange.SetNumberOfSecondaries(2);
     aParticleChange.AddSecondary(theAlphaTrack);
     aParticleChange.AddSecondary(theBe9Track);
  
    // G4cout << "Made it to the end ! " << G4endl;
   }
 else if(ReactionName == "N_C12_P_B12")
   {
     // Charge exchange Reaction copied from MENATE
     G4double Q_value = -12.587*MeV;

     G4ThreeVector MomDir_P;
     G4ThreeVector MomDir_B12;

     G4double Available_Eng = 0.9230769*KinEng_Int+Q_value;

     if(Available_Eng < 0.)
       { Available_Eng = 0.; }

     // the proton

     G4double cos_theta_iso = 1.-(2.*G4UniformRand());

     //1/169. = Mp*Mn/(Mn+MC12)^2
     // .9230769 = MB12/(Mn+MC12)
     // .1478106 = 2*sqrt(Mn*MB12*Mp/(Mc12+Mn)^3)

     G4double T_P = KinEng_Int/169. + 0.9230769*Available_Eng + 
                    (0.1478106*sqrt(KinEng_Int*Available_Eng))*cos_theta_iso;
     
     G4double theta_P = 0.;
     G4double phi_P = 0.;

     if(T_P > 1.e-5)
       {
	 // 1/13. = sqrt(Mp*Mn)/(Mn+Mc)
	 // 0.9607689 = sqrt(MB12/(Mn+MC12))

	 G4double Arg = (sqrt(KinEng_Int)/13. + 0.9607689*sqrt(Available_Eng)*cos_theta_iso)/(sqrt(T_P)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_P = acos(Arg);
	 phi_P = 2.*Pi*G4UniformRand();
       }

     MomDir_P = GenMomDir(MomDir_Int,theta_P,phi_P);

 // B12 - Need to check kinematics - maybe not right - 18 Apr 2008, BTR

     G4double T_B12 = KinEng_Int + Q_value - T_P;
     G4double theta_B12 = Pi - theta_P;
     G4double phi_B12 = Pi + phi_P;

     MomDir_B12 = GenMomDir(MomDir_Int,theta_B12,phi_B12);

     // Generate a Secondary Proton
     G4DynamicParticle* theSecProton = new G4DynamicParticle;
     G4ParticleDefinition* theSecPDefinition = G4Proton::Proton();
     theSecProton->SetDefinition(theSecPDefinition);
     theSecProton->SetKineticEnergy(T_P);
     theSecProton->SetMomentumDirection(MomDir_P);

     // Generate a Secondary B12
     G4DynamicParticle* theSecB12 = new G4DynamicParticle;
     // GetIon() Method works whether particle exists in physicslist or not. 
     // Arguements are GetIon(Charge,Mass,ExcitationEng)
     //G4ParticleDefinition* theSecB12Definition = G4ParticleTable::GetParticleTable()->GetIon(5,12,0.);
     G4ParticleDefinition* theSecB12Definition = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(5,12,0.);
     theSecB12->SetDefinition(theSecB12Definition);
     theSecB12->SetKineticEnergy(T_B12);
     theSecB12->SetMomentumDirection(MomDir_B12);

    // Kill the Parent Neutron -----------------------
     aParticleChange.ProposeTrackStatus(fStopAndKill);
     aParticleChange.ProposeEnergy(0.);
     aParticleChange.ProposePosition(thePosition);
  
     // Set final tracks in motion!
     G4Track* thePTrack = new G4Track(theSecProton,GlobalTime,thePosition);
     thePTrack->SetTouchableHandle(theTouchable);
     G4Track* theB12Track = new G4Track(theSecB12,GlobalTime,thePosition);
     theB12Track->SetTouchableHandle(theTouchable);

     aParticleChange.SetNumberOfSecondaries(2);
     aParticleChange.AddSecondary(thePTrack);
     aParticleChange.AddSecondary(theB12Track);
  
    // G4cout << "Made it to the end ! " << G4endl;
   }
 else if(ReactionName == "N_C12_NNP_B11")
   {
     // Reaction copied from MENATE
     // Treated as :
     // n + 12C ---> 12C* + n'
     // 12C*    ---> 11B + p

     G4double Q_value = -15.956*MeV;

     G4ThreeVector MomDir_N;
     G4ThreeVector MomDir_P;
     G4ThreeVector MomDir_B11;

     // Scattered neutron (n')
     G4double Available_Eng = 0.9230769*KinEng_Int+Q_value;

     if(Available_Eng < 0.)
       { Available_Eng = 1.e-3; }

     G4double Eng_Evap1 = Evaporate_Eng(AMass_Material,Available_Eng);

     G4double cos_theta_iso = 1.-2.*G4UniformRand();

     // 1/169. = (Mn/(Mn+Mc))^2
     // 0.1538462 = 2Mn/(Mn+Mc)

     G4double T_N = KinEng_Int/169. + Eng_Evap1 + (0.1538462*sqrt(KinEng_Int*Eng_Evap1))*cos_theta_iso;
     
     G4double theta_N = 0.;
     G4double phi_N = 0.;

     if(T_N > 1.e-5)
       {
	 // 1/13. = Mn/(Mn+Mc)

	 G4double Arg = (sqrt(KinEng_Int)/13. + (sqrt(Eng_Evap1)*cos_theta_iso))/(sqrt(T_N)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_N = acos(Arg);
	 phi_N = 2.*Pi*G4UniformRand();
       }

     MomDir_N = GenMomDir(MomDir_Int,theta_N,phi_N);

     // Scattered 12C*
     // 1/13. = Mn(Mc+Mn)
     // 1.0833333 = (Mc+Mn)/Mc

     G4double T_C12Star = KinEng_Int/13. + 1.0833333*Eng_Evap1 - T_N;

     G4double theta_C12Star = 0.;
     G4double phi_C12Star = phi_N+Pi;

     if(T_C12Star > 1.e-5)
       {
	 // 0.2664694 = sqrt(Mn*Mc)/(Mn+Mc)
	 // 0.2886751 = sqrt(Mn/Mc)
	 G4double Arg = (0.2664694*sqrt(KinEng_Int) - (0.2886751*sqrt(Eng_Evap1)*cos_theta_iso))/(sqrt(T_C12Star)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_C12Star = acos(Arg);
       }
     
     // Now the proton is emitted in the frame of the scattered C12*
     // At the end, the angles are put back into the frame of the original neutron
 
     Available_Eng = KinEng_Int + Q_value - T_N - T_C12Star;

     if(Available_Eng < 0.)
       { Available_Eng = 0.; }

     cos_theta_iso = 1.-(2.*G4UniformRand());

     // 1/12. = Mp/MC12
     // .9166667 = MB11/MC12
     // .5527708 = 2*sqrt(MB11*Mp)/Mc

     G4double T_P = T_C12Star/12. + 0.9166667*Available_Eng + 
                    (0.5527708*sqrt(Available_Eng*T_C12Star))*cos_theta_iso;
     
     G4double theta_C12_P = 0.;
     G4double phi_C12_P = 0.;

     if(T_P > 1.e-5)
       {
	 // 0.2886751 = sqrt(Mp/Mc)
	 // 0.9574271 = sqrt((MB11)/(MC12))

	 G4double Arg = (0.2886751*sqrt(T_C12Star) + 0.9574271*sqrt(Available_Eng)*cos_theta_iso)/(sqrt(T_P)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_C12_P = acos(Arg);
	 phi_C12_P = 2.*Pi*G4UniformRand();
       }

     // Now Start With Z-Axis and then go to incoming neutron frame ?
     //G4ThreeVector MomDir_ZAxis = G4ThreeVector(0.,0.,1.);

     G4ThreeVector MomDir_ZAxis = MomDir_Int;
     G4ThreeVector MomDir_C12Star = GenMomDir(MomDir_ZAxis,theta_C12Star,phi_C12Star);

     MomDir_P = GenMomDir(MomDir_C12Star,theta_C12_P,phi_C12_P);

     // B11 - Need to check kinematics - maybe not right - 18 Apr 2008, BTR

     G4double T_B11 = KinEng_Int + Q_value - T_N - T_P; // Remaining Eng
     if(T_B11 < 0.)
       {T_B11 = 1e-9;}

     G4double theta_B11 = Pi - theta_C12_P;
     G4double phi_B11 = Pi + phi_C12_P;

     MomDir_B11 = GenMomDir(MomDir_Int,theta_B11,phi_B11);

// Generate a Secondary Neutron
     G4DynamicParticle* theSecNeutron = new G4DynamicParticle;
     G4ParticleDefinition* theSecNDefinition = G4Neutron::Neutron();
     theSecNeutron->SetDefinition(theSecNDefinition);
     theSecNeutron->SetKineticEnergy(T_N);
     theSecNeutron->SetMomentumDirection(MomDir_N);

// Generate a Secondary Proton
     G4DynamicParticle* theSecProton = new G4DynamicParticle;
     G4ParticleDefinition* theSecPDefinition = G4Proton::Proton();
     theSecProton->SetDefinition(theSecPDefinition);
     theSecProton->SetKineticEnergy(T_P);
     theSecProton->SetMomentumDirection(MomDir_P);

     // Generate a Secondary B11
     G4DynamicParticle* theSecB11 = new G4DynamicParticle;
     // GetIon() Method works whether particle exists in physicslist or not. 
     // Arguements are GetIon(Charge,Mass,ExcitationEng)
     //G4ParticleDefinition* theSecB11Definition = G4ParticleTable::GetParticleTable()->GetIon(5,11,0.);
     G4ParticleDefinition* theSecB11Definition = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(5,11,0.);
     theSecB11->SetDefinition(theSecB11Definition);
     theSecB11->SetKineticEnergy(T_B11);
     theSecB11->SetMomentumDirection(MomDir_B11);

    // Kill the Parent Neutron -----------------------
     aParticleChange.ProposeTrackStatus(fStopAndKill);
     aParticleChange.ProposeEnergy(0.);
     aParticleChange.ProposePosition(thePosition);
  
     // Set final tracks in motion!
     G4Track* theNTrack = new G4Track(theSecNeutron,GlobalTime,thePosition);
     theNTrack->SetTouchableHandle(theTouchable);
     G4Track* thePTrack = new G4Track(theSecProton,GlobalTime,thePosition);
     thePTrack->SetTouchableHandle(theTouchable);
     G4Track* theB11Track = new G4Track(theSecB11,GlobalTime,thePosition);
     theB11Track->SetTouchableHandle(theTouchable);

     aParticleChange.SetNumberOfSecondaries(3);
     aParticleChange.AddSecondary(theNTrack);
     aParticleChange.AddSecondary(thePTrack);
     aParticleChange.AddSecondary(theB11Track);
  
    // G4cout << "Made it to the end ! " << G4endl;
   }
 else if(ReactionName == "N_C12_N2N_C11")
   {
     // Reaction copied from MENATE
     // Treated as :
     // n + 12C ---> 12C* + n'
     // 12C*    ---> 11C + n

     G4double Q_value = -18.721*MeV;

     G4ThreeVector MomDir_N1;
     G4ThreeVector MomDir_N2;
     G4ThreeVector MomDir_C11;

     // Scattered neutron (n')
     G4double Available_Eng = 0.9230769*KinEng_Int+Q_value;

     if(Available_Eng < 0.)
       { Available_Eng = 1.e-3; }

     G4double Eng_Evap1 = Evaporate_Eng(AMass_Material,Available_Eng);

     G4double cos_theta_iso = 1.-(2.*G4UniformRand());

     // 1/169. = (Mn/(Mn+Mc))^2
     // 0.1538462 = 2Mn/(Mn+Mc)

     G4double T_N1 = KinEng_Int/169. + Eng_Evap1 + (0.1538462*sqrt(KinEng_Int*Eng_Evap1))*cos_theta_iso;
     
     G4double theta_N1 = 0.;
     G4double phi_N1 = 0.;

     if(T_N1 > 1.e-5)
       {
	 // 1/13. = Mn/(Mn+Mc)

	 G4double Arg = (sqrt(KinEng_Int)/13. + (sqrt(Eng_Evap1)*cos_theta_iso))/(sqrt(T_N1)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_N1 = acos(Arg);
	 phi_N1 = 2.*Pi*G4UniformRand();
       }

     MomDir_N1 = GenMomDir(MomDir_Int,theta_N1,phi_N1);

     // Scattered 12C*
     // 1/13. = Mn(Mc+Mn)
     // 1.0833333 = (Mc+Mn)/Mc

     G4double T_C12Star = KinEng_Int/13. + 1.0833333*Eng_Evap1 - T_N1;

     G4double theta_C12Star = 0.;
     G4double phi_C12Star = phi_N1+Pi;

     if(T_C12Star > 1.e-5)
       {
	 // 0.2664694 = sqrt(Mn*Mc)/(Mn+Mc)
	 // 0.2886751 = sqrt(Mn/Mc)
	 G4double Arg = (0.2664694*sqrt(KinEng_Int) - (0.2886751*sqrt(Eng_Evap1)*cos_theta_iso))/(sqrt(T_C12Star)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_C12Star = acos(Arg);
       }
     
     // Now neutron2 is emitted in the frame of the scattered C12*
     // At the end, the angles are put back into the frame of the original neutron
 
     Available_Eng = KinEng_Int + Q_value - T_N1 - T_C12Star;

     if(Available_Eng < 0.)
       { Available_Eng = 1.e-3; }

     G4double Eng_Evap2 = Evaporate_Eng(AMass_Material,Available_Eng);

     cos_theta_iso = 1.-(2.*G4UniformRand());

     // .5773503 = 2*sqrt(Mn/Mc)

     G4double T_N2 = T_C12Star/12. + Eng_Evap2 + 
                    (0.5773503*sqrt(T_C12Star*Eng_Evap2))*cos_theta_iso;
     
     G4double theta_C12_N2 = acos(cos_theta_iso);
     G4double phi_C12_N2 = 2.*Pi*G4UniformRand();
	 
     // Now Start With Z-Axis and then go to incoming neutron frame ?
     //G4ThreeVector MomDir_ZAxis = G4ThreeVector(0.,0.,1.);
    
     G4ThreeVector MomDir_ZAxis = MomDir_Int;
     G4ThreeVector MomDir_C12Star = GenMomDir(MomDir_ZAxis,theta_C12Star,phi_C12Star);

     MomDir_N2 = GenMomDir(MomDir_C12Star,theta_C12_N2,phi_C12_N2);

     // C11 - Need to check kinematics - maybe not right - 18 Apr 2008, BTR

     G4double T_C11 = KinEng_Int + Q_value - T_N1 - T_N2; // Remaining Eng
     if(T_C11 <= 0.)
       {T_C11 = 1e-9;}

     G4double theta_C11 = Pi - theta_C12_N2;
     G4double phi_C11 = Pi + phi_C12_N2;

     MomDir_C11 = GenMomDir(MomDir_Int,theta_C11,phi_C11);

// Generate a Secondary Neutron1
     G4DynamicParticle* theSecNeutron1 = new G4DynamicParticle;
     G4ParticleDefinition* theSecN1Definition = G4Neutron::Neutron();
     theSecNeutron1->SetDefinition(theSecN1Definition);
     theSecNeutron1->SetKineticEnergy(T_N1);
     theSecNeutron1->SetMomentumDirection(MomDir_N1);

// Generate a Secondary Neutron2
     G4DynamicParticle* theSecNeutron2 = new G4DynamicParticle;
     G4ParticleDefinition* theSecN2Definition = G4Neutron::Neutron();
     theSecNeutron2->SetDefinition(theSecN2Definition);
     theSecNeutron2->SetKineticEnergy(T_N2);
     theSecNeutron2->SetMomentumDirection(MomDir_N2);

     // Generate a Secondary C11
     G4DynamicParticle* theSecC11 = new G4DynamicParticle;
     // GetIon() Method works whether particle exists in physicslist or not. 
     // Arguements are GetIon(Charge,Mass,ExcitationEng)
     //G4ParticleDefinition* theSecC11Definition = G4ParticleTable::GetParticleTable()->GetIon(6,11,0.);
     G4ParticleDefinition* theSecC11Definition = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(6,11,0.);
     theSecC11->SetDefinition(theSecC11Definition);
     theSecC11->SetKineticEnergy(T_C11);
     theSecC11->SetMomentumDirection(MomDir_C11);

    // Kill the Parent Neutron -----------------------
     aParticleChange.ProposeTrackStatus(fStopAndKill);
     aParticleChange.ProposeEnergy(0.);
     aParticleChange.ProposePosition(thePosition);
  
     // Set final tracks in motion!
     G4Track* theN1Track = new G4Track(theSecNeutron1,GlobalTime,thePosition);
     theN1Track->SetTouchableHandle(theTouchable);
     G4Track* theN2Track = new G4Track(theSecNeutron2,GlobalTime,thePosition);
     theN2Track->SetTouchableHandle(theTouchable);
     G4Track* theC11Track = new G4Track(theSecC11,GlobalTime,thePosition);
     theC11Track->SetTouchableHandle(theTouchable);

     aParticleChange.SetNumberOfSecondaries(3);
     aParticleChange.AddSecondary(theN1Track);
     aParticleChange.AddSecondary(theN2Track);
     aParticleChange.AddSecondary(theC11Track);

     /*
     G4double T_N_Sum = T_N1+T_N2;
     G4cout << "n, 2n C11 event! " << G4endl;
     G4cout << "T_N1 = " << T_N1 << "T_N2 = " << T_N2 << " , T_Sum = " << T_N_Sum << G4endl;
     G4cout << "Total Eng Final State : " << T_N_Sum+T_C11 << " , Eng_Int+Qvalue = " << KinEng_Int+Q_value << G4endl; 
     */     
    // G4cout << "Made it to the end ! " << G4endl;
   }
else if(ReactionName == "N_C12_NN3Alpha")
   {
     // Reaction copied from MENATE
     // Treated as :
     // n + 12C ---> 12C* + n'
     // 12C*    ---> 8Be + alpha1
     // 8Be*    ---> alpha2+alpha3

     G4double Q_value = -7.275*MeV;

     G4ThreeVector MomDir_N;
     G4ThreeVector MomDir_Alpha1;
     G4ThreeVector MomDir_Alpha2;
     G4ThreeVector MomDir_Alpha3;

     // Scattered neutron (n')
     G4double Available_Eng = 0.9230769*KinEng_Int+Q_value;

     if(Available_Eng < 0.)
       { Available_Eng = 1.e-3; }

     G4double Eng_Evap1 = Evaporate_Eng(AMass_Material,Available_Eng);

     G4double cos_theta_iso = 1.-(2.*G4UniformRand());

     // 1/169. = (Mn/(Mn+Mc))^2
     // 0.1538462 = 2Mn/(Mn+Mc)

     G4double T_N = KinEng_Int/169. + Eng_Evap1 + (0.1538462*sqrt(KinEng_Int*Eng_Evap1))*cos_theta_iso;
     
     G4double theta_N = 0.;
     G4double phi_N = 0.;

     if(T_N > 1.e-5)
       {
	 // 1/13. = Mn/(Mn+Mc)

	 G4double Arg = (sqrt(KinEng_Int)/13. + (sqrt(Eng_Evap1)*cos_theta_iso))/(sqrt(T_N)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_N = acos(Arg);
	 phi_N = 2.*Pi*G4UniformRand();
       }

     MomDir_N = GenMomDir(MomDir_Int,theta_N,phi_N);

     // Scattered 12C*
     // 1/13. = Mn(Mc+Mn)
     // 1.0833333 = (Mc+Mn)/Mc

     G4double T_C12Star = KinEng_Int/13. + 1.0833333*Eng_Evap1 - T_N;

     G4double theta_C12Star = 0.;
     G4double phi_C12Star = phi_N+Pi;

     if(T_C12Star > 1.e-5)
       {
	 // 0.2664694 = sqrt(Mn*Mc)/(Mn+Mc)
	 // 0.2886751 = sqrt(Mn/Mc)
	 G4double Arg = (0.2664694*sqrt(KinEng_Int) - (0.2886751*sqrt(Eng_Evap1)*cos_theta_iso))/(sqrt(T_C12Star)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_C12Star = acos(Arg);
       }
     
     // Now Alpha1 is emitted in the frame of the scattered C12*
     // At the end, the angles are put back into the frame of the original neutron
 
     Available_Eng = (KinEng_Int + Q_value - T_N - T_C12Star)*G4UniformRand();

     if(Available_Eng < 0.)
       { Available_Eng = 0.; }

     cos_theta_iso = 1.-(2.*G4UniformRand());

     // 1./3. = MA1/MC12
     // 0.6666667 = MBe8/MC12
     // 0.9428090 = 2*sqrt(Malpha*M8Be)/Mc12)

     G4double T_Alpha1 = T_C12Star/3. + 0.6666667*Available_Eng + 
                    (0.9428090*sqrt(Available_Eng*T_C12Star))*cos_theta_iso;
     
     G4double theta_Alpha1 = 0.;
     G4double phi_Alpha1 = 0.;

     if(T_Alpha1 > 1.e-5)
       {
	 // 0.5773503 = sqrt(Malpha/MC12)
	 // 0.8164966 = sqrt((MBe8)/(MC12))

	 G4double Arg = (0.5773503*sqrt(T_C12Star) + 0.8164966*sqrt(Available_Eng)*cos_theta_iso)/(sqrt(T_Alpha1)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_Alpha1 = acos(Arg);
	 phi_Alpha1 = 2.*Pi*G4UniformRand();
       }

     // Now Start With Z-Axis and then go to incoming neutron frame ?
     //  G4ThreeVector MomDir_ZAxis = G4ThreeVector(0.,0.,1.);
     G4ThreeVector MomDir_ZAxis = MomDir_Int;
     G4ThreeVector MomDir_C12Star = GenMomDir(MomDir_ZAxis,theta_C12Star,phi_C12Star);

     MomDir_Alpha1 = GenMomDir(MomDir_C12Star,theta_Alpha1,phi_Alpha1);

     // Be8* Kinematics --- Particle decayed into the other alphas
     // Angles in frame of scattered C12*

     G4double T_Be8Star = T_C12Star + Available_Eng - T_Alpha1; // Remaining Eng
    
     G4double theta_Be8Star = 0.;
     G4double phi_Be8Star = phi_Alpha1+Pi;

     if(T_Be8Star > 1.e-5)
       {
	 // 0.5773503 = sqrt(Malpha/MC12)
	 // 0.8164966 = sqrt(M8Be/MC12)
	 G4double Arg = (0.8164966*sqrt(T_C12Star) - (0.5773503*sqrt(Available_Eng)*cos_theta_iso))/(sqrt(T_Be8Star)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_Be8Star = acos(Arg);
       }

     G4ThreeVector MomDir_Be8Star = GenMomDir(MomDir_C12Star,theta_Be8Star,phi_Be8Star);

     // Now Decay 8Be g.s. into 2 Alphas
     // Q_value = 0.092*MeV

     G4double Q_valueBe8 = 0.092*MeV;
     Available_Eng = KinEng_Int+Q_value+Q_valueBe8-T_N-T_Alpha1-T_Be8Star; // Changed 23 Apr 2008 - BTR

     if(Available_Eng < 0.)
       {Available_Eng = 0.;}

     cos_theta_iso = 1.-(2.*G4UniformRand());

     // 0.5 = MA2/Be8
 
     G4double T_Alpha2 = 0.5*(T_Be8Star+Available_Eng) + 
                    (sqrt(Available_Eng*T_Be8Star))*cos_theta_iso;
      if(T_Alpha2 < 0.)
       {T_Alpha2 = 1e-9;}

     G4double theta_Alpha2 = 0.;
     G4double phi_Alpha2 = 0.;

     if(T_Alpha2 > 1.e-5)
       {
	 // 0.7071068 = sqrt(Malpha/MBe8)
	
	 G4double Arg = (0.7071068*(sqrt(T_Be8Star) + sqrt(Available_Eng)*cos_theta_iso))/(sqrt(T_Alpha2)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_Alpha2 = acos(Arg);
	 phi_Alpha2 = 2.*Pi*G4UniformRand();
       }

     MomDir_Alpha2 = GenMomDir(MomDir_Be8Star,theta_Alpha2,phi_Alpha2);

     // Alpha3 emitted in frame of Be8

     G4double T_Alpha3 = T_Be8Star + Available_Eng - T_Alpha2;
    if(T_Alpha3 < 0.)
       {T_Alpha3 = 1e-9;}
  
     G4double theta_Alpha3 = 0.;
     G4double phi_Alpha3 = phi_Alpha2+Pi;

     if(T_Alpha3 > 1.e-5)
       {
	 // 0.7071068 = sqrt(Malpha/MBe8)
	
	 G4double Arg = (0.7071068*(sqrt(T_Be8Star) - sqrt(Available_Eng)*cos_theta_iso))/(sqrt(T_Alpha3)); 
	 if( Arg > 1.)
	   { Arg = 1.; }
	 theta_Alpha3 = acos(Arg);
       }

     MomDir_Alpha3 = GenMomDir(MomDir_Be8Star,theta_Alpha3,phi_Alpha3);

// Generate a Secondary Neutron
     G4DynamicParticle* theSecNeutron = new G4DynamicParticle;
     G4ParticleDefinition* theSecNDefinition = G4Neutron::Neutron();
     theSecNeutron->SetDefinition(theSecNDefinition);
     theSecNeutron->SetKineticEnergy(T_N);
     theSecNeutron->SetMomentumDirection(MomDir_N);

// Generate a Secondary Alpha1
     G4DynamicParticle* theSecAlpha1 = new G4DynamicParticle;
     G4ParticleDefinition* theSecA1Definition = G4Alpha::Alpha();
     theSecAlpha1->SetDefinition(theSecA1Definition);
     theSecAlpha1->SetKineticEnergy(T_Alpha1);
     theSecAlpha1->SetMomentumDirection(MomDir_Alpha1);

// Generate a Secondary Alpha2
     G4DynamicParticle* theSecAlpha2 = new G4DynamicParticle;
     G4ParticleDefinition* theSecA2Definition = G4Alpha::Alpha();
     theSecAlpha2->SetDefinition(theSecA2Definition);
     theSecAlpha2->SetKineticEnergy(T_Alpha2);
     theSecAlpha2->SetMomentumDirection(MomDir_Alpha2);

// Generate a Secondary Alpha3
     G4DynamicParticle* theSecAlpha3 = new G4DynamicParticle;
     G4ParticleDefinition* theSecA3Definition = G4Alpha::Alpha();
     theSecAlpha3->SetDefinition(theSecA3Definition);
     theSecAlpha3->SetKineticEnergy(T_Alpha3);
     theSecAlpha3->SetMomentumDirection(MomDir_Alpha3);

    
    // Kill the Parent Neutron -----------------------
     aParticleChange.ProposeTrackStatus(fStopAndKill);
     aParticleChange.ProposeEnergy(0.);
     aParticleChange.ProposePosition(thePosition);
  
     // Set final tracks in motion!
     G4Track* theNTrack = new G4Track(theSecNeutron,GlobalTime,thePosition);
     theNTrack->SetTouchableHandle(theTouchable);
     G4Track* theA1Track = new G4Track(theSecAlpha1,GlobalTime,thePosition);
     theA1Track->SetTouchableHandle(theTouchable);
     G4Track* theA2Track = new G4Track(theSecAlpha2,GlobalTime,thePosition);
     theA2Track->SetTouchableHandle(theTouchable);
     G4Track* theA3Track = new G4Track(theSecAlpha3,GlobalTime,thePosition);
     theA3Track->SetTouchableHandle(theTouchable);

     aParticleChange.SetNumberOfSecondaries(4);
     aParticleChange.AddSecondary(theNTrack);
     aParticleChange.AddSecondary(theA1Track);
     aParticleChange.AddSecondary(theA2Track);
     aParticleChange.AddSecondary(theA3Track);

     /*
     G4double T_A_Sum = T_Alpha1+T_Alpha2+T_Alpha3;

     G4cout << "n, n'3alpha event! " << G4endl;
     G4cout << "T_N = " << T_N << G4endl;
     G4cout << "A1 = " << T_Alpha1 << " ,A2 = " << T_Alpha2 << " , A3 = " << T_Alpha3 << ", A_Sum = " << T_A_Sum << G4endl;
     G4cout << "Total Eng Final State : " << T_A_Sum+T_N << " , Eng_Int-Qvalue = " << KinEng_Int-7.275+0.092 << G4endl; 
     */
    // G4cout << "Made it to the end ! " << G4endl;
   }


 return pParticleChange;
}
