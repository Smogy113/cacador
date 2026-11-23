#!/usr/bin/env python3
"""Esculpe e exporta a pele do cavalo: +Y para cima, +Z para a frente, em metros.

Use Python 3.10+ com numpy, scipy, scikit-image e fast-simplification. Execute este
arquivo após editar as seções abaixo. As duas malhas de saída entram no executável.
"""

from io import StringIO
from pathlib import Path

import fast_simplification
import numpy as np
from scipy.interpolate import PchipInterpolator
from scipy.ndimage import gaussian_filter, map_coordinates
from skimage.measure import marching_cubes


PASSO = .008
RECORTE = ((-.46, .46), (.036, 2.28), (-1.08, 1.65))
SAIDA = Path(__file__).resolve().parents[1] / "src/core/horse_base_data.inc"


def perfil(x, t, v, secoes):
    """Distância aproximada a seções elípticas com frente e verso independentes.

    Cada linha define a altura ao longo do eixo, centro lateral, centro no perfil,
    semilargura, raio no sentido positivo do perfil e raio no sentido negativo.
    """
    medidas = np.asarray(secoes, dtype=np.float32)
    eixo = medidas[:, 0]
    altura = np.clip(t, eixo[0], eixo[-1])
    lateral, centro, rx, frente, verso = PchipInterpolator(eixo, medidas[:, 1:], axis=0)(
        altura
    ).astype(np.float32).transpose((3, 0, 1, 2))
    dv = v - centro
    rv = np.where(dv > 0, frente, verso)
    radial = (np.sqrt(((x - lateral) / rx) ** 2 + (dv / rv) ** 2) - 1) * np.minimum(rx, rv)
    return np.maximum(radial, np.maximum(eixo[0] - t, t - eixo[-1]))


def unir(a, b, raio):
    """Liga os volumes com uma transição arredondada de raio finito."""
    folga = np.maximum(raio - np.abs(a - b), 0)
    return np.minimum(a, b) - folga * folga / (4 * raio)


def elipsoide(x, t, v, centro, raios):
    a, b, c = centro
    rx, rt, rv = raios
    return (np.sqrt(((x - a) / rx) ** 2 + ((t - b) / rt) ** 2 + ((v - c) / rv) ** 2) - 1) * min(raios)


