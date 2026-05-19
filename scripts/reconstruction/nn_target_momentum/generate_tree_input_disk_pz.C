// [EN] Usage: root -l -q 'scripts/reconstruction/nn_target_momentum/generate_tree_input_disk_pz.C+(10000,20260227,150,500,700,-12.4883,0.0089,-1069.4585,"tree_input.root",3.0)' / [CN] 用法: root -l -q 'scripts/reconstruction/nn_target_momentum/generate_tree_input_disk_pz.C+(10000,20260227,150,500,700,-12.4883,0.0089,-1069.4585,"tree_input.root",3.0)'
//
// [EN] target_angle_deg samples the (px,py) disk and pz range in target frame, then RotateY(+angle) into lab frame for the gun, so that downstream build_dataset.C's RotateY(-angle) recovers exactly the target-frame disk as training labels. Pass 0 to keep the legacy lab-frame disk behavior.
// [CN] target_angle_deg：先在靶系撒 (px,py) 圆盘 + pz 区间，再 RotateY(+angle) 转到 lab 系作为 gun，使 build_dataset.C 的 RotateY(-angle) 还原出干净的靶系圆盘 label。传 0 即退回旧的 lab 系采样。

#include "TBeamSimData.hh"

#include "TFile.h"
#include "TLorentzVector.h"
#include "TRandom3.h"
#include "TTree.h"
#include "TVector3.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

void generate_tree_input_disk_pz(
    Long64_t n_events = 10000,
    UInt_t seed = 20260227,
    double r_max_mevc = 150.0,
    double pz_min_mevc = 500.0,
    double pz_max_mevc = 700.0,
    double vertex_x_mm = -12.4883,
    double vertex_y_mm = 0.0089,
    double vertex_z_mm = -1069.4585,
    const char* out_root = "data/nn_target_momentum/tree_input_disk_pz.root",
    double target_angle_deg = 0.0
) {
    if (!out_root) {
        std::cerr << "[generate_tree_input_disk_pz] output path is null" << std::endl;
        return;
    }
    if (n_events <= 0) {
        std::cerr << "[generate_tree_input_disk_pz] n_events must be positive" << std::endl;
        return;
    }
    if (!(std::isfinite(r_max_mevc) && r_max_mevc > 0.0)) {
        std::cerr << "[generate_tree_input_disk_pz] r_max_mevc must be positive" << std::endl;
        return;
    }
    if (!(std::isfinite(pz_min_mevc) && std::isfinite(pz_max_mevc) && pz_max_mevc > pz_min_mevc)) {
        std::cerr << "[generate_tree_input_disk_pz] invalid pz range" << std::endl;
        return;
    }
    if (!std::isfinite(target_angle_deg)) {
        std::cerr << "[generate_tree_input_disk_pz] invalid target_angle_deg" << std::endl;
        return;
    }

    const double target_angle_rad = target_angle_deg * M_PI / 180.0;

    const std::filesystem::path out_path(out_root);
    if (!out_path.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(out_path.parent_path(), ec);
        if (ec) {
            std::cerr << "[generate_tree_input_disk_pz] failed to create output directory: "
                      << out_path.parent_path() << std::endl;
            return;
        }
    }

    TFile out_file(out_root, "RECREATE");
    if (out_file.IsZombie()) {
        std::cerr << "[generate_tree_input_disk_pz] failed to create output ROOT: " << out_root << std::endl;
        return;
    }

    TTree tree("tree_input", "Input tree for disk(px,py)+uniform(pz) proton events");
    gBeamSimDataArray = new TBeamSimDataArray();
    tree.Branch("TBeamSimData", &gBeamSimDataArray);

    constexpr double kProtonMassMeV = 938.2720813;
    constexpr double kTwoPi = 6.283185307179586476925286766559;

    TRandom3 rng(seed);
    TVector3 vertex(vertex_x_mm, vertex_y_mm, vertex_z_mm);

    for (Long64_t i = 0; i < n_events; ++i) {
        gBeamSimDataArray->clear();

        // [EN] Sample radius as sqrt(U) to enforce uniform density over disk area. (px_t, py_t, pz_t) live in the target frame. / [CN] 半径按sqrt(U)采样以保证圆盘面积均匀分布。(px_t, py_t, pz_t) 定义在靶系。
        const double radius = std::sqrt(rng.Uniform(0.0, 1.0)) * r_max_mevc;
        const double phi = rng.Uniform(0.0, kTwoPi);
        const double px_target = radius * std::cos(phi);
        const double py_target = radius * std::sin(phi);
        const double pz_target = rng.Uniform(pz_min_mevc, pz_max_mevc);

        // [EN] Rotate from target frame back to lab frame: lab = R_y(+angle) · target. Geant4 gun fires in lab; build_dataset.C undoes this with R_y(-angle). / [CN] 由靶系旋回 lab 系：lab = R_y(+angle) · target。Geant4 在 lab 系发射，build_dataset.C 用 R_y(-angle) 反旋拿到靶系 label。
        TVector3 p_lab(px_target, py_target, pz_target);
        if (target_angle_rad != 0.0) {
            p_lab.RotateY(target_angle_rad);
        }
        const double px = p_lab.X();
        const double py = p_lab.Y();
        const double pz = p_lab.Z();

        const double e = std::sqrt(px * px + py * py + pz * pz + kProtonMassMeV * kProtonMassMeV);
        TLorentzVector p4(px, py, pz, e);

        TBeamSimData proton(1, 1, p4, vertex);
        proton.fPrimaryParticleID = 0;
        proton.fParticleName = "proton";
        gBeamSimDataArray->push_back(proton);

        tree.Fill();
    }

    tree.Write();
    out_file.Close();

    std::cout << "[generate_tree_input_disk_pz] done" << std::endl;
    std::cout << "[generate_tree_input_disk_pz] n_events=" << n_events << std::endl;
    std::cout << "[generate_tree_input_disk_pz] seed=" << seed << std::endl;
    std::cout << "[generate_tree_input_disk_pz] r_max_mevc=" << r_max_mevc << std::endl;
    std::cout << "[generate_tree_input_disk_pz] pz_range=[" << pz_min_mevc << ", " << pz_max_mevc << "]" << std::endl;
    std::cout << "[generate_tree_input_disk_pz] target_angle_deg=" << target_angle_deg
              << " (disk sampled in target frame, rotated to lab for gun)" << std::endl;
    std::cout << "[generate_tree_input_disk_pz] output=" << out_root << std::endl;
}
