// 生成smsimulator的root文件，np,分别fill

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <string>
#include <TFile.h>
#include <TTree.h>
#include "TBeamSimData.hh"

int TransDat2root() {
    const char* smsDir = getenv("SMSIMDIR");
    if (!smsDir) {
        std::cerr << "Environment variable SMSIMDIR is not set" << std::endl;
        return 1;
    }
    std::string smsBase(smsDir);
    // 输入文件路径
    std::string inputFile = smsBase + "/inputdat/unpol/d+Pb208E190g050xyz/dbreakb01.dat";
    // 输出文件夹路径
    std::string outDir = smsBase + "/d_work/rootfiles/d+Pb208E190g050xyz/";
    // 输出ROOT文件名
    std::string outFile = outDir + "dbreakb01_seperate.root";

    // 创建输出文件夹
    system((std::string("mkdir -p ") + outDir).c_str());

    // 打开输入文件
    std::ifstream fin(inputFile);
    if (!fin.is_open()) {
        std::cerr << "Cannot open input file: " << inputFile << std::endl;
        return 1;
    }

    // 打开ROOT文件
    TFile *f = new TFile(outFile.c_str(), "RECREATE");
    TTree *tree = new TTree("BeamSimTree", "BeamSimTree");

    TBeamSimData data;
    tree->Branch("BeamSimData", &data);

    std::string line;
    int lineCount = 0;
    // 跳过前两行表头
    std::getline(fin, line);
    std::getline(fin, line);

    while (std::getline(fin, line)) {
        std::istringstream iss(line);
        int no;
        double pxp, pyp, pzp, pxn, pyn, pzn;
        if (!(iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn)) continue;

        // 质子
        data.fParticleName = "proton";
        data.fZ = 1;
        data.fA = 1;
        data.fMomentum.SetPxPyPzE(pxp, pyp, pzp, 0);
        data.fPosition.SetXYZ(0, 0, 0);
        data.fTime = 0;
        data.fIsAccepted = true;
        tree->Fill();

        // 中子
        data.fParticleName = "neutron";
        data.fZ = 0;
        data.fA = 1;
        data.fMomentum.SetPxPyPzE(pxn, pyn, pzn, 0);
        data.fPosition.SetXYZ(0, 0, 0);
        data.fTime = 0;
        data.fIsAccepted = true;
        tree->Fill();

        lineCount += 2;
    }

    f->Write();
    f->Close();
    fin.close();

    std::cout << "转换完成，共写入 " << lineCount << " 条数据到 " << outFile << std::endl;
    return 0;
}