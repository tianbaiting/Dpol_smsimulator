{
  // [EN] Print event count of every ypol g4input ROOT under
  //      data/simulation/g4input/20260413ypol/.
  // [CN] 打印 g4input/20260413ypol 下每个 ypol 输入 ROOT 的事件数。
  TString base = "data/simulation/g4input/20260413ypol";
  for (auto iso : {"Sn112", "Sn124"}) {
    for (auto gam : {"g050", "g060", "g070", "g080"}) {
      for (auto dir : {"ynp", "ypn"}) {
        TString path = Form("%s/d+%sE190/d+%sE190%s%s-RP360/dbreak.root",
                            base.Data(), iso, iso, gam, dir);
        TFile *f = TFile::Open(path);
        if (!f || f->IsZombie()) {
          printf("MISS  %s\n", path.Data());
          continue;
        }
        TTree *t = (TTree*)f->Get("tree");
        Long64_t n = t ? t->GetEntries() : -1;
        printf("%-9s %-6s %-4s  N=%lld\n", iso, gam, dir, n);
        delete f;
      }
    }
  }
}
