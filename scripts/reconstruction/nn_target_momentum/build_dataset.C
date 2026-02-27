// [EN] Usage: root -l -q 'scripts/reconstruction/nn_target_momentum/build_dataset.C+("sim.root","geom.mac",0,0.5,0.5,"out.root","out.csv")' / [CN] 用法: root -l -q 'scripts/reconstruction/nn_target_momentum/build_dataset.C+("sim.root","geom.mac",0,0.5,0.5,"out.root","out.csv")'

#include "TClonesArray.h"
#include "TFile.h"
#include "TSystem.h"
#include "TTree.h"

#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "TBeamSimData.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct SampleRow {
    long long event_id = -1;
    double x[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double y[3] = {0.0, 0.0, 0.0};
};

struct TruthBeamInfo {
    bool has_proton = false;
    TVector3 proton_momentum_target = TVector3(0.0, 0.0, 0.0);
};

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string Trim(const std::string& input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::string ExpandPathToken(std::string token) {
    const char* sms_dir = std::getenv("SMSIMDIR");
    const std::string sms = sms_dir ? std::string(sms_dir) : std::string();

    const std::string key1 = "{SMSIMDIR}";
    const std::string key2 = "$SMSIMDIR";

    std::size_t pos = token.find(key1);
    while (pos != std::string::npos) {
        token.replace(pos, key1.size(), sms);
        pos = token.find(key1, pos + sms.size());
    }

    pos = token.find(key2);
    while (pos != std::string::npos) {
        token.replace(pos, key2.size(), sms);
        pos = token.find(key2, pos + sms.size());
    }
    return token;
}

bool ResolveMacroPath(const std::string& raw_path, const std::filesystem::path& base_dir, std::filesystem::path* out_path) {
    if (!out_path) return false;
    std::filesystem::path path = ExpandPathToken(raw_path);
    if (!path.is_absolute()) {
        path = base_dir / path;
    }

    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        *out_path = canonical;
        return true;
    }

    *out_path = path.lexically_normal();
    return std::filesystem::exists(*out_path);
}

bool LoadGeometryRecursive(GeometryManager& geo, const std::filesystem::path& macro_path, std::set<std::string>& visited) {
    const std::string key = macro_path.lexically_normal().string();
    if (visited.count(key)) return true;

    std::ifstream fin(macro_path);
    if (!fin.is_open()) {
        std::cerr << "[build_dataset] Cannot open geometry macro: " << macro_path << std::endl;
        return false;
    }
    visited.insert(key);

    std::string line;
    while (std::getline(fin, line)) {
        const std::string clean = Trim(line);
        if (clean.empty() || clean[0] == '#') continue;
        if (clean.rfind("/control/execute", 0) == 0) {
            std::stringstream ss(clean);
            std::string cmd;
            std::string include_path_raw;
            ss >> cmd >> include_path_raw;
            if (include_path_raw.empty()) continue;

            std::filesystem::path include_path;
            if (!ResolveMacroPath(include_path_raw, macro_path.parent_path(), &include_path)) {
                std::cerr << "[build_dataset] Warning: unresolved include macro: " << include_path_raw << std::endl;
                continue;
            }
            if (!LoadGeometryRecursive(geo, include_path, visited)) return false;
        }
    }

    return geo.LoadGeometry(macro_path.string());
}

bool IsFinite(const TVector3& value) {
    return std::isfinite(value.X()) && std::isfinite(value.Y()) && std::isfinite(value.Z());
}

TruthBeamInfo ExtractTruthBeam(const std::vector<TBeamSimData>* beam, double target_angle_rad) {
    TruthBeamInfo info;
    if (!beam) return info;

    for (const auto& particle : *beam) {
        const std::string pname = ToLower(particle.fParticleName.Data());
        if ((particle.fZ == 1 && particle.fA == 1) || pname == "proton" || particle.fPDGCode == 2212) {
            TVector3 momentum = particle.fMomentum.Vect();
            momentum.RotateY(-target_angle_rad);
            info.has_proton = true;
            info.proton_momentum_target = momentum;
            break;
        }
    }
    return info;
}

}  // namespace

