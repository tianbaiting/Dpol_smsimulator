/**
 * @file run_geo_acceptance_batch.C
 * @brief ROOT脚本 - 批量几何接受度分析
 * 
 * 使用GeoAcceptanceManager进行批量分析
 * 
 * 使用方法:
 *   root -l run_geo_acceptance_batch.C
 *   或
 *   root -l 'run_geo_acceptance_batch.C("output_dir")'
 * 
 * @author Auto-generated
 * @date 2025-11-30
 */

#include <iostream>
#include <vector>
#include <string>
#include "TSystem.h"
#include "TString.h"
#include "TROOT.h"


// 加载必要的库
void LoadLibraries() {
    cout << "加载库..." << endl;
    
    // 加载Analysis库
    if (gSystem->Load("lib/libAnalysis.so") < 0) {
        cerr << "错误: 无法加载libAnalysis.so" << endl;
        cerr << "请确保已编译Analysis库" << endl;
        return;
    }
    cout << "✓ 已加载 libAnalysis.so" << endl;
    
    // 加载GeoAcceptance库
    if (gSystem->Load("lib/libGeoAcceptance.so") < 0) {
        cerr << "错误: 无法加载libGeoAcceptance.so" << endl;
        cerr << "请确保已编译GeoAcceptance库" << endl;
        return;
    }
    cout << "✓ 已加载 libGeoAcceptance.so" << endl;
}

// 主函数
void run_geo_acceptance_batch(const char* outputDir = "./geo_acceptance_batch_results") {
    
    cout << "\n" << string(70, '=') << endl;
    cout << "          几何接受度批量分析 (ROOT版本)" << endl;
    cout << "     Geometry Acceptance Batch Analysis (ROOT)" << endl;
    cout << string(70, '=') << endl << endl;
    
    // 加载库
    LoadLibraries();
    
    // 包含头文件
    gROOT->ProcessLine("#include \"GeoAcceptanceManager.hh\"");
    
    // 定义磁场配置
    struct FieldConfig {
        string name;
        string file;
        double strength;
    };
    
    vector<FieldConfig> fieldConfigs = {
        {"0.8T", "configs/simulation/geometry/filed_map/141114-0,8T-6000/field.dat", 0.8},
        {"1.0T", "configs/simulation/geometry/filed_map/180626-1,00T-6000/field.dat", 1.0},
        {"1.2T", "configs/simulation/geometry/filed_map/180626-1,20T-6000/field.dat", 1.2},
        {"1.4T", "configs/simulation/geometry/filed_map/180703-1,40T-6000/field.dat", 1.4}
    };
    
    // 定义偏转角度
    vector<double> deflectionAngles = {0.0, 5.0, 10.0};
    
    cout << "配置:" << endl;
    cout << "  磁场数量: " << fieldConfigs.size() << endl;
    cout << "  偏转角度: ";
    for (size_t i = 0; i < deflectionAngles.size(); i++) {
        cout << deflectionAngles[i] << "°";
        if (i < deflectionAngles.size() - 1) cout << ", ";
    }
    cout << "\n  输出目录: " << outputDir << "\n" << endl;
    
    // 创建输出目录
    gSystem->mkdir(outputDir, kTRUE);
    
    // 遍历每个磁场配置
    int successCount = 0;
    
    for (size_t i = 0; i < fieldConfigs.size(); i++) {
        const auto& config = fieldConfigs[i];
        
        cout << "\n" << string(70, '=') << endl;
        cout << "[" << (i+1) << "/" << fieldConfigs.size() << "] ";
        cout << "分析磁场: " << config.name << " (" << config.strength << " T)" << endl;
        cout << string(70, '=') << endl;
        
        // 检查磁场文件是否存在
        if (gSystem->AccessPathName(config.file.c_str())) {
            cerr << "错误: 磁场文件不存在: " << config.file << endl;
            continue;
        }
        
        try {
            // 创建分析管理器
            cout << "创建GeoAcceptanceManager..." << endl;
            GeoAcceptanceManager* manager = new GeoAcceptanceManager();
            
            // 配置分析
            cout << "配置分析参数..." << endl;
            manager->AddFieldMap(config.file, config.strength);
            
            for (double angle : deflectionAngles) {
                manager->AddDeflectionAngle(angle);
            }
            
            // 设置路径
            manager->SetQMDDataPath("data/qmdrawdata");
            
            // 创建该磁场的输出目录
            TString fieldOutputDir = Form("%s/results_%s", outputDir, config.name.c_str());
            gSystem->mkdir(fieldOutputDir, kTRUE);
            manager->SetOutputPath(fieldOutputDir.Data());
            
            // 运行分析
            cout << "\n开始分析..." << endl;
            bool success = manager->RunFullAnalysis();
            
            if (success) {
                cout << "✓ 分析成功: " << config.name << endl;
                successCount++;
            } else {
                cerr << "✗ 分析失败: " << config.name << endl;
            }
            
            // 清理
            delete manager;
            
        } catch (const exception& e) {
            cerr << "异常: " << e.what() << endl;
        }
    }
    
    // 生成汇总
    cout << "\n" << string(70, '=') << endl;
    cout << "批量分析完成!" << endl;
    cout << "成功: " << successCount << "/" << fieldConfigs.size() << endl;
    cout << "输出目录: " << outputDir << endl;
    cout << string(70, '=') << endl << endl;
    
    // 生成汇总脚本
    TString summaryScript = Form("%s/plot_summary.C", outputDir);
    ofstream scriptFile(summaryScript.Data());
    
    scriptFile << "// 汇总分析结果\n";
    scriptFile << "void plot_summary() {\n";
    scriptFile << "    gStyle->SetOptStat(0);\n";
    scriptFile << "    \n";
    scriptFile << "    TCanvas *c1 = new TCanvas(\"c1\", \"Acceptance Comparison\", 1400, 1000);\n";
    scriptFile << "    c1->Divide(2, 2);\n";
    scriptFile << "    \n";
    
    for (size_t i = 0; i < fieldConfigs.size(); i++) {
        scriptFile << "    // " << fieldConfigs[i].name << "\n";
        scriptFile << "    c1->cd(" << (i+1) << ");\n";
        scriptFile << "    TFile *f" << i << " = TFile::Open(\"results_" 
                   << fieldConfigs[i].name << "/acceptance_results.root\");\n";
        scriptFile << "    if (f" << i << " && !f" << i << "->IsZombie()) {\n";
        scriptFile << "        TTree *tree" << i << " = (TTree*)f" << i << "->Get(\"acceptance\");\n";
        scriptFile << "        if (tree" << i << ") {\n";
        scriptFile << "            tree" << i << "->SetMarkerStyle(20);\n";
        scriptFile << "            tree" << i << "->SetMarkerColor(kBlue);\n";
        scriptFile << "            tree" << i << "->Draw(\"pdcAcceptance:deflectionAngle\", \"\", \"P\");\n";
        scriptFile << "            gPad->SetTitle(\"" << fieldConfigs[i].name << " - PDC Acceptance\");\n";
        scriptFile << "            gPad->SetGrid();\n";
        scriptFile << "        }\n";
        scriptFile << "    }\n\n";
    }
    
    scriptFile << "    c1->SaveAs(\"acceptance_comparison.png\");\n";
    scriptFile << "    cout << \"图表已保存: acceptance_comparison.png\" << endl;\n";
    scriptFile << "}\n";
    
    scriptFile.close();
    
    cout << "✓ 汇总脚本已创建: " << summaryScript << endl;
    cout << "  运行方法: cd " << outputDir << " && root -l plot_summary.C" << endl << endl;
}

