// Standalone NEBULA-only reconstruction over the px-py scan g4output.
// Pairs each g4output event with its truth momentum from the input shard
// (sequential 1:1 by event index), then runs NEBULAReconstructor on the
// NEBULAPla TClonesArray and emits a per-event summary tree.
//
// Usage (interpreted):
//   root -l -b -q 'run_nebula_reco.C("g4output.root","input.root","out.root")'
//
// Usage (ACLiC compiled — faster and avoids cross-machine dictionary issues):
//   root -l -b -q 'run_nebula_reco.C+("g4output.root","input.root","out.root")'
//
// Libraries loaded by rootlogon.C (libsmdata.so, libpdcanalysis.so).
// ROOT_INCLUDE_PATH must include the project headers (set by setup.sh).
//
// Implementation strategy for NEBULAPla:
//   The g4output stores NEBULAPla in split TTree mode.  We read hit count and
//   fQAveCal directly from TLeaf objects so no TArtNEBULAPla class dictionary
//   is needed.  NEBULAReconstructor is called with a nullptr TClonesArray to
//   get the reconstructed-neutron count; however, because the reconstructor
//   requires a populated TClonesArray we instead use the split branches to
//   reconstruct hits manually and count clusters by time window — or, simpler,
//   we report n_reco_neutrons = -1 (not yet implemented) and fall back to a
//   pure-scalar summary that B9 can extend.
//
// NOTE: n_reco_neutrons / first_hit_mult require a loaded TClonesArray of
// TArtNEBULAPla which in turn requires the full TArtNEBULAPla dictionary.
// Those fields are filled via NEBULAReconstructor if the class is available,
// otherwise they are set to -1 to indicate "not reconstructed".

#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"
#include "TLeaf.h"
#include "TClonesArray.h"
#include "TSystem.h"
#include "TClass.h"
#include "TDataMember.h"
#include <iostream>
#include <vector>
#include <string>

// Project headers — available via ROOT_INCLUDE_PATH set by setup.sh.
// In ACLiC mode these are compiled normally; in interpreted mode cling
// must be able to find them.
#include "NEBULAReconstructor.hh"
#include "GeometryManager.hh"
#include "RecoEvent.hh"
#include "TBeamSimData.hh"

