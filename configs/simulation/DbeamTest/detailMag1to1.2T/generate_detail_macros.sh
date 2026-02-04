#!/bin/bash
set -euo pipefail

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CXX=${CXX:-g++}
ROOT_CFLAGS=$(root-config --cflags)
ROOT_LIBS=$(root-config --libs)

INCLUDES="-I${SMSIMDIR}/libs/geo_accepentce/include -I${SMSIMDIR}/libs/analysis/include -I${SMSIMDIR}/libs/smlogger/include"
LIBS="-L${SMSIMDIR}/build/lib -lGeoAcceptance -lpdcanalysis -lsmlogger"
RPATH="-Wl,-rpath,${SMSIMDIR}/build/lib"

OUT_BIN="${SCRIPT_DIR}/generate_detail_macros"
SRC="${SCRIPT_DIR}/generate_detail_macros.cc"

${CXX} -std=c++20 -O2 ${ROOT_CFLAGS} ${INCLUDES} "${SRC}" -o "${OUT_BIN}" ${LIBS} ${ROOT_LIBS} ${RPATH}

export LD_LIBRARY_PATH="${SMSIMDIR}/build/lib:${LD_LIBRARY_PATH:-}"

"${OUT_BIN}" "${SCRIPT_DIR}"
