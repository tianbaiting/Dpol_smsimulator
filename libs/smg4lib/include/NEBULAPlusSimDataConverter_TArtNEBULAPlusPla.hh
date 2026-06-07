#ifndef _PLUSSIMDATACONVERTER_TARTNEBULAPLUSLA_HH_
#define _PLUSSIMDATACONVERTER_TARTNEBULAPLUSLA_HH_

#include "SimDataConverter.hh"
#include "SimDataInitializer.hh"

#include "TNEBULAPlusSimParameter.hh"
#include <map>

class TTree;
class TClonesArray;

class NEBULAPlusSimDataConverter_TArtNEBULAPlusPla : public SimDataConverter
{
public:
  NEBULAPlusSimDataConverter_TArtNEBULAPlusPla(TString name="NEBULAPlusSimDataConverter_TArtNEBULAPlusPla");
  virtual ~NEBULAPlusSimDataConverter_TArtNEBULAPlusPla();

  // called in RunActionBasic from SimDataManager
  virtual int Initialize();
  virtual int DefineBranch(TTree* tree);
  virtual int ConvertSimData();

  // called in EventActionBasic from SimDataManager
  virtual int ClearBuffer();

  Double_t MeVeeRelation_FOX(Double_t T, std::string name, Int_t pdgcode);
  Double_t MeVtoMeVee(Double_t Tin, Double_t Tout, std::string name, Int_t pdgcode);

  void SetIncludeResolution(bool tf){fIncludeResolution = tf;}
  bool SetIncludeResolution(){return fIncludeResolution;}

protected:
  bool fIncludeResolution;
  bool fDetectorEnabled;

  TNEBULAPlusSimParameter *fNEBULAPlusSimParameter;
  TClonesArray *fNEBULAPlusSimDataArray;  // Each element is a hit
  TClonesArray *fNEBULAPlusPlaArray;      // Each element is a module with hit

  class tmp_data{
  public:
    tmp_data()
      : id(0), layer(0), sublayer(0),
	qu(0), qd(0), q(0), light(0),
	tu(0), td(0), t(0),
	pos(0,0,0), beta(0,0,0), p(0,0,0),
	t_sim(0), q_sim(0), pos_sim(0,0,0)
    {;}
    ~tmp_data(){;}
  public:
    Int_t id,layer,sublayer;
    Double_t qu,qd,q, light;
    Double_t tu,td,t;
    TVector3 pos;
    TVector3 beta,p;
    Double_t t_sim;
    Double_t q_sim;
    TVector3 pos_sim;
  };
  std::map<int,tmp_data> fDataBufferMap;


};
#endif