void build_dataset(const char* sim_root = nullptr,
                   const char* geom_macro = nullptr,
                   int max_events = 0,
                   double sigma_u = 0.5,
                   double sigma_v = 0.5,
                   const char* out_root = "data/nn_target_momentum/dataset_train.root",
                   const char* out_csv = "data/nn_target_momentum/dataset_train.csv") {
    gSystem->Load("libsmdata.so");
    if (gSystem->Load("libpdcanalysis.so") < 0) {
        gSystem->Load("libanalysis.so");
    }

    if (!sim_root || !geom_macro || !out_root || !out_csv) {
        std::cout << "[build_dataset] Please call build_dataset(sim_root, geom_macro, ...)." << std::endl;
        std::cout << "[build_dataset] Example:" << std::endl;
        std::cout << "  root -l -q 'scripts/reconstruction/nn_target_momentum/build_dataset.C+(\"sim.root\",\"geom.mac\",0,0.5,0.5,\"dataset.root\",\"dataset.csv\")'" << std::endl;
        return;
    }

    GeometryManager geo;
    std::filesystem::path geom_path;
    std::set<std::string> visited;
    if (!ResolveMacroPath(geom_macro, std::filesystem::current_path(), &geom_path)) {
        std::cerr << "[build_dataset] Failed to resolve geometry macro: " << geom_macro << std::endl;
        return;
    }
    if (!LoadGeometryRecursive(geo, geom_path, visited)) {
        std::cerr << "[build_dataset] Failed to load geometry macro: " << geom_macro << std::endl;
        return;
    }

    const double target_angle_rad = geo.GetTargetAngleRad();

    TFile in_file(sim_root, "READ");
    if (in_file.IsZombie()) {
        std::cerr << "[build_dataset] Failed to open input ROOT: " << sim_root << std::endl;
        return;
    }
    TTree* tree = dynamic_cast<TTree*>(in_file.Get("tree"));
    if (!tree) {
        std::cerr << "[build_dataset] Missing tree 'tree' in input ROOT." << std::endl;
        return;
    }

    if (!tree->GetBranch("FragSimData")) {
        std::cerr << "[build_dataset] Missing required branch 'FragSimData'." << std::endl;
        return;
    }
    if (!tree->GetBranch("beam")) {
        std::cerr << "[build_dataset] Missing required branch 'beam'." << std::endl;
        return;
    }

    TClonesArray* frag_array = nullptr;
    std::vector<TBeamSimData>* beam = nullptr;
    tree->SetBranchAddress("FragSimData", &frag_array);
    tree->SetBranchAddress("beam", &beam);

    PDCSimAna pdc_ana(geo);
    pdc_ana.SetSmearing(sigma_u, sigma_v);

    const Long64_t n_entries = tree->GetEntries();
    const Long64_t n_to_process = (max_events > 0 && n_entries > max_events) ? max_events : n_entries;

    std::vector<SampleRow> rows;
    rows.reserve(static_cast<std::size_t>(std::max<Long64_t>(0, n_to_process / 2)));

    long long missing_truth = 0;
    long long missing_track = 0;
    long long invalid_values = 0;

    std::cout << "[build_dataset] Processing events: " << n_to_process << std::endl;
    for (Long64_t i = 0; i < n_to_process; ++i) {
        tree->GetEntry(i);

        if (i > 0 && i % 10000 == 0) {
            std::cout << "[build_dataset] Event " << i << "/" << n_to_process << std::endl;
        }

        const TruthBeamInfo truth = ExtractTruthBeam(beam, target_angle_rad);
        if (!truth.has_proton) {
            ++missing_truth;
            continue;
        }

        const RecoEvent pdc_event = pdc_ana.ProcessEvent(frag_array);
        if (pdc_event.tracks.empty()) {
            ++missing_track;
            continue;
        }

        const RecoTrack& track = pdc_event.tracks.front();
        if (!IsFinite(track.start) || !IsFinite(track.end) || !IsFinite(truth.proton_momentum_target)) {
            ++invalid_values;
            continue;
        }

        SampleRow row;
        row.event_id = static_cast<long long>(i);
        row.x[0] = track.start.X();
        row.x[1] = track.start.Y();
        row.x[2] = track.start.Z();
        row.x[3] = track.end.X();
        row.x[4] = track.end.Y();
        row.x[5] = track.end.Z();

        row.y[0] = truth.proton_momentum_target.X();
        row.y[1] = truth.proton_momentum_target.Y();
        row.y[2] = truth.proton_momentum_target.Z();

        bool ok = true;
        for (double value : row.x) {
            if (!std::isfinite(value)) ok = false;
        }
        for (double value : row.y) {
            if (!std::isfinite(value)) ok = false;
        }
        if (!ok) {
            ++invalid_values;
            continue;
        }

        rows.push_back(row);
    }

    const std::filesystem::path out_root_path(out_root);
    const std::filesystem::path out_csv_path(out_csv);
    std::error_code ec;
    if (!out_root_path.parent_path().empty()) {
        std::filesystem::create_directories(out_root_path.parent_path(), ec);
    }
    if (!out_csv_path.parent_path().empty()) {
        std::filesystem::create_directories(out_csv_path.parent_path(), ec);
    }

    TFile out_file(out_root, "RECREATE");
    if (out_file.IsZombie()) {
        std::cerr << "[build_dataset] Failed to create output ROOT: " << out_root << std::endl;
        return;
    }

    long long event_id = -1;
    double x[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double y[3] = {0.0, 0.0, 0.0};
    double pdc1_x = 0.0;
    double pdc1_y = 0.0;
    double pdc1_z = 0.0;
    double pdc2_x = 0.0;
    double pdc2_y = 0.0;
    double pdc2_z = 0.0;
    double px_truth = 0.0;
    double py_truth = 0.0;
    double pz_truth = 0.0;

    TTree out_tree("nn_dataset", "PDC-to-target-momentum dataset");
    out_tree.Branch("event_id", &event_id, "event_id/L");
    out_tree.Branch("x", x, "x[6]/D");
    out_tree.Branch("y", y, "y[3]/D");
    out_tree.Branch("pdc1_x", &pdc1_x, "pdc1_x/D");
    out_tree.Branch("pdc1_y", &pdc1_y, "pdc1_y/D");
    out_tree.Branch("pdc1_z", &pdc1_z, "pdc1_z/D");
    out_tree.Branch("pdc2_x", &pdc2_x, "pdc2_x/D");
    out_tree.Branch("pdc2_y", &pdc2_y, "pdc2_y/D");
    out_tree.Branch("pdc2_z", &pdc2_z, "pdc2_z/D");
    out_tree.Branch("px_truth", &px_truth, "px_truth/D");
    out_tree.Branch("py_truth", &py_truth, "py_truth/D");
    out_tree.Branch("pz_truth", &pz_truth, "pz_truth/D");

    std::ofstream csv(out_csv);
    if (!csv.is_open()) {
        std::cerr << "[build_dataset] Failed to create output CSV: " << out_csv << std::endl;
        return;
    }
    csv << "event_id,pdc1_x,pdc1_y,pdc1_z,pdc2_x,pdc2_y,pdc2_z,px_truth,py_truth,pz_truth\n";
    csv << std::setprecision(12);

    for (const SampleRow& row : rows) {
        event_id = row.event_id;
        for (int i = 0; i < 6; ++i) x[i] = row.x[i];
        for (int i = 0; i < 3; ++i) y[i] = row.y[i];

        pdc1_x = row.x[0];
        pdc1_y = row.x[1];
        pdc1_z = row.x[2];
        pdc2_x = row.x[3];
        pdc2_y = row.x[4];
        pdc2_z = row.x[5];
        px_truth = row.y[0];
        py_truth = row.y[1];
        pz_truth = row.y[2];

        out_tree.Fill();

        csv << event_id << ','
            << pdc1_x << ',' << pdc1_y << ',' << pdc1_z << ','
            << pdc2_x << ',' << pdc2_y << ',' << pdc2_z << ','
            << px_truth << ',' << py_truth << ',' << pz_truth << '\n';
    }

    out_tree.Write();
    out_file.Close();
    csv.close();

    std::cout << "[build_dataset] Done." << std::endl;
    std::cout << "[build_dataset] Total events:   " << n_to_process << std::endl;
    std::cout << "[build_dataset] Samples kept:  " << rows.size() << std::endl;
    std::cout << "[build_dataset] Missing truth: " << missing_truth << std::endl;
    std::cout << "[build_dataset] Missing track: " << missing_track << std::endl;
    std::cout << "[build_dataset] Invalid value: " << invalid_values << std::endl;
    std::cout << "[build_dataset] Output ROOT:   " << out_root << std::endl;
    std::cout << "[build_dataset] Output CSV:    " << out_csv << std::endl;
}

void build_nn_target_dataset(const char* sim_root,
                             const char* geom_macro,
                             int max_events = 0,
                             double sigma_u = 0.5,
                             double sigma_v = 0.5,
                             const char* out_root = "data/nn_target_momentum/dataset_train.root",
                             const char* out_csv = "data/nn_target_momentum/dataset_train.csv") {
    build_dataset(sim_root, geom_macro, max_events, sigma_u, sigma_v, out_root, out_csv);
}
