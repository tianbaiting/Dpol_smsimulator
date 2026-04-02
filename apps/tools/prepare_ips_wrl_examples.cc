#include "QMDInputMetadata.hh"
#include "TBeamSimData.hh"
#include "TSimData.hh"

#include <TClonesArray.h>
#include <TFile.h>
#include <TLorentzVector.h>
#include <TTree.h>
#include <TVector3.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr std::array<int, 3> kElasticBuckets = {1, 5, 10};
constexpr std::array<int, 3> kAlleventBuckets = {1, 4, 7};
constexpr std::array<double, 7> kPxScanValues = {150.0, 100.0, 50.0, 0.0, -50.0, -100.0, -150.0};
constexpr double kFixedPyMeVc = 0.0;
constexpr double kFixedPzMeVc = 627.0;
constexpr double kUnknownBimp = -1.0;
constexpr double kProtonMassMeV = 938.2720813;
constexpr double kNeutronMassMeV = 939.5654133;

struct Options {
    fs::path base_dir;
    fs::path output_dir;
    fs::path offset0_output_dir;
};

struct EventMetadata {
    int original_event_id = -1;
    int source_file_index = -1;
    double bimp = kUnknownBimp;
};

struct ElasticCandidate {
    Long64_t entry = -1;
    EventMetadata metadata;
    double relative_momentum_mevc = 0.0;
};

struct AlleventCandidate {
    Long64_t entry = -1;
    EventMetadata metadata;
    int fragment_count = 0;
    int charged_fragment_count = 0;
    bool simulated_at_offset0 = false;
    bool ips_hit_at_offset0 = false;
};

struct ManifestRow {
    std::string category;
    std::string label;
    fs::path source_input_root;
    fs::path source_output_root;
    fs::path generated_input_root;
    Long64_t tree_entry = -1;
    int original_event_id = -1;
    int source_file_index = -1;
    double bimp = kUnknownBimp;
    int fragment_count = 0;
    int charged_fragment_count = 0;
    double relative_momentum_mevc = std::numeric_limits<double>::quiet_NaN();
    bool ips_hit_at_offset0 = false;
    std::string selection_rule;
    std::string notes;
};

std::string BucketTag(int bucket) {
    std::ostringstream oss;
    oss << 'b' << std::setw(2) << std::setfill('0') << bucket;
    return oss.str();
}

std::string CsvQuote(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 2);
    escaped.push_back('"');
    for (char c : text) {
        if (c == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}

void PrintUsage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " --base-dir <ips_scan_base_dir> --output-dir <ips_wrl_dir> "
        << "[--offset0-output-dir <allevent_offset0_output_dir>]\n";
}