def pele(x, y, z):
    # As seções do tronco correm da garupa à frente do peito. O dorso é mais
    # raso atrás da cernelha e o volume das costelas desce no meio da barriga.
    tronco = perfil(x, z, y, [
        (-1.026, 0, 1.458, .009, .015, .024),
        (-.997,  0, 1.446, .106, .120, .120),
        (-.925,  0, 1.406, .224, .227, .236),
        (-.800,  0, 1.362, .278, .315, .310),
        (-.603,  0, 1.350, .297, .335, .345),
        (-.388,  0, 1.330, .300, .315, .380),
        (-.146,  0, 1.311, .336, .311, .400),
        ( .105,  0, 1.300, .341, .339, .401),
        ( .355,  0, 1.302, .308, .396, .402),
        ( .540,  0, 1.301, .261, .433, .387),
        ( .693,  0, 1.287, .212, .380, .366),
        ( .805,  0, 1.277, .159, .286, .307),
        ( .889,  0, 1.265, .092, .175, .170),
        ( .932,  0, 1.274, .011, .029, .020),
    ])
    pescoco = perfil(x, y, z, [
        (1.112, 0, .730, .060, .072, .099),
        (1.238, 0, .708, .167, .191, .245),
        (1.380, 0, .735, .212, .225, .335),
        (1.533, 0, .764, .190, .224, .386),
        (1.654, 0, .815, .161, .202, .389),
        (1.764, 0, .865, .147, .181, .346),
        (1.864, 0, .938, .126, .173, .303),
        (1.941, 0, 1.021, .109, .149, .263),
        (2.009, 0, 1.096, .101, .107, .184),
        (2.055, 0, 1.145, .076, .057, .088),
        (2.074, 0, 1.162, .033, .024, .029),
    ])
    corpo = unir(tronco, pescoco, .116)

    # Perfil da cabeça no eixo da nuca ao focinho. A mandíbula tem mais
    # profundidade atrás dos olhos e o focinho abre de novo nas narinas.
    t = -.8 * (y - 2.015) + .6 * (z - 1.170)
    v =  .6 * (y - 2.015) + .8 * (z - 1.170)
    cabeca = perfil(x, t, v, [
        (-.101, 0, -.003, .015, .019, .018),
        (-.066, 0, -.006, .066, .063, .071),
        ( .002, 0,  .000, .106, .085, .092),
        ( .098, 0, -.003, .118, .093, .124),
        ( .176, 0, -.011, .136, .091, .141),
        ( .249, 0, -.012, .119, .081, .136),
        ( .317, 0, -.004, .096, .076, .099),
        ( .403, 0,  .007, .070, .074, .073),
        ( .467, 0,  .006, .073, .074, .066),
        ( .519, 0,  .004, .087, .076, .068),
        ( .554, 0,  .001, .082, .065, .064),
        ( .584, 0, -.006, .060, .043, .049),
        ( .605, 0, -.015, .015, .014, .016),
    ])
    # Órbitas, aberturas nasais e separação entre os lábios.
    lado = np.abs(x)
    maxilar = elipsoide(lado, t, v, (.082, .243, -.061), (.053, .105, .086))
    sobrancelha = elipsoide(lado, t, v, (.092, .113, .047), (.031, .052, .037))
    cabeca = unir(unir(cabeca, maxilar, .032), sobrancelha, .027)
    orbita = elipsoide(lado, t, v, (.115, .147, .049), (.023, .037, .028))
    narina = elipsoide(lado, t, v + .38 * (t - .515), (.071, .515, .050), (.029, .031, .021))
    cabeca = -unir(-cabeca, orbita, .012)
    cabeca = -unir(-cabeca, narina, .007)
    labio = t - (.577 - .59 * np.maximum(-v, 0))
    cabeca += .009 * np.exp(-(labio / .009) ** 2) * np.clip((.054 - v) / .035, 0, 1)
    corpo = unir(corpo, cabeca, .067)

    orelhas = perfil(lado, y, z, [
        (2.002, .067, 1.155, .025, .027, .033),
        (2.065, .082, 1.151, .038, .039, .040),
        (2.126, .096, 1.162, .038, .036, .036),
        (2.168, .112, 1.172, .027, .025, .023),
        (2.202, .123, 1.181, .014, .014, .012),
        (2.218, .127, 1.184, .003, .004, .004),
    ])
    concha = elipsoide(lado - .30 * (y - 2.131), y, z - .22 * (y - 2.131),
                      (.101, 2.131, 1.197), (.027, .066, .033))
    corpo = unir(corpo, -unir(-orelhas, concha, .004), .024)

    # Membros dianteiros: escápula, cotovelo, carpo, canela e boleto.
    anteriores = perfil(lado, y, z, [
        ( .070, .211, .653, .035, .043, .042),
        ( .102, .214, .644, .041, .043, .039),
        ( .151, .213, .623, .044, .046, .049),
        ( .193, .213, .610, .054, .050, .058),
        ( .238, .215, .619, .044, .043, .044),
        ( .345, .216, .626, .035, .039, .036),
        ( .483, .215, .636, .042, .043, .040),
        ( .586, .216, .638, .058, .058, .051),
        ( .634, .215, .632, .063, .057, .058),
        ( .720, .213, .609, .062, .067, .067),
        ( .812, .210, .580, .069, .079, .076),
        ( .937, .211, .544, .083, .099, .107),
        (1.054, .213, .541, .102, .147, .118),
        (1.193, .213, .561, .117, .165, .140),
        (1.348, .202, .580, .106, .186, .146),
        (1.484, .178, .536, .093, .164, .138),
        (1.567, .153, .495, .056, .093, .094),
    ])
    # As coxas ficam dentro da garupa. O joelho avança antes do jarrete,
    # que recua e dá à perna posterior seu perfil angulado.
    posteriores = perfil(lado, y, z, [
        ( .071, .217, -.707, .034, .043, .044),
        ( .108, .219, -.719, .041, .045, .039),
        ( .159, .220, -.745, .044, .047, .048),
        ( .201, .220, -.766, .052, .056, .055),
        ( .254, .218, -.779, .040, .044, .047),
        ( .365, .217, -.808, .035, .042, .041),
        ( .483, .215, -.843, .046, .049, .051),
        ( .577, .215, -.858, .063, .060, .073),
        ( .630, .215, -.822, .068, .083, .080),
        ( .744, .215, -.738, .074, .100, .106),
        ( .863, .215, -.650, .086, .130, .130),
        ( .970, .213, -.595, .103, .151, .179),
        (1.083, .215, -.631, .124, .169, .220),
        (1.202, .213, -.703, .142, .198, .219),
        (1.331, .201, -.741, .145, .195, .204),
        (1.453, .178, -.757, .130, .192, .158),
        (1.554, .139, -.740, .092, .163, .101),
    ])
    corpo = unir(unir(corpo, anteriores, .088), posteriores, .102)

    # Volumes de superfície da escápula e do músculo glúteo. A prega do
    # flanco fica na transição entre as costelas e a coxa.
    dy, dz = y - 1.380, z - .506
    escapula = elipsoide(lado, .90 * dy - .436 * dz, .436 * dy + .90 * dz,
                         (.233, 0, 0), (.099, .280, .136))
    dy, dz = y - 1.364, z + .748
    gluteo = elipsoide(lado, .890 * dy - .456 * dz, .456 * dy + .890 * dz,
                       (.244, 0, 0), (.093, .252, .147))
    peitoral = elipsoide(lado, y, z, (.108, 1.234, .769), (.125, .218, .164))
    corpo = unir(unir(unir(corpo, escapula, .050), gluteo, .042), peitoral, .057)
    corpo += .005 * np.exp(-(x / .045) ** 2 - ((y - 1.225) / .126) ** 2 - ((z - .928) / .058) ** 2)
    dy, dz = y - 1.138, z + .428
    prega = elipsoide(lado, .90 * dy + .436 * dz, -.436 * dy + .90 * dz,
                      (.327, 0, 0), (.043, .174, .051))
    return -unir(-corpo, prega, .030)


