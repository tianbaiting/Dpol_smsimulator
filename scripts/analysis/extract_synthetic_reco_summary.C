// [EN] Usage: root -l -b -q 'scripts/analysis/extract_synthetic_reco_summary.C+("analysis.root","summary.csv")' / [CN] 用法: root -l -b -q 'scripts/analysis/extract_synthetic_reco_summary.C+("analysis.root","summary.csv")'

#include "TFile.h"
#include "TTree.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

namespace {

struct RunningStat {
    long long count = 0;
    double sum = 0.0;
    double sumSq = 0.0;

    void Fill(double x) {
        ++count;
        sum += x;
        sumSq += x * x;
    }

    double Mean() const {
        if (count <= 0) return 0.0;
        return sum / static_cast<double>(count);
    }

    double RMS() const {
        if (count <= 0) return 0.0;
        const double mean = Mean();
        const double var = sumSq / static_cast<double>(count) - mean * mean;
        return (var > 0.0) ? std::sqrt(var) : 0.0;
    }
};

} // namespace

void extract_synthetic_reco_summary(const char* analysis_root, const char* out_csv) {
    TFile inFile(analysis_root, "READ");
    if (inFile.IsZombie()) {
        std::cerr << "[extract_synthetic_reco_summary] Failed to open: " << analysis_root << std::endl;
        return;
    }

    TTree* metrics = dynamic_cast<TTree*>(inFile.Get("metrics"));
    if (!metrics) {
        std::cerr << "[extract_synthetic_reco_summary] Missing tree: metrics" << std::endl;
        return;
    }

    bool hasTruthP = false;
    bool hasTruthN = false;
    bool hasRecoTrack = false;
    bool minuitDone = false;
    bool minuitSuccess = false;
    bool minuitUsable = false;
    bool threeDone = false;
    bool threeSuccess = false;
    bool threeUsable = false;
    bool neutronDone = false;
    double pdcD1 = -1.0;
    double pdcD2 = -1.0;
    double dPxMinuit = 0.0;
    double dPxThree = 0.0;
    double dPxNeutron = 0.0;

    metrics->SetBranchAddress("has_truth_p", &hasTruthP);
    metrics->SetBranchAddress("has_truth_n", &hasTruthN);
    metrics->SetBranchAddress("has_reco_track", &hasRecoTrack);
    metrics->SetBranchAddress("minuit_done", &minuitDone);
    metrics->SetBranchAddress("minuit_success", &minuitSuccess);
    const bool hasMinuitUsableBranch = (metrics->GetBranch("minuit_usable") != nullptr);
    if (hasMinuitUsableBranch) {
        metrics->SetBranchAddress("minuit_usable", &minuitUsable);
    }
    metrics->SetBranchAddress("three_done", &threeDone);
    metrics->SetBranchAddress("three_success", &threeSuccess);
    const bool hasThreeUsableBranch = (metrics->GetBranch("three_usable") != nullptr);
    if (hasThreeUsableBranch) {
        metrics->SetBranchAddress("three_usable", &threeUsable);
    }
    metrics->SetBranchAddress("neutron_done", &neutronDone);
    metrics->SetBranchAddress("pdc_d1", &pdcD1);
    metrics->SetBranchAddress("pdc_d2", &pdcD2);
    metrics->SetBranchAddress("dpx_minuit", &dPxMinuit);
    metrics->SetBranchAddress("dpx_three", &dPxThree);
    metrics->SetBranchAddress("dpx_neutron", &dPxNeutron);

    long long truthPCount = 0;
    long long truthNCount = 0;
    long long recoTrackCount = 0;
    long long truthPAndRecoTrack = 0;
    long long truthNAndRecoNeutron = 0;
    long long minuitDoneCount = 0;
    long long minuitSuccessCount = 0;
    long long minuitUsableCount = 0;
    long long threeDoneCount = 0;
    long long threeSuccessCount = 0;
    long long threeUsableCount = 0;

    RunningStat statPdcD1;
    RunningStat statPdcD2;
    RunningStat statMinuit;
    RunningStat statThree;
    RunningStat statNeutron;

    const long long nEntries = metrics->GetEntries();
    for (long long i = 0; i < nEntries; ++i) {
        metrics->GetEntry(i);

        if (!hasMinuitUsableBranch) {
            minuitUsable = minuitSuccess;
        }
        if (!hasThreeUsableBranch) {
            threeUsable = threeSuccess;
        }

        if (hasTruthP) ++truthPCount;
        if (hasTruthN) ++truthNCount;

        if (hasRecoTrack) {
            ++recoTrackCount;
            if (pdcD1 >= 0.0) statPdcD1.Fill(pdcD1);
            if (pdcD2 >= 0.0) statPdcD2.Fill(pdcD2);
            if (hasTruthP) ++truthPAndRecoTrack;
        }
        if (hasTruthN && neutronDone) ++truthNAndRecoNeutron;

        if (minuitDone) {
            ++minuitDoneCount;
            if (minuitSuccess) ++minuitSuccessCount;
            if (minuitUsable && std::isfinite(dPxMinuit)) {
                ++minuitUsableCount;
                statMinuit.Fill(dPxMinuit);
            }
        }
        if (threeDone) {
            ++threeDoneCount;
            if (threeSuccess) ++threeSuccessCount;
            if (threeUsable && std::isfinite(dPxThree)) {
                ++threeUsableCount;
                statThree.Fill(dPxThree);
            }
        }
        if (neutronDone) {
            statNeutron.Fill(dPxNeutron);
        }
    }

    const double pdcTrackEff = (truthPCount > 0) ? static_cast<double>(truthPAndRecoTrack) / static_cast<double>(truthPCount) : 0.0;
    const double neutronRecoEff = (truthNCount > 0) ? static_cast<double>(truthNAndRecoNeutron) / static_cast<double>(truthNCount) : 0.0;
    const double minuitSuccessEff = (minuitDoneCount > 0) ? static_cast<double>(minuitSuccessCount) / static_cast<double>(minuitDoneCount) : 0.0;
    const double minuitUsableEff = (minuitDoneCount > 0) ? static_cast<double>(minuitUsableCount) / static_cast<double>(minuitDoneCount) : 0.0;
    const double threeSuccessEff = (threeDoneCount > 0) ? static_cast<double>(threeSuccessCount) / static_cast<double>(threeDoneCount) : 0.0;
    const double threeUsableEff = (threeDoneCount > 0) ? static_cast<double>(threeUsableCount) / static_cast<double>(threeDoneCount) : 0.0;

    std::ofstream csv(out_csv);
    if (!csv.is_open()) {
        std::cerr << "[extract_synthetic_reco_summary] Failed to open output: " << out_csv << std::endl;
        return;
    }

    csv << "metric,count,mean,rms\n";
    csv << "truth_proton_events," << truthPCount << ",0,0\n";
    csv << "truth_neutron_events," << truthNCount << ",0,0\n";
    csv << "pdc_track_events," << recoTrackCount << ",0,0\n";
    csv << "pdc_track_eff_vs_truth_proton," << truthPCount << "," << pdcTrackEff << ",0\n";
    csv << "nebula_reco_eff_vs_truth_neutron," << truthNCount << "," << neutronRecoEff << ",0\n";
    csv << "pdc_d1," << statPdcD1.count << "," << statPdcD1.Mean() << "," << statPdcD1.RMS() << "\n";
    csv << "pdc_d2," << statPdcD2.count << "," << statPdcD2.Mean() << "," << statPdcD2.RMS() << "\n";
    csv << "proton_minuit_done," << minuitDoneCount << ",0,0\n";
    csv << "proton_minuit_success," << minuitDoneCount << "," << minuitSuccessEff << ",0\n";
    csv << "proton_minuit_usable," << minuitDoneCount << "," << minuitUsableEff << ",0\n";
    csv << "proton_three_done," << threeDoneCount << ",0,0\n";
    csv << "proton_three_success," << threeDoneCount << "," << threeSuccessEff << ",0\n";
    csv << "proton_three_usable," << threeDoneCount << "," << threeUsableEff << ",0\n";
    csv << "proton_dpx_minuit," << statMinuit.count << "," << statMinuit.Mean() << "," << statMinuit.RMS() << "\n";
    csv << "proton_dpx_threepoint," << statThree.count << "," << statThree.Mean() << "," << statThree.RMS() << "\n";
    csv << "neutron_dpx_tof," << statNeutron.count << "," << statNeutron.Mean() << "," << statNeutron.RMS() << "\n";
    csv.close();

    std::cerr << "[extract_synthetic_reco_summary] Wrote: " << out_csv << std::endl;
}
