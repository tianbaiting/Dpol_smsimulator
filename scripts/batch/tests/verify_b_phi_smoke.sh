#!/usr/bin/env bash
# [EN] Verify input fUserDouble[1] (b_phi) matches output truth_b_phi for one
# ypol smoke pair and one zpol smoke pair (per spec §verification step 4–5).
# [CN] 验证 input ROOT 的 fUserDouble[1] (b_phi) 与 output ROOT 的 truth_b_phi
# 在同一事件 (entry i) 上数值一致, 对 ypol 和 zpol 各取一对样本 (规格§验证 step 4–5)。
#
# Run on remote (spana03) after smoke or full gen-output has produced ≥1
# matched input+output pair for ypol Sn112 g050 ynp and zpol Sn112 g050 znp b01.
set -euo pipefail

REMOTE_DIR="${REMOTE_DIR:-/home/tbt/workspace/Dpol_smsimulator}"
cd "$REMOTE_DIR"

eval "$(/home/tbt/.local/bin/micromamba shell hook -s bash)" >/dev/null 2>&1
micromamba activate anaroot-env >/dev/null 2>&1
source setup_spana.sh >/dev/null 2>&1

cat > /tmp/verify_b_phi.C << 'EOF'
{
  // [EN] Pairs to check: (input ROOT path, output ROOT path).
  // [CN] 待验证的 (input, output) 对.
  std::vector<std::pair<std::string,std::string>> pairs = {
    {"data/simulation/g4input/20260413ypol/d+Sn112E190/d+Sn112E190g050ynp-RP360/dbreak.root",
     "data/simulation/g4output/y_pol/phi_random/d+Sn112E190g050ynp/dbreak.root"},
    {"data/simulation/g4input/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb01.root",
     "data/simulation/g4output/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb01.root"},
  };

  int failed = 0;
  for (auto& p : pairs) {
    TFile *fi = TFile::Open(p.first.c_str());
    TFile *fo = TFile::Open(p.second.c_str());
    if (!fi || fi->IsZombie() || !fo || fo->IsZombie()) {
      printf("MISS %s vs %s\n", p.first.c_str(), p.second.c_str());
      ++failed;
      continue;
    }
    TTree *ti = (TTree*)fi->Get("tree");
    TTree *to = (TTree*)fo->Get("tree");
    if (!ti || !to) {
      printf("NO-TREE %s vs %s\n", p.first.c_str(), p.second.c_str());
      ++failed;
      continue;
    }

    std::vector<TBeamSimData> *iarr = nullptr;
    ti->SetBranchAddress("TBeamSimData", &iarr);

    Double_t out_b_phi = 0;
    if (to->SetBranchAddress("truth_b_phi", &out_b_phi) < 0) {
      printf("NO-truth_b_phi-branch %s\n", p.second.c_str());
      ++failed;
      continue;
    }

    // [EN] Sample first 5 events; require all to match within 1e-9.
    Long64_t n = std::min<Long64_t>(5, std::min(ti->GetEntries(), to->GetEntries()));
    int local_fail = 0;
    for (Long64_t i = 0; i < n; ++i) {
      ti->GetEntry(i);
      to->GetEntry(i);
      if (iarr->empty()) { printf("EMPTY-input ev=%lld %s\n", i, p.first.c_str()); ++local_fail; continue; }
      Double_t in_b_phi = iarr->at(0).fUserDouble[1];
      Double_t delta = std::fabs(in_b_phi - out_b_phi);
      if (delta > 1e-9) {
        printf("MISMATCH %s ev=%lld in=%.9f out=%.9f delta=%.3e\n",
               p.first.c_str(), i, in_b_phi, out_b_phi, delta);
        ++local_fail;
      }
    }
    if (local_fail == 0) {
      printf("OK %-90s entries=%lld\n", p.first.c_str(), n);
    } else {
      printf("FAIL %s (%d / %lld events mismatched)\n", p.first.c_str(), local_fail, n);
      failed += local_fail;
    }
    delete fi; delete fo;
  }

  if (failed == 0) {
    printf("\n[verify_b_phi] PASS — all sampled (input.fUserDouble[1] == output.truth_b_phi) within 1e-9\n");
  } else {
    printf("\n[verify_b_phi] FAIL — %d mismatches\n", failed);
    gSystem->Exit(1);
  }
}
EOF

root -l -b -q /tmp/verify_b_phi.C
