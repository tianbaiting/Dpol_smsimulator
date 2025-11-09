
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


方法 A: 传统 Make 构建
$ . setup.sh
$ cd smg4lib
$ make
$ cd ../simtrace
$ make
...（对 simdayone、get_pdgmass、crosssection 等同理）

(现在已经删除传统 Make 构建这种方式)

方法 B: CMake 构建（推荐）
$ . setup.sh
$ mkdir build && cd build
$ cmake ..
$ make -j$(nproc)

sim_deuteron 可执行文件将自动复制到 bin/ 目录


第二部 编译 pdcana

```bash
cd 

```