def gerar_superficie():
    eixos = [np.arange(a, b + PASSO, PASSO, dtype=np.float32) for a, b in RECORTE]
    campo = gaussian_filter(pele(*np.ix_(*eixos)), sigma=.68)
    vertices, faces, _, _ = marching_cubes(campo, 0, gradient_direction="ascent", allow_degenerate=False)
    origem = np.array([eixo[0] for eixo in eixos])
    return campo, origem, vertices * PASSO + origem, faces


def simplificar(campo, origem, vertices, faces, limite):
    pontos, triangulos = fast_simplification.simplify(
        vertices.astype(np.float64), faces, target_count=limite, agg=5
    )
    # Normais da escultura preservam as curvas pequenas após a decimação.
    coordenadas = ((pontos - origem) / PASSO).T
    normais = np.column_stack([
        map_coordinates(np.gradient(campo, axis=eixo), coordenadas, order=1) for eixo in range(3)
    ])
    normais /= np.linalg.norm(normais, axis=1, keepdims=True)
    pos = np.rint(pontos * 10000).astype(np.int16)
    normais = np.rint(normais * 127).astype(np.int8)
    # Solda pontos muito próximos que caem na mesma coordenada de 0,1 mm.
    pos, primeiros, mapa = np.unique(pos, axis=0, return_index=True, return_inverse=True)
    normais = normais[primeiros]
    triangulos = mapa.reshape(-1)[triangulos]
    a, b, c = pos[triangulos].astype(np.int64).transpose((1, 0, 2))
    normais_faces = np.cross(b - a, c - a)
    ativos = np.any(normais_faces, axis=1)
    triangulos = triangulos[ativos]
    normais_faces = normais_faces[ativos]
    usados, indices = np.unique(triangulos, return_inverse=True)
    triangulos = indices.reshape(-1, 3)
    pos, normais = pos[usados], normais[usados]
    if np.sum(normais_faces * normais[triangulos].mean(axis=1)) < 0:
        triangulos = triangulos[:, ::-1]
    return pos, normais, triangulos