Options ParseArgs(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--base-dir" && i + 1 < argc) {
            opts.base_dir = fs::path(argv[++i]);
        } else if (arg == "--output-dir" && i + 1 < argc) {
            opts.output_dir = fs::path(argv[++i]);
        } else if (arg == "--offset0-output-dir" && i + 1 < argc) {
            opts.offset0_output_dir = fs::path(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (opts.base_dir.empty() || opts.output_dir.empty()) {
        throw std::runtime_error("base-dir and output-dir are required");
    }
    opts.base_dir = fs::absolute(opts.base_dir);
    opts.output_dir = fs::absolute(opts.output_dir);
    if (opts.offset0_output_dir.empty()) {
        opts.offset0_output_dir =
            opts.base_dir / "results" / "balanced_beamOn300" / "kept_runs" / "recommended" / "g4output" / "scan" / "allevent";
    }
    opts.offset0_output_dir = fs::absolute(opts.offset0_output_dir);
    return opts;
}

EventMetadata ExtractMetadata(const std::vector<TBeamSimData>* beam) {
    EventMetadata metadata;
    if (!beam || beam->empty()) {
        return metadata;
    }
    const auto& primary = beam->front();
    if (primary.fUserInt.size() > static_cast<std::size_t>(qmd_input_metadata::kOriginalEventIdIndex)) {
        metadata.original_event_id = primary.fUserInt[qmd_input_metadata::kOriginalEventIdIndex];
    }
    if (primary.fUserInt.size() > static_cast<std::size_t>(qmd_input_metadata::kSourceFileIndex)) {
        metadata.source_file_index = primary.fUserInt[qmd_input_metadata::kSourceFileIndex];
    }
    if (primary.fUserDouble.size() > static_cast<std::size_t>(qmd_input_metadata::kBimpIndex)) {
        metadata.bimp = primary.fUserDouble[qmd_input_metadata::kBimpIndex];
    }
    return metadata;
}

bool HasIPSHit(TClonesArray* frag_hits) {
    if (!frag_hits) {
        return false;
    }
    const int entries = frag_hits->GetEntriesFast();
    for (int i = 0; i < entries; ++i) {
        auto* hit = static_cast<TSimData*>(frag_hits->At(i));
        if (!hit) {
            continue;
        }
        if (hit->fDetectorName == "IPS" && hit->fEnergyDeposit > 0.0) {
            return true;
        }
    }
    return false;
}

double Median(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2U;
    if ((values.size() % 2U) == 1U) {
        return values[mid];
    }
    return 0.5 * (values[mid - 1U] + values[mid]);
}

double ApproximateMassMeV(int z, int a) {
    if (z == 1 && a == 1) {
        return kProtonMassMeV;
    }
    if (z == 0 && a == 1) {
        return kNeutronMassMeV;
    }
    const int n = std::max(0, a - z);
    return static_cast<double>(z) * kProtonMassMeV + static_cast<double>(n) * kNeutronMassMeV;
}

fs::path ElasticInputRoot(const Options& opts, int bucket) {
    return opts.base_dir / "input" / "balanced" / ("elastic_" + BucketTag(bucket) + ".root");
}

fs::path AlleventInputRoot(const Options& opts, int bucket) {
    return opts.base_dir / "input" / "balanced" / ("allevent_" + BucketTag(bucket) + ".root");
}

fs::path AlleventOffset0OutputRoot(const Options& opts, int bucket) {
    return opts.offset0_output_dir / ("allevent_scan_allevent_" + BucketTag(bucket) + "_off_0_00000.root");
}

std::vector<bool> LoadIPSHitFlags(const fs::path& output_root) {
    std::vector<bool> flags;
    if (!fs::exists(output_root)) {
        const std::string output_root_str = output_root.string();
        spdlog::warn("Missing offset0 simulation output, IPS-hit preference will be skipped: {}", output_root_str.c_str());
        return flags;
    }

    TFile file(output_root.c_str(), "READ");
    if (file.IsZombie()) {
        throw std::runtime_error("Cannot open simulation output: " + output_root.string());
    }
    auto* tree = dynamic_cast<TTree*>(file.Get("tree"));
    if (!tree) {
        throw std::runtime_error("Missing tree in simulation output: " + output_root.string());
    }

    TClonesArray* frag_hits = nullptr;
    tree->SetBranchAddress("FragSimData", &frag_hits);
    const Long64_t entries = tree->GetEntries();
    flags.reserve(static_cast<std::size_t>(entries));
    for (Long64_t i = 0; i < entries; ++i) {
        tree->GetEntry(i);
        flags.push_back(HasIPSHit(frag_hits));
    }
    return flags;
}

std::vector<ElasticCandidate> LoadElasticCandidates(const fs::path& input_root) {
    TFile file(input_root.c_str(), "READ");
    if (file.IsZombie()) {
        throw std::runtime_error("Cannot open elastic input: " + input_root.string());
    }
    auto* tree = dynamic_cast<TTree*>(file.Get("tree"));
    if (!tree) {
        throw std::runtime_error("Missing tree in elastic input: " + input_root.string());
    }

    std::vector<TBeamSimData>* beam = nullptr;
    tree->SetBranchAddress("TBeamSimData", &beam);

    std::vector<ElasticCandidate> candidates;
    const Long64_t entries = tree->GetEntries();
    candidates.reserve(static_cast<std::size_t>(entries));
    for (Long64_t i = 0; i < entries; ++i) {
        tree->GetEntry(i);
        if (!beam || beam->empty()) {
            continue;
        }
        const TBeamSimData* proton = nullptr;
        const TBeamSimData* neutron = nullptr;
        for (const auto& particle : *beam) {
            if (particle.fZ == 1 && particle.fA == 1 && proton == nullptr) {
                proton = &particle;
            } else if (particle.fZ == 0 && particle.fA == 1 && neutron == nullptr) {
                neutron = &particle;
            }
        }
        if (!proton || !neutron) {
            continue;
        }

        ElasticCandidate candidate;
        candidate.entry = i;
        candidate.metadata = ExtractMetadata(beam);
        candidate.relative_momentum_mevc = (proton->fMomentum.Vect() - neutron->fMomentum.Vect()).Mag();
        candidates.push_back(candidate);
    }
    return candidates;
}

ElasticCandidate SelectElasticRepresentative(const std::vector<ElasticCandidate>& candidates) {
    if (candidates.empty()) {
        throw std::runtime_error("No elastic candidates available");
    }
    std::vector<double> rel_p_values;
    rel_p_values.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        rel_p_values.push_back(candidate.relative_momentum_mevc);
    }
    const double median_rel_p = Median(rel_p_values);

    const auto comparator = [&](const ElasticCandidate& lhs, const ElasticCandidate& rhs) {
        const double lhs_dist = std::abs(lhs.relative_momentum_mevc - median_rel_p);
        const double rhs_dist = std::abs(rhs.relative_momentum_mevc - median_rel_p);
        if (std::abs(lhs_dist - rhs_dist) > 1.0e-9) {
            return lhs_dist < rhs_dist;
        }
        if (lhs.metadata.original_event_id != rhs.metadata.original_event_id) {
            return lhs.metadata.original_event_id < rhs.metadata.original_event_id;
        }
        return lhs.entry < rhs.entry;
    };
    return *std::min_element(candidates.begin(), candidates.end(), comparator);
}

