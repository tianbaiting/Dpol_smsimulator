#include <TFile.h>
#include <TTree.h>
#include "TBeamSimData.hh"
#include "QMDInputMetadata.hh"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
constexpr double kPi = 3.14159265358979323846;

void check(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "[FAIL] " << msg << std::endl;
        std::exit(1);
    }
    std::cout << "[OK] " << msg << std::endl;
}
}

void test_geninput_qmdrawdata_roundtrip(const char* work_dir) {
    using namespace qmd_input_metadata;

    // [EN] y_pol: 3 events with rpphi 0/90/180°.
    {
        const std::string path = std::string(work_dir)
            + "/out/y_pol/phi_random/d+Sn124E190g050ynp/dbreak.root";
        TFile* f = TFile::Open(path.c_str(), "READ");
        check(f && !f->IsZombie(), "y_pol file open: " + path);
        TTree* t = (TTree*)f->Get("tree");
        check(t != nullptr, "y_pol tree present");
        TBeamSimDataArray* arr = nullptr;
        t->SetBranchAddress("TBeamSimData", &arr);
        check(t->GetEntries() == 3, "y_pol has 3 entries");

        const double expected[] = {0.0, kPi / 2.0, kPi};
        for (int i = 0; i < 3; ++i) {
            t->GetEntry(i);
            const double bphi = arr->at(0).fUserDouble[kBPhiIndex];
            check(std::abs(bphi - expected[i]) < 1e-6,
                  "y_pol event " + std::to_string(i) + " b_phi");
            const double bimp = arr->at(0).fUserDouble[kBimpIndex];
            check(std::abs(bimp - 5.0) < 1e-6,
                  "y_pol event " + std::to_string(i) + " bimp");
        }
        f->Close();
    }

    // [EN] z_pol: 3 events with rotation. b_phi must be in [0, 2π).
    {
        const std::string path = std::string(work_dir)
            + "/out/z_pol/b_discrete/d+Sn124E190g050znp/dbreakb05.root";
        TFile* f = TFile::Open(path.c_str(), "READ");
        check(f && !f->IsZombie(), "z_pol file open: " + path);
        TTree* t = (TTree*)f->Get("tree");
        check(t != nullptr, "z_pol tree present");
        TBeamSimDataArray* arr = nullptr;
        t->SetBranchAddress("TBeamSimData", &arr);

        for (Long64_t i = 0; i < t->GetEntries(); ++i) {
            t->GetEntry(i);
            const double bphi = arr->at(0).fUserDouble[kBPhiIndex];
            check(bphi >= 0.0 && bphi < 2.0 * kPi,
                  "z_pol event " + std::to_string(i) + " b_phi in [0, 2π)");
        }
        f->Close();
    }

    std::cout << "ALL CHECKS PASSED" << std::endl;
}
