#pragma once
#include "core/body.h"

// Cor da pele da cabeca esculpida num ponto da superficie (coordenadas da escultura, sdf::cabeca)
// e sua normal: a mesma usada nos vertices da malha e no volume desenhado por raymarching.
struct CorPele {
    Rgb cor;
    bool labio = false;
    float pelos = 0;      // densidade de pelo (sobrancelha, barba, linha do cabelo)
    float compPelo = 0;   // comprimento do pelo (m)
    Vec3f dirPelo{};      // direcao de crescimento; zero = sem pelo
    float aoMax = 1.f;    // fundo das narinas: limita a oclusao
};
CorPele corDaPele(Vec3f p, Vec3f n, const Visual& vis);