std::vector<AlleventCandidate> LoadAlleventCandidates(const fs::path& input_root, const std::vector<bool>& ips_hit_flags) {
    TFile file(input_root.c_str(), "READ");
    if (file.IsZombie()) {
        throw std::runtime_error("Cannot open allevent input: " + input_root.string());
    }
    auto* tree = dynamic_cast<TTree*>(file.Get("tree"));
    if (!tree) {
        throw std::runtime_error("Missing tree in allevent input: " + input_root.string());
    }

    std::vector<TBeamSimData>* beam = nullptr;
    tree->SetBranchAddress("TBeamSimData", &beam);

    std::vector<AlleventCandidate> candidates;
    const Long64_t entries = tree->GetEntries();
    candidates.reserve(static_cast<std::size_t>(entries));
    for (Long64_t i = 0; i < entries; ++i) {
        tree->GetEntry(i);
        if (!beam || beam->empty()) {
            continue;
        }

        AlleventCandidate candidate;
        candidate.entry = i;
        candidate.metadata = ExtractMetadata(beam);
        candidate.fragment_count = static_cast<int>(beam->size());
        for (const auto& particle : *beam) {
            if (particle.fZ > 0) {
                ++candidate.charged_fragment_count;
            }
        }
        if (i < static_cast<Long64_t>(ips_hit_flags.size())) {
            candidate.simulated_at_offset0 = true;
            candidate.ips_hit_at_offset0 = ips_hit_flags[static_cast<std::size_t>(i)];
        }
        candidates.push_back(candidate);
    }
    return candidates;
}

