#include "G4PhysListFactorySAMURAI.hh"
#include "PhysicsListBasicMessenger.hh"
#include "G4VModularPhysicsList.hh"
//#include "CHIPS.hh"// not necessary for Geant4.10
#include "FTFP_BERT.hh"
#include "FTFP_BERT_TRV.hh"
//#include "FTFP_BERT_DE.hh"
#include "FTF_BIC.hh"
#include "LBE.hh"
//#include "LHEP.hh"// not necessary for Geant4.10
#include "QBBC.hh"
//#include "QGSC_BERT.hh"// not necessary for Geant4.10
#include "QGSP_BERT.hh"
//#include "QGSP_BERT_CHIPS.hh"// not necessary for Geant4.10
#include "QGSP_BERT_HP.hh"
#include "QGSP_BIC.hh"
#include "QGSP_BIC_HP.hh"
#include "QGSP_FTFP_BERT.hh"
#include "QGS_BIC.hh"
#include "QGSP_INCLXX.hh"
#include "Shielding.hh"

// original hadron physics
#include "QGSP_BERT_XS.hh"
#include "QGSP_BERT_XSHP.hh"
#include "QGSP_BIC_XS.hh"
#include "QGSP_BIC_XSHP.hh"
#include "QGSP_INCLXX_XS.hh"
#include "QGSP_INCLXX_XSHP.hh"
#include "QGSP_MENATE_R.hh"
#include "QGSP_MENATE_R_inel.hh"

#include "G4EmStandardPhysics.hh"
#include "G4EmStandardPhysics_option1.hh"
#include "G4EmStandardPhysics_option2.hh"
#include "G4EmStandardPhysics_option3.hh"
#include "G4EmLivermorePhysics.hh"
#include "G4EmPenelopePhysics.hh"

G4PhysListFactorySAMURAI::G4PhysListFactorySAMURAI() 
  : verbose(1)
{
  listnames_em.push_back("");
  listnames_em.push_back("_EMV");
  listnames_em.push_back("_EMX");
  listnames_em.push_back("_EMY");
  listnames_em.push_back("_LIV");
  listnames_em.push_back("_PEN");

  fPhysicsListBasicMessenger = new PhysicsListBasicMessenger();
}

G4PhysListFactorySAMURAI::~G4PhysListFactorySAMURAI()
{}

G4VModularPhysicsList* G4PhysListFactorySAMURAI::GetReferencePhysList(const G4String& name)
{
  // analysis on the string 
  size_t n = name.size();

  // last characters in the string
  size_t em_opt = 0;
  G4String em_name = "";

  // check EM options
  if(n > 4){
    em_name = name.substr(n - 4, 4);
    for(size_t i=1; i<listnames_em.size(); ++i){
      if(listnames_em[i] == em_name){
	em_opt = i;
        n -= 4;
        break; 
      }
    }
    if(0 == em_opt){
      em_name = "";
    }
  }

  // hadronic pHysics List
  G4String had_name = name.substr(0, n);

  if(0 < verbose){
    G4cout << "G4PhysListFactorySAMURAI::GetReferencePhysList <" << had_name
	   << em_name << ">  EMoption= " << em_opt << G4endl;
  }
  G4VModularPhysicsList* p = 0;
//  if(had_name == "CHIPS")                {p = new CHIPS(verbose);}
//  else if(had_name == "FTFP_BERT")       {p = new FTFP_BERT(verbose);}
  if(had_name == "FTFP_BERT")       {p = new FTFP_BERT(verbose);}
  else if(had_name == "FTFP_BERT_TRV")   {p = new FTFP_BERT_TRV(verbose);}
  //  else if(had_name == "FTFP_BERT_DE")   {p = new FTFP_BERT_DE(verbose);}
  else if(had_name == "FTF_BIC")         {p = new FTF_BIC(verbose);}
  else if(had_name == "LBE")             {p = new LBE();}
  //else if(had_name == "LHEP")            {p = new LHEP(verbose);}
  else if(had_name == "QBBC")            {p = new QBBC(verbose);}
  //else if(had_name == "QGSC_BERT")       {p = new QGSC_BERT(verbose);}
  else if(had_name == "QGSP_BERT")       {p = new QGSP_BERT(verbose);}
  //else if(had_name == "QGSP_BERT_CHIPS") {p = new QGSP_BERT_CHIPS(verbose);}
  else if(had_name == "QGSP_BERT_HP")    {p = new QGSP_BERT_HP(verbose);}
  else if(had_name == "QGSP_BIC")        {p = new QGSP_BIC(verbose);}
  else if(had_name == "QGSP_BIC_HP")     {p = new QGSP_BIC_HP(verbose);}
  else if(had_name == "QGSP_FTFP_BERT")  {p = new QGSP_FTFP_BERT(verbose);}
  else if(had_name == "QGS_BIC")         {p = new QGS_BIC(verbose);}
  else if(had_name == "QGSP_INCLXX")     {p = new QGSP_INCLXX(verbose);}
  else if(had_name == "Shielding")       {p = new Shielding(verbose);}
  else if(had_name == "ShieldingLEND")   {p = new Shielding(verbose,"LEND");}
  else if(had_name == "QGSP_BERT_XS")    {p = new QGSP_BERT_XS(verbose);}
  else if(had_name == "QGSP_BERT_XSHP")  {p = new QGSP_BERT_XSHP(verbose);}
  else if(had_name == "QGSP_BIC_XS")     {p = new QGSP_BIC_XS(verbose);}
  else if(had_name == "QGSP_BIC_XSHP")   {p = new QGSP_BIC_XSHP(verbose);}
  else if(had_name == "QGSP_INCLXX_XS")  {p = new QGSP_INCLXX_XS(verbose);}
  else if(had_name == "QGSP_INCLXX_XSHP"){p = new QGSP_INCLXX_XSHP(verbose);}
  else if(had_name == "QGSP_MENATE_R")   {p = new QGSP_MENATE_R(verbose);}
  else if(had_name == "QGSP_MENATE_R_inel"){p = new QGSP_MENATE_R_inel(verbose);}
  else{
    G4cout << "### G4PhysListFactorySAMURAI WARNING: "
	   << "PhysicsList " << had_name << " is not known"
	   << G4endl;
    G4cout << "### see G4PhysListFactorySAMURAI.cc" << G4endl;
    return 0;
  }
  if(p){
    G4cout << "<<< Reference Physics List " << had_name
	   << em_name << " is built" << G4endl;
    G4int ver = p->GetVerboseLevel();
    p->SetVerboseLevel(0);
    if(0 < em_opt){
      if(1 == em_opt){ 
	p->ReplacePhysics(new G4EmStandardPhysics_option1(verbose)); 
      }else if(2 == em_opt){
	p->ReplacePhysics(new G4EmStandardPhysics_option2(verbose)); 
      }else if(3 == em_opt){
	p->ReplacePhysics(new G4EmStandardPhysics_option3(verbose)); 
      }else if(4 == em_opt){
	p->ReplacePhysics(new G4EmLivermorePhysics(verbose)); 
      }else if(5 == em_opt){
	p->ReplacePhysics(new G4EmPenelopePhysics(verbose)); 
      }
    }
    p->SetVerboseLevel(ver);

    fPhysicsListBasicMessenger->Register(p);
  }
  G4cout << G4endl;
  return p;
}