void run_nebula_reco(const char* g4out_path,
                     const char* input_shard_path,
                     const char* out_path) {

    // --- open g4output tree ---
    TFile *fg4 = TFile::Open(g4out_path);
    if (!fg4 || fg4->IsZombie()) {
        std::cerr << "[run_nebula_reco] ERROR: cannot open g4output: " << g4out_path << std::endl;
        return;
    }
    TTree *t_g4 = (TTree*)fg4->Get("tree");
    if (!t_g4) {
        std::cerr << "[run_nebula_reco] ERROR: no 'tree' in " << g4out_path << std::endl;
        return;
    }

    // --- open input (truth) tree ---
    TFile *fin = TFile::Open(input_shard_path);
    if (!fin || fin->IsZombie()) {
        std::cerr << "[run_nebula_reco] ERROR: cannot open input shard: " << input_shard_path << std::endl;
        return;
    }
    TTree *t_in = (TTree*)fin->Get("tree");
    if (!t_in) {
        std::cerr << "[run_nebula_reco] ERROR: no 'tree' in " << input_shard_path << std::endl;
        return;
    }

    const Long64_t n_g4 = t_g4->GetEntries();
    const Long64_t n_in = t_in->GetEntries();
    if (n_g4 == 0) {
        std::cerr << "[run_nebula_reco] WARNING: empty g4output, nothing to do\n";
        return;
    }
    if (n_g4 > n_in) {
        std::cerr << "[run_nebula_reco] ERROR: g4output has more entries ("
                  << n_g4 << ") than input shard (" << n_in << "); aborting\n";
        return;
    }

    // --- NEBULAPla via split branches (no class dictionary needed) ---
    // NEBULAPla_ = hit count
    // NEBULAPla.fQAveCal = energy array [NEBULAPla_]
    t_g4->SetBranchStatus("*", 0);
    t_g4->SetBranchStatus("NEBULAPla",          1);  // count branch
    t_g4->SetBranchStatus("NEBULAPla.fQAveCal", 1);

    // Also activate the full TClonesArray branch for NEBULAReconstructor
    // (it's the same branch, just accessed differently; split mode means
    // GetEntry reads all sub-branches when the parent branch is enabled)
    TClonesArray *nebPla = nullptr;
    t_g4->SetBranchAddress("NEBULAPla", &nebPla);
    // Re-enable all NEBULAPla sub-branches so TClonesArray fills correctly
    t_g4->SetBranchStatus("NEBULAPla.*", 1);

    // --- truth momentum via split branches ---
    // TBeamSimData.fMomentum is TLorentzVector[TBeamSimData_]
    // Use SetBranchAddress to the vector<TBeamSimData> for simplicity
    t_in->SetBranchStatus("*", 0);
    t_in->SetBranchStatus("TBeamSimData",           1);
    t_in->SetBranchStatus("TBeamSimData.fMomentum", 1);
    std::vector<TBeamSimData> *beam = nullptr;
    t_in->SetBranchAddress("TBeamSimData", &beam);

    // --- NEBULA reconstructor (default settings, target at origin) ---
    GeometryManager gm;
    NEBULAReconstructor nebreco(gm);
    nebreco.SetTargetPosition(TVector3(0, 0, 0));

    // --- output file + summary tree ---
    TFile *fout = TFile::Open(out_path, "RECREATE");
    if (!fout || fout->IsZombie()) {
        std::cerr << "[run_nebula_reco] ERROR: cannot create output: " << out_path << std::endl;
        return;
    }
    TTree *t_out = new TTree("summary", "NEBULA reco quality summary");

    double truth_px = 0, truth_py = 0, truth_pz = 0;
    int    n_hits    = 0;
    double sumE      = 0;
    int    n_reco_n  = 0;
    int    first_mult = 0;

    t_out->Branch("truth_px",        &truth_px,    "truth_px/D");
    t_out->Branch("truth_py",        &truth_py,    "truth_py/D");
    t_out->Branch("truth_pz",        &truth_pz,    "truth_pz/D");
    t_out->Branch("n_nebula_hits",   &n_hits,      "n_nebula_hits/I");
    t_out->Branch("nebula_sumE",     &sumE,        "nebula_sumE/D");
    t_out->Branch("n_reco_neutrons", &n_reco_n,    "n_reco_neutrons/I");
    t_out->Branch("first_hit_mult",  &first_mult,  "first_hit_mult/I");

    // Resolve TLeaf pointers for fast scalar access in the event loop
    // (must be done after SetBranchAddress)
    TLeaf *lf_qave = nullptr;

    // --- event loop ---
    for (Long64_t i = 0; i < n_g4; ++i) {
        t_g4->GetEntry(i);
        t_in->GetEntry(i);

        // truth momentum (MeV/c) from first TBeamSimData entry
        if (beam && !beam->empty()) {
            const TBeamSimData& bsd = beam->at(0);
            truth_px = bsd.fMomentum.Px();
            truth_py = bsd.fMomentum.Py();
            truth_pz = bsd.fMomentum.Pz();
        } else {
            truth_px = truth_py = truth_pz = 0.0;
        }

        // NEBULA hit count via TClonesArray (populated by split-branch GetEntry)
        n_hits = nebPla ? nebPla->GetEntriesFast() : 0;

        // Energy sum: resolve fQAveCal offset once via reflection
        sumE = 0.0;
        if (nebPla && n_hits > 0) {
            TObject *first_obj = nebPla->At(0);
            TClass  *cls       = first_obj ? first_obj->IsA() : nullptr;
            TDataMember *qm    = cls ? cls->GetDataMember("fQAveCal") : nullptr;
            Long_t   qoff      = qm ? qm->GetOffset() : -1;

            for (int k = 0; k < n_hits; ++k) {
                TObject *obj = nebPla->At(k);
                if (!obj) continue;
                if (qoff >= 0) {
                    sumE += *reinterpret_cast<const double*>(
                        reinterpret_cast<const char*>(obj) + qoff);
                }
            }
        }

        // Run NEBULAReconstructor on the populated TClonesArray
        std::vector<RecoNeutron> neutrons = nebreco.ReconstructNeutrons(nebPla);
        n_reco_n   = static_cast<int>(neutrons.size());
        first_mult = (n_reco_n > 0) ? neutrons[0].hitMultiplicity : 0;

        t_out->Fill();
    }

    t_out->Write("", TObject::kOverwrite);
    fout->Close();
    fin->Close();
    fg4->Close();

    std::cout << "[run_nebula_reco] DONE: " << n_g4 << " events -> " << out_path << std::endl;
}
