// [EN] Usage: build the binary, then run `GenInputRoot_qmdrawdata --mode [ypol|zpol|both] --source [elastic|allevent|both]` / [CN] 用法：先编译，再运行 `GenInputRoot_qmdrawdata --mode [ypol|zpol|both] --source [elastic|allevent|both]`
// [EN] Example: `GenInputRoot_qmdrawdata --mode ypol --source elastic` / [CN] 示例：`GenInputRoot_qmdrawdata --mode ypol --source elastic`
// [EN] Override paths with `--input-base PATH --output-base PATH` / [CN] 通过 `--input-base PATH --output-base PATH` 覆盖输入输出路径
// [EN] Zpol elastic input keeps only b/b_max events per file because yield scales with b / [CN] Z极化弹性输入按每个文件的 b/b_max 保留事件数，因为产额与 b 成正比
#include <TFile.h>
#include <TTree.h>
#include <TLorentzVector.h>
#include <TVector3.h>

#include "QMDInputMetadata.hh"
#include "TBeamSimData.hh"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

TBeamSimDataArray* gBeamSimDataArray = nullptr;

namespace fs = std::filesystem;

namespace {

constexpr double kMp = 938.27;
constexpr double kMn = 939.57;
constexpr double kUnknownBimp = -1.0;

enum class Mode {
    kYpol,
    kZpol,
    kBoth
};

enum class SourceMode {
    kElastic,
    kAllevent,
    kBoth
};

struct Options {
    fs::path input_base;
    fs::path output_base;
    Mode mode = Mode::kBoth;
    SourceMode source = SourceMode::kElastic;
    std::string target_filter;
    bool cut_unphysical = true;
    double cut_ypol_axis_limit = 150.0;
    double cut_zpol_axis_limit = 150.0;
    bool rotate_ypol = false;
    bool rotate_zpol = true;
};

struct HeaderInfo {
    double b = std::numeric_limits<double>::quiet_NaN();
    long events = -1;
};

struct ZFileInfo {
    fs::path path;
    double b = 0.0;
    long events = 0;
};

struct GeminiRow {
    int z = 0;
    int n = 0;
    double p1 = 0.0;
    double p2 = 0.0;
    double p3 = 0.0;
    double weight = 0.0;
    double bimp = kUnknownBimp;
    int isim = -1;
    int ifrg = -1;
};

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

bool ContainsInsensitive(std::string haystack, std::string needle) {
    std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return haystack.find(needle) != std::string::npos;
}

qmd_input_metadata::PolarizationKind DetectPolarization(const fs::path& path) {
    const std::string s = path.string();
    if (s.find("y_pol") != std::string::npos || s.find("-ypol") != std::string::npos) {
        return qmd_input_metadata::PolarizationKind::kY;
    }
    if (s.find("z_pol") != std::string::npos || s.find("-zpol") != std::string::npos) {
        return qmd_input_metadata::PolarizationKind::kZ;
    }
    return qmd_input_metadata::PolarizationKind::kUnknown;
}

bool MatchesMode(const fs::path& path, Mode mode) {
    const auto pol = DetectPolarization(path);
    if (pol == qmd_input_metadata::PolarizationKind::kUnknown) {
        return false;
    }
    if (mode == Mode::kBoth) {
        return true;
    }
    if (mode == Mode::kYpol) {
        return pol == qmd_input_metadata::PolarizationKind::kY;
    }
    return pol == qmd_input_metadata::PolarizationKind::kZ;
}

std::optional<double> ExtractB(const std::string& line) {
    const std::string key = "b=";
    auto pos = line.find(key);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos += key.size();
    while (pos < line.size() && line[pos] == ' ') {
        ++pos;
    }
    size_t end = pos;
    while (end < line.size() && (std::isdigit(static_cast<unsigned char>(line[end])) || line[end] == '.' || line[end] == '-')) {
        ++end;
    }
    if (end <= pos) {
        return std::nullopt;
    }
    return std::stod(line.substr(pos, end - pos));
}

std::optional<long> ExtractEvents(const std::string& line) {
    const std::string key = "events";
    auto pos = line.find(key);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    size_t end = pos;
    size_t start = end;
    while (start > 0 && std::isdigit(static_cast<unsigned char>(line[start - 1]))) {
        --start;
    }
    if (end <= start) {
        return std::nullopt;
    }
    return std::stol(line.substr(start, end - start));
}

HeaderInfo ReadHeaderInfo(const fs::path& path) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open input file: " + path.string());
    }
    std::string header1;
    std::string header2;
    if (!std::getline(fin, header1)) {
        throw std::runtime_error("Missing header line 1 in: " + path.string());
    }
    if (!std::getline(fin, header2)) {
        throw std::runtime_error("Missing header line 2 in: " + path.string());
    }

    HeaderInfo info;
    auto b = ExtractB(header1);
    if (b.has_value()) {
        info.b = *b;
    }
    auto events = ExtractEvents(header1);
    if (events.has_value()) {
        info.events = *events;
    } else {
        long count = 0;
        std::string line;
        while (std::getline(fin, line)) {
            std::istringstream iss(line);
            int no = 0;
            double pxp = 0.0, pyp = 0.0, pzp = 0.0, pxn = 0.0, pyn = 0.0, pzn = 0.0;
            if (iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn) {
                ++count;
            }
        }
        info.events = count;
    }
    return info;
}

