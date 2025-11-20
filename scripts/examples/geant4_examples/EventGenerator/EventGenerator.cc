#include "TBeamSimData.hh"

#include "TH1.h"
#include "TH2.h"
#include "TF1.h"
#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TRandom.h"
#include "Math/Polynomial.h"
#include "Math/Interpolator.h"
#include <algorithm> 
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>

using namespace std;

void ReadInputFile(TString fname);
void ReadGammaFile();
void GenerateEvents();
TVector3 GetNextVector(TVector3 vector_in, float theta);

//____________________________________________________
int mass                = 1;
int z                   = 1;
int charge              = 1;
int numevent_total      = 10000;
int mass_f              = 1;
int z_f                 = 1;
int charge_f            = 1; 
int shape_target        = 1;            // shape target 1:square 2:circle

float energy_beam_mean  = 0.;
float energy_beam_sigma = 0.;           // Energy_of_beam A MeV
float target_X          = 40.0;         // if shape_target=1 --> (X,Y,Z)=size of target (in mm)
float target_Y          = 40.0;         // if shape_target=2 --> (R,Z)=size of target (in mm) and (X,Y) is the center position
float target_Z          = 10.0;   
float target_R          = 5.0;   
float ZPosShift          = 0.0;          

char temp[300];
char gamma_in[300]      = "dummy.in";
char root_in[300]       = "dummyIn.root";
char root_out[300]      = "dummy.root";
char tempparticle[300];

float theta_low         = 0.0;
float theta_high        = 180.;           // Angle covered[degree]
float x_beam_mean       = 0.0;             // mm
float y_beam_mean       = 0.0; 
float x_beam_sigma      = 0.0;
float y_beam_sigma      = 0.0;
float theta_beam_mean   = 0.0;
float theta_beam_sigma  = 0.0;
//_______________________________________________
double sum_dist;
double sum_dist_uptonow[18001];
//______________________________________________
float evnum;
float decayIDevnum;
float halflife,decay_time_after_interaction;
float e_rest,e_doppler;
float theta_gamma_rest, theta_gamma_lab;
  //_______________________________________________
