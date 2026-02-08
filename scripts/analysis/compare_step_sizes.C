#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TSystem.h"
#include "TKey.h"
#include "TObjArray.h"
#include "TString.h"
#include "TMath.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include "TROOT.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "TBeamSimData.hh"
#include "TSimData.hh"
#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"

namespace {

struct PdcHit {
  std::string name;
  TVector3 position;
  double energy;
  int stepNo;
};

std::string ToLower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return std::tolower(c); });
  return out;
}

std::vector<double> ParseCsvDoubles(const char* csv) {
  std::vector<double> values;
  if (!csv) return values;
  std::stringstream ss(csv);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (item.empty()) continue;
    values.push_back(std::atof(item.c_str()));
  }
  return values;
}

TTree* FindFirstTree(TFile& file, const char* preferred) {
  if (preferred && file.Get(preferred)) {
    return dynamic_cast<TTree*>(file.Get(preferred));
  }
  TIter next(file.GetListOfKeys());
  while (TKey* key = (TKey*)next()) {
    TObject* obj = key->ReadObj();
    if (obj && obj->InheritsFrom(TTree::Class())) {
      return static_cast<TTree*>(obj);
    }
  }
  return nullptr;
}

bool IsPdcName(const std::string& detector, const std::string& module) {
  std::string d = ToLower(detector);
  std::string m = ToLower(module);
  return (d.find("pdc") != std::string::npos) || (m.find("pdc") != std::string::npos);
}

std::string SelectPdcKey(const std::string& detector, const std::string& module) {
  std::string m = ToLower(module);
  if (m.find("pdc1") != std::string::npos) return "PDC1";
  if (m.find("pdc2") != std::string::npos) return "PDC2";
  std::string d = ToLower(detector);
  if (d.find("pdc1") != std::string::npos) return "PDC1";
  if (d.find("pdc2") != std::string::npos) return "PDC2";
  return "PDC";
}

std::vector<PdcHit> ExtractPdcHits(TClonesArray* fragArray) {
  std::map<std::string, PdcHit> best;
  if (!fragArray) return {};
  int n = fragArray->GetEntriesFast();
  for (int i = 0; i < n; ++i) {
    auto* hit = dynamic_cast<TSimData*>(fragArray->At(i));
    if (!hit) continue;
    if (!IsPdcName(hit->fDetectorName.Data(), hit->fModuleName.Data())) continue;
    std::string key = SelectPdcKey(hit->fDetectorName.Data(), hit->fModuleName.Data());
    PdcHit candidate;
    candidate.name = key;
    candidate.position = hit->fPostPosition;
    candidate.energy = hit->fEnergyDeposit;
    candidate.stepNo = hit->fStepNo;
    auto it = best.find(key);
    if (it == best.end() || candidate.stepNo < it->second.stepNo) {
      best[key] = candidate;
    }
  }
  std::vector<PdcHit> hits;
  hits.reserve(best.size());
  for (const auto& kv : best) hits.push_back(kv.second);
  return hits;
}

double MinDistanceToTrajectory(const std::vector<ParticleTrajectory::TrajectoryPoint>& traj,
                               const TVector3& point) {
  double minDist = 1e30;
  for (const auto& pt : traj) {
    double d = (pt.position - point).Mag();
    if (d < minDist) minDist = d;
  }
  return minDist;
}

void EnsureIncludePaths() {
  const char* sms = std::getenv("SMSIMDIR");
  if (!sms) return;
  std::string inc1 = std::string("-I") + sms + "/libs/analysis/include";
  std::string inc2 = std::string("-I") + sms + "/libs/smg4lib/include";
  gSystem->AddIncludePath(inc1.c_str());
  gSystem->AddIncludePath(inc2.c_str());
}

void LoadAnalysisLib() {
  const char* sms = std::getenv("SMSIMDIR");
  if (!sms) return;
  std::string candidate1 = std::string(sms) + "/build/lib/libpdcanalysis.so";
  std::string candidate2 = std::string(sms) + "/lib/libpdcanalysis.so";
  if (gSystem->Load(candidate1.c_str()) >= 0) return;
  if (gSystem->Load(candidate2.c_str()) >= 0) return;
  gSystem->Load("libpdcanalysis.so");
}

} // namespace