AlleventCandidate SelectAlleventRepresentative(const std::vector<AlleventCandidate>& candidates) {
    if (candidates.empty()) {
        throw std::runtime_error("No allevent candidates available");
    }

    std::vector<double> charged_values;
    charged_values.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        charged_values.push_back(static_cast<double>(candidate.charged_fragment_count));
    }
    const double median_charged = Median(charged_values);

    std::vector<const AlleventCandidate*> pool;
    pool.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        if (candidate.ips_hit_at_offset0) {
            pool.push_back(&candidate);
        }
    }
    if (pool.empty()) {
        for (const auto& candidate : candidates) {
            pool.push_back(&candidate);
        }
    }

    const auto comparator = [&](const AlleventCandidate* lhs, const AlleventCandidate* rhs) {
        const double lhs_dist = std::abs(static_cast<double>(lhs->charged_fragment_count) - median_charged);
        const double rhs_dist = std::abs(static_cast<double>(rhs->charged_fragment_count) - median_charged);
        if (std::abs(lhs_dist - rhs_dist) > 1.0e-9) {
            return lhs_dist < rhs_dist;
        }
        if (lhs->fragment_count != rhs->fragment_count) {
            return lhs->fragment_count > rhs->fragment_count;
        }
        if (lhs->metadata.original_event_id != rhs->metadata.original_event_id) {
            return lhs->metadata.original_event_id < rhs->metadata.original_event_id;
        }
        return lhs->entry < rhs->entry;
    };

    return **std::min_element(pool.begin(), pool.end(), comparator);
}

void CopySingleEntryTree(const fs::path& input_root, const fs::path& output_root, Long64_t entry) {
    TFile input(input_root.c_str(), "READ");
    if (input.IsZombie()) {
        throw std::runtime_error("Cannot open input for copy: " + input_root.string());
    }
    auto* source_tree = dynamic_cast<TTree*>(input.Get("tree"));
    if (!source_tree) {
        throw std::runtime_error("Missing tree in input for copy: " + input_root.string());
    }

    fs::create_directories(output_root.parent_path());
    TFile output(output_root.c_str(), "RECREATE");
    if (output.IsZombie()) {
        throw std::runtime_error("Cannot create copied ROOT: " + output_root.string());
    }
    output.cd();
    auto* copied_tree = source_tree->CopyTree("", "", 1, entry);
    if (!copied_tree || copied_tree->GetEntries() != 1) {
        throw std::runtime_error("Failed to copy entry " + std::to_string(entry) + " from " + input_root.string());
    }
    copied_tree->SetName("tree");
    copied_tree->SetTitle(source_tree->GetTitle());
    copied_tree->Write();
}

void WriteSingleSpeciesPxScanRoot(
    const fs::path& output_root,
    int z,
    int a,
    const std::string& particle_name
) {
    fs::create_directories(output_root.parent_path());
    TFile output(output_root.c_str(), "RECREATE");
    if (output.IsZombie()) {
        throw std::runtime_error("Cannot create px-scan ROOT: " + output_root.string());
    }

    auto beam_array = std::make_unique<TBeamSimDataArray>();
    gBeamSimDataArray = beam_array.get();
    TTree tree("tree", (particle_name + " px scan for IPS WRL export").c_str());
    tree.Branch("TBeamSimData", &gBeamSimDataArray);

    const TVector3 position(0.0, 0.0, 0.0);
    const double mass = ApproximateMassMeV(z, a);
    beam_array->clear();
    int particle_id = 0;
    for (double px : kPxScanValues) {
        const TLorentzVector momentum(
            px,
            kFixedPyMeVc,
            kFixedPzMeVc,
            std::sqrt(px * px + kFixedPyMeVc * kFixedPyMeVc + kFixedPzMeVc * kFixedPzMeVc + mass * mass)
        );
        TBeamSimData beam(z, a, momentum, position);
        beam.fParticleName = particle_name.c_str();
        beam.fPrimaryParticleID = particle_id++;
        beam.fTime = 0.0;
        beam.fIsAccepted = true;
        // [EN] Keep the px scan in one tree entry so a single BeamOn call overlays the full momentum family in one WRL scene. / [CN] 将px扫描放在同一个tree entry里，使一次BeamOn就能在同一张WRL图中叠加整组动量轨迹。
        beam_array->push_back(beam);
    }
    tree.Fill();
    tree.Write();
    gBeamSimDataArray = nullptr;
}

