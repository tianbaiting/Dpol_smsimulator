#include "TFile.h"
#include "TTree.h"
#include "TMath.h"
#include "TVector3.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct BinStat {
    int total = 0;
    int hits = 0;
    int incidenceCount = 0;
    double incidenceSum = 0.0;
    double incidenceSumSq = 0.0;
};

std::vector<double> BuildPxGrid(double pxMin, double pxMax, double pxStep) {
    std::vector<double> pxValues;
    const double eps = std::abs(pxStep) * 1e-9;
    for (double px = pxMin; px <= pxMax + eps; px += pxStep) {
        pxValues.push_back(px);
    }
    return pxValues;
}

void PrintUsage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --input <sim.root> --summary <summary.csv> --detail <detail.csv>"
              << " --px-min <value> --px-max <value> --px-step <value>"
              << " --repeat <events_per_px> --pdc-angle-deg <value>"
              << " --pdc-z1-cm <value> --pdc-z2-cm <value>\n";
}

double Clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

} // namespace

int main(int argc, char** argv) {
    std::string inputPath;
    std::string summaryPath;
    std::string detailPath;
    double pxMin = -150.0;
    double pxMax = 150.0;
    double pxStep = 10.0;
    int repeatPerPx = 1;
    double pdcAngleSamuraiDeg = 65.0;
    double pdcZ1Cm = 400.0;
    double pdcZ2Cm = 500.0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            inputPath = argv[++i];
        } else if (arg == "--summary" && i + 1 < argc) {
            summaryPath = argv[++i];
        } else if (arg == "--detail" && i + 1 < argc) {
            detailPath = argv[++i];
        } else if (arg == "--px-min" && i + 1 < argc) {
            pxMin = std::atof(argv[++i]);
        } else if (arg == "--px-max" && i + 1 < argc) {
            pxMax = std::atof(argv[++i]);
        } else if (arg == "--px-step" && i + 1 < argc) {
            pxStep = std::atof(argv[++i]);
        } else if (arg == "--repeat" && i + 1 < argc) {
            repeatPerPx = std::atoi(argv[++i]);
        } else if (arg == "--pdc-angle-deg" && i + 1 < argc) {
            pdcAngleSamuraiDeg = std::atof(argv[++i]);
        } else if (arg == "--pdc-z1-cm" && i + 1 < argc) {
            pdcZ1Cm = std::atof(argv[++i]);
        } else if (arg == "--pdc-z2-cm" && i + 1 < argc) {
            pdcZ2Cm = std::atof(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            PrintUsage(argv[0]);
            return 2;
        }
    }

    if (inputPath.empty() || summaryPath.empty() || detailPath.empty()) {
        PrintUsage(argv[0]);
        return 2;
    }
    if (pxStep <= 0.0 || repeatPerPx <= 0 || pxMax < pxMin) {
        std::cerr << "Invalid px grid or repeat arguments.\n";
        return 2;
    }
    if (std::abs(pdcZ2Cm - pdcZ1Cm) < 1e-9) {
        std::cerr << "Invalid PDC Z configuration: z1 and z2 must be different.\n";
        return 2;
    }

    const std::vector<double> pxValues = BuildPxGrid(pxMin, pxMax, pxStep);
    const int nPx = static_cast<int>(pxValues.size());
    std::vector<BinStat> stats(nPx);

    TFile inFile(inputPath.c_str(), "READ");
    if (inFile.IsZombie()) {
        std::cerr << "Failed to open input ROOT: " << inputPath << "\n";
        return 1;
    }

    TTree* tree = dynamic_cast<TTree*>(inFile.Get("tree"));
    if (!tree) {
        std::cerr << "Cannot find TTree 'tree' in " << inputPath << "\n";
        return 1;
    }

    const bool hasOk1 = (tree->GetBranch("OK_PDC1") != nullptr);
    const bool hasOk2 = (tree->GetBranch("OK_PDC2") != nullptr);
    const bool hasPdcTrackNo = (tree->GetBranch("PDCTrackNo") != nullptr);
    const bool hasPdc1X = (tree->GetBranch("PDC1X") != nullptr);
    const bool hasPdc2X = (tree->GetBranch("PDC2X") != nullptr);

    Bool_t okPdc1 = kFALSE;
    Bool_t okPdc2 = kFALSE;
    Int_t pdcTrackNo = 0;
    Double_t pdc1X = 0.0;
    Double_t pdc2X = 0.0;

    if (!hasOk1 || !hasOk2) {
        if (!hasPdcTrackNo) {
            std::cerr << "Missing acceptance branches: need OK_PDC1/OK_PDC2 or PDCTrackNo.\n";
            return 1;
        }
    }
    if (!hasPdc1X || !hasPdc2X) {
        std::cerr << "Missing incidence branches: need PDC1X and PDC2X.\n";
        return 1;
    }

    if (hasOk1) tree->SetBranchAddress("OK_PDC1", &okPdc1);
    if (hasOk2) tree->SetBranchAddress("OK_PDC2", &okPdc2);
    if (hasPdcTrackNo) tree->SetBranchAddress("PDCTrackNo", &pdcTrackNo);
    tree->SetBranchAddress("PDC1X", &pdc1X);
    tree->SetBranchAddress("PDC2X", &pdc2X);

    const Long64_t nEntries = tree->GetEntries();
    const Long64_t expectedEvents = static_cast<Long64_t>(nPx) * static_cast<Long64_t>(repeatPerPx);
    const Long64_t nUse = std::min(nEntries, expectedEvents);

    const double pdcAngleLabDeg = -pdcAngleSamuraiDeg;
    const double pdcAngleLabRad = pdcAngleLabDeg * TMath::DegToRad();
    const TVector3 pdcNormalLab(std::sin(pdcAngleLabRad), 0.0, std::cos(pdcAngleLabRad));

    int incidenceTotalCount = 0;
    double incidenceTotalSum = 0.0;
    double incidenceTotalSumSq = 0.0;

    for (Long64_t i = 0; i < nUse; ++i) {
        tree->GetEntry(i);

        const int pxIdx = static_cast<int>(i / repeatPerPx);
        if (pxIdx < 0 || pxIdx >= nPx) continue;
        BinStat& bin = stats[pxIdx];
        bin.total++;

        bool hit1 = false;
        bool hit2 = false;
        bool hasIncidence = false;
        double incidenceDeg = 0.0;

        if (hasOk1) hit1 = (okPdc1 != kFALSE);
        if (hasOk2) hit2 = (okPdc2 != kFALSE);
        if ((!hasOk1 || !hasOk2) && hasPdcTrackNo) {
            const bool bothFromTrackNo = (pdcTrackNo > 0);
            if (!hasOk1) hit1 = bothFromTrackNo;
            if (!hasOk2) hit2 = bothFromTrackNo;
        }

        if (std::isfinite(pdc1X) && std::isfinite(pdc2X)) {
            const double dx = pdc2X - pdc1X;
            const double dz = pdcZ2Cm - pdcZ1Cm;
            TVector3 trackDir(dx, 0.0, dz);
            if (trackDir.Mag2() > 1e-12) {
                trackDir = trackDir.Unit();
                const double cosTheta = Clamp(std::abs(trackDir.Dot(pdcNormalLab)), -1.0, 1.0);
                incidenceDeg = std::acos(cosTheta) * TMath::RadToDeg();
                hasIncidence = true;
            }
        }

        const bool both = hit1 && hit2;
        if (both) {
            bin.hits++;
        }

        if (hasIncidence) {
            bin.incidenceCount++;
            bin.incidenceSum += incidenceDeg;
            bin.incidenceSumSq += incidenceDeg * incidenceDeg;
            incidenceTotalCount++;
            incidenceTotalSum += incidenceDeg;
            incidenceTotalSumSq += incidenceDeg * incidenceDeg;
        }
    }

    // [EN] Compute per-px acceptance and summary metrics. / [CN] 计算按px分箱接受率与汇总指标。
    std::vector<double> effValues;
    effValues.reserve(nPx);
    double edgeMinAbs130 = 1.0;
    double edgeSumAbs130 = 0.0;
    int edgeCountAbs130 = 0;

    for (int i = 0; i < nPx; ++i) {
        const BinStat& bin = stats[i];
        const double eff = (bin.total > 0) ? static_cast<double>(bin.hits) / static_cast<double>(bin.total) : 0.0;
        effValues.push_back(eff);
        if (std::abs(pxValues[i]) >= 130.0) {
            edgeMinAbs130 = std::min(edgeMinAbs130, eff);
            edgeSumAbs130 += eff;
            edgeCountAbs130++;
        }
    }

    double effMean = 0.0;
    double effMeanSq = 0.0;
    for (double e : effValues) {
        effMean += e;
        effMeanSq += e * e;
    }
    if (!effValues.empty()) {
        effMean /= static_cast<double>(effValues.size());
        effMeanSq /= static_cast<double>(effValues.size());
    }
    const double effStd = std::sqrt(std::max(0.0, effMeanSq - effMean * effMean));
    const double effCv = (effMean > 1e-12) ? (effStd / effMean) : 0.0;
    if (edgeCountAbs130 == 0) {
        edgeMinAbs130 = 0.0;
    }
    const double edgeMeanAbs130 = (edgeCountAbs130 > 0) ? (edgeSumAbs130 / static_cast<double>(edgeCountAbs130)) : 0.0;

    const int totalEventsUsed = static_cast<int>(nUse);
    int totalHits = 0;
    for (const auto& bin : stats) totalHits += bin.hits;
    const double trackEff = (totalEventsUsed > 0) ? static_cast<double>(totalHits) / static_cast<double>(totalEventsUsed) : 0.0;

    const double incidenceMean = (incidenceTotalCount > 0) ? (incidenceTotalSum / static_cast<double>(incidenceTotalCount)) : 0.0;
    const double incidenceRms = (incidenceTotalCount > 0)
        ? std::sqrt(std::max(0.0, incidenceTotalSumSq / static_cast<double>(incidenceTotalCount) - incidenceMean * incidenceMean))
        : 0.0;

    std::ofstream detail(detailPath);
    detail << "px,total,hits,acceptance,incidence_mean_deg,incidence_rms_deg\n";
    for (int i = 0; i < nPx; ++i) {
        const BinStat& bin = stats[i];
        const double eff = (bin.total > 0) ? static_cast<double>(bin.hits) / static_cast<double>(bin.total) : 0.0;
        const double incMean = (bin.incidenceCount > 0) ? (bin.incidenceSum / static_cast<double>(bin.incidenceCount)) : 0.0;
        const double incRms = (bin.incidenceCount > 0)
            ? std::sqrt(std::max(0.0, bin.incidenceSumSq / static_cast<double>(bin.incidenceCount) - incMean * incMean))
            : 0.0;
        detail << std::fixed << std::setprecision(6)
               << pxValues[i] << ","
               << bin.total << ","
               << bin.hits << ","
               << eff << ","
               << incMean << ","
               << incRms << "\n";
    }
    detail.close();

    std::ofstream summary(summaryPath);
    summary << "metric,count,mean,rms\n";
    summary << "total_events_used," << totalEventsUsed << ",0,0\n";
    summary << "pdc_track_eff_vs_truth_proton," << totalEventsUsed << "," << trackEff << ",0\n";
    summary << "pdc_eff_mean_vs_px," << effValues.size() << "," << effMean << "," << effStd << "\n";
    summary << "pdc_eff_cv_vs_px," << effValues.size() << "," << effCv << ",0\n";
    summary << "pdc_eff_edge_min_abs130," << edgeCountAbs130 << "," << edgeMinAbs130 << ",0\n";
    summary << "pdc_eff_edge_mean_abs130," << edgeCountAbs130 << "," << edgeMeanAbs130 << ",0\n";
    summary << "pdc_incidence_angle_deg," << incidenceTotalCount << "," << incidenceMean << "," << incidenceRms << "\n";
    summary << "pdc_angle_samurai_deg,1," << pdcAngleSamuraiDeg << ",0\n";
    summary << "pdc_angle_lab_deg,1," << pdcAngleLabDeg << ",0\n";
    summary << "pdc_angle_from_macro,1," << pdcAngleSamuraiDeg << ",0\n";
    summary.close();

    std::cout << "[analyze_pdc_scan_root] entries=" << nEntries
              << " used=" << nUse
              << " track_eff=" << trackEff
              << " edge_min_abs130=" << edgeMinAbs130
              << " cv=" << effCv
              << " incidence_rms_deg=" << incidenceRms
              << "\n";
    std::cout << "[analyze_pdc_scan_root] summary=" << summaryPath << "\n";
    std::cout << "[analyze_pdc_scan_root] detail=" << detailPath << "\n";

    return 0;
}
