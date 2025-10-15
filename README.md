readme.txt
16 Oct 2019 --- version 5.0 (compiled with Geant4.10.05.p01)
28 Dec 2017 --- version 4.0 (compiled with Geant4.9.6.p04)
18 Jul 2014 --- version 3.2 (compiled with Geant4.9.6.p02)

--------
摘要
--------
"smsimulator" 由 R. Tanaka、Y. Kondo 和 M. Yasuda 开发，用于模拟 SAMURAI 实验。从版本 5.0 起兼容 Geant4.10.X，4.X 或更早版本兼容 Geant4.9.X。从 3.0 版开始，实现了从 Geant4 步骤到可观测量（例如 TArtNEBULAPla）的转换。该版本需要 ANAROOT 库。simtrace（用于模拟带电粒子在 SAMURAI 磁场中的轨迹）也从此版本起包含在内。

当前 smsimulator 包含：
 - README: 本文件
 - setup.sh: 设置脚本
 - smg4lib: 用于为 SAMURAI 模拟创建库的主要文件
 - crosssection: 创建截面数据集，并用 gnuplot 绘图
 - get_pdgmass: 显示重离子 PDG 质量的简单程序
 - simtrace: 检查 SAMURAI 磁场中轨迹的简单示例
 - simdayone: 重离子 + 中子 的模拟示例
 - sim_samurai21: 包含 NeuLAND 的 28O 实验模拟示例
 - sim_tm1510: TM1510，SAMURAI 配合 DALI、NeuLAND、NEBULA
 - sim_dali: DALI
 - sim_s21dali: 用作对象的 MINOS 与 DALI 配合的 SAMURAI21 示例
 - sim_catana: CATANA

----------
使用方法
----------
(1) 安装 Geant4
smsimulator 5.x 兼容 Geant4.10.x。smsimulator 4.x 兼容 Geant4.9.x。（建议使用 Version4.9.6.p02 或更高版本以便进行中子模拟。）如果使用旧版本，除以下 (1.a) 外可能还需其他修改。详情见：
http://be.nucl.ap.titech.ac.jp/~ryuki/iroiro/geant4/  
（页面为日文）

