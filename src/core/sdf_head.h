#pragma once
#include <cstdint>
#include <vector>
#include "core/math.h"

// Cabeca esculpida como volume (funcao de distancia com sinal, SDF): soma suave de formas
// anatomicas (cranio, testa, arcos, macas, maxila, mandibula, queixo, boca, labios, nariz,
// palpebras, orelhas, pescoco e seus musculos). Coordenadas em metros, homem de 1,80 m:
// x = esquerda do personagem, y = altura, z = frente.
namespace sdf {

// < 0 dentro da cabeca, > 0 fora
float cabeca(Vec3f p);
Vec3f gradiente(Vec3f p);  // normalizado (normal da superficie)
// so os labios (vermelhao): < 0 dentro; usado para pintar a boca exatamente onde ela foi esculpida
float labios(Vec3f p);
// narinas (< 0 dentro do corte); simetrica em x
float narina(Vec3f p);

// centro do globo ocular esquerdo (+x); o direito e espelhado
constexpr float OLHO_X = 0.0315f, OLHO_Y = 1.703f, OLHO_Z = 0.0785f, OLHO_R = 0.0122f;
constexpr float OLHO_RECUO = 0.0016f;  // o globo fica um pouco atras das palpebras
// abertura do olho (meia largura, meia altura em cima e embaixo)
constexpr float ABERT_W = 0.0148f, ABERT_HC = 0.0047f, ABERT_HB = 0.0043f, PALP_R = 0.0136f;

struct VertSup {
    Vec3f pos, normal;
    float ao;
};
struct MalhaSup {
    std::vector<VertSup> v;
    std::vector<uint32_t> idx;
};
// Malha da superficie (y >= yMin) com celulas de lado h (metros).
MalhaSup malhar(float h, float yMin = 1.49f);

}  // namespace sdf
