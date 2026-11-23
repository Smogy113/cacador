#!/usr/bin/env python3
"""Edita o banco de gravações do jogo e exporta os arquivos Ogg Vorbis.

Requer Python com NumPy e SciPy e FFmpeg com libvorbis. A pasta de entrada
deve conter as prévias de alta qualidade do Freesound, nomeadas <id>.mp3.
As fontes e os créditos estão em data/sons/ORIGEM.md.
"""

import argparse
from functools import lru_cache
from pathlib import Path
import subprocess

import numpy as np
from scipy.signal import butter, sosfiltfilt


TAXA = 32000
DESTINO = Path(__file__).resolve().parents[1] / "data/sons"


def quadros(segundos):
    return round(segundos * TAXA)


@lru_cache(maxsize=8)
def ler(fontes, numero, canais):
    resultado = subprocess.check_output([
        "ffmpeg", "-nostdin", "-v", "error", "-i", str(fontes / f"{numero}.mp3"),
        "-ac", str(canais), "-ar", str(TAXA), "-f", "f32le", "-",
    ])
    return np.frombuffer(resultado, dtype="<f4").reshape(-1, canais)


def cortar(fontes, numero, de, ate, canais=1):
    dados = ler(fontes, numero, canais)
    if not 0 <= quadros(de) < quadros(ate) <= len(dados):
        raise ValueError(f"Corte fora da gravação {numero} ({de} a {ate} s)")
    return dados[quadros(de):quadros(ate)].copy()


def filtrar(dados, graves, agudos):
    filtro = butter(2, (graves, agudos), "bandpass", fs=TAXA, output="sos")
    return sosfiltfilt(filtro, dados, axis=0).astype(np.float32)


def fades(dados, entrada, saida):
    n, m = quadros(entrada), quadros(saida)
    dados[:n] *= np.linspace(0, 1, n)[:, None]
    dados[-m:] *= np.linspace(1, 0, m)[:, None]
    return dados


def ajustar(dados, pico, rms=None):
    ganho = pico / max(1e-9, np.max(np.abs(dados)))
    if rms is not None:
        ganho = min(ganho, rms / max(1e-9, np.sqrt(np.mean(dados ** 2))))
    return dados * ganho


def exportar(pasta, nome, dados, qualidade=4):
    subprocess.run([
        "ffmpeg", "-nostdin", "-v", "error", "-y", "-f", "f32le",
        "-ac", str(dados.shape[1]), "-ar", str(TAXA), "-i", "-",
        "-c:a", "libvorbis", "-q:a", str(qualidade), "-fflags", "+bitexact", str(pasta / f"{nome}.ogg"),
    ], input=dados.astype("<f4").tobytes(), check=True)


def preparar(dados, graves, agudos, pico, entrada=.004, saida=.036):
    return ajustar(fades(filtrar(dados, graves, agudos), entrada, saida), pico)


def exportar_banco(pasta, nome, dados):
    for i, amostra in enumerate(dados, start=1):
        exportar(pasta, f"{nome}_{i:02d}", amostra)


def passos(fontes, pasta):
    # Cada marca corresponde ao ataque de uma pisada na prévia da gravação.
    superficies = [
        ("terra",   521588, [.111, .884, 1.586, 2.245], .29, 9500),
        ("capim",   521587, [.117, .814, 1.516, 2.284], .32, 9600),
        ("pedra",   521590, [.107, .696, 1.275, 2.403], .25, 10000),
        ("madeira", 521589, [.128, .546, 1.401, 2.261], .21, 8500),
        ("agua",    454623, [.735, 1.824, 3.158, 4.583], .34, 11300),
    ]
    for nome, numero, ataques, duracao, agudos in superficies:
        amostras = [
            preparar(cortar(fontes, numero, t - .040, t + duracao), 70, agudos, .64)
            for t in ataques
        ]
        exportar_banco(pasta, f"passo_{nome}", amostras)

    macios = [
        preparar(cortar(fontes, numero, t - .032, t + .13), 54, 7900, .72, saida=.027)
        for numero, t in [(684446, .343), (684446, .626), (813383, 1.647), (813383, 2.094), (813383, 2.946)]
    ]
    firmes = [
        preparar(cortar(fontes, 175356, t - .025, t + .12), 76, 10800, .72, saida=.027)
        for t in [.701, 1.195, 2.023, 2.544]
    ]
    molhados = []
    for casco, t in zip(macios, superficies[-1][2]):
        espirro = preparar(cortar(fontes, 454623, t - .040, t + .27), 105, 10800, .63)
        espirro *= .61
        espirro[:len(casco)] += casco * .77
        molhados.append(ajustar(espirro, .72))
    exportar_banco(pasta, "casco_terra", macios)
    exportar_banco(pasta, "casco_firme", firmes)
    exportar_banco(pasta, "casco_agua", molhados)


