#!/usr/bin/env python3
"""Gera a malha humana compacta usada pelo jogo.

Entrada esperada: base.obj e um target do MakeHuman hm08. Os dois sao assets
CC0; este script nao usa nem incorpora codigo do MakeHuman. A saida contem so
posicoes quantizadas e indices triangulados, pronta para inclusao no binario.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def ler_base(caminho: Path) -> tuple[list[list[float]], list[list[int]]]:
    vertices: list[list[float]] = []
    faces: list[list[int]] = []
    grupo = ""
    with caminho.open(encoding="utf-8") as arq:
        for linha in arq:
            if linha.startswith("v "):
                vertices.append([float(v) for v in linha.split()[1:4]])
            elif linha.startswith("g "):
                grupo = linha.split(maxsplit=1)[1].strip()
            elif linha.startswith("f ") and grupo == "body":
                faces.append([int(v.split("/")[0]) - 1 for v in linha.split()[1:]])
    if len(vertices) < 13380 or not faces:
        raise ValueError("base hm08 invalida ou grupo 'body' ausente")
    usados = {i for face in faces for i in face}
    if usados != set(range(len(usados))):
        raise ValueError("o grupo body deixou de ocupar o prefixo continuo esperado")
    return vertices[: len(usados)], faces


def aplicar_target(vertices: list[list[float]], caminho: Path, peso: float) -> None:
    with caminho.open(encoding="utf-8") as arq:
        for linha in arq:
            if not linha or linha[0] == "#":
                continue
            partes = linha.split()
            if len(partes) != 4:
                continue
            i = int(partes[0])
            if i >= len(vertices):
                continue
            for eixo in range(3):
                vertices[i][eixo] += float(partes[eixo + 1]) * peso


def normalizar(vertices: list[list[float]], altura: float) -> None:
    min_y = min(v[1] for v in vertices)
    max_y = max(v[1] for v in vertices)
    escala = altura / (max_y - min_y)
    centro_x = (min(v[0] for v in vertices) + max(v[0] for v in vertices)) * 0.5
    for v in vertices:
        v[0] = (v[0] - centro_x) * escala
        v[1] = (v[1] - min_y) * escala
        v[2] *= escala


def triangular(faces: list[list[int]]) -> list[int]:
    indices: list[int] = []
    for face in faces:
        if len(face) < 3:
            continue
        for i in range(1, len(face) - 1):
            indices.extend((face[0], face[i], face[i + 1]))
    return indices


def escrever_inc(caminho: Path, vertices: list[list[float]], indices: list[int]) -> None:
    quantizados = [round(c * 10000.0) for v in vertices for c in v]
    if any(not -32768 <= q <= 32767 for q in quantizados):
        raise ValueError("vertice fora do alcance da quantizacao de 0,1 mm")
    with caminho.open("w", encoding="utf-8") as arq:
        arq.write("// Gerado por tools/generate_human_base.py a partir de assets MakeHuman hm08 CC0.\n")
        arq.write("// Nao editar: posicoes XYZ em unidades de 0,1 mm e triangulos uint16.\n")
        arq.write(f"constexpr uint32_t HUMANO_VERTICES = {len(vertices)};\n")
        arq.write(f"constexpr uint32_t HUMANO_INDICES = {len(indices)};\n")
        arq.write("constexpr int16_t HUMANO_POS[] = {\n")
        for i in range(0, len(quantizados), 18):
            arq.write("    " + ",".join(str(q) for q in quantizados[i : i + 18]) + ",\n")
        arq.write("};\nconstexpr uint16_t HUMANO_IDX[] = {\n")
        for i in range(0, len(indices), 24):
            arq.write("    " + ",".join(str(q) for q in indices[i : i + 24]) + ",\n")
        arq.write("};\n")


def escrever_obj(caminho: Path, vertices: list[list[float]], faces: list[list[int]]) -> None:
    with caminho.open("w", encoding="utf-8") as arq:
        arq.write("# Previa da base humana CC0 normalizada para o jogo\n")
        for v in vertices:
            arq.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")
        for face in faces:
            arq.write("f " + " ".join(str(i + 1) for i in face) + "\n")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("base", type=Path, help="makehuman/data/3dobjs/base.obj")
    ap.add_argument("target", type=Path, help="target macro masculino jovem hm08")
    ap.add_argument("saida", type=Path, help="arquivo .inc gerado")
    ap.add_argument("--peso", type=float, default=1.0)
    ap.add_argument("--target-extra", action="append", default=[], metavar="ARQUIVO:PESO",
                    help="target adicional e seu peso (pode ser repetido)")
    ap.add_argument("--altura", type=float, default=1.80)
    ap.add_argument("--obj", type=Path, help="tambem grava uma previa OBJ")
    op = ap.parse_args()

    vertices, faces = ler_base(op.base)
    aplicar_target(vertices, op.target, op.peso)
    for extra in op.target_extra:
        nome, peso = extra.rsplit(":", 1)
        aplicar_target(vertices, Path(nome), float(peso))
    normalizar(vertices, op.altura)
    indices = triangular(faces)
    op.saida.parent.mkdir(parents=True, exist_ok=True)
    escrever_inc(op.saida, vertices, indices)
    if op.obj:
        escrever_obj(op.obj, vertices, faces)
    print(f"{len(vertices)} vertices, {len(indices) // 3} triangulos")


if __name__ == "__main__":
    main()
