# Building smsimulator with CMake

This document describes how to build the `smsimulator` project using CMake.

## 1. Prerequisites

Before building the project, you need to have the following software installed and configured on your system:

*   **ROOT:** A data analysis framework. Make sure the `ROOTSYS` environment variable is set and `thisroot.sh` is sourced.
*   **Geant4:** A toolkit for the simulation of the passage of particles through matter. Make sure the `G4INCLUDE` and `G4LIB` environment variables are set and `geant4.sh` is sourced.
*   **ANAROOT:** An analysis framework for SAMURAI experiments. Make sure the `TARTSYS` environment variable is set.
*   **Xerces-C:** A validating XML parser.
*   **CMake:** A cross-platform build system generator.

## 2. Environment Setup

Before running CMake, you need to set up the environment for the required dependencies. This is typically done by sourcing setup scripts provided by the respective software packages. For example:

```bash
source /path/to/root/bin/thisroot.sh
source /path/to/geant4/bin/geant4.sh
source /path/to/anaroot/bin/setup.sh
```

Please adjust the paths according to your system configuration. The `setup.sh` script in this project can be used as a reference.

## 3. Build Instructions

Once the environment is set up, you can build the project using the following steps:

1.  **Create a build directory:** It is recommended to create a separate build directory to keep the build files separate from the source code.

    ```bash
    mkdir build
    ```

2.  **Navigate to the build directory:**

    ```bash
    cd build
    ```

3.  **Run CMake:** Run CMake to configure the project and generate the build files.

    ```bash
    cmake ..
    ```

4.  **Build the project:** Use `make` to compile the source code and create the executables and libraries.

    ```bash
    make
    ```

After the build is complete, you will find the executables in the `build/sources/sim_deuteron` directory and the libraries in the `build/sources/smg4lib` subdirectories.