void WriteManifest(const fs::path& manifest_path, const std::vector<ManifestRow>& rows) {
    fs::create_directories(manifest_path.parent_path());
    std::ofstream out(manifest_path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot create manifest: " + manifest_path.string());
    }
    out << "category,label,source_input_root,source_output_root,generated_input_root,tree_entry,"
        << "original_event_id,source_file_index,bimp,fragment_count,charged_fragment_count,"
        << "relative_momentum_mevc,ips_hit_at_offset0,selection_rule,notes\n";
    for (const auto& row : rows) {
        out << CsvQuote(row.category) << ','
            << CsvQuote(row.label) << ','
            << CsvQuote(row.source_input_root.string()) << ','
            << CsvQuote(row.source_output_root.string()) << ','
            << CsvQuote(row.generated_input_root.string()) << ','
            << row.tree_entry << ','
            << row.original_event_id << ','
            << row.source_file_index << ','
            << std::fixed << std::setprecision(6) << row.bimp << ','
            << row.fragment_count << ','
            << row.charged_fragment_count << ',';
        if (std::isfinite(row.relative_momentum_mevc)) {
            out << row.relative_momentum_mevc;
        }
        out << ','
            << (row.ips_hit_at_offset0 ? 1 : 0) << ','
            << CsvQuote(row.selection_rule) << ','
            << CsvQuote(row.notes) << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options opts = ParseArgs(argc, argv);
        const fs::path inputs_dir = opts.output_dir / "inputs";
        const fs::path selection_dir = opts.output_dir / "selection";
        fs::create_directories(inputs_dir);
        fs::create_directories(selection_dir);

        std::vector<ManifestRow> manifest_rows;

        const fs::path proton_scan_root = inputs_dir / "proton_px_scan_pz627_offset0.root";
        WriteSingleSpeciesPxScanRoot(proton_scan_root, 1, 1, "proton");
        manifest_rows.push_back(ManifestRow{
            "scan",
            "proton_px_scan",
            {},
            {},
            proton_scan_root,
            0,
            -1,
            -1,
            kUnknownBimp,
            static_cast<int>(kPxScanValues.size()),
            static_cast<int>(kPxScanValues.size()),
            std::numeric_limits<double>::quiet_NaN(),
            false,
            "fixed_pz_px_family",
            "species=proton;px=[150,100,50,0,-50,-100,-150] MeV/c;py=0 MeV/c;pz=627 MeV/c"
        });

        const fs::path neutron_scan_root = inputs_dir / "neutron_px_scan_pz627_offset0.root";
        WriteSingleSpeciesPxScanRoot(neutron_scan_root, 0, 1, "neutron");
        manifest_rows.push_back(ManifestRow{
            "scan",
            "neutron_px_scan",
            {},
            {},
            neutron_scan_root,
            0,
            -1,
            -1,
            kUnknownBimp,
            static_cast<int>(kPxScanValues.size()),
            0,
            std::numeric_limits<double>::quiet_NaN(),
            false,
            "fixed_pz_px_family",
            "species=neutron;px=[150,100,50,0,-50,-100,-150] MeV/c;py=0 MeV/c;pz=627 MeV/c"
        });

        for (int bucket : kElasticBuckets) {
            const fs::path input_root = ElasticInputRoot(opts, bucket);
            const auto candidates = LoadElasticCandidates(input_root);
            const auto selected = SelectElasticRepresentative(candidates);
            const fs::path output_root = inputs_dir / ("elastic_" + BucketTag(bucket) + "_typical_offset0.root");
            CopySingleEntryTree(input_root, output_root, selected.entry);

            std::vector<double> rel_p_values;
            rel_p_values.reserve(candidates.size());
        for (const auto& candidate : candidates) {
            rel_p_values.push_back(candidate.relative_momentum_mevc);
        }
        const double median_rel_p = Median(rel_p_values);
        const std::string bucket_tag = BucketTag(bucket);
        manifest_rows.push_back(ManifestRow{
            "elastic",
            bucket_tag,
            input_root,
            {},
            output_root,
                selected.entry,
                selected.metadata.original_event_id,
                selected.metadata.source_file_index,
                selected.metadata.bimp,
                2,
                1,
                selected.relative_momentum_mevc,
                false,
                "median_relative_momentum",
                "selected_by_min|rel_p-median|;median_rel_p_mevc=" + std::to_string(median_rel_p)
            });
            spdlog::info(
                "Selected elastic {} entry={} original_event_id={} rel_p={:.3f} MeV/c",
                bucket_tag.c_str(),
                selected.entry,
                selected.metadata.original_event_id,
                selected.relative_momentum_mevc
            );
        }

        for (int bucket : kAlleventBuckets) {
            const fs::path input_root = AlleventInputRoot(opts, bucket);
            const fs::path output_root = AlleventOffset0OutputRoot(opts, bucket);
            const auto ips_hit_flags = LoadIPSHitFlags(output_root);
            const auto candidates = LoadAlleventCandidates(input_root, ips_hit_flags);
            const auto selected = SelectAlleventRepresentative(candidates);
            const fs::path mini_root = inputs_dir / ("allevent_" + BucketTag(bucket) + "_typical_offset0.root");
            CopySingleEntryTree(input_root, mini_root, selected.entry);

            std::vector<double> charged_values;
            charged_values.reserve(candidates.size());
            for (const auto& candidate : candidates) {
                charged_values.push_back(static_cast<double>(candidate.charged_fragment_count));
            }
            const double median_charged = Median(charged_values);
            const bool any_ips_hit = std::any_of(candidates.begin(), candidates.end(), [](const AlleventCandidate& candidate) {
                return candidate.ips_hit_at_offset0;
            });
            const std::string bucket_tag = BucketTag(bucket);
            manifest_rows.push_back(ManifestRow{
                "allevent",
                bucket_tag,
                input_root,
                output_root,
                mini_root,
                selected.entry,
                selected.metadata.original_event_id,
                selected.metadata.source_file_index,
                selected.metadata.bimp,
                selected.fragment_count,
                selected.charged_fragment_count,
                std::numeric_limits<double>::quiet_NaN(),
                selected.ips_hit_at_offset0,
                any_ips_hit ? "ips_hit_then_median_charged" : "median_charged_fallback_full_bucket",
                "median_charged=" + std::to_string(median_charged) + ";simulated_at_offset0=" +
                    std::string(selected.simulated_at_offset0 ? "yes" : "no")
            });
            spdlog::info(
                "Selected allevent {} entry={} original_event_id={} nch={} fragments={} ips_hit={}",
                bucket_tag.c_str(),
                selected.entry,
                selected.metadata.original_event_id,
                selected.charged_fragment_count,
                selected.fragment_count,
                selected.ips_hit_at_offset0 ? "yes" : "no"
            );
        }

        const fs::path manifest_path = selection_dir / "selection_manifest.csv";
        WriteManifest(manifest_path, manifest_rows);
        const std::string output_dir_str = opts.output_dir.string();
        const std::string manifest_path_str = manifest_path.string();
        spdlog::info("Prepared IPS WRL inputs under {}", output_dir_str.c_str());
        spdlog::info("Selection manifest: {}", manifest_path_str.c_str());
    } catch (const std::exception& ex) {
        spdlog::error("prepare_ips_wrl_examples failed: {}", ex.what());
        return 1;
    }
    return 0;
}
