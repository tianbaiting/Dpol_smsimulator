#ifndef TSIMPARAMETER_HH
#define TSIMPARAMETER_HH

#include "TNamed.h"
#include "TString.h"

class TSimParameter : public TNamed
{
public:
  TSimParameter(TString name);
  virtual ~TSimParameter();
  virtual void Print(Option_t* option="") const = 0;// interited from TNamed

public:
  ClassDef(TSimParameter, 1)
};

#endif
