#ifndef _EVENTTRUTHCONVERTER_HH_
#define _EVENTTRUTHCONVERTER_HH_

#include "TObject.h"
#include "SimDataConverter.hh"

class TTree;

// [EN] Reads per-event truth metadata (b_phi, bimp, phi_np_truth) stamped by
//      GenInputRoot_qmdrawdata into TBeamSimData::fUserDouble[0..2] and writes
//      them as scalar branches truth_bimp/truth_b_phi/truth_phi_np in the
//      output ROOT tree. Reaction-plane reconstruction validation downstream
//      compares reco phi_RP against truth_b_phi.
// [CN] 把 GenInputRoot_qmdrawdata 在 fUserDouble[0..2] 写入的 per-event truth
//      元数据透传到输出 ROOT tree 的标量 branch,供下游反应平面 reco 验证。
class EventTruthConverter : public SimDataConverter
{
public:
  EventTruthConverter(TString name = "EventTruthConverter");
  ~EventTruthConverter() override;

  int Initialize() override;
  int DefineBranch(TTree* tree) override;
  int ConvertSimData() override;
  int ClearBuffer() override;

private:
  Double_t fBimp;
  Double_t fBPhi;
  Double_t fPhiNpTruth;
};

#endif
