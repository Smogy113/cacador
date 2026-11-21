#!/usr/bin/env bash
# Uso: scripts/screenshot.sh saida.png [argumentos extras do jogo]
# Roda o jogo sem tela (Xvfb + Mesa por software), salva um PNG e confere que nao e uma cor so.
set -euo pipefail
cd "$(dirname "$0")/.."
SAIDA="$(realpath -m "$1")"; shift
rm -f "$SAIDA"
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" ./build/cacador --screenshot "$SAIDA" "$@" > /tmp/cacador-run.log 2>&1 || { tail -20 /tmp/cacador-run.log; exit 1; }
python3 - "$SAIDA" <<'PY'
import sys, zlib, struct
d = open(sys.argv[1], 'rb').read()
assert d[:8] == b'\x89PNG\r\n\x1a\n', 'nao e PNG'
i, idat, w = 8, b'', 0
while i < len(d):
    n = struct.unpack('>I', d[i:i+4])[0]; t = d[i+4:i+8]
    if t == b'IHDR': w = struct.unpack('>I', d[i+8:i+12])[0]
    if t == b'IDAT': idat += d[i+8:i+8+n]
    i += 12 + n
raw = zlib.decompress(idat)
amostra = set(raw[::997])
print('OK', sys.argv[1], 'valores distintos na amostra:', len(amostra))
assert len(amostra) > 4, 'imagem de uma cor so'
PY
