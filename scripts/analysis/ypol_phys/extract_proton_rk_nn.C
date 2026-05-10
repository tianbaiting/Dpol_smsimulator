// [EN] Extract per-event truth proton p4, RK reco proton p, and NN reco proton p
//      from two recoTree ROOT files (RK and NN backends share the same input
//      events because both consume the same sampled/*.root). Writes one CSV row
//      per truth-proton event.
// [CN] 从同一 sampled root 重建出的 RK 和 NN 两个 recoTree, 抽取真值 + RK + NN 质子动量, 写入 CSV.
//
// Usage:
//   root -b -l -q -e '.x extract_proton_rk_nn.C("rk.root","nn.root","out.csv","tag")'
void extract_proton_rk_nn(const char* rk_path, const char* nn_path,
                          const char* csv_path, const char* tag = "") {
    cout << "[extract_rk_nn] " << tag << ": " << rk_path << " + " << nn_path
         << " -> " << csv_path << endl;

    TFile* fr = TFile::Open(rk_path, "READ");
    TFile* fn = TFile::Open(nn_path, "READ");
    if (!fr || fr->IsZombie() || !fn || fn->IsZombie()) {
        cout << "[extract_rk_nn] FAIL: open" << endl; return;
    }
    TTree* tr = (TTree*)fr->Get("recoTree");
    TTree* tn = (TTree*)fn->Get("recoTree");
    if (!tr || !tn) { cout << "[extract_rk_nn] FAIL: recoTree" << endl; return; }
    const Long64_t n = tr->GetEntries();
    if (tn->GetEntries() != n) {
        cout << "[extract_rk_nn] WARN: entry mismatch RK=" << n
             << " NN=" << tn->GetEntries() << endl;
    }

    Bool_t truth_has_proton_r=false, truth_has_proton_n=false;
    TLorentzVector* truth_proton_p4_r = nullptr;
    TLorentzVector* truth_proton_p4_n = nullptr;
    vector<double>* rk_px = nullptr; vector<double>* rk_py = nullptr; vector<double>* rk_pz = nullptr;
    vector<double>* nn_px = nullptr; vector<double>* nn_py = nullptr; vector<double>* nn_pz = nullptr;
    vector<int>*    rk_status = nullptr;
    vector<int>*    nn_status = nullptr;

    tr->SetBranchAddress("truth_has_proton", &truth_has_proton_r);
    tr->SetBranchAddress("truth_proton_p4", &truth_proton_p4_r);
    tr->SetBranchAddress("reco_proton_px", &rk_px);
    tr->SetBranchAddress("reco_proton_py", &rk_py);
    tr->SetBranchAddress("reco_proton_pz", &rk_pz);
    tr->SetBranchAddress("reco_proton_status", &rk_status);

    tn->SetBranchAddress("truth_has_proton", &truth_has_proton_n);
    tn->SetBranchAddress("truth_proton_p4", &truth_proton_p4_n);
    tn->SetBranchAddress("reco_proton_px", &nn_px);
    tn->SetBranchAddress("reco_proton_py", &nn_py);
    tn->SetBranchAddress("reco_proton_pz", &nn_pz);
    tn->SetBranchAddress("reco_proton_status", &nn_status);

    ofstream out(csv_path);
    out << "tag,event_index,truth_has_proton,"
        << "truth_pxp,truth_pyp,truth_pzp,"
        << "rk_pxp,rk_pyp,rk_pzp,rk_status,"
        << "nn_pxp,nn_pyp,nn_pzp,nn_status" << endl;
    out << std::setprecision(8);

    int n_with_truth = 0, n_with_rk = 0, n_with_nn = 0;
    for (Long64_t i = 0; i < n; ++i) {
        tr->GetEntry(i);
        tn->GetEntry(i);
        const int has_truth = truth_has_proton_r ? 1 : 0;
        const double tpxp = truth_proton_p4_r ? truth_proton_p4_r->Px() : 0;
        const double tpyp = truth_proton_p4_r ? truth_proton_p4_r->Py() : 0;
        const double tpzp = truth_proton_p4_r ? truth_proton_p4_r->Pz() : 0;
        if (has_truth) ++n_with_truth;

        double rkpx=0, rkpy=0, rkpz=0; int rkst=-1;
        if (rk_px && !rk_px->empty()) {
            rkpx = rk_px->at(0); rkpy = rk_py->at(0); rkpz = rk_pz->at(0);
            rkst = rk_status ? rk_status->at(0) : 0;
            ++n_with_rk;
        }
        double nnpx=0, nnpy=0, nnpz=0; int nnst=-1;
        if (nn_px && !nn_px->empty()) {
            nnpx = nn_px->at(0); nnpy = nn_py->at(0); nnpz = nn_pz->at(0);
            nnst = nn_status ? nn_status->at(0) : 0;
            ++n_with_nn;
        }

        out << tag << "," << i << "," << has_truth << ","
            << tpxp << "," << tpyp << "," << tpzp << ","
            << rkpx << "," << rkpy << "," << rkpz << "," << rkst << ","
            << nnpx << "," << nnpy << "," << nnpz << "," << nnst << endl;
    }
    out.close();
    fr->Close(); fn->Close();
    cout << "[extract_rk_nn] DONE: " << n << " rows; truth=" << n_with_truth
         << " rk=" << n_with_rk << " nn=" << n_with_nn << endl;
}
