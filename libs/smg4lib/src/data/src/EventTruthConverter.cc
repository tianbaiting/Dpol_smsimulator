#include "EventTruthConverter.hh"
#include "TBeamSimData.hh"
#include "QMDInputMetadata.hh"

#include "TTree.h"

#include <cmath>
#include <iostream>
#include <limits>

EventTruthConverter::EventTruthConverter(TString name)
  : SimDataConverter(name)
{
  ClearBuffer();
}

EventTruthConverter::~EventTruthConverter() = default;

int EventTruthConverter::Initialize()
{
  return 0;
}

int EventTruthConverter::DefineBranch(TTree* tree)
{
  if (fDataStore && tree) {
    tree->Branch("truth_bimp",   &fBimp,       "truth_bimp/D");
    tree->Branch("truth_b_phi",  &fBPhi,       "truth_b_phi/D");
    tree->Branch("truth_phi_np", &fPhiNpTruth, "truth_phi_np/D");
  }
  return 0;
}

int EventTruthConverter::ConvertSimData()
{
  // [EN] gBeamSimDataArray is filled by PrimaryGeneratorActionBasic::BeamTypeTree
  //      at the start of each event; per-event metadata is replicated on every
  //      particle's fUserDouble. Read from the first particle.
  // [CN] gBeamSimDataArray 在 BeamTypeTree 中填充;每个粒子 fUserDouble 携带相同
  //      per-event 元数据,从第 0 号粒子读取即可。
  if (!gBeamSimDataArray || gBeamSimDataArray->empty()) {
    ClearBuffer();
    return 0;
  }

  const auto& first = gBeamSimDataArray->at(0);
  if (first.fUserDouble.size() <= qmd_input_metadata::kPhiNpTruthIndex) {
    static bool warned = false;
    if (!warned) {
      std::cerr << "[EventTruthConverter] WARN: input fUserDouble too short ("
                << first.fUserDouble.size()
                << "); writing NaN truth metadata. Regenerate g4 input with newer GenInputRoot_qmdrawdata."
                << std::endl;
      warned = true;
    }
    ClearBuffer();
    return 0;
  }

  fBimp       = first.fUserDouble[qmd_input_metadata::kBimpIndex];
  fBPhi       = first.fUserDouble[qmd_input_metadata::kBPhiIndex];
  fPhiNpTruth = first.fUserDouble[qmd_input_metadata::kPhiNpTruthIndex];
  return 0;
}

int EventTruthConverter::ClearBuffer()
{
  fBimp       = std::numeric_limits<double>::quiet_NaN();
  fBPhi       = std::numeric_limits<double>::quiet_NaN();
  fPhiNpTruth = std::numeric_limits<double>::quiet_NaN();
  return 0;
}
