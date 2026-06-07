#include "NEBULAPlusSimParameterReader.hh"

#include <iostream>
#include <map>
#include <sstream>
#include <fstream>
#include <math.h>
//____________________________________________________________________
NEBULAPlusSimParameterReader::NEBULAPlusSimParameterReader()
{
  std::cout<<"NEBULAPlusSimParameterReader"<<std::endl;
  fSimDataManager = SimDataManager::GetSimDataManager();
  fNEBULAPlusSimParameter = (TNEBULAPlusSimParameter*)fSimDataManager->FindParameter("NEBULAPlusParameter");
  if (fNEBULAPlusSimParameter==0){
    fNEBULAPlusSimParameter = new TNEBULAPlusSimParameter("NEBULAPlusParameter");
    fSimDataManager->AddParameter(fNEBULAPlusSimParameter);
  }
}
//____________________________________________________________________
NEBULAPlusSimParameterReader::~NEBULAPlusSimParameterReader()
{;}
//____________________________________________________________________
void NEBULAPlusSimParameterReader::ReadNEBULAPlusParameters(const char* PrmFileName)
{
  std::vector<std::vector<TString> > PrmArray;
  CSVToVector(PrmFileName, &PrmArray);
  if (PrmArray.size()==0) {
    std::cout<< "\x1b[33m" // yellow colored text
	  << __FILE__ << ": No Parameter is Loaded from "<< PrmFileName
	  << "\x1b[0m" // red colored text
	  <<std::endl;
    return;
  }
  fNEBULAPlusSimParameter->fParameterFileName = PrmFileName;

  for (int iline=0;iline<(int)PrmArray.size();++iline){
      std::vector<TString> str_line = PrmArray[iline];

      if ("Position"==str_line[0]) fNEBULAPlusSimParameter->fPosition.SetXYZ(str_line[1].Atof(),
									 str_line[2].Atof(),
									 str_line[3].Atof());

      if ("NeutSize"==str_line[0]) fNEBULAPlusSimParameter->fNeutSize.SetXYZ(str_line[1].Atof(),
									 str_line[2].Atof(),
									 str_line[3].Atof());

      if ("VetoSize"==str_line[0]) fNEBULAPlusSimParameter->fVetoSize.SetXYZ(str_line[1].Atof(),
									 str_line[2].Atof(),
									 str_line[3].Atof());

      if ("Angle"==str_line[0]) fNEBULAPlusSimParameter->fAngle.SetXYZ(str_line[1].Atof(),
								   str_line[2].Atof(),
								   str_line[3].Atof());

      if ("TimeReso"==str_line[0]) fNEBULAPlusSimParameter->fTimeReso = str_line[1].Atof();
      if ("Q_factor"==str_line[0]) fNEBULAPlusSimParameter->fQ_factor = str_line[1].Atof();
  }
//  std::cout<< "ReadParamNEBULAPlus : "<<(*fNEBULAPlusSimParameter) <<std::endl;
//    G4RunManager::GetRunManager()->GeometryHasBeenModified();

  fNEBULAPlusSimParameter->fIsLoaded = true;
}
//____________________________________________________________________
void NEBULAPlusSimParameterReader::ReadNEBULAPlusDetectorParameters(const char* PrmFileName)
{

  std::vector<std::vector<TString> > PrmArray;
  CSVToVector(PrmFileName, &PrmArray);
  if (PrmArray.size()==0) {
    std::cout<< "\x1b[33m" // yellow colored text
	  << __FILE__ << ": No Parameter is Loaded from "<< PrmFileName
	  << "\x1b[0m" // reset color
	  <<std::endl;
    return;
  }
  fNEBULAPlusSimParameter->fDetectorParameterFileName = PrmFileName;

  std::map<int,TDetectorSimParameter> *Map = &(fNEBULAPlusSimParameter->fNEBULAPlusDetectorParameterMap);

  std::vector<TString> str_line0 = PrmArray[0];// 1st line = list of Name of PRM
  for (int iline=1;iline<(int)PrmArray.size();++iline){
    TDetectorSimParameter para("");
    std::vector<TString> str_line = PrmArray[iline];
    int id=0;

    for (int i=0;i<(int)str_line.size();++i){
      if      ("ID"       ==str_line0[i]) {
	id = str_line[i].Atoi();
	para.fID     = id;
      }
      else if ("Layer"    ==str_line0[i]) para.fLayer  = str_line[i].Atoi();
      else if ("SubLayer" ==str_line0[i]) para.fSubLayer = str_line[i].Atoi();

      else if ("DetectorType"==str_line0[i])para.fDetectorType = str_line[i];

      else if ("PositionX"==str_line0[i]) para.fPosition.SetX(str_line[i].Atof());
      else if ("PositionY"==str_line0[i]) para.fPosition.SetY(str_line[i].Atof());
      else if ("PositionZ"==str_line0[i]) para.fPosition.SetZ(str_line[i].Atof());

      else if ("AngleX"==str_line0[i]) para.fAngle.SetX(str_line[i].Atof());
      else if ("AngleY"==str_line0[i]) para.fAngle.SetY(str_line[i].Atof());
      else if ("AngleZ"==str_line0[i]) para.fAngle.SetZ(str_line[i].Atof());

    }
    std::stringstream ss;
    ss<<para.fDetectorType.Data()<<para.fID;
    para.SetName(ss.str().c_str());

    std::cout<<para<<std::endl;

    Map->insert(std::pair<int,TDetectorSimParameter>(id,para));
  }
  //-------------------------------------------------
  // Set Parameteres like the number of detectors etc.
  if (! fNEBULAPlusSimParameter->fIsLoaded ){
    std::cout<< "\x1b[31m" // red colored text
	  << "Error @  "<<__FILE__ << std::endl
	  <<"NEBULAPlusSimParameter must be Loaded before reading "
	  << PrmFileName
	  << "\x1b[0m" // reset color
	  <<std::endl;
    return;
  }

  std::map<int,TDetectorSimParameter>::iterator it = Map->begin();
  while (it != Map->end()){
    TDetectorSimParameter para = it->second;
    TVector3 Size;// Detector size
    if        (para.fDetectorType=="Neut"){
      fNEBULAPlusSimParameter->fNeutNum++;
      if (para.fLayer==1) fNEBULAPlusSimParameter->fNeutNumPerLayer++;
      if (para.fLayer==1 && para.fSubLayer==1) fNEBULAPlusSimParameter->fNeutNumPerSubLayer++;
      Size = fNEBULAPlusSimParameter->fNeutSize;

    }else if  (para.fDetectorType=="Veto"){
      fNEBULAPlusSimParameter->fVetoNum++;
      if (para.fLayer==1) fNEBULAPlusSimParameter->fVetoNumPerLayer++;
      if (para.fLayer==1 && para.fSubLayer==1) fNEBULAPlusSimParameter->fVetoNumPerSubLayer++;
      Size = fNEBULAPlusSimParameter->fVetoSize;
    }

    fNEBULAPlusSimParameter->fLayerNum = std::max(para.fLayer,fNEBULAPlusSimParameter->fLayerNum);
    // SubLayerNum is not implemented...
    //    fNEBULAPlusSimParameter->fSubLayerNum  = std::max(fNEBULAPlusSimParameter->fSubLayerNum);

    Double_t LeftEdge   = para.fPosition.x() - 0.5*Size.x();
    Double_t RightEdge  = para.fPosition.x() + 0.5*Size.x();
    Double_t BottomEdge = para.fPosition.y() - 0.5*Size.y();
    Double_t TopEdge    = para.fPosition.y() + 0.5*Size.y();
    Double_t FrontEdge  = para.fPosition.z() - 0.5*Size.z();
    Double_t BackEdge   = para.fPosition.z() + 0.5*Size.z();

    Double_t MaxX = std::max(fabs(LeftEdge),  fabs(RightEdge));
    Double_t MaxY = std::max(fabs(BottomEdge),fabs(TopEdge));
    Double_t MaxZ = std::max(fabs(FrontEdge), fabs(BackEdge));

    fNEBULAPlusSimParameter->fSize.SetX( std::max(2.0*MaxX, fNEBULAPlusSimParameter->fSize.x()) );
    fNEBULAPlusSimParameter->fSize.SetY( std::max(2.0*MaxY, fNEBULAPlusSimParameter->fSize.y()) );
    fNEBULAPlusSimParameter->fSize.SetZ( std::max(2.0*MaxZ, fNEBULAPlusSimParameter->fSize.z()) );
    // Rotation is not supported...

    it++;
  }

  std::cout<< "NEBULAPlusSimParameterReader : "<<fNEBULAPlusSimParameter <<std::endl;
  //  G4RunManager::GetRunManager()->GeometryHasBeenModified();
}
//____________________________________________________________________
void NEBULAPlusSimParameterReader::CSVToVector(const char* PrmFileName,
					   std::vector<std::vector<TString> >* PrmArray)
{
  std::ifstream ifs(PrmFileName);
  if (ifs.fail()) {
    std::cout << "\x1b[31m" // red colored text
	   <<__FILE__<<": File open error \""<<PrmFileName<<"\""
	   << "\x1b[0m" // reset color
	   <<std::endl;
    return;
  }

  std::string line;
  int iline=0;
  while(getline(ifs,line,'\n')){

    std::vector<TString>* str_line = new std::vector<TString>;

    /* Skip space line */
    TString tsline=line;
    tsline=tsline.Strip(TString::kBoth);
    if (tsline.IsNull()) {iline++;continue;}

    // skip comment line
    if (tsline.First("//")==0) {iline++;continue;}

    //--------------------------------------
    /* Scan one line */
    std::istringstream ss(line);
    std::string s;
    while (getline(ss,s,',')){
      TString ts=s;
      ts=ts.Strip(TString::kBoth);
      if (ts.IsNull()) continue;// skip space column
      str_line->push_back(ts);
    }

    PrmArray->push_back(*str_line);
    ++iline;//increment of line number
  }
  ifs.close();
}
//______________________________________________________________________________
