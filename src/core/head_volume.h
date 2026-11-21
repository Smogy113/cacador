#pragma once
#include <cstdint>
#include <vector>
#include "core/body.h"

// A cabeca esculpida (sdf::cabeca) amostrada num bloco 3D, para ser desenhada pixel a pixel
// (raymarching) na placa de video. Duas grades: distancias (fina) e cor da pele (mais grossa).
// As fatias z ficam lado a lado num atlas 2D (textura comum: funciona sem texturas 3D).
struct CorVoxel { float r, g, b, a; };

struct VolumeCabeca {
    Vec3f lo, hi;             // caixa (coordenadas da escultura), igual para as duas grades
    int nx = 0, ny = 0, nz = 0;
    float h = 0;              // lado do voxel de distancia
    std::vector<float> dist;  // nx*ny*nz, x mais rapido; longe da superficie: valor conservador (nunca maior que o real)
    int cx = 0, cy = 0, cz = 0;
    float hc = 0;             // lado do voxel de cor
    // RGBA8: rgb = cor da pele (corDaPele); a = 0,5 pele lisa, 0,5 -> 1 labio, 0,5 -> 0 pelos
    std::vector<uint8_t> cor;
    int colunas = 0, linhas = 0;        // atlas de distancias: fatia k na coluna k % colunas, linha k / colunas
    int colunasCor = 0, linhasCor = 0;  // atlas de cor
    float amostra(Vec3f p) const;       // trilinear (fora da caixa: distancia ate a caixa + borda)
    CorVoxel amostraCor(Vec3f p) const; // trilinear, 0..1
};

VolumeCabeca gerarVolume(float h, float hCor, const Visual& vis);
std::vector<uint16_t> atlasMeiaPrecisao(const VolumeCabeca& v);  // distancias em meia precisao (half float)
std::vector<uint8_t> atlasCor(const VolumeCabeca& v);             // RGBA8
uint16_t floatParaMeia(float f);
float meiaParaFloat(uint16_t h);
