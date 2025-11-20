
```
smsimulator5.5 on  main [$?⇡] via △ v3.31.6 took 29s 
❯ tree -L 2       
.
├── AxisDefinition.eps
├── bin
│   └── sim_deuteron
├── build
│   ├── a.txt
│   ├── CMakeCache.txt
│   ├── CMakeFiles
│   ├── cmake_install.cmake
│   ├── Makefile
│   └── sources
├── CMakeLists.txt
├── doc
│   ├── aki_report.en.md
│   ├── assets
│   ├── BUILD_GUIDE.md
│   ├── cmake_tutorial_smsimulator.md
│   ├── DPOL_Analysis_Report.md
│   ├── HOW_TO_BUILD_AND_USE.md
│   ├── image-1.png
│   ├── image-2.png
│   ├── image.png
│   ├── MagneticField_README.md
│   ├── MagneticField_ROTATION_README.md
│   ├── MagneticField_SUMMARY.md
│   ├── PARTICLE_TRAJECTORY_SUMMARY.md
│   ├── pdcana.html
│   ├── pdcana_src.html
│   ├── smsimulator.pdf
│   └── zhuanhuan
├── d_work
│   ├── a.txt
│   ├── BATCH_RUN_README.md
│   ├── batch_run_ypol.py
│   ├── batch_run_ypol.sh
│   ├── compile_and_test.sh
│   ├── detector_geometry.gdml
│   ├── g4_00.wrl
│   ├── geometry
│   ├── macros
│   ├── macros_temp
│   ├── magnetic_field_test.root
│   ├── output_tree
│   ├── plots
│   ├── reconp_originnp
│   ├── rootfiles
│   ├── rootlogon.C
│   ├── simulation_image.mac
│   ├── simulation_interactive.mac
│   ├── simulation.mac
│   ├── simulation_vis.mac
│   ├── sources
│   ├── SUMMARY.md
│   ├── temp_dat_files
│   ├── test
│   └── vis.mac
├── QMDdata
│   └── rawdata
├── README
├── README.md
├── setup_debian.sh
├── setup_forcmake.sh
├── setup.sh
└── sources
    ├── CMakeLists.txt
    ├── sim_deuteron
    └── smg4lib
    ```

i wanna reconstruc the struture to



```
smsimulator/
├── CMakeLists.txt
├── README.md
├── setup.sh
├── cmake/
├── libs/
│   ├── smg4lib/
│   ├── sim_deuteron_core/
│   └── analysis/
├── apps/
│   ├── sim_deuteron/
│   ├── run_reconstruction/
│   └── tools/
├── configs/
│   ├── simulation/
│   ├── reconstruction/
│   └── batch/
├── data/
│   ├── input/
│   ├── magnetic_field/
│   ├── simulation/
│   └── reconstruction/
├── scripts/
├── tests/
├── docs/                   # 文档独立目录
└── results/               # .gitignore

```