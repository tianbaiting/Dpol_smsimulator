// [EN] Usage: build the binary, then run `GenInputRoot_qmdrawdata --mode [ypol|zpol|both]` / [CN] 用法：先编译，再运行 `GenInputRoot_qmdrawdata --mode [ypol|zpol|both]`
// [EN] Example: `GenInputRoot_qmdrawdata --mode ypol` (uses $SMSIMDIR/data/qmdrawdata/rawdata or qmdrawdata) / [CN] 示例：`GenInputRoot_qmdrawdata --mode ypol`（默认使用 $SMSIMDIR/data/qmdrawdata/rawdata 或 qmdrawdata）
// [EN] Override paths with `--input-base PATH --output-base PATH` / [CN] 通过 `--input-base PATH --output-base PATH` 覆盖输入输出路径
// [EN] Zpol keeps only b/b_max events per file because yield scales with b / [CN] Zpol按每个文件的 b/b_max 保留事件数，因为产额与 b 成正比
#include <TFile.h>
#include <TTree.h>
#include <TLorentzVector.h>
#include <TVector3.h>
#include "TBeamSimData.hh"

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

TBeamSimDataArray* gBeamSimDataArray = nullptr;

namespace fs = std::filesystem;

namespace {

constexpr double kMp = 938.27;
constexpr double kMn = 939.57;

enum class Mode {
    kYpol,
    kZpol,
    kBoth
};

struct Options {
    fs::path input_base;
    fs::path output_base;
    Mode mode = Mode::kBoth;
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

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
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

std::vector<fs::path> CollectDatFiles(const fs::path& root_dir) {
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
        if (!StartsWith(filename, "dbreak")) {
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

void ConvertFile(const fs::path& input, const fs::path& output, std::optional<long> max_events) {
    std::ifstream fin(input);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open input file: " + input.string());
    }

    std::string line;
    if (!std::getline(fin, line)) {
        throw std::runtime_error("Missing header line 1 in: " + input.string());
    }
    if (!std::getline(fin, line)) {
        throw std::runtime_error("Missing header line 2 in: " + input.string());
    }

    fs::create_directories(output.parent_path());
    TFile out_file(output.c_str(), "RECREATE");
    if (out_file.IsZombie()) {
        throw std::runtime_error("Cannot create output file: " + output.string());
    }

    TTree tree("tree", "Input tree for simultaneous n-p");
    auto beam_array = std::make_unique<TBeamSimDataArray>();
    // [EN] Keep ownership local while exposing the pointer required by ROOT I/O / [CN] 用本地所有权管理数组，同时向ROOT I/O暴露所需指针
    gBeamSimDataArray = beam_array.get();
    tree.Branch("TBeamSimData", &gBeamSimDataArray);

    const TVector3 position(0.0, 0.0, 0.0);
    long event_count = 0;

    while (std::getline(fin, line)) {
        if (max_events.has_value() && event_count >= *max_events) {
            break;
        }
        std::istringstream iss(line);
        int no = 0;
        double pxp = 0.0, pyp = 0.0, pzp = 0.0, pxn = 0.0, pyn = 0.0, pzn = 0.0;
        if (!(iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn)) {
            continue;
        }

        gBeamSimDataArray->clear();

        // [EN] Use relativistic energy to keep primaries on-shell for Geant4 / [CN] 使用相对论能量保证Geant4初级粒子在壳面上
        const double Ep = std::sqrt(pxp * pxp + pyp * pyp + pzp * pzp + kMp * kMp);
        TLorentzVector momentum_p(pxp, pyp, pzp, Ep);
        TBeamSimData proton(1, 1, momentum_p, position);
        proton.fParticleName = "proton";
        proton.fPrimaryParticleID = 0;
        proton.fTime = 0.0;
        proton.fIsAccepted = true;
        gBeamSimDataArray->push_back(proton);

        const double En = std::sqrt(pxn * pxn + pyn * pyn + pzn * pzn + kMn * kMn);
        TLorentzVector momentum_n(pxn, pyn, pzn, En);
        TBeamSimData neutron(0, 1, momentum_n, position);
        neutron.fParticleName = "neutron";
        neutron.fPrimaryParticleID = 1;
        neutron.fTime = 0.0;
        neutron.fIsAccepted = true;
        gBeamSimDataArray->push_back(neutron);

        tree.Fill();
        ++event_count;

        if (event_count % 10000 == 0) {
            spdlog::info("Processed {} events from {}", event_count, input.string());
        }
    }

    out_file.cd();
    tree.Write();
    out_file.Close();
    spdlog::info("Wrote {} events to {}", event_count, output.string());
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
        } else if (arg == "--input-base" && i + 1 < argc) {
            opts.input_base = fs::path(argv[++i]);
        } else if (arg == "--output-base" && i + 1 < argc) {
            opts.output_base = fs::path(argv[++i]);
        } else if (arg == "--help") {
            spdlog::info("Usage: GenInputRoot_qmdrawdata --mode [ypol|zpol|both] --input-base PATH --output-base PATH");
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    return opts;
}

fs::path ResolveInputBase(const fs::path& cli_input) {
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

void ProcessYpol(const fs::path& input_base, const fs::path& output_base) {
    const fs::path root_dir = input_base / "y_pol" / "phi_random";
    const auto files = CollectDatFiles(root_dir);
    if (files.empty()) {
        spdlog::warn("No y_pol phi_random dbreak*.dat found under {}", root_dir.string());
        return;
    }
    for (const auto& file : files) {
        const auto out = MakeOutputPath(file, input_base, output_base);
        spdlog::info("Converting y_pol file {}", file.string());
        ConvertFile(file, out, std::nullopt);
    }
}

void ProcessZpol(const fs::path& input_base, const fs::path& output_base) {
    const fs::path root_dir = input_base / "z_pol" / "b_discrete";
    const auto files = CollectDatFiles(root_dir);
    if (files.empty()) {
        spdlog::warn("No z_pol b_discrete dbreak*.dat found under {}", root_dir.string());
        return;
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
            if (entry.b > b_max) {
                b_max = entry.b;
            }
        }
        if (b_max <= 0.0) {
            spdlog::warn("Non-positive b_max in {}, keeping all events", dir.string());
        }
        for (const auto& entry : entries) {
            long keep_events = entry.events;
            if (b_max > 0.0) {
                // [EN] Keep only b/b_max fraction because event yield scales with impact parameter b / [CN] 仅保留 b/b_max 的事件数，因为事件产额与碰撞参数 b 成正比
                const double ratio = entry.b / b_max;
                keep_events = static_cast<long>(std::floor(entry.events * ratio));
                if (keep_events < 0) {
                    keep_events = 0;
                }
                if (keep_events > entry.events) {
                    keep_events = entry.events;
                }
            }
            const auto out = MakeOutputPath(entry.path, input_base, output_base);
            spdlog::info("Converting z_pol file {} with b={} (keep {}/{})", entry.path.string(), entry.b, keep_events, entry.events);
            ConvertFile(entry.path, out, keep_events);
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        const auto opts = ParseArgs(argc, argv);
        const auto input_base = ResolveInputBase(opts.input_base);
        const auto output_base = ResolveOutputBase(opts.output_base);

        if (opts.mode == Mode::kYpol || opts.mode == Mode::kBoth) {
            ProcessYpol(input_base, output_base);
        }
        if (opts.mode == Mode::kZpol || opts.mode == Mode::kBoth) {
            ProcessZpol(input_base, output_base);
        }
    } catch (const std::exception& ex) {
        spdlog::error("Failed: {}", ex.what());
        return 1;
    }
    return 0;
}