struct Level  {
  int ID;
  float excitationProbability;
  float beginOfTotalExcitationProbability;
  float endOfTotalExcitationProbability;
  float energy;
  double halflife;
  int numberOfDecayBranches;
  int decayIntoLevelId[5];
  float decayIntoLevelProbability[5];
  float totalDecayProbability;
  float beginOfDecayProbability[5];
  float endOfDecayProbability[5];
} SLevel[100];
//_______________________________________________
float gTotalLevelExcitationProbability = 0.;
int gNumberOfLevels                    = 0; 
int gNumberOfExcitedLevels             = 0; 
//_______________________________________________
TTree *Tree;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
void EventGenerator(TString fname="./EventGenerator/input/EventGenerator.in") {
  ReadInputFile(fname);// Reading the input file 
  ReadGammaFile();     // Reading the gamma file 
  //set tree structure
  TFile *rootfile = new TFile(root_out,"RECREATE");
  rootfile->cd();
  Tree = new TTree("tree_input","Input tree for simulation");
  gBeamSimDataArray = new TBeamSimDataArray();
  Tree->Branch("TBeamSimData", gBeamSimDataArray);
  GenerateEvents();
  Tree->Write();
  rootfile->Write();
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo...... 
void ReadInputFile(TString fname)  {
  // The Input files are of free type.
  string str;
  ifstream ifs;
  ifs.open(fname.Data());
  while(getline(ifs,str))  {
    TString str_tmp(str);
    if (str[0]=='#') continue; // comment line

    str_tmp = str_tmp.Strip(TString::kBoth);
    if (str_tmp.Length()==0) continue;// skip empty line

    sscanf(str.c_str(),"%s",temp);
    if(strcmp(temp,"GAMMAINPUT")==0)  {
      sscanf(str.c_str(),"%s %s",temp, &gamma_in);
      printf("%s %s \n",temp,gamma_in);
    }
    else if(strcmp(temp,"NUMBEROFEVENTS")==0)  {
      sscanf(str.c_str(),"%s %i ",temp,&numevent_total);
      printf("%s %i\n",temp,numevent_total);
    }
    else if(strcmp(temp,"OUTPUTFILE")==0)  {
      sscanf(str.c_str(),"%s %s ",temp,&root_out); 
      printf("%s %s \n",temp,root_out);
    }
    else if(strcmp(temp,"TARGET")==0)  {
      sscanf(str.c_str(),"%s %i %f %f %f %f",temp,&shape_target,&target_X,&target_Y,&target_Z,&target_R);
      if(shape_target<0 && shape_target>2)  {
        cout<<"Could not read your input keyword. Aborting program."<<endl; 
        abort();
      }
      printf("%s %i %f %f %f %f\n",temp,shape_target,target_X,target_Y,target_Z,target_R);
    }
    else if(strcmp(temp,"ZPOSSHIFT")==0)  {
      sscanf(str.c_str(),"%s %f",temp,&ZPosShift); 
      printf("%s %f\n",temp,ZPosShift);
    }
    else if(strcmp(temp,"BEAMENERGY")==0)  {
      sscanf(str.c_str(),"%s %f %f",temp,&energy_beam_mean,&energy_beam_sigma); 
      printf("%s %f %f \n",temp,energy_beam_mean,energy_beam_sigma);
    }
    else if(strcmp(temp,"BEAMPOSITION")==0)  {
      sscanf(str.c_str(),"%s %f %f %f %f",temp,&x_beam_mean,&x_beam_sigma,&y_beam_mean,&y_beam_sigma);
      printf("%s %f %f %f %f \n",temp,x_beam_mean,x_beam_sigma,y_beam_mean,y_beam_sigma);
    }
    else if(strcmp(temp,"BEAMANGLE")==0)  {
      sscanf(str.c_str(),"%s %f %f",temp,&theta_beam_mean,&theta_beam_sigma);
      printf("%s %f %f\n",temp,theta_beam_mean,theta_beam_sigma);
    }
    else if(strcmp(temp,"BEAMISOTOPE")==0)  {
      sscanf(str.c_str(),"%s %i %i %i ",temp,&mass,&z,&charge); 
      printf("%s %i %i %i\n",temp,mass,z,charge);
    }
    else if(strcmp(temp,"FRAGMENTISOTOPE")==0)  {
      sscanf(str.c_str(),"%s %i %i %i ",temp,&mass_f,&z_f,&charge_f); 
      printf("%s %i %i %i\n",temp,mass_f,z_f,charge_f);
    }
    else if(strcmp(temp,"THETARANGE")==0)  {
      sscanf(str.c_str(),"%s %f %f",temp,&theta_low,&theta_high);
      printf("%s %f %f \n",temp,theta_low,theta_high);
    }
    else if(strcmp(temp,"END")==0) {
      printf("ReadInputFile: reading %s, done\n",fname.Data());
      break;
    }
    else {
      cout<<"Could not read your input keyword. Aborting program."<<endl; 
      abort();
    }
  }
  ifs.close();
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
void ReadGammaFile()  {
  // At first, initializing the struct of SLevel[100]
  cout<<"Reading Gamma input file"<<endl;
  for(int i=0;i<100;i++)  {
    SLevel[i].ID = -1;
    SLevel[i].excitationProbability             = 0.;
    SLevel[i].beginOfTotalExcitationProbability = 0.;
    SLevel[i].endOfTotalExcitationProbability   = 0.;
    SLevel[i].energy                            = 0.;
    SLevel[i].halflife                          = 0.;
    SLevel[i].numberOfDecayBranches             = 0;
    SLevel[i].totalDecayProbability             = 0.;
    for(int j=0;j<5;j++)  {
      SLevel[i].decayIntoLevelId[j]             = 0;
      SLevel[i].decayIntoLevelProbability[j]    = 0.;
      SLevel[i].beginOfDecayProbability[j]      = 0.;
      SLevel[i].endOfDecayProbability[j]        = 0.;
    }
  }
  int levelID,decayLevelID; 
  float levelExcitationProbability, levelEnergy, levelHalflife;
  float branchRatio;
  
  FILE *gammaIn = fopen(gamma_in,"r");
  while(!feof(gammaIn))  {
    fscanf(gammaIn,"%s ",&temp);
    if(strcmp(temp,"LEVEL")==0)  {
      fscanf(gammaIn,"%i %f %f %f",&levelID,&levelExcitationProbability,&levelEnergy,&levelHalflife);
      // Check if the input values are greater than zero
      if(levelID < 0 || levelExcitationProbability < 0. || levelEnergy < 0.){
        cout<<"At least one of your LEVEL input values is smaller than zero. Aborting program."<<endl; 
        abort();
      }
      // Check if the level has been assigned already
      if(SLevel[levelID].ID != -1)  {
        cout<<"This LEVEL has been assigned already. Aborting program."<<endl; 
        abort();
      }
      SLevel[levelID].ID=levelID;
      SLevel[levelID].excitationProbability=levelExcitationProbability;
      // Determine the range of this level within the total excitation probabilty
      // To be used later for the determination of the initial state of excitation
      SLevel[levelID].beginOfTotalExcitationProbability = gTotalLevelExcitationProbability;
      gTotalLevelExcitationProbability = gTotalLevelExcitationProbability + levelExcitationProbability;
      
      cout<<"gTotalLevelExcitationProbability: "<<gTotalLevelExcitationProbability<<endl;
      
      SLevel[levelID].endOfTotalExcitationProbability = gTotalLevelExcitationProbability;
      SLevel[levelID].energy=levelEnergy;
      SLevel[levelID].halflife=levelHalflife;
      gNumberOfLevels++;
      if(levelEnergy>0.) gNumberOfExcitedLevels++;
    } 
    else if(strcmp(temp,"DECAY")==0)  {
      fscanf(gammaIn,"%i %i %f",&levelID,&decayLevelID,&branchRatio);
      // Setting the maximum of decay branches to five:
      int branchID = SLevel[levelID].numberOfDecayBranches;
      if(branchID>4)  {
        cout<<"This LEVEL has already five decay branches. Aborting program."<<endl; 
        abort();
      }
      SLevel[levelID].decayIntoLevelId[branchID] = decayLevelID;
      SLevel[levelID].decayIntoLevelProbability[branchID] = branchRatio;
      // Determine the range of this decay within all decay branches
      // To be used later for the determination of the decay branch
      SLevel[levelID].beginOfDecayProbability[branchID] = SLevel[levelID].totalDecayProbability;
      SLevel[levelID].totalDecayProbability = SLevel[levelID].totalDecayProbability + branchRatio;
      cout<<" Total Decay Probability of Level "<<SLevel[levelID].ID<<": "<< SLevel[levelID].totalDecayProbability<<endl;
      SLevel[levelID].endOfDecayProbability[branchID] = SLevel[levelID].totalDecayProbability;
      SLevel[levelID].numberOfDecayBranches++;
    }
    else if(strcmp(temp,"END")==0) break;
    else  {
      cout<<"Could not read your input keyword of the gamma-ray decay file. Aborting program."<<endl; 
      abort();
    }
  }
  fclose(gammaIn);
} 
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo...... 
void GenerateEvents()  {
  cout<<"Running the EventGenerator"<<endl;
  // Making a theta distribution
  sum_dist = 0.0;
  // The zet axis is the beam axis
  for(int m2=(int)(theta_low*100);m2<(int)(theta_high*100+1);m2++){
    double angle  = m2/100.0;//degree
    // isotropic distribution
    sum_dist = sum_dist + 1.0*sin(angle*(TMath::Pi()/180.));
    sum_dist_uptonow[m2] = sum_dist;
  }

  // Starting to loop through the incident particle events:

  TLorentzVector P_f;//4D momentum vector of charged particle after reaction
  TLorentzVector P_g;//4D momentum vector of gamma-ray(Lab-frame)
  TVector3 pos_vec_r;//reaction point<--set as the position of "particle"
  TVector3 pos_vec_d;//gamma-decay point<-- set as the position of "gamma"
  TBeamSimData d_particle(z_f,mass_f,P_f,pos_vec_r);
  TBeamSimData d_gamma(0,0,P_g,pos_vec_d);
  d_gamma.fParticleName="gamma";

  for(int i=0;i<numevent_total;i++){
    if(i%1000==0) cout<<"\r " << i << "/" << numevent_total << "   DONE!" << flush;
   
    // Reseting the decay ID and the time of the event to zero;
    decay_time_after_interaction = 0.;
    decayIDevnum                 = 0.;
    double reaction_x       = -9999;
    double reaction_y       = -9999;
    double reaction_z       = -9999;

    double theta_beam       = -9999;
    double phi_beam         = -9999;
    double energy_vertex    = -9999;
    double beta_vertex      = -9999;
    double gamma_vertex      = -9999;

    // Defining the reaction point
 
    if(shape_target==1&&x_beam_sigma>0&&y_beam_sigma>0){//square
      while(fabs(reaction_x)>target_X||fabs(reaction_y)>target_Y){
	reaction_x = gRandom->Gaus(x_beam_mean,x_beam_sigma);
	reaction_y = gRandom->Gaus(y_beam_mean,y_beam_sigma);
	reaction_z = gRandom->Uniform(-1.0*target_Z/2.0,1.0*target_Z/2.0);  
	// cout<<"Z_Point of fragmentation: "<<z_p0<<endl; 
      }
    }
    else if(shape_target==2&&x_beam_sigma>0&&y_beam_sigma>0){//circle
      while((pow(reaction_x-target_X,2)+pow(reaction_y-target_Y,2))>pow(target_R,2)){
	reaction_x = gRandom->Gaus(x_beam_mean,x_beam_sigma);
	reaction_y = gRandom->Gaus(y_beam_mean,y_beam_sigma);
	reaction_z = gRandom->Uniform(-1.0*target_Z/2.0,1.0*target_Z/2.0);  
	// cout<<"Z_Point of fragmentation: "<<z_p0<<endl;
      }
    }
    else{//point
      reaction_x = x_beam_mean;
      reaction_y = y_beam_mean;
      reaction_z = 0.;
    }
    reaction_z+=ZPosShift;//add Z-offset 

    pos_vec_r.SetXYZ(reaction_x,reaction_y,reaction_z);

    // Defining the beam momentum vector
    if(theta_beam_sigma>0) theta_beam = gRandom->Gaus(theta_beam_mean,theta_beam_sigma);
    else theta_beam = theta_beam_mean;
    theta_beam*=1./1000.;//mmrad-->rad
    phi_beam = gRandom->Uniform(0,2.0*TMath::Pi()); 
    if(energy_beam_sigma>0) energy_vertex = gRandom->Gaus(energy_beam_mean,energy_beam_sigma);
    else energy_vertex = energy_beam_mean;

    gamma_vertex = (931.494+energy_vertex)/931.494;
    beta_vertex  =  sqrt(1.0 - 1.0/gamma_vertex/gamma_vertex);//beta at reaction point   
    double p_f = 931.494*beta_vertex*gamma_vertex;

    P_f.SetPxPyPzE(p_f*sin(theta_beam)*cos(phi_beam),
		   p_f*sin(theta_beam)*sin(phi_beam),
		   p_f*cos(theta_beam),
		   energy_vertex+931.494);

    //Define charged particle at reaction point
    d_particle.fPrimaryParticleID=1;
    d_particle.fPosition=pos_vec_r;
    d_particle.fMomentum=P_f;
    gBeamSimDataArray->push_back(d_particle);

    //gamma emission part
    // Selecting the populated excitation level
    float randNumber       = gRandom->Uniform(0.0,gTotalLevelExcitationProbability);
    int populatedLevelID   = 0;
    int decayIntoLevelID   = 0;
    float excitationEnergy = 1.;
    float decayEnergy      = 0.;
    
    double meanlife, decayTime;

    for(int j=0;j<gNumberOfLevels;j++){
      if(randNumber>=SLevel[j].beginOfTotalExcitationProbability && 
	 randNumber<SLevel[j].endOfTotalExcitationProbability)
	populatedLevelID = j; 
    }

    //Ok, we have the initialy excited level. Now we have to determine the decay pattern
    while(excitationEnergy != 0. && SLevel[populatedLevelID].numberOfDecayBranches >0 && 
	  SLevel[populatedLevelID].totalDecayProbability>0.)  {
      //cout<<"START GAMMA EMISSION LOOP"<<endl;
      randNumber = gRandom->Uniform(0.0,SLevel[populatedLevelID].totalDecayProbability);
      for(int j=0;j<SLevel[populatedLevelID].numberOfDecayBranches;j++)  {  
	if(randNumber>=SLevel[populatedLevelID].beginOfDecayProbability[j] && 
	   randNumber<SLevel[populatedLevelID].endOfDecayProbability[j])  { 
	  decayIntoLevelID = SLevel[populatedLevelID].decayIntoLevelId[j];
	  decayEnergy      = SLevel[populatedLevelID].energy - SLevel[decayIntoLevelID].energy;
	      
	  if(decayEnergy<0.)  {
	    cout<<"Decay energy smaller than zero. Aborting program."<<endl; 
	    abort();
	  }
	}
      }
      //assume gamma_vertex = constant during the decay
      halflife                     = SLevel[populatedLevelID].halflife;
      meanlife                     = (halflife/log(2.0)*gamma_vertex);//ns 
      decayTime                    = gRandom->Exp(meanlife); //ns
      // Only the decay time for the level!!!
      decay_time_after_interaction = decay_time_after_interaction + decayTime;   
      // Decay time from reaction point	  
      
      //------------------------------------------ 
      //Now comes the deexcitation part!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      //------------------------------------------
	  

      
      // Determine the theta distribution!!!
      double ran_dist = gRandom->Uniform(0.0,sum_dist);
      for(int mmm=1;mmm<18001;mmm++)  {
	if(ran_dist>=sum_dist_uptonow[mmm-1] && ran_dist<=sum_dist_uptonow[mmm])  {
	  theta_gamma_rest = mmm/100.0/180.0*3.14159;
	  break;
	}
      }
      // Define the gamma momentum vector	  
      theta_gamma_lab = acos((cos(theta_gamma_rest)+beta_vertex)/(1.0+beta_vertex*cos(theta_gamma_rest)));
      TVector3 beam_in  = TVector3(P_f.Px(), P_f.Py(),P_f.Pz());
      //gamma-ray vector in Lab-frame      
      TVector3 gamma_out = GetNextVector(beam_in, theta_gamma_lab);
    
      e_rest    = decayEnergy;
      e_doppler = e_rest*(sqrt(1.0-beta_vertex*beta_vertex)/(1.0-beta_vertex*cos(gamma_out.Theta())));
      P_g.SetPxPyPzE(e_doppler*gamma_out.X()/1000.,
		     e_doppler*gamma_out.Y()/1000.,
		     e_doppler*gamma_out.Z()/1000.,
		     e_doppler/1000.); // unit:MeV

      // Define the decay point	  
      TVector3 beta_f(P_f.Px(),P_f.Py(),P_f.Pz());
      pos_vec_d =  pos_vec_r;
      if(energy_vertex>0){
	pos_vec_d +=  (beta_f)*(300*decay_time_after_interaction/energy_vertex); //mm
      }
      
      //Define gamma at the decay point
      d_gamma.fPrimaryParticleID=decayIDevnum+2;//PPID=1 for charged fragment(particle) 
      d_gamma.fPosition=pos_vec_d;//decay point
      d_gamma.fMomentum=P_g;
      d_gamma.fTime=decay_time_after_interaction;
      gBeamSimDataArray->push_back(d_gamma);

      //Setting the new excitation energy and populated level
      excitationEnergy = SLevel[decayIntoLevelID].energy;
      populatedLevelID = SLevel[decayIntoLevelID].ID;                   
      decayIDevnum++;
      // end while loop through decay scheme
    }// end the loop throug the number of gammas
    //cout<<"OK 1-EVENT"<<endl;
    Tree->Fill();
    gBeamSimDataArray->clear();
  }
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo...... 
TVector3 GetNextVector(TVector3 vector_in, float theta)  {
  double x1     = vector_in.X();
  double y1     = vector_in.Y();
  double z1     = vector_in.Z();
  double l1     = sqrt(x1*x1 + y1*y1 + z1*z1);
  double theta1 = 0.0;
  if(l1>0)  theta1 = acos(z1/l1);
  if(l1==0) theta1 = 0.0;

  double randphi    = gRandom->Uniform(0.0,2*TMath::Pi());

  TVector3 rotatedPos(0.0,0.0,1.0);
  rotatedPos.RotateY(theta1);//theta of gamma
  rotatedPos.RotateY(theta);//theta of beam
  rotatedPos.RotateZ(randphi);

  return rotatedPos;
}
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo...... 
