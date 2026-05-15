// dump_nebula_reco_golden.cc
//
// One-shot tool: run the legacy NEBULAReconstructor against a fixed input ROOT
// file and write a deterministic representation of its output into a golden
// ROOT file.  This file is the regression pin for the Phase 2 refactor.
//
// Smearing is set to zero (SetPositionSmearing(0) / SetTimeSmearing(0)) so
// that the output is fully deterministic.  The Phase 2 replacement must match
// this golden file with the same smearing-disabled configuration.
//
// Usage:
//   dump_nebula_reco_golden <input.root> <output_golden.root>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include <TFile.h>
#include <TTree.h>
#include <TClonesArray.h>
#include <TVector3.h>

#include "SMLogger.hh"
#include "NEBULAReconstructor.hh"
#include "GeometryManager.hh"
#include "RecoEvent.hh"  // defines RecoNeutron

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: dump_nebula_reco_golden <input.root> <output.root>\n");
        return 1;
    }
    const char* inpath  = argv[1];
    const char* outpath = argv[2];

    // Initialize logger in sync mode to avoid the spdlog async thread-pool
    // racing with ROOT's atexit cleanup.  Warn level reduces noise.
    {
        SMLogger::LogConfig cfg;
        cfg.async  = false;
        cfg.level  = SMLogger::LogLevel::WARN;
        cfg.console = true;
        cfg.file    = false;
        SMLogger::Logger::Instance().Initialize(cfg);
    }

    // -----------------------------------------------------------------------
    // Open input
    // -----------------------------------------------------------------------
    TFile* fin = TFile::Open(inpath, "READ");
    if (!fin || fin->IsZombie()) {
        printf("ERROR: cannot open input file: %s\n", inpath);
        return 2;
    }

    TTree* t = dynamic_cast<TTree*>(fin->Get("tree"));
    if (!t) {
        // Fallback names seen in other SAMURAI sim outputs
        t = dynamic_cast<TTree*>(fin->Get("g4tree"));
    }
    if (!t) {
        printf("ERROR: no 'tree' (or 'g4tree') TTree found in %s\n", inpath);
        fin->Close();
        return 3;
    }

    TClonesArray* neb = nullptr;
    t->SetBranchAddress("NEBULAPla", &neb);

    // -----------------------------------------------------------------------
    // Build NEBULAReconstructor
    // -----------------------------------------------------------------------
    // Allocate on the heap: NEBULAReconstructor inherits TObject (ClassDef),
    // and ROOT tracks TObject instances for cleanup at exit.  A stack object
    // would be destroyed twice (once by the dtor, once by ROOT's exit hook)
    // causing heap corruption.  Manually delete before fin->Close() below.
    GeometryManager geo;  // default constructor (no path argument needed)

    NEBULAReconstructor* reco = new NEBULAReconstructor(geo);

    // Disable stochastic smearing so the output is bit-for-bit reproducible
    reco->SetPositionSmearing(0.0);
    reco->SetTimeSmearing(0.0);

    // Target at origin (default configuration for smoke test)
    reco->SetTargetPosition(TVector3(0, 0, 0));

    // -----------------------------------------------------------------------
    // Open output and build golden tree
    // -----------------------------------------------------------------------
    TFile* fout = TFile::Open(outpath, "RECREATE");
    if (!fout || fout->IsZombie()) {
        printf("ERROR: cannot open output file: %s\n", outpath);
        fin->Close();
        return 4;
    }

    TTree* tout = new TTree("golden", "legacy NEBULAReconstructor outputs (smearing disabled)");

    // Per-event scalar summaries (deterministic floats)
    int ev_nNeutrons   = 0;
    double ev_totE     = 0.0;
    int ev_totHitMult  = 0;

    // First-neutron kinematics (NaN-safe: written as 0 when no neutron)
    double n0_posX = 0, n0_posY = 0, n0_posZ = 0;
    double n0_dirX = 0, n0_dirY = 0, n0_dirZ = 0;
    double n0_beta = 0, n0_tof  = 0, n0_flen = 0;
    double n0_ene  = 0;
    int    n0_mult = 0;

    tout->Branch("ev_nNeutrons",  &ev_nNeutrons,  "ev_nNeutrons/I");
    tout->Branch("ev_totE",       &ev_totE,        "ev_totE/D");
    tout->Branch("ev_totHitMult", &ev_totHitMult,  "ev_totHitMult/I");
    tout->Branch("n0_posX",  &n0_posX,  "n0_posX/D");
    tout->Branch("n0_posY",  &n0_posY,  "n0_posY/D");
    tout->Branch("n0_posZ",  &n0_posZ,  "n0_posZ/D");
    tout->Branch("n0_dirX",  &n0_dirX,  "n0_dirX/D");
    tout->Branch("n0_dirY",  &n0_dirY,  "n0_dirY/D");
    tout->Branch("n0_dirZ",  &n0_dirZ,  "n0_dirZ/D");
    tout->Branch("n0_beta",  &n0_beta,  "n0_beta/D");
    tout->Branch("n0_tof",   &n0_tof,   "n0_tof/D");
    tout->Branch("n0_flen",  &n0_flen,  "n0_flen/D");
    tout->Branch("n0_ene",   &n0_ene,   "n0_ene/D");
    tout->Branch("n0_mult",  &n0_mult,  "n0_mult/I");

    // Full candidate vector (for detailed comparison by future tests)
    std::vector<RecoNeutron>* cands = new std::vector<RecoNeutron>();
    tout->Branch("cands", &cands);

    // -----------------------------------------------------------------------
    // Event loop
    // -----------------------------------------------------------------------
    Long64_t nEntries = t->GetEntries();
    for (Long64_t i = 0; i < nEntries; ++i) {
        t->GetEntry(i);

        *cands = reco->ReconstructNeutrons(neb);

        // Reset per-event scalars
        ev_nNeutrons  = static_cast<int>(cands->size());
        ev_totE       = 0.0;
        ev_totHitMult = 0;
        n0_posX = n0_posY = n0_posZ = 0;
        n0_dirX = n0_dirY = n0_dirZ = 0;
        n0_beta = n0_tof = n0_flen = n0_ene = 0;
        n0_mult = 0;

        for (const auto& n : *cands) {
            ev_totE       += n.energy;
            ev_totHitMult += n.hitMultiplicity;
        }

        if (!cands->empty()) {
            const RecoNeutron& n0 = (*cands)[0];
            n0_posX = n0.position.X();
            n0_posY = n0.position.Y();
            n0_posZ = n0.position.Z();
            n0_dirX = n0.direction.X();
            n0_dirY = n0.direction.Y();
            n0_dirZ = n0.direction.Z();
            n0_beta = n0.beta;
            n0_tof  = n0.timeOfFlight;
            n0_flen = n0.flightLength;
            n0_ene  = n0.energy;
            n0_mult = n0.hitMultiplicity;
        }

        tout->Fill();
    }

    tout->Write("", TObject::kOverwrite);
    fout->Close();
    fin->Close();

    // Print success before triggering any destructors that race with ROOT/spdlog
    printf("OK: wrote %lld events to %s\n", static_cast<long long>(nEntries), outpath);
    fflush(stdout);

    // Delete before ROOT's exit hook sees them to avoid double-free
    delete reco;
    delete cands;

    // Shut down the logger explicitly before ROOT's atexit hooks fire, to avoid
    // the spdlog thread-pool racing with ROOT's TClass cleanup on exit.
    SMLogger::Logger::Instance().Shutdown();
    return 0;
}
