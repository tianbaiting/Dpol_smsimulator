#include <TFile.h>
#include <TTree.h>
#include <TClonesArray.h>
#include <TSystem.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <TLegend.h>
#include <TLatex.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
// #include "TArtNEBULAPla.hh"  // 如果需要具体的NEBULA类定义

struct AcceptanceResult {
    std::string target;
    std::string gamma;
    std::string filename;
    Long64_t total_events;
    Long64_t pdc_hits;
    Long64_t nebula_hits;
    Long64_t coincidence_hits;
    double pdc_acceptance;
    double nebula_acceptance;
    double coincidence_acceptance;
    
    AcceptanceResult() : total_events(0), pdc_hits(0), nebula_hits(0), 
                        coincidence_hits(0), pdc_acceptance(0.0), 
                        nebula_acceptance(0.0), coincidence_acceptance(0.0) {}
};

class AcceptanceAnalyzer {
private:
    std::vector<AcceptanceResult> results;
    std::string output_dir;
    
public:
    AcceptanceAnalyzer(const std::string& outdir = "plots/acceptance") : output_dir(outdir) {
        // 创建输出目录
        gSystem->mkdir(output_dir.c_str(), kTRUE);
        
        // 设置ROOT样式
        gStyle->SetOptStat(0);
        gStyle->SetPalette(55); // Rainbow palette
    }
    
    AcceptanceResult AnalyzeFile(const std::string& root_file_path) {
        AcceptanceResult result;
        
        // 从文件路径解析target和gamma信息
        std::string filename = root_file_path.substr(root_file_path.find_last_of("/") + 1);
        result.filename = filename;
        
        // 解析文件名：ypol_np_Pb208_g0500000.root -> target=Pb208, gamma=g050
        size_t pos1 = filename.find("_np_");
        size_t pos2 = filename.find("_g");
        size_t pos3 = filename.find(".root");
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            result.target = filename.substr(pos1 + 4, pos2 - pos1 - 4);
            std::string gamma_full = filename.substr(pos2 + 1, pos3 - pos2 - 1);
            result.gamma = gamma_full.substr(0, 4); // 取前4个字符，如g050
        } else {
            result.target = "unknown";
            result.gamma = "unknown";
        }
        
        std::cout << "\n[" << result.target << " " << result.gamma << "] Analyzing: " << filename << std::endl;
        
        // 加载必要的库
        gSystem->Load("libsmdata.so");
        
        TFile* file = TFile::Open(root_file_path.c_str());
        if (!file || file->IsZombie()) {
            std::cerr << "  ✗ Error: Cannot open ROOT file: " << root_file_path << std::endl;
            return result;
        }
        
        TTree* tree = (TTree*)file->Get("tree");
        if (!tree) {
            std::cerr << "  ✗ Error: Cannot find tree in ROOT file!" << std::endl;
            file->Close();
            return result;
        }
        
        // 按照 reconstruct_one_event.cc 的方式绑定分支
        TClonesArray *fragArr = nullptr;
        TClonesArray *nebulaArr = nullptr;
        
        bool has_frag = tree->GetBranch("FragSimData") != nullptr;
        bool has_nebula = tree->GetBranch("NEBULAPla") != nullptr;
        
        if (has_frag) tree->SetBranchAddress("FragSimData", &fragArr);
        if (has_nebula) tree->SetBranchAddress("NEBULAPla", &nebulaArr);
        
        Long64_t nentries = tree->GetEntries();
        result.total_events = nentries;
        
        std::cout << "  📊 Processing " << nentries << " events..." << std::endl;
        
        // 逐事件分析
        for (Long64_t i = 0; i < nentries; ++i) {
            tree->GetEntry(i);
            
            bool has_pdc_hit = false;
            bool has_nebula_hit = false;
            
            // 检查 FragSimData (PDC hits)
            if (fragArr) {
                int nfrag = fragArr->GetEntriesFast();
                if (nfrag > 0) {
                    has_pdc_hit = true;
                }
            }
            
            // 检查 NEBULAPla (NEBULA hits)
            if (nebulaArr) {
                int nneb = nebulaArr->GetEntriesFast();
                if (nneb > 0) {
                    has_nebula_hit = true;
                }
            }
            
            // 统计
            if (has_pdc_hit) result.pdc_hits++;
            if (has_nebula_hit) result.nebula_hits++;
            if (has_pdc_hit && has_nebula_hit) result.coincidence_hits++;
            
            // 每1000个事件显示进度
            if ((i + 1) % 1000 == 0 || (i + 1) == nentries) {
                std::cout << "    📈 Processed " << (i + 1) << "/" << nentries << " events\r" << std::flush;
            }
        }
        std::cout << std::endl;
        
        // 计算接受率
        if (nentries > 0) {
            result.pdc_acceptance = (double)result.pdc_hits / nentries * 100.0;
            result.nebula_acceptance = (double)result.nebula_hits / nentries * 100.0;
            result.coincidence_acceptance = (double)result.coincidence_hits / nentries * 100.0;
        }
        