def disparos(fontes, pasta):
    campo = []
    sala = []
    longe = []
    abafado = []
    for i, t in enumerate([.248, 2.096, 4.046, 5.918]):
        tomada = cortar(fontes, 835200, t - .028, t + .80)
        campo.append(preparar(tomada, 48, 12500, .80, entrada=.002, saida=.15))

        # Reflexões curtas, amortecidas e assadas no arquivo. A primeira
        # pressão continua sendo a gravação, com a cauda de campo atenuada.
        fechado = filtrar(tomada, 49, 10200)
        atraso = filtrar(fades(fechado[:quadros(.11)].copy(), .002, .025), 115, 5300)
        inicio, fim = quadros(.067), quadros(.147)
        fechado[inicio:fim] *= np.linspace(1, .42, fim - inicio)[:, None]
        fechado[fim:] *= .42
        for tempo, ganho in [(.013, .38), (.029, .32), (.046, .28), (.073, .23), (.101, .16), (.149, .10), (.206, .066)]:
            k = quadros(tempo + i * .0009)
            fechado[k:k + len(atraso)] += ganho * atraso
        sala.append(ajustar(fades(fechado, .002, .13), .81))
        if i < 3:
            longe.append(preparar(tomada, 46, 2550 - i * 310, .71, entrada=.002, saida=.17))
            tomada_abafada = tomada if i < 2 else cortar(fontes, 803496, .028, .90)
            abafado.append(preparar(tomada_abafada, 43, 720 + i * 65, .66, entrada=.002, saida=.16))
    for nome, amostras in [("campo", campo), ("sala", sala), ("longe", longe), ("abafado", abafado)]:
        exportar_banco(pasta, f"tiro_{nome}", amostras)


def tempo(dados, fator):
    resultado = subprocess.check_output([
        "ffmpeg", "-nostdin", "-v", "error", "-f", "f32le", "-ar", str(TAXA),
        "-ac", "1", "-i", "-", "-af", f"atempo={fator}", "-f", "f32le", "-",
    ], input=dados.astype("<f4").tobytes())
    return np.frombuffer(resultado, dtype="<f4").reshape(-1, 1).copy()


def foley(fontes, pasta):
    recarga = tempo(cortar(fontes, 177863, .21, 3.35), 1.77)
    recarga2 = np.zeros((quadros(1.88), 1), dtype=np.float32)
    # Montagem dos eventos metálicos na janela de recarga de dois segundos.
    eventos = [
        (177863, .265, .56, .02, .53),
        (881371, 1.05, 1.26, .38, .40),
        (881371, 3.25, 3.44, .61, .35),
        (881371, 6.15, 6.31, .83, .47),
        (881371, 8.90, 9.13, 1.07, .42),
        (881371, 11.16, 11.38, 1.28, .37),
        (881371, 16.02, 16.35, 1.51, .72),
    ]
    for numero, de, ate, quando, pico in eventos:
        trecho = preparar(cortar(fontes, numero, de, ate), 105, 10900, pico, saida=.026)
        i = quadros(quando)
        recarga2[i:i + len(trecho)] += trecho
    exportar_banco(pasta, "recarga", [
        preparar(recarga, 95, 12000, .70),
        preparar(recarga2, 95, 12000, .70),
    ])
    movimento = [(842232, .33, .82), (842232, 1.09, 1.59), (776515, 2.69, 3.17)]
    batente = [(779881, 7.38, 7.79), (779881, 18.29, 18.73), (850566, 4.28, 4.75)]
    for nome, eventos in [("porta_movimento", movimento), ("porta_batente", batente)]:
        amostras = [
            preparar(cortar(fontes, numero, de, ate), 55, 9100, .66, saida=.065)
            for numero, de, ate in eventos
        ]
        exportar_banco(pasta, nome, amostras)


def paisagem(fontes, pasta):
    aves = [(693113, .24, 1.71), (693113, 4.14, 5.98), (693113, 8.14, 9.76),
            (795995, 2.23, 4.15), (795995, 8.62, 10.53)]
    exportar_banco(pasta, "ave", [
        preparar(cortar(fontes, numero, de, ate), 385, 7850, .58, entrada=.064, saida=.15)
        for numero, de, ate in aves
    ])
    for nome, numero, de, ate, banda, rms in [
        ("vento", 530908, 1.1, 39.6, (61, 5100), .068),
        ("insetos", 645329, 13.4, 47.5, (370, 7900), .062),
    ]:
        dados = filtrar(cortar(fontes, numero, de, ate, canais=2), *banda)
        n = quadros(1.6)
        ang = np.linspace(0, np.pi / 2, n)[:, None]
        emenda = dados[-n:] * np.cos(ang) + dados[:n] * np.sin(ang)
        loop = np.concatenate((dados[n:-n], emenda))
        exportar(pasta, nome, ajustar(loop, .51, rms), qualidade=1.7)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fontes", type=Path)
    parser.add_argument("--destino", type=Path, default=DESTINO)
    args = parser.parse_args()
    args.destino.mkdir(parents=True, exist_ok=True)
    for grupo in (passos, disparos, foley, paisagem):
        grupo(args.fontes, args.destino)


if __name__ == "__main__":
    main()