def projetar(campo, origem, centro, direcao, de, ate):
    """Encontra a saída da pele ao longo de cada raio da superfície de apoio."""
    centro, direcao = np.broadcast_arrays(centro, direcao)

    def distancia(t):
        q = centro + t[..., None] * direcao
        coords = np.moveaxis((q - origem) / PASSO, -1, 0)
        return map_coordinates(campo, coords, order=1, mode="constant", cval=1.0)

    baixo = np.full(centro.shape[:-1], de)
    alto = np.full(centro.shape[:-1], ate)
    if np.any(distancia(baixo) > 0) or np.any(distancia(alto) < 0):
        raise ValueError("Raio de detalhe fora da escultura")
    for _ in range(15):
        meio = (baixo + alto) * .5
        dentro = distancia(meio) < 0
        baixo = np.where(dentro, meio, baixo)
        alto = np.where(dentro, alto, meio)
    return centro + ((baixo + alto) * .5)[..., None] * direcao


def malha_crina(campo, origem):
    # u percorre a crista, v cai do lado esquerdo para o lado direito.
    u, v = np.meshgrid(np.linspace(0, 1, 41), np.linspace(0, 1, 11), indexing="ij")
    ondulacao = np.sin(13 * u + 4 * v) * .022 + np.sin(31 * u - 6 * v) * .014
    z = 1.116 - .682 * u + (ondulacao - .04 * np.sin(np.pi * u)) * v
    y = np.interp(u, [0, .19, .46, .73, 1], [1.882, 1.771, 1.632, 1.552, 1.521])
    comprimento = np.interp(u, [0, .13, .35, .57, .83, 1], [1.47, 1.97, 2.10, 2.28, 2.00, 1.15])
    pontas = .10 * np.sin(71 * u + .65) + .044 * np.sin(151 * u + 1.44)
    a = -.38 + (comprimento + pontas + .38) * v
    direcao = np.stack((np.sin(a), np.cos(a), np.zeros_like(a)), axis=-1)
    centro = np.stack((np.zeros_like(y), y, z), axis=-1)
    casca = projetar(campo, origem, centro, direcao, 0., .67)
    capa = np.sin(np.pi * u) ** .30 * np.sin(np.pi * v) ** .55
    fios = np.cos(97 * u + 1.7 * np.sin(8 * v + 8 * u))
    volume = .011 + (.021 + .004 * fios) * capa
    return casca + volume[..., None] * direcao


def malha_topete(campo, origem):
    u, v = np.meshgrid(np.linspace(0, 1, 15), np.linspace(0, 1, 9), indexing="ij")
    largura = np.interp(v, [0, .19, .55, .82, 1], [.044, .065, .053, .033, .010])
    x = (2 * u - 1) * largura + .051 * v ** 1.22 - .012
    t = -.029 + (.234 + .014 * np.cos(22 * u + .45)) * v
    centro = np.stack((x, 2.015 - .8 * t - .020, 1.170 + .6 * t - .0267), axis=-1)
    direcao = np.array([0., .6, .8])
    casca = projetar(campo, origem, centro, direcao, 0., .23)
    capa = np.sin(np.pi * u) ** .35 * np.sin(np.pi * (.18 + .82 * v)) ** .55
    volume = .007 + (.016 + .003 * np.cos(43 * u + 5 * v)) * capa
    return casca + volume[..., None] * direcao


