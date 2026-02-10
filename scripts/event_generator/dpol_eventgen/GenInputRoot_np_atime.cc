// 生成smsimulator的root文件，np同时fill

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <string>
#include <TFile.h>
#include <TTree.h>
#include "TBeamSimData.hh"

void GenInputRoot_np_atime(const std::string& inputFile /*= SMSIMDIR-based default*/, const TVector3& position = TVector3(0, 0, 0))
{
    std::string resolvedInput = inputFile;
    if (resolvedInput.empty()) {
        const char* smsDir = getenv("SMSIMDIR");
        if (!smsDir) {
            std::cerr << "Environment variable SMSIMDIR is not set and no inputFile provided" << std::endl;
            return;
        }
        resolvedInput = std::string(smsDir) + "/inputdat/pol/y_pol/phi_fixed/d+Sn124E190g050ynp/dbreakb09.dat";
    }

    //get the inputfile dir (last two)


    // 根据输入文件路径生成输出文件夹和文件名
    // 例如输入: /home/tbt/workspace/dpol/smsimulator5.5/inputdat/pol/y_pol/phi_fixed/d+Sn124E190g050ynp/dbreakb09.dat
    // 输出: /home/tbt/workspace/dpol/smsimulator5.5/d_work/rootfiles/d+Sn124E190g050ynp/dbreakb09.root

    // 获取父目录名（最后一级文件夹）
    size_t lastSlash = inputFile.find_last_of('/');
    size_t secondLastSlash = inputFile.find_last_of('/', lastSlash - 1);
    std::string folderName = inputFile.substr(secondLastSlash + 1, lastSlash - secondLastSlash - 1);

    // 获取文件名并替换扩展名为 .root
    std::string fileName = inputFile.substr(lastSlash + 1);
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos != std::string::npos) {
        fileName = fileName.substr(0, dotPos) + ".root";
    } else {
        fileName += ".root";
    }

    // 拼接输出目录和文件名
    const char* smsDir = getenv("SMSIMDIR");
    std::string outDir;
    if (smsDir) outDir = std::string(smsDir) + "/data/simulation/g4input/" + folderName + "/";
    else outDir = "/data/simulation/g4input/" + folderName + "/"; // fallback
    std::string outFile = outDir + fileName;


    // 创建输出文件夹
    system(("mkdir -p " + outDir).c_str());

    // 打开输入文件
    std::ifstream fin(inputFile);
    if (!fin.is_open()) {
        std::cerr << "Cannot open input file: " << inputFile << std::endl;
        return;
    }

    // 创建ROOT文件和树
    TFile *f = new TFile(outFile.c_str(), "RECREATE");
    TTree *tree = new TTree("tree", "Input tree for simultaneous n-p");
    
    // 创建TBeamSimDataArray用于存储多粒子
    gBeamSimDataArray = new TBeamSimDataArray();
    tree->Branch("TBeamSimData", &gBeamSimDataArray);

    std::string line;
    int eventCount = 0;
    
    // 跳过前两行表头
    std::getline(fin, line);
    std::getline(fin, line);
    
    std::cout << "开始转换数据..." << std::endl;

    while (std::getline(fin, line)) {
        std::istringstream iss(line);
        int no;
        double pxp, pyp, pzp, pxn, pyn, pzn;
        if (!(iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn)) continue;

        // 清空数组准备新事件
        gBeamSimDataArray->clear();
        
        // 使用传入的position参数
        // TVector3 position(0, 0, 0); // mm, 束流起始位置 ？ 获取靶点位置 以后补全

        // 质子数据 (Z=1, A=1)
        double Mp = 938.27; // MeV, 质子静止质量
        double Ep = sqrt(pxp*pxp + pyp*pyp + pzp*pzp + Mp*Mp);
        TLorentzVector momentum_p(pxp, pyp, pzp, Ep);
        
        TBeamSimData proton(1, 1, momentum_p, position);
        proton.fParticleName = "proton";
        proton.fPrimaryParticleID = 0; // 质子ID为0
        proton.fTime = 0.0;
        proton.fIsAccepted = true;
        gBeamSimDataArray->push_back(proton);

        // 中子数据 (Z=0, A=1)  
        double Mn = 939.57; // MeV, 中子静止质量
        double En = sqrt(pxn*pxn + pyn*pyn + pzn*pzn + Mn*Mn);
        TLorentzVector momentum_n(pxn, pyn, pzn, En);
        
        TBeamSimData neutron(0, 1, momentum_n, position);
        neutron.fParticleName = "neutron";
        neutron.fPrimaryParticleID = 1; // 中子ID为1
        neutron.fTime = 0.0;
        neutron.fIsAccepted = true;
        gBeamSimDataArray->push_back(neutron);

        // 填充树（一个事件包含质子和中子）
        tree->Fill();
        eventCount++;
        
        if (eventCount % 10000 == 0) {
            std::cout << "已处理 " << eventCount << " 个事件..." << std::endl;
        }
    }

    // 写入并关闭文件
    f->Write();
    f->Close();
    fin.close();

    std::cout << "转换完成！" << std::endl;
    std::cout << "总共处理了 " << eventCount << " 个事件" << std::endl;
    std::cout << "每个事件包含1个质子和1个中子" << std::endl;
    std::cout << "输出文件：" << outFile << std::endl;
}
