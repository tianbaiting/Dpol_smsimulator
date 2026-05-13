// [EN] Extract per-event observables from NN reco ROOT for one cell.
//      Output CSV columns: tag, b_segment, event_index, n_reco_proton,
//      truth_has_proton, truth_has_neutron,
//      truth_p{x,y,z}p, truth_ep, truth_p{x,y,z}n, truth_en,
//      nn_p{x,y,z}p, nn_ep, nn_p, nn_status,
//      n_reco_neutrons, reco_p{x,y,z}n, reco_n_energy
// [CN] 从 NN reco ROOT 抽出每事件 truth + NN proton + NEBULA neutron, 写一份 CSV.
//
// Usage:
//   root -b -l -q -e '.x extract_observables.C("input_reco.root","out.csv","tag","b00")'
// b_segment is the empty string for ypol cells, "b01".."b10" for zpol cells.
void extract_observables(const char* input_path, const char* csv_path,
                         const char* tag, const char* b_segment = "") {
    cout << "[extract_obs] " << tag << "  b=" << b_segment
         << "  " << input_path << " -> " << csv_path << endl;
    TFile* f = TFile::Open(input_path, "READ");
    if (!f || f->IsZombie()) { cout << "[extract_obs] FAIL: open" << endl; return; }
    TTree* t = (TTree*)f->Get("recoTree");
    if (!t) { cout << "[extract_obs] FAIL: no recoTree" << endl; return; }
    const Long64_t n = t->GetEntries();

    Bool_t truth_has_proton=false, truth_has_neutron=false;
    TLorentzVector* truth_proton_p4 = nullptr;
    TLorentzVector* truth_neutron_p4 = nullptr;
    vector<double>* nn_px = nullptr; vector<double>* nn_py = nullptr; vector<double>* nn_pz = nullptr;
    vector<double>* nn_e  = nullptr; vector<double>* nn_p  = nullptr;
    vector<int>*    nn_status = nullptr;
    RecoEvent* re = nullptr;
    t->SetBranchAddress("truth_has_proton", &truth_has_proton);
    t->SetBranchAddress("truth_has_neutron", &truth_has_neutron);
    t->SetBranchAddress("truth_proton_p4", &truth_proton_p4);
    t->SetBranchAddress("truth_neutron_p4", &truth_neutron_p4);
    t->SetBranchAddress("reco_proton_px", &nn_px);
    t->SetBranchAddress("reco_proton_py", &nn_py);
    t->SetBranchAddress("reco_proton_pz", &nn_pz);
    t->SetBranchAddress("reco_proton_e",  &nn_e);
    t->SetBranchAddress("reco_proton_p",  &nn_p);
    t->SetBranchAddress("reco_proton_status", &nn_status);
    t->SetBranchAddress("recoEvent", &re);

    // Append mode if csv already has content (concatenation across b-segments)
    bool need_header = true;
    {
        std::ifstream probe(csv_path);
        if (probe.good() && probe.peek() != std::ifstream::traits_type::eof()) need_header = false;
    }
    ofstream out(csv_path, std::ios::app);
    out << std::setprecision(8);
    if (need_header) {
        out << "tag,b_segment,event_index,n_reco_proton,"
            << "truth_has_proton,truth_has_neutron,"
            << "truth_pxp,truth_pyp,truth_pzp,truth_ep,"
            << "truth_pxn,truth_pyn,truth_pzn,truth_en,"
            << "nn_pxp,nn_pyp,nn_pzp,nn_ep,nn_p,nn_status,"
            << "n_reco_neutrons,reco_pxn,reco_pyn,reco_pzn,reco_n_energy,hit_mult_n" << endl;
    }

    int n_with_nn = 0;
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
        const int n_reco = nn_px ? (int)nn_px->size() : 0;
        double nnpx_=0, nnpy_=0, nnpz_=0, nne_=0, nnp_=0; int nnst_=-1;
        if (n_reco > 0) {
            nnpx_ = nn_px->at(0); nnpy_ = nn_py->at(0); nnpz_ = nn_pz->at(0);
            nne_  = nn_e->at(0);  nnp_  = nn_p->at(0);
            nnst_ = nn_status ? nn_status->at(0) : 0;
            ++n_with_nn;
        }
        // Reco neutron from RecoEvent.neutrons[0] (frame-rotated to target frame upstream).
        int n_reco_neutrons = 0;
        double rpxn_=0, rpyn_=0, rpzn_=0, ren_=0, rbeta_=0;
        int hit_mult_n_ = 0;
        if (re) {
            n_reco_neutrons = (int)re->neutrons.size();
            if (n_reco_neutrons > 0) {
                const auto& neu = re->neutrons[0];
                hit_mult_n_ = neu.hitMultiplicity;
                rbeta_ = neu.beta; ren_ = neu.energy;
                if (rbeta_ > 0 && rbeta_ < 1.0) {
                    const double m_n = 939.565;
                    const double gamma_n = 1.0 / sqrt(1.0 - rbeta_*rbeta_);
                    const double p_mag = m_n * gamma_n * rbeta_;
                    rpxn_ = p_mag * neu.direction.X();
                    rpyn_ = p_mag * neu.direction.Y();
                    rpzn_ = p_mag * neu.direction.Z();
                }
            }
        }
        out << tag << "," << b_segment << "," << i << "," << n_reco << ","
            << (int)truth_has_proton << "," << (int)truth_has_neutron << ","
            << tpxp << "," << tpyp << "," << tpzp << "," << tep << ","
            << tpxn << "," << tpyn << "," << tpzn << "," << ten << ","
            << nnpx_ << "," << nnpy_ << "," << nnpz_ << "," << nne_ << "," << nnp_ << "," << nnst_ << ","
            << n_reco_neutrons << "," << rpxn_ << "," << rpyn_ << "," << rpzn_ << "," << ren_ << "," << hit_mult_n_ << endl;
    }
    out.close();
    f->Close();
    cout << "[extract_obs] DONE: " << n << " rows; nn_ok=" << n_with_nn
         << "  csv=" << csv_path << endl;
}
