#!/usr/bin/env bash
# Gera build-win/cacador.exe (estatico, sem DLLs) + data/
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release > /tmp/cacador-win.log
cmake --build build-win -j8 --target cacador >> /tmp/cacador-win.log
x86_64-w64-mingw32-strip build-win/cacador.exe
rm -rf build-win/data && cp -r data build-win/data
ls -la build-win/cacador.exe