std::vector<fs::path> CollectDatFiles(const fs::path& root_dir, const std::string& prefix) {
    std::vector<fs::path> files;
    if (!fs::exists(root_dir)) {
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto& path = entry.path();
        if (path.extension() != ".dat") {
            continue;
        }
        const auto filename = path.filename().string();
        if (!StartsWith(filename, prefix)) {
            continue;
        }
        files.push_back(path);
    }
    std::sort(files.begin(), files.end());
    return files;
}

fs::path MakeOutputPath(const fs::path& input, const fs::path& input_base, const fs::path& output_base) {
    std::error_code ec;
    const auto rel = fs::relative(input, input_base, ec);
    if (ec) {
        throw std::runtime_error("Cannot compute relative path for: " + input.string());
    }
    fs::path out = output_base / rel;
    out.replace_extension(".root");
    return out;
}

fs::path ResolveAlleventRoot(const fs::path& input_base) {
    if (input_base.filename() == "allevent") {
        return input_base;
    }
    return input_base / "allevent";
}

bool ParseBoolOption(std::string value) {
    for (auto& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (value == "on" || value == "true" || value == "1" || value == "yes" || value == "y") {
        return true;
    }
    if (value == "off" || value == "false" || value == "0" || value == "no" || value == "n") {
        return false;
    }
    throw std::runtime_error("Invalid boolean option value: " + value);
}

std::vector<fs::path> ApplyTargetFilter(const std::vector<fs::path>& files, const std::string& target_filter) {
    if (target_filter.empty()) {
        return files;
    }
    std::vector<fs::path> filtered;
    filtered.reserve(files.size());
    for (const auto& file : files) {
        if (ContainsInsensitive(file.string(), target_filter)) {
            filtered.push_back(file);
        }
    }
    return filtered;
}

std::vector<fs::path> ApplyModeFilter(const std::vector<fs::path>& files, Mode mode) {
    if (mode == Mode::kBoth) {
        return files;
    }
    std::vector<fs::path> filtered;
    filtered.reserve(files.size());
    for (const auto& file : files) {
        if (MatchesMode(file, mode)) {
            filtered.push_back(file);
        }
    }
    return filtered;
}

double ApproximateNuclearMassMeV(int z, int a) {
    const int n = std::max(0, a - z);
    return static_cast<double>(z) * kMp + static_cast<double>(n) * kMn;
}

std::string ParticleNameForZA(int z, int a) {
    if (z == 1 && a == 1) {
        return "proton";
    }
    if (z == 0 && a == 1) {
        return "neutron";
    }
    if (z == 1 && a == 2) {
        return "deuteron";
    }
    if (z == 1 && a == 3) {
        return "triton";
    }
    if (z == 2 && a == 3) {
        return "He3";
    }
    if (z == 2 && a == 4) {
        return "alpha";
    }
    return "";
}

void StampMetadata(
    TBeamSimData& beam,
    qmd_input_metadata::SourceKind source,
    int original_event_id,
    int source_file_index,
    qmd_input_metadata::PolarizationKind pol,
    double bimp
) {
    beam.fUserInt.assign(qmd_input_metadata::kPolarizationKindIndex + 1, 0);
    beam.fUserDouble.assign(qmd_input_metadata::kBimpIndex + 1, kUnknownBimp);
    beam.fUserInt[qmd_input_metadata::kSourceKindIndex] = static_cast<int>(source);
    beam.fUserInt[qmd_input_metadata::kOriginalEventIdIndex] = original_event_id;
    beam.fUserInt[qmd_input_metadata::kSourceFileIndex] = source_file_index;
    beam.fUserInt[qmd_input_metadata::kPolarizationKindIndex] = static_cast<int>(pol);
    beam.fUserDouble[qmd_input_metadata::kBimpIndex] = bimp;
}

bool ParseGeminiRow(const std::string& line, GeminiRow& row) {
    std::istringstream iss(line);
    double j = 0.0;
    double m = 0.0;
    if (!(iss >> row.z >> row.n >> row.p1 >> row.p2 >> row.p3 >> j >> m >> row.weight >> row.bimp >> row.isim >> row.ifrg)) {
        return false;
    }
    return true;
}

TLorentzVector BuildApproxP4(int z, int a, double px, double py, double pz) {
    const double mass = ApproximateNuclearMassMeV(z, a);
    const double energy = std::sqrt(px * px + py * py + pz * pz + mass * mass);
    return TLorentzVector(px, py, pz, energy);
}

void WriteElasticEvent(
    int event_no,
    double pxp,
    double pyp,
    double pzp,
    double pxn,
    double pyn,
    double pzn,
    qmd_input_metadata::PolarizationKind pol,
    double bimp,
    int source_file_index,
    TTree& tree,
    const TVector3& position
) {
    gBeamSimDataArray->clear();

    const TLorentzVector momentum_p = BuildApproxP4(1, 1, pxp, pyp, pzp);
    TBeamSimData proton(1, 1, momentum_p, position);
    proton.fParticleName = "proton";
    proton.fPrimaryParticleID = 0;
    proton.fTime = 0.0;
    proton.fIsAccepted = true;
    StampMetadata(
        proton,
        qmd_input_metadata::SourceKind::kElastic,
        event_no,
        source_file_index,
        pol,
        bimp
    );
    gBeamSimDataArray->push_back(proton);

    const TLorentzVector momentum_n = BuildApproxP4(0, 1, pxn, pyn, pzn);
    TBeamSimData neutron(0, 1, momentum_n, position);
    neutron.fParticleName = "neutron";
    neutron.fPrimaryParticleID = 1;
    neutron.fTime = 0.0;
    neutron.fIsAccepted = true;
    StampMetadata(
        neutron,
        qmd_input_metadata::SourceKind::kElastic,
        event_no,
        source_file_index,
        pol,
        bimp
    );
    gBeamSimDataArray->push_back(neutron);

    tree.Fill();
}

void FlushAlleventGroup(
    std::vector<TBeamSimData>& primaries,
    int current_isim,
    double current_bimp,
    int source_file_index,
    qmd_input_metadata::PolarizationKind pol,
    TTree& tree
) {
    if (primaries.empty()) {
        return;
    }

    gBeamSimDataArray->clear();
    for (std::size_t i = 0; i < primaries.size(); ++i) {
      primaries[i].fPrimaryParticleID = static_cast<int>(i);
      primaries[i].fTime = 0.0;
      primaries[i].fIsAccepted = true;
      StampMetadata(
          primaries[i],
          qmd_input_metadata::SourceKind::kAllevent,
          current_isim,
          source_file_index,
          pol,
          current_bimp
      );
      gBeamSimDataArray->push_back(primaries[i]);
    }
    tree.Fill();
    primaries.clear();
}

void AppendAlleventPrimaries(const GeminiRow& row, std::vector<TBeamSimData>& primaries) {
    const int a = row.z + row.n;
    if (a <= 0) {
        return;
    }

    if (row.z == 0 && a > 1) {
        const TLorentzVector neutron_momentum = BuildApproxP4(0, 1, row.p1, row.p2, row.p3);
        for (int i = 0; i < a; ++i) {
            TBeamSimData neutron(0, 1, neutron_momentum, TVector3(0.0, 0.0, 0.0));
            neutron.fParticleName = "neutron";
            primaries.push_back(neutron);
        }
        return;
    }

    const double px = row.p1 * static_cast<double>(a);
    const double py = row.p2 * static_cast<double>(a);
    const double pz = row.p3 * static_cast<double>(a);
    TBeamSimData beam(row.z, a, BuildApproxP4(row.z, a, px, py, pz), TVector3(0.0, 0.0, 0.0));
    beam.fParticleName = ParticleNameForZA(row.z, a).c_str();
    primaries.push_back(beam);
}

void ConvertElasticFile(
    const fs::path& input,
    const fs::path& output,
    std::optional<long> max_events,
    qmd_input_metadata::PolarizationKind pol,
    const Options& opts,
    int source_file_index
) {
    const std::string input_str = input.string();
    const std::string output_str = output.string();
    std::ifstream fin(input);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open input file: " + input_str);
    }

    std::string header1;
    std::string header2;
    if (!std::getline(fin, header1)) {
        throw std::runtime_error("Missing header line 1 in: " + input_str);
    }
    if (!std::getline(fin, header2)) {
        throw std::runtime_error("Missing header line 2 in: " + input_str);
    }

    const double bimp = ExtractB(header1).value_or(kUnknownBimp);

    fs::create_directories(output.parent_path());
    TFile out_file(output.c_str(), "RECREATE");
    if (out_file.IsZombie()) {
        throw std::runtime_error("Cannot create output file: " + output_str);
    }

    TTree tree("tree", "Input tree for simultaneous n-p");
    auto beam_array = std::make_unique<TBeamSimDataArray>();
    gBeamSimDataArray = beam_array.get();
    tree.Branch("TBeamSimData", &gBeamSimDataArray);

    const TVector3 position(0.0, 0.0, 0.0);
    long raw_line_count = 0;
    long parsed_event_count = 0;
    long selected_event_count = 0;
    long cut_removed_count = 0;
    long rotated_event_count = 0;
    long written_event_count = 0;

    std::string line;
    while (std::getline(fin, line)) {
        ++raw_line_count;
        std::istringstream iss(line);
        int no = 0;
        double pxp = 0.0;
        double pyp = 0.0;
        double pzp = 0.0;
        double pxn = 0.0;
        double pyn = 0.0;
        double pzn = 0.0;
        if (!(iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn)) {
            continue;
        }
        ++parsed_event_count;
        if (max_events.has_value() && selected_event_count >= *max_events) {
            break;
        }
        ++selected_event_count;

        if (opts.cut_unphysical) {
            const double axis_delta = (pol == qmd_input_metadata::PolarizationKind::kY)
                ? std::abs(pyp - pyn)
                : std::abs(pzp - pzn);
            const double axis_limit = (pol == qmd_input_metadata::PolarizationKind::kY)
                ? opts.cut_ypol_axis_limit
                : opts.cut_zpol_axis_limit;
            if (axis_delta >= axis_limit) {
                // [EN] Remove breakup events with excessive p-n relative momentum on the polarization axis because they are unphysical ImQMD artefacts. / [CN] 去除极化轴上p-n相对动量过大的破裂事例，因为它们是ImQMD的非物理伪象。
                ++cut_removed_count;
                continue;
            }
        }

        const bool do_rotate = (pol == qmd_input_metadata::PolarizationKind::kY && opts.rotate_ypol)
            || (pol == qmd_input_metadata::PolarizationKind::kZ && opts.rotate_zpol);
        if (do_rotate) {
            const double sum_px = pxp + pxn;
            const double sum_py = pyp + pyn;
            const double phi = std::atan2(sum_py, sum_px);
            const double angle = -phi;
            const double cos_a = std::cos(angle);
            const double sin_a = std::sin(angle);

            const double rot_pxp = cos_a * pxp - sin_a * pyp;
            const double rot_pyp = sin_a * pxp + cos_a * pyp;
            const double rot_pxn = cos_a * pxn - sin_a * pyn;
            const double rot_pyn = sin_a * pxn + cos_a * pyn;

            pxp = rot_pxp;
            pyp = rot_pyp;
            pxn = rot_pxn;
            pyn = rot_pyn;
            ++rotated_event_count;
        }

        WriteElasticEvent(no, pxp, pyp, pzp, pxn, pyn, pzn, pol, bimp, source_file_index, tree, position);
        ++written_event_count;

        if (written_event_count % 10000 == 0) {
            spdlog::info("Processed {} accepted elastic events from {}", written_event_count, input_str.c_str());
        }
    }

    out_file.cd();
    tree.Write();
    out_file.Close();
    spdlog::info(
        "Wrote {} elastic events to {} (raw_lines={}, parsed={}, selected={}, cut_removed={}, rotated={})",
        written_event_count,
        output_str.c_str(),
        raw_line_count,
        parsed_event_count,
        selected_event_count,
        cut_removed_count,
        rotated_event_count
    );
}

