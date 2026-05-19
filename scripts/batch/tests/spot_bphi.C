{
  for (auto fn : {"data/simulation/g4input/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb01.root",
                  "data/simulation/g4input/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb05.root",
                  "data/simulation/g4input/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb10.root",
                  "data/simulation/g4input/20260413ypol/d+Sn112E190/d+Sn112E190g050ynp-RP360/dbreak.root"}) {
    TFile *f = TFile::Open(fn);
    if (!f || f->IsZombie()) { printf("MISS %s\n", fn); continue; }
    TTree *t = (TTree*)f->Get("tree");
    Long64_t n = t->GetEntries();
    std::vector<TBeamSimData> *arr = nullptr;
    t->SetBranchAddress("TBeamSimData", &arr);
    t->GetEntry(0);
    const auto& p = arr->at(0);
    printf("%-90s  N=%-8lld  bimp=%-8.3f  b_phi=%-8.4f  phi_np=%-8.4f\n",
           fn, n, p.fUserDouble[0], p.fUserDouble[1], p.fUserDouble[2]);
    delete f;
  }
}