void compare_step_sizes(const char* sim_root,
                        const char* field_table,
                        double field_rotation_deg,
                        const char* geom_macro,
                        const char* out_root,
                        const char* step_sizes_csv = "1,2,5,10,20",
                        const char* out_csv = "",
                        int max_events = -1)
{
  // [EN] Load headers and shared libraries for analysis classes. / [CN] 加载分析类头文件与共享库。
  EnsureIncludePaths();
  LoadAnalysisLib();
  gSystem->Load("libsmdata.so");

  if (!sim_root || !field_table || !geom_macro || !out_root) {
    std::cerr << "Missing required arguments" << std::endl;
    return;
  }

  std::vector<double> stepSizes = ParseCsvDoubles(step_sizes_csv);
  if (stepSizes.empty()) {
    std::cerr << "No step sizes provided" << std::endl;
    return;
  }

  GeometryManager geo;
  if (!geo.LoadGeometry(geom_macro)) {
    std::cerr << "Failed to load geometry macro: " << geom_macro << std::endl;
    return;
  }

  MagneticField magField;
  // [EN] Use table map so the field matches the Geant4 macro. / [CN] 使用table磁场以匹配Geant4宏配置。
  if (!magField.LoadFieldMap(field_table)) {
    std::cerr << "Failed to load magnetic field table: " << field_table << std::endl;
    return;
  }
  magField.SetRotationAngle(field_rotation_deg);

  TFile inFile(sim_root, "READ");
  if (inFile.IsZombie()) {
    std::cerr << "Failed to open sim root: " << sim_root << std::endl;
    return;
  }

  TTree* tree = FindFirstTree(inFile, "tree");
  if (!tree) {
    std::cerr << "No TTree found in sim root" << std::endl;
    return;
  }

  TClonesArray* fragArray = nullptr;
  tree->SetBranchAddress("FragSimData", &fragArray);

  std::vector<TBeamSimData>* beam = nullptr;
  if (tree->GetBranch("beam")) {
    tree->SetBranchAddress("beam", &beam);
  }

  TFile outFile(out_root, "RECREATE");
  TTree outTree("step_scan", "PDC distance vs step size");
  TTree trajTree("traj", "Trajectory points per step size");
  TTree summaryTree("summary", "Summary stats per step size");

  double stepSize = 0.0;
  Long64_t eventId = -1;
  int particleId = -1;
  char pdcName[32] = {0};
  double hitX = 0.0, hitY = 0.0, hitZ = 0.0;
  double minDist = 0.0;
  int trajPoints = 0;

  std::vector<double> trajX;
  std::vector<double> trajY;
  std::vector<double> trajZ;

  outTree.Branch("step_size", &stepSize, "step_size/D");
  outTree.Branch("event_id", &eventId, "event_id/L");
  outTree.Branch("particle_id", &particleId, "particle_id/I");
  outTree.Branch("pdc_name", pdcName, "pdc_name/C");
  outTree.Branch("hit_x", &hitX, "hit_x/D");
  outTree.Branch("hit_y", &hitY, "hit_y/D");
  outTree.Branch("hit_z", &hitZ, "hit_z/D");
  outTree.Branch("min_dist", &minDist, "min_dist/D");
  outTree.Branch("traj_points", &trajPoints, "traj_points/I");

  trajTree.Branch("step_size", &stepSize, "step_size/D");
  trajTree.Branch("event_id", &eventId, "event_id/L");
  trajTree.Branch("particle_id", &particleId, "particle_id/I");
  trajTree.Branch("x", &trajX);
  trajTree.Branch("y", &trajY);
  trajTree.Branch("z", &trajZ);

  double summaryStep = 0.0;
  double meanDist = 0.0;
  double rmsDist = 0.0;
  double maxDist = 0.0;
  int count = 0;
  summaryTree.Branch("step_size", &summaryStep, "step_size/D");
  summaryTree.Branch("mean_dist", &meanDist, "mean_dist/D");
  summaryTree.Branch("rms_dist", &rmsDist, "rms_dist/D");
  summaryTree.Branch("max_dist", &maxDist, "max_dist/D");
  summaryTree.Branch("count", &count, "count/I");

  std::map<double, std::vector<double>> distAccum;

  Long64_t nEntries = tree->GetEntries();
  if (max_events > 0 && nEntries > max_events) nEntries = max_events;

  for (Long64_t i = 0; i < nEntries; ++i) {
    tree->GetEntry(i);
    eventId = i;

    std::vector<TBeamSimData> localBeam;
    if (beam) {
      localBeam = *beam;
    }

    std::vector<PdcHit> pdcHits = ExtractPdcHits(fragArray);

    if (localBeam.empty()) {
      std::cerr << "Event " << i << ": no beam data" << std::endl;
      continue;
    }

    for (size_t b = 0; b < localBeam.size(); ++b) {
      const auto& particle = localBeam[b];
      if (particle.fZ != 1 || particle.fA != 1) continue;

      particleId = static_cast<int>(b);

      double targetAngleRad = geo.GetTargetAngleRad();
      TVector3 targetPos = geo.GetTargetPosition();

      TLorentzVector momentum = particle.fMomentum;
      TVector3 mom3 = momentum.Vect();
      mom3.RotateY(-targetAngleRad);
      momentum.SetVect(mom3);

      for (double ss : stepSizes) {
        stepSize = ss;
        ParticleTrajectory trajectory(&magField);
        trajectory.SetStepSize(stepSize);
        trajectory.SetMaxTime(80.0);
        trajectory.SetMaxDistance(6000.0);
        trajectory.SetMinMomentum(10.0);

        auto traj = trajectory.CalculateTrajectory(targetPos, momentum, particle.fCharge, particle.fMass);
        trajPoints = static_cast<int>(traj.size());

        trajX.clear();
        trajY.clear();
        trajZ.clear();
        trajX.reserve(traj.size());
        trajY.reserve(traj.size());
        trajZ.reserve(traj.size());
        for (const auto& pt : traj) {
          trajX.push_back(pt.position.X());
          trajY.push_back(pt.position.Y());
          trajZ.push_back(pt.position.Z());
        }
        trajTree.Fill();

        for (const auto& hit : pdcHits) {
          std::snprintf(pdcName, sizeof(pdcName), "%s", hit.name.c_str());
          hitX = hit.position.X();
          hitY = hit.position.Y();
          hitZ = hit.position.Z();
          minDist = MinDistanceToTrajectory(traj, hit.position);
          outTree.Fill();
          distAccum[stepSize].push_back(minDist);
        }
      }
    }
  }

  // [EN] Summarize per-step statistics for quick comparison. / [CN] 汇总每个步长的统计以便快速比较。
  for (const auto& kv : distAccum) {
    summaryStep = kv.first;
    const auto& vals = kv.second;
    count = static_cast<int>(vals.size());
    if (vals.empty()) {
      meanDist = 0.0;
      rmsDist = 0.0;
      maxDist = 0.0;
      summaryTree.Fill();
      continue;
    }
    double sum = 0.0;
    double sum2 = 0.0;
    maxDist = 0.0;
    for (double v : vals) {
      sum += v;
      sum2 += v * v;
      if (v > maxDist) maxDist = v;
    }
    meanDist = sum / vals.size();
    rmsDist = std::sqrt(sum2 / vals.size());
    summaryTree.Fill();
  }

  outFile.Write();
  outFile.Close();

  if (out_csv && std::string(out_csv).size() > 0) {
    std::ofstream fout(out_csv);
    if (fout.is_open()) {
      fout << "step_size,mean_dist,rms_dist,max_dist,count\n";
      for (const auto& kv : distAccum) {
        const auto& vals = kv.second;
        if (vals.empty()) continue;
        double sum = 0.0;
        double sum2 = 0.0;
        double localMax = 0.0;
        for (double v : vals) {
          sum += v;
          sum2 += v * v;
          if (v > localMax) localMax = v;
        }
        double mean = sum / vals.size();
        double rms = std::sqrt(sum2 / vals.size());
        fout << kv.first << "," << mean << "," << rms << "," << localMax << "," << vals.size() << "\n";
      }
      fout.close();
    }
  }

  std::cout << "Done. Output: " << out_root << std::endl;
}