void ConvertAlleventFile(
    const fs::path& input,
    const fs::path& output,
    qmd_input_metadata::PolarizationKind pol,
    int source_file_index
) {
    const std::string input_str = input.string();
    const std::string output_str = output.string();
    std::ifstream fin(input);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open input file: " + input_str);
    }

    fs::create_directories(output.parent_path());
    TFile out_file(output.c_str(), "RECREATE");
    if (out_file.IsZombie()) {
        throw std::runtime_error("Cannot create output file: " + output_str);
    }

    TTree tree("tree", "Input tree for GEMINI all-event fragments");
    auto beam_array = std::make_unique<TBeamSimDataArray>();
    gBeamSimDataArray = beam_array.get();
    tree.Branch("TBeamSimData", &gBeamSimDataArray);

    std::string line;
    int current_isim = -1;
    double current_bimp = kUnknownBimp;
    long raw_line_count = 0;
    long parsed_row_count = 0;
    long written_event_count = 0;
    long expanded_neutral_cluster_rows = 0;
    long expanded_neutral_cluster_neutrons = 0;
    std::vector<TBeamSimData> primaries;

    while (std::getline(fin, line)) {
        ++raw_line_count;
        GeminiRow row;
        if (!ParseGeminiRow(line, row)) {
            continue;
        }
        if (row.z == 0 && row.n == 0) {
            continue;
        }
        ++parsed_row_count;
        if (current_isim != -1 && row.isim != current_isim) {
            FlushAlleventGroup(primaries, current_isim, current_bimp, source_file_index, pol, tree);
            ++written_event_count;
        }
        current_isim = row.isim;
        current_bimp = row.bimp;

        const int a = row.z + row.n;
        if (a <= 0) {
            continue;
        }
        if (row.z == 0 && a > 1) {
            // [EN] Geant4 tree primaries cannot instantiate neutral multi-neutron clusters, so approximate them as co-moving free neutrons with the fragment per-nucleon momentum. / [CN] Geant4树输入不能直接实例化中性多中子簇，因此把它们近似展开成同速自由中子，并保留该碎片的每核子动量。
            ++expanded_neutral_cluster_rows;
            expanded_neutral_cluster_neutrons += a;
        }
        AppendAlleventPrimaries(row, primaries);
    }

    if (!primaries.empty()) {
        FlushAlleventGroup(primaries, current_isim, current_bimp, source_file_index, pol, tree);
        ++written_event_count;
    }

    out_file.cd();
    tree.Write();
    out_file.Close();
    spdlog::info(
        "Wrote {} all-event groups to {} (raw_lines={}, parsed_rows={}, expanded_neutral_rows={}, expanded_neutral_neutrons={})",
        written_event_count,
        output_str.c_str(),
        raw_line_count,
        parsed_row_count,
        expanded_neutral_cluster_rows,
        expanded_neutral_cluster_neutrons
    );
}

