// Extract per-event truth and reco proton + truth neutron 4-momentum from
// reco_*.root. Reco neutron is deferred (rootmap library mismatch); truth_neutron
// is used as a proxy in downstream analysis.
//
// Run as: root -b -l -q -e '.x extract_phys_observables.C("input.root","output.csv","tag")'
// (Pure interpreter mode, no ACLiC compilation, to avoid library-loading issues.)

void extract_phys_observables(const char* input_path, const char* output_csv_path, const char* tag = "") {
    cout << "[extract] " << input_path << " -> " << output_csv_path << endl;
    TFile* f = TFile::Open(input_path, "READ");
    if (!f || f->IsZombie()) { cout << "[extract] FAIL: open" << endl; return; }
    TTree* t = (TTree*)f->Get("recoTree");
    if (!t) { cout << "[extract] FAIL: no recoTree" << endl; return; }
    const Long64_t n = t->GetEntries();
    cout << "[extract] entries = " << n << endl;

    Bool_t truth_has_proton, truth_has_neutron;
    TLorentzVector* truth_proton_p4 = nullptr;
    TLorentzVector* truth_neutron_p4 = nullptr;
    vector<double>* rpx = nullptr;
    vector<double>* rpy = nullptr;
    vector<double>* rpz = nullptr;
    vector<double>* rE  = nullptr;
    vector<double>* rp  = nullptr;
    vector<int>* rstatus = nullptr;
    vector<double>* rchi2 = nullptr;
    t->SetBranchAddress("truth_has_proton", &truth_has_proton);
    t->SetBranchAddress("truth_has_neutron", &truth_has_neutron);
    t->SetBranchAddress("truth_proton_p4", &truth_proton_p4);
    t->SetBranchAddress("truth_neutron_p4", &truth_neutron_p4);
    t->SetBranchAddress("reco_proton_px", &rpx);
    t->SetBranchAddress("reco_proton_py", &rpy);
    t->SetBranchAddress("reco_proton_pz", &rpz);
    t->SetBranchAddress("reco_proton_e", &rE);
    t->SetBranchAddress("reco_proton_p", &rp);
    t->SetBranchAddress("reco_proton_status", &rstatus);
    t->SetBranchAddress("reco_proton_chi2_reduced", &rchi2);

    ofstream out(output_csv_path);
    out << "tag,event_index,n_reco_proton,truth_has_proton,truth_has_neutron,"
        << "truth_pxp,truth_pyp,truth_pzp,truth_ep,"
        << "truth_pxn,truth_pyn,truth_pzn,truth_en,"
        << "reco_pxp,reco_pyp,reco_pzp,reco_ep,reco_p,"
        << "reco_proton_status,reco_proton_chi2_red,"
        << "n_reco_neutrons,reco_pxn,reco_pyn,reco_pzn,reco_n_energy" << endl;

    int n_with_reco = 0;
    for (Long64_t i = 0; i < n; ++i) {
        t->GetEntry(i);
        const double tpxp = truth_proton_p4 ? truth_proton_p4->Px() : 0;
        const double tpyp = truth_proton_p4 ? truth_proton_p4->Py() : 0;
        const double tpzp = truth_proton_p4 ? truth_proton_p4->Pz() : 0;
        const double tep  = truth_proton_p4 ? truth_proton_p4->E()  : 0;
        const double tpxn = truth_neutron_p4 ? truth_neutron_p4->Px() : 0;
        const double tpyn = truth_neutron_p4 ? truth_neutron_p4->Py() : 0;
        const double tpzn = truth_neutron_p4 ? truth_neutron_p4->Pz() : 0;
        const double ten  = truth_neutron_p4 ? truth_neutron_p4->E()  : 0;
        const int n_reco = rpx ? (int)rpx->size() : 0;
        double rpxp_=0, rpyp_=0, rpzp_=0, rep_=0, rp_=0, rchi2_=0;
        int rstatus_ = -1;
        if (n_reco > 0) {
            rpxp_ = rpx->at(0); rpyp_ = rpy->at(0); rpzp_ = rpz->at(0);
            rep_  = rE->at(0);  rp_   = rp->at(0);
            rchi2_ = rchi2 ? rchi2->at(0) : 0;
            rstatus_ = rstatus ? rstatus->at(0) : 0;
            ++n_with_reco;
        }
        out << tag << "," << i << "," << n_reco << ","
            << (int)truth_has_proton << "," << (int)truth_has_neutron << ","
            << tpxp << "," << tpyp << "," << tpzp << "," << tep << ","
            << tpxn << "," << tpyn << "," << tpzn << "," << ten << ","
            << rpxp_ << "," << rpyp_ << "," << rpzp_ << "," << rep_ << "," << rp_ << ","
            << rstatus_ << "," << rchi2_ << ","
            << "0,0,0,0,0" << endl;
    }
    out.close();
    f->Close();
    cout << "[extract] DONE: " << n << " rows, " << n_with_reco << " with reco proton -> " << output_csv_path << endl;
}