// 简化版本 - 单个磁场分析
void run_single_field(const char* fieldName = "1.0T", 
                     const char* outputDir = "./single_field_results") {
    
    cout << "\n单磁场分析: " << fieldName << endl;
    
    LoadLibraries();
    gROOT->ProcessLine("#include \"GeoAcceptanceManager.hh\"");
    
    // 磁场配置映射
    map<string, pair<string, double>> fieldMap = {
        {"0.8T", {"configs/simulation/geometry/filed_map/141114-0,8T-6000/field.dat", 0.8}},
        {"1.0T", {"configs/simulation/geometry/filed_map/180626-1,00T-6000/field.dat", 1.0}},
        {"1.2T", {"configs/simulation/geometry/filed_map/180626-1,20T-6000/field.dat", 1.2}},
        {"1.4T", {"configs/simulation/geometry/filed_map/180703-1,40T-6000/field.dat", 1.4}}
    };
    
    if (fieldMap.find(fieldName) == fieldMap.end()) {
        cerr << "错误: 未知的磁场名称: " << fieldName << endl;
        cerr << "可用选项: 0.8T, 1.0T, 1.2T, 1.4T" << endl;
        return;
    }
    
    auto config = fieldMap[fieldName];
    
    GeoAcceptanceManager* manager = new GeoAcceptanceManager();
    manager->AddFieldMap(config.first, config.second);
    manager->AddDeflectionAngle(0.0);
    manager->AddDeflectionAngle(5.0);
    manager->AddDeflectionAngle(10.0);
    manager->SetQMDDataPath("data/qmdrawdata");
    manager->SetOutputPath(outputDir);
    
    manager->RunFullAnalysis();
    
    delete manager;
}