SourceMode ParseSourceMode(const std::string& value) {
    if (value == "elastic") {
        return SourceMode::kElastic;
    }
    if (value == "allevent") {
        return SourceMode::kAllevent;
    }
    if (value == "both") {
        return SourceMode::kBoth;
    }
    throw std::runtime_error("Unknown source mode: " + value);
}

Options ParseArgs(int argc, char* argv[]) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "ypol") {
                opts.mode = Mode::kYpol;
            } else if (mode == "zpol") {
                opts.mode = Mode::kZpol;
            } else if (mode == "both") {
                opts.mode = Mode::kBoth;
            } else {
                throw std::runtime_error("Unknown mode: " + mode);
            }
        } else if (arg == "--source" && i + 1 < argc) {
            opts.source = ParseSourceMode(argv[++i]);
        } else if (arg == "--input-base" && i + 1 < argc) {
            opts.input_base = fs::path(argv[++i]);
        } else if (arg == "--output-base" && i + 1 < argc) {
            opts.output_base = fs::path(argv[++i]);
        } else if (arg == "--target-filter" && i + 1 < argc) {
            opts.target_filter = std::string(argv[++i]);
        } else if (arg == "--cut-unphysical" && i + 1 < argc) {
            opts.cut_unphysical = ParseBoolOption(argv[++i]);
        } else if (arg == "--cut-ypol-axis-limit" && i + 1 < argc) {
            opts.cut_ypol_axis_limit = std::stod(argv[++i]);
        } else if (arg == "--cut-zpol-axis-limit" && i + 1 < argc) {
            opts.cut_zpol_axis_limit = std::stod(argv[++i]);
        } else if (arg == "--rotate-ypol" && i + 1 < argc) {
            opts.rotate_ypol = ParseBoolOption(argv[++i]);
        } else if (arg == "--rotate-zpol" && i + 1 < argc) {
            opts.rotate_zpol = ParseBoolOption(argv[++i]);
        } else if (arg == "--help") {
            spdlog::info(
                "Usage: GenInputRoot_qmdrawdata --mode [ypol|zpol|both] "
                "[--source elastic|allevent|both] "
                "--input-base PATH --output-base PATH "
                "[--target-filter Sn124] "
                "[--cut-unphysical on|off] [--cut-ypol-axis-limit 150] [--cut-zpol-axis-limit 150] "
                "[--rotate-ypol on|off] [--rotate-zpol on|off]"
            );
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    return opts;
}