def malha_manta(campo, origem):
    u, v = np.meshgrid(np.linspace(0, 1, 13), np.linspace(-1, 1, 17), indexing="ij")
    # Abas mais estreitas nos cantos, arredondadas sobre o casco das costelas.
    z = -.476 + .805 * u - .032 * v * v
    a = v * (1.13 + .24 * np.sin(np.pi * u) ** .55)
    centro = np.stack((np.zeros_like(a), np.full_like(a, 1.307), z), axis=-1)
    direcao = np.stack((np.sin(a), np.cos(a), np.zeros_like(a)), axis=-1)
    casca = projetar(campo, origem, centro, direcao, 0., .54)
    return casca + .014 * direcao


def malha_olho(campo, origem):
    # Só o lado +X é assado. O jogo espelha o olho para o outro lado.
    ang = np.linspace(0, 2 * np.pi, 16, endpoint=False)
    r = np.arange(1, 5, dtype=np.float32)[:, None] / 4
    u = np.r_[0, (r * np.cos(ang)).ravel()]
    v = np.r_[0, (r * np.sin(ang)).ravel()]
    plano = np.array([.099, 1.930, 1.301])
    frente = np.array([.83, .10, .55])
    frente /= np.linalg.norm(frente)
    eixo = np.array([-.55, -.30, .83])
    eixo -= np.dot(eixo, frente) * frente
    eixo /= np.linalg.norm(eixo)
    acima = np.cross(eixo, frente)
    centro = plano + .032 * u[:, None] * eixo + .012 * v[:, None] * acima
    casca = projetar(campo, origem, centro, frente, -.071, .098)
    # Abaula a córnea alguns milímetros. O contorno encosta nas pálpebras
    # esculpidas, o que dá profundidade à abertura mesmo com luz lateral.
    margem = np.clip(1 - np.hypot(u, v), 0, 1)
    return casca + (.006 * np.sqrt(margem) + .0015)[:, None] * frente


def escrever_array(arq, tipo, nome, dados, colunas):
    valores = dados.ravel().tolist()
    arq.write(f"constexpr {tipo} {nome}[] = {{\n")
    for i in range(0, len(valores), colunas):
        arq.write("    " + ",".join(map(str, valores[i:i + colunas])) + ",\n")
    arq.write("};\n")


def main():
    campo, origem, vertices, faces = gerar_superficie()
    arq = StringIO()
    arq.write("// Gerado por tools/generate_horse_base.py.\n")
    arq.write("// Posições em décimos de milímetro e normais em [-127,127].\n")
    for nome, limite in [("PERTO", 5020), ("LONGE", 1260)]:
        pos, normais, tris = simplificar(campo, origem, vertices, faces, limite)
        escrever_array(arq, "int16_t", nome + "_POS", pos, 18)
        escrever_array(arq, "int8_t", nome + "_NORMAL", normais, 18)
        escrever_array(arq, "uint16_t", nome + "_IDX", tris, 24)
        print(f"{nome}: {len(pos)} vértices, {len(tris)} triângulos")
    for nome, gerar in [("CRINA", malha_crina), ("TOPETE", malha_topete),
                        ("MANTA", malha_manta), ("OLHO", malha_olho)]:
        pontos = gerar(campo, origem)
        if pontos.ndim == 3:
            arq.write(f"constexpr int {nome}_SEC = {pontos.shape[0]}, {nome}_PONTOS = {pontos.shape[1]};\n")
        escrever_array(arq, "int16_t", nome + "_POS", np.rint(pontos * 10000).astype(np.int16), 18)
    SAIDA.write_text(arq.getvalue(), encoding="utf-8")

if __name__ == "__main__":
    main()