(1.a) 修改 G4INCLCascade.cc 以避免致命中止（对 smsimulator 5.x 不必要）
---------------------------------------------------------------------------
    G4bool INCL::prepareReaction(const ParticleSpecies &projectileSpecies, const G4double kineticEnergy, const G4int A, const G4int Z) {
        if(A < 0 || A > 300 || Z < 1 || Z > 200) {
            ERROR("Unsupported target: A = " << A << " Z = " << Z << std::endl);
            ERROR("Target configuration rejected." << std::endl);
            return false;
        }

        //--> added to avoid abort
        if (projectileSpecies.theA==1 && A==1){
            return false;
        }
        //<--

        // Initialise the maximum universe radius
        initUniverseRadius(projectileSpecies, kineticEnergy, A, Z);
---------------------------------------------------------------------------

(1.b) 编译 Geant4

(2) 安装 ANAROOT
(2.a)
ANAROOT 可从 RIBFDAQ 网页下载：
http://ribf.riken.jp/RIBFDAQ/

(3) 编译 smsimulator
根据系统修改 smsimulator/setup.sh。

$ . setup.sh
$ cd smg4lib
$ make
$ cd ../simtrace
$ make
...（对 simdayone、get_pdgmass、crosssection 等同理）

(4) 运行 Geant4
将工作目录 "smsimulator/work" 复制到其他位置。（建议不要直接在 "smsimulator/work" 下工作，以便将来更新 smsimulator。）SAMURAI 磁体的场图可在以下下载：
http://ribf.riken.jp/SAMURAI/。在 work/xxxxx/g4mac/examples/ 中可以找到一些 Geant4 宏的示例。

- vis.mac
    可视化示例

- example_Pencil.mac
    铅笔束示例

- example_Gaus.mac
    具有位置和角度高斯分布的单能束示例

- example_Tree.mac
    用于三体相空间衰变的 Tree 输入示例

(4.a) NEBULA 的探测器几何由 smsimulator/simdayone/geometry/ 中的两个 csv 文件给出。
    - NEBULADetectors_xxx.csv : 每个探测器的位置
    - NEBULA_xxx.csv : 整个 NEBULA 系统的几何

同时包含了一些用于创建参数文件的 perl 脚本。使用示例：

$ ./CreatePara_NEBULAFull.pl > NEBULADetector_Full.csv

(4.b) 输出 root 文件
输出的 root 文件包含 Tree，其中有：
    - Geant4 步骤数据
    - 参数
    - 从 Geant4 步骤转换得到的可观测量
由于存储 Geant4 步骤会导致文件较大，可通过以下方式跳过存储 Geant4 步骤：
    - 在 geant4 宏中使用 /action/data/NEBULA/StoreSteps false
    - 或在代码中调用 NEBULASimDatainitializer::SetDataStore(false)。

(5) 分析 Geant4 输出
在 smsimulator/xxxx/macros/examples/ 中有分析 Geant4 输出的 ROOT 宏示例。

    - GenerateInputTree_PhaseSpaceDecay.cc
        用于为三体相空间衰变的模拟生成输入 tree 的示例。

    - analysis_example.cc
        分析 Geant4 输出的简单示例。

(5.a) 串扰（crosstalk）分析
如果要使用串扰分析示例，请执行以下操作。

(5.a.1) 修改 ANAROOT 数据类
在 TArtNEBULAPla.hh（ANAROOT 类）中添加以下行以支持 TClonesArray::Sort。

---------------------------------------------------------------------------
    // 覆盖用于基于 TAveSlw 排序的函数
public:
    Bool_t IsEqual(TObject *obj) const {return fTAveSlw == ((TArtNEBULAPla*)obj)->fTAveSlw;}
    Bool_t IsSortable() const {return kTRUE;}
    Int_t Compare(const TObject *obj) const{
        if (fTAveSlw < ((TArtNEBULAPla*)obj)->fTAveSlw) return -1;
        else if (fTAveSlw > ((TArtNEBULAPla*)obj)->fTAveSlw) return 1;
        else return 0;
    }
---------------------------------------------------------------------------

(5.a.2) 为 ROOT 制作库文件
在 smsimulator/work/smanalib/ 中制作库文件。
$ cd smsimulator/work/smanalib/
$ ./auto.sh
$ make
$ make install

修改 rootlogon.C 以加载该库并添加 include 路径。可在 smsimulator/work/sim_samurai21/macros/examples/analysis_crosstalk_example.cc 中找到使用 TArtCrosstalk_XXX 类的示例。

-----------
更新信息
-----------
- version 4.2
    - 在 TFragSimData 中添加了 TargetThickness

- version 3.2
    - 实现了 NeuLAND
    - 实现了串扰分析示例

- version 3.0
    - 合并了 simtrace
    - 实现了从 Geant4 步骤数据到 TArtNEBULAPla 的转换
    - 修改了数据类的定义
    - 支持在实验室内填充空气
    - 整理了 Messenger 的命令名称
    - BeamTypeDemocraticDecay 被移除（可通过 BeamTypeTree 实现）
    - 为未来使用准备了各个元素的 DetectorConstruction

-----
提示
-----
- 大量事件模拟
    为避免中止，在 g4mac/xxx.mac 中加入以下行：
    /control/suppressAbortion 1
    在 Geant4.10.x 中已有改进。

- 将代码更新以适配 smsimulator 5.x + Geant4.10.x。
    使用单位时，记得添加 "#include "G4SystemOfUnits.hh""。

- 通过 messenger 发送命令
    在 Geant4 交互模式下可通过以下方式查看已实现命令列表：
    Idle> ls /action/
    Idle> ls /samurai/

- 事件发生器
    - 可以通过 tree 向 smsimulator 输入任意分布。get_pdgmass 对获取重离子质量值很有用。

- 与 NeuLAND 一起为你的实验进行模拟
    - smsimulator3.2/sim_samurai21 提供了使用 NEBULA+NeuLAND 的 28O 实验示例。你可以复制该目录为 sim_samuraiXX 并根据你的实验设置进行修改。

- 你的串扰算法
    - 可以自行开发串扰算法。基类 TArtCrosstalkAnalysis 对此很有用。复制 TArtCrosstalkAnalysis_XXX.cc 与 .hh 并进行修改。别忘了在 smanalib/sources/smana_linkdef.hh 中添加一行以在 ROOT 中使用它。

- 你的数据类
    若要创建自定义数据类，请创建以下类：
    - TXXXSimData : 用于存储 Geant4 步骤的数据类
    - XXXSimDataInitializer : 初始化器（继承 SimDataInitializer）
    - XXXSimDataConverter : 从 Geant4 步骤转换到可观测量的转换器（继承 SimDataConverter）
    - XXXSD : 灵敏探测器类（继承 G4VSensitiveDetector）

    然后可通过 SimDataManager 控制它们。

-----
待办事项
-----
- 若干功能尚未包含
    - 重离子的分辨率
    - 靶中能量损失差异（对 Erel 分辨率是重要影响之一）

- 关于模拟器有效性/评估的文档。