fs::path ResolveInputBase(const fs::path& cli_input, SourceMode source) {
    if (!cli_input.empty()) {
        return cli_input;
    }
    const char* sms_dir = std::getenv("SMSIMDIR");
    if (!sms_dir) {
        throw std::runtime_error("SMSIMDIR is not set and no --input-base provided");
    }
    fs::path base = fs::path(sms_dir) / "data" / "qmdrawdata";
    fs::path raw = base / "rawdata";
    fs::path qmd = base / "qmdrawdata";

    if (source == SourceMode::kAllevent || source == SourceMode::kBoth) {
        if (fs::exists(qmd / "allevent")) {
            return qmd;
        }
        if (fs::exists(raw / "allevent")) {
            return raw;
        }
    }

    if (fs::exists(raw)) {
        return raw;
    }
    if (fs::exists(qmd)) {
        return qmd;
    }
    throw std::runtime_error("Cannot find input base under: " + base.string());
}

fs::path ResolveOutputBase(const fs::path& cli_output) {
    if (!cli_output.empty()) {
        return cli_output;
    }
    const char* sms_dir = std::getenv("SMSIMDIR");
    if (!sms_dir) {
        throw std::runtime_error("SMSIMDIR is not set and no --output-base provided");
    }
    return fs::path(sms_dir) / "data" / "simulation" / "g4input";
}

