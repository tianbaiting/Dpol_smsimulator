#ifndef TRunSIMPARAMETER_HH
#define TRunSIMPARAMETER_HH

#include "TSimParameter.hh"
#include "TNamed.h"
#include "TString.h"
#include "TDatime.h"


class TRunSimParameter : public TSimParameter
{
public:
  TRunSimParameter(TString name="RunParameter");
  virtual ~TRunSimParameter();
  virtual void Print(Option_t* option="") const;// interited from TNamed
  friend std::ostream& operator<<(std::ostream& out,  const TRunSimParameter& prm);

  void SetTreeName(TString name){fTreeName = name;}
  void AppendHeader(TString str){fHeader += " " + str;}

public:
  TString fRunName;
  TString fSaveDir;
  TString fOverWrite;
  TString fHeader;
  TString fTreeName;
  TString fTreeTitle;
  TDatime fStartTime;
  TDatime fStopTime;

  ClassDef(TRunSimParameter, 1)
};

#endif
