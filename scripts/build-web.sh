#!/usr/bin/env bash
# Gera build-web/index.html + .wasm (versao de teste no navegador)
set -euo pipefail
cd "$(dirname "$0")/.."
source "$HOME/emsdk/emsdk_env.sh" > /dev/null 2>&1
emcmake cmake -B build-web -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release > /tmp/cacador-web.log
cmake --build build-web -j8 --target cacador >> /tmp/cacador-web.log
ls -la build-web/index.html build-web/index.wasm