std::vector<fs::path> CollectElasticFiles(const fs::path& input_base) {
    std::vector<fs::path> files;
    const fs::path allevent_dir = ResolveAlleventRoot(input_base);
    if (fs::exists(allevent_dir)) {
        files = CollectDatFiles(allevent_dir, "dbreak");
    } else {
        auto y_files = CollectDatFiles(input_base / "y_pol" / "phi_random", "dbreak");
        auto z_files = CollectDatFiles(input_base / "z_pol" / "b_discrete", "dbreak");
        files.reserve(y_files.size() + z_files.size());
        files.insert(files.end(), y_files.begin(), y_files.end());
        files.insert(files.end(), z_files.begin(), z_files.end());
        std::sort(files.begin(), files.end());
        files.erase(std::unique(files.begin(), files.end()), files.end());
    }
    return files;
}

void ProcessYpolElasticFiles(
    const std::vector<fs::path>& files,
    const fs::path& input_base,
    const fs::path& output_base,
    const Options& opts,
    int& source_file_index
) {
    for (const auto& file : files) {
        const auto out = MakeOutputPath(file, input_base, output_base);
        spdlog::info("Converting elastic y_pol file {}", file.string().c_str());
        ConvertElasticFile(
            file,
            out,
            std::nullopt,
            qmd_input_metadata::PolarizationKind::kY,
            opts,
            source_file_index++
        );
    }
}

