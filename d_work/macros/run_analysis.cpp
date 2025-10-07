   #include "TFile.h"
   #include "TTree.h"
   #include "TClonesArray.h"
   #include "TVector3.h"
   #include "TString.h"
   #include <iostream>

   // Replace these with the real headers from your project
   #include "PDCRotation.h"
   #include "PDCSmearer.h"
   #include "TSimData.h"
   #include "TFragSimParameter.h"

   void run_analysis(const char* inputPath = "path/to/your/output.root",
                 double sigmaU = 0.2, double sigmaV = 0.2, double sigmaX = 0.2) {
      // 打开输入文件
      TFile* inFile = TFile::Open(inputPath);
      if (!inFile || inFile->IsZombie()) {
         std::cerr << "Error: cannot open file: " << inputPath << std::endl;
         return;
      }

      // 读取 tree 和参数对象
      TTree* inTree = dynamic_cast<TTree*>(inFile->Get("tree"));
      TFragSimParameter* fragPrm = dynamic_cast<TFragSimParameter*>(inFile->Get("FragParameter"));
      if (!inTree || !fragPrm) {
         std::cerr << "Error: missing tree or FragParameter in file." << std::endl;
         inFile->Close();
         delete inFile;
         return;
      }

      double pdcAngle = fragPrm->fPDCAngle;

      // 初始化工具
      PDCRotation rotation(pdcAngle);
      PDCSmearer smearer(rotation, sigmaU, sigmaV, sigmaX);

      // 设定分支
      TClonesArray* simDataArray = nullptr;
      if (inTree->GetBranch("FragSimData") == nullptr) {
         std::cerr << "Error: branch FragSimData not found." << std::endl;
         inFile->Close();
         delete inFile;
         return;
      }
      inTree->SetBranchAddress("FragSimData", &simDataArray);

      // 事件循环
      Long64_t nEntries = inTree->GetEntries();
      for (Long64_t i = 0; i < nEntries; ++i) {
         inTree->GetEntry(i);
         if (!simDataArray) continue;

         int nHits = simDataArray->GetEntriesFast();
         for (int j = 0; j < nHits; ++j) {
            TSimData* hit = dynamic_cast<TSimData*>(simDataArray->At(j));
            if (!hit) continue;

            // 比较 detector 名称（TString 支持 ==）
            TString detName = hit->fDetectorName;
            if (detName == "PDC1" || detName == "PDC2") {
               TVector3 originalPos = (hit->fPrePosition + hit->fPostPosition) * 0.5;
               TVector3 smearedPos = smearer.Smear(originalPos, hit->fModuleName);

               // 在此使用 smearedPos 进行后续处理（例如径迹重建）
               // std::cout << "Original X=" << originalPos.X() << "  Smeared X=" << smearedPos.X() << std::endl;
            }
         }
      }

      inFile->Close();
      delete inFile;
   }
