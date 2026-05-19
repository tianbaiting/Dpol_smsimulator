{
  TFile *fi = TFile::Open("data/simulation/g4input/20260413ypol/d+Sn112E190/d+Sn112E190g050ypn-RP360/dbreak.root");
  TFile *fo = TFile::Open("data/simulation/g4output/y_pol/phi_random/d+Sn112E190g050ypn/dbreak0000.root");
  TTree *ti = (TTree*)fi->Get("tree");
  TTree *to = (TTree*)fo->Get("tree");

  std::vector<TBeamSimData> *iarr = nullptr;
  ti->SetBranchAddress("TBeamSimData", &iarr);

  Double_t out_bphi=0, out_bimp=0, out_phinp=0;
  to->SetBranchAddress("truth_b_phi", &out_bphi);
  to->SetBranchAddress("truth_bimp", &out_bimp);
  to->SetBranchAddress("truth_phi_np", &out_phinp);

  printf("input N=%lld  output N=%lld\n", ti->GetEntries(), to->GetEntries());
  printf("%-6s %-10s %-10s %-10s %-10s %-10s %-10s %-12s\n",
         "ev", "in_bimp", "in_bphi", "in_phinp", "out_bimp", "out_bphi", "out_phinp", "delta_bphi");
  for (Long64_t i = 0; i < std::min<Long64_t>(5, std::min(ti->GetEntries(), to->GetEntries())); ++i) {
    ti->GetEntry(i); to->GetEntry(i);
    Double_t in_bimp = iarr->at(0).fUserDouble[0];
    Double_t in_bphi = iarr->at(0).fUserDouble[1];
    Double_t in_phinp = iarr->at(0).fUserDouble[2];
    printf("%-6lld %-10.4f %-10.4f %-10.4f %-10.4f %-10.4f %-10.4f %-12.3e\n",
           i, in_bimp, in_bphi, in_phinp, out_bimp, out_bphi, out_phinp,
           std::fabs(in_bphi - out_bphi));
  }
}