void ProcessZpolElasticFiles(
    const std::vector<fs::path>& files,
    const fs::path& input_base,
    const fs::path& output_base,
    const Options& opts,
    int& source_file_index
) {
    if (files.empty()) {
        return;
    }

    if (opts.rotate_zpol) {
        spdlog::warn(
            "rotate-zpol is ON: generated files under {} will contain rotated z_pol momenta.",
            output_base.string().c_str()
        );
    }

    std::map<fs::path, std::vector<ZFileInfo>> grouped;
    for (const auto& file : files) {
        const auto info = ReadHeaderInfo(file);
        if (!std::isfinite(info.b)) {
            throw std::runtime_error("Missing b value in header: " + file.string());
        }
        if (info.events <= 0) {
            throw std::runtime_error("Invalid event count in header: " + file.string());
        }
        grouped[file.parent_path()].push_back(ZFileInfo{file, info.b, info.events});
    }

    for (auto& [dir, entries] : grouped) {
        double b_max = 0.0;
        for (const auto& entry : entries) {
            b_max = std::max(b_max, entry.b);
        }
        if (b_max <= 0.0) {
            spdlog::warn("Non-positive b_max in {}, keeping all events", dir.string().c_str());
        }

        for (const auto& entry : entries) {
            long keep_events = entry.events;
            if (b_max > 0.0) {
                const double ratio = entry.b / b_max;
                keep_events = static_cast<long>(std::floor(entry.events * ratio));
                keep_events = std::max<long>(0, std::min<long>(keep_events, entry.events));
            }
            const auto out = MakeOutputPath(entry.path, input_base, output_base);
            spdlog::info(
                "Converting elastic z_pol file {} with b={} (keep {}/{})",
                entry.path.string().c_str(),
                entry.b,
                keep_events,
                entry.events
            );
            ConvertElasticFile(
                entry.path,
                out,
                keep_events,
                qmd_input_metadata::PolarizationKind::kZ,
                opts,
                source_file_index++
            );
        }
    }
}

void ProcessElastic(const fs::path& input_base, const fs::path& output_base, const Options& opts) {
    auto files = CollectElasticFiles(input_base);
    files = ApplyTargetFilter(files, opts.target_filter);
    files = ApplyModeFilter(files, opts.mode);
    if (files.empty()) {
        spdlog::warn("No polarized elastic dbreak*.dat found under {}", input_base.string().c_str());
        return;
    }

    std::vector<fs::path> y_files;
    std::vector<fs::path> z_files;
    for (const auto& file : files) {
        const auto pol = DetectPolarization(file);
        if (pol == qmd_input_metadata::PolarizationKind::kY) {
            y_files.push_back(file);
        } else if (pol == qmd_input_metadata::PolarizationKind::kZ) {
            z_files.push_back(file);
        }
    }

    int source_file_index = 0;
    ProcessYpolElasticFiles(y_files, input_base, output_base, opts, source_file_index);
    ProcessZpolElasticFiles(z_files, input_base, output_base, opts, source_file_index);
}

void ProcessAllevent(const fs::path& input_base, const fs::path& output_base, const Options& opts) {
    const fs::path root_dir = ResolveAlleventRoot(input_base);
    auto files = CollectDatFiles(root_dir, "geminiout");
    files = ApplyTargetFilter(files, opts.target_filter);
    files = ApplyModeFilter(files, opts.mode);
    if (files.empty()) {
        spdlog::warn("No allevent geminiout*.dat found under {}", root_dir.string().c_str());
        return;
    }

    int source_file_index = 0;
    for (const auto& file : files) {
        const auto out = MakeOutputPath(file, input_base, output_base);
        const auto pol = DetectPolarization(file);
        spdlog::info("Converting all-event file {}", file.string().c_str());
        ConvertAlleventFile(file, out, pol, source_file_index++);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        const auto opts = ParseArgs(argc, argv);
        const auto input_base = ResolveInputBase(opts.input_base, opts.source);
        const auto output_base = ResolveOutputBase(opts.output_base);

        if (opts.source == SourceMode::kElastic || opts.source == SourceMode::kBoth) {
            ProcessElastic(input_base, output_base, opts);
        }

        if (opts.source == SourceMode::kAllevent || opts.source == SourceMode::kBoth) {
            ProcessAllevent(input_base, output_base, opts);
        }
    } catch (const std::exception& ex) {
        spdlog::error("Failed: {}", ex.what());
        return 1;
    }
    return 0;
}