        // 打印结果
        std::cout << "  Total events:        " << result.total_events << std::endl;
        std::cout << "  PDC hits:            " << result.pdc_hits 
                  << " (" << std::fixed << std::setprecision(2) << result.pdc_acceptance << "%)" << std::endl;
        std::cout << "  NEBULA hits:         " << result.nebula_hits 
                  << " (" << std::fixed << std::setprecision(2) << result.nebula_acceptance << "%)" << std::endl;
        std::cout << "  Coincidence hits:    " << result.coincidence_hits 
                  << " (" << std::fixed << std::setprecision(2) << result.coincidence_acceptance << "%)" << std::endl;
        
        file->Close();
        return result;
    }
    
    void ScanDirectory(const std::string& base_path) {
        std::cout << "======================================================================" << std::endl;
        std::cout << "  PDC & NEBULA Acceptance Analysis (C++ Version)" << std::endl;
        std::cout << "======================================================================" << std::endl;
        std::cout << "Scanning directory: " << base_path << std::endl;
        
        // 手动列出预期的文件（因为C++没有方便的目录遍历）
        std::vector<std::string> targets = {"Pb208", "Xe130"};
        std::vector<std::string> gammas = {"g050", "g060", "g070", "g080"};
        
        for (const auto& target : targets) {
            for (const auto& gamma : gammas) {
                std::string subdir = base_path + "/" + target + "_" + gamma;
                std::string filename = "ypol_np_" + target + "_" + gamma + "0000.root";
                std::string full_path = subdir + "/" + filename;
                
                // 检查文件是否存在
                TFile* test_file = TFile::Open(full_path.c_str());
                if (test_file && !test_file->IsZombie()) {
                    test_file->Close();
                    
                    // 分析文件
                    AcceptanceResult result = AnalyzeFile(full_path);
                    results.push_back(result);
                } else {
                    std::cout << "\n⚠ File not found: " << full_path << std::endl;
                    if (test_file) test_file->Close();
                }
            }
        }
    }
    
    void PrintSummaryTable() {
        std::cout << "\n======================================================================" << std::endl;
        std::cout << "  SUMMARY TABLE" << std::endl;
        std::cout << "======================================================================" << std::endl;
        std::cout << std::left << std::setw(10) << "Target" 
                  << std::setw(8) << "Gamma" 
                  << std::setw(8) << "Total" 
                  << std::setw(12) << "PDC %" 
                  << std::setw(12) << "NEBULA %" 
                  << std::setw(12) << "Coinc %" << std::endl;
        std::cout << "----------------------------------------------------------------------" << std::endl;
        
        for (const auto& result : results) {
            std::cout << std::left << std::setw(10) << result.target
                      << std::setw(8) << result.gamma
                      << std::setw(8) << result.total_events
                      << std::setw(12) << std::fixed << std::setprecision(2) << result.pdc_acceptance
                      << std::setw(12) << std::fixed << std::setprecision(2) << result.nebula_acceptance
                      << std::setw(12) << std::fixed << std::setprecision(2) << result.coincidence_acceptance
                      << std::endl;
        }
        std::cout << "======================================================================" << std::endl;
    }
    
    void CreatePlots() {
        if (results.empty()) {
            std::cout << "No results to plot!" << std::endl;
            return;
        }
        
        std::cout << "\nGenerating plots..." << std::endl;
        
        // 图1: 按target分组的条形图
        TCanvas *c1 = new TCanvas("c1", "Acceptance by Target", 1200, 600);
        c1->Divide(2, 1);
        
        // Pb208 和 Xe130 的数据
        std::map<std::string, std::map<std::string, AcceptanceResult*>> data_map;
        for (auto& result : results) {
            data_map[result.target][result.gamma] = &result;
        }
        
        std::vector<std::string> gamma_labels = {"050", "060", "070", "080"};
        std::vector<std::string> gammas = {"g050", "g060", "g070", "g080"};
        
        int pad_idx = 1;
        for (const auto& target_pair : data_map) {
            c1->cd(pad_idx++);
            
            std::string target = target_pair.first;
            
            // 创建直方图
            TH1F *h_pdc = new TH1F(Form("h_pdc_%s", target.c_str()), 
                                   Form("Target: %s;Gamma;Acceptance (%%)", target.c_str()), 
                                   4, 0, 4);
            TH1F *h_nebula = new TH1F(Form("h_nebula_%s", target.c_str()), "", 4, 0, 4);
            TH1F *h_coinc = new TH1F(Form("h_coinc_%s", target.c_str()), "", 4, 0, 4);
            
            // 填充数据
            for (int i = 0; i < 4; i++) {
                std::string gamma = gammas[i];
                auto it = target_pair.second.find(gamma);
                if (it != target_pair.second.end()) {
                    AcceptanceResult* result = it->second;
                    h_pdc->SetBinContent(i + 1, result->pdc_acceptance);
                    h_nebula->SetBinContent(i + 1, result->nebula_acceptance);
                    h_coinc->SetBinContent(i + 1, result->coincidence_acceptance);
                }
                h_pdc->GetXaxis()->SetBinLabel(i + 1, gamma_labels[i].c_str());
            }
            
            // 设置样式
            h_pdc->SetFillColor(kGreen + 1);
            h_pdc->SetLineColor(kGreen + 2);
            h_nebula->SetFillColor(kOrange + 1);
            h_nebula->SetLineColor(kOrange + 2);
            h_coinc->SetFillColor(kViolet + 1);
            h_coinc->SetLineColor(kViolet + 2);
            
            h_pdc->SetMaximum(100);
            h_pdc->Draw("bar");
            h_nebula->Draw("bar same");
            h_coinc->Draw("bar same");
            
            // 图例
            TLegend *leg = new TLegend(0.7, 0.7, 0.9, 0.9);
            leg->AddEntry(h_pdc, "PDC", "f");
            leg->AddEntry(h_nebula, "NEBULA", "f");
            leg->AddEntry(h_coinc, "Coincidence", "f");
            leg->Draw();
            
            gPad->SetGrid();
        }
        
        std::string plot_file1 = output_dir + "/acceptance_by_target.png";
        c1->SaveAs(plot_file1.c_str());
        std::cout << "✓ Saved: " << plot_file1 << std::endl;
        
        // 图2: 热力图
        TCanvas *c2 = new TCanvas("c2", "Acceptance Heatmaps", 1800, 600);
        c2->Divide(3, 1);
        
        // PDC 热力图
        c2->cd(1);
        TH2F *h2_pdc = new TH2F("h2_pdc", "PDC Acceptance (%);Gamma;Target", 
                                4, -0.5, 3.5, 2, -0.5, 1.5);
        
        // NEBULA 热力图
        c2->cd(2);
        TH2F *h2_nebula = new TH2F("h2_nebula", "NEBULA Acceptance (%);Gamma;Target", 
                                   4, -0.5, 3.5, 2, -0.5, 1.5);
        
        // Coincidence 热力图
        c2->cd(3);
        TH2F *h2_coinc = new TH2F("h2_coinc", "Coincidence Acceptance (%);Gamma;Target", 
                                  4, -0.5, 3.5, 2, -0.5, 1.5);
        
        // 设置标签
        for (int i = 0; i < 4; i++) {
            h2_pdc->GetXaxis()->SetBinLabel(i + 1, gamma_labels[i].c_str());
            h2_nebula->GetXaxis()->SetBinLabel(i + 1, gamma_labels[i].c_str());
            h2_coinc->GetXaxis()->SetBinLabel(i + 1, gamma_labels[i].c_str());
        }
        h2_pdc->GetYaxis()->SetBinLabel(1, "Pb208");
        h2_pdc->GetYaxis()->SetBinLabel(2, "Xe130");
        h2_nebula->GetYaxis()->SetBinLabel(1, "Pb208");
        h2_nebula->GetYaxis()->SetBinLabel(2, "Xe130");
        h2_coinc->GetYaxis()->SetBinLabel(1, "Pb208");
        h2_coinc->GetYaxis()->SetBinLabel(2, "Xe130");
        
        // 填充数据
        for (const auto& result : results) {
            int gamma_bin = -1;
            for (int i = 0; i < 4; i++) {
                if (result.gamma == gammas[i]) {
                    gamma_bin = i + 1;
                    break;
                }
            }
            
            int target_bin = (result.target == "Pb208") ? 1 : 2;
            
            if (gamma_bin > 0) {
                h2_pdc->SetBinContent(gamma_bin, target_bin, result.pdc_acceptance);
                h2_nebula->SetBinContent(gamma_bin, target_bin, result.nebula_acceptance);
                h2_coinc->SetBinContent(gamma_bin, target_bin, result.coincidence_acceptance);
            }
        }
        
        c2->cd(1);
        h2_pdc->SetMaximum(100);
        h2_pdc->Draw("colz text");
        
        c2->cd(2);
        h2_nebula->SetMaximum(100);
        h2_nebula->Draw("colz text");
        
        c2->cd(3);
        h2_coinc->SetMaximum(100);
        h2_coinc->Draw("colz text");
        
        std::string plot_file2 = output_dir + "/acceptance_heatmap.png";
        c2->SaveAs(plot_file2.c_str());
        std::cout << "✓ Saved: " << plot_file2 << std::endl;
        
        delete c1;
        delete c2;
    }
};

void analyze_acceptance() {
    // 获取环境变量
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir) {
        std::cerr << "✗ Error: SMSIMDIR environment variable is not set" << std::endl;
        return;
    }
    
    std::string base_path = std::string(smsDir) + "/d_work/output_tree/ypol";
    std::string plot_dir = std::string(smsDir) + "/d_work/plots/acceptance";
    
    // 创建分析器
    AcceptanceAnalyzer analyzer(plot_dir);
    
    // 扫描并分析
    analyzer.ScanDirectory(base_path);
    
    // 打印汇总表
    analyzer.PrintSummaryTable();
    
    // 生成图表
    analyzer.CreatePlots();
    
    std::cout << "\n✓ Analysis completed!" << std::endl;
    std::cout << "Results saved to: " << plot_dir << std::endl;
}