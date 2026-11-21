#pragma once
#include <cstdint>
#include <vector>
#include "core/math.h"

// Formas analiticas das construcoes (as mesmas que o MeshBuilder desenha), para tracar raios contra
// o vilarejo real no shader: uma "formula de distancia" da cena.
enum class TipoForma : uint8_t { Caixa, Prisma, Cilindro };
struct FormaCena {
    TipoForma tipo;
    Vec3f centro;
    Vec3f meia;   // meias-extensoes locais; cilindro: (raio, meia altura, raio)
    float rotY;   // como o MeshBuilder: girarY
    Rgb cor;
};
// Distancia com sinal (< 0 dentro). Caixa e cilindro exatos; prisma: limite inferior (nunca maior que a real).
float distanciaForma(const FormaCena& f, Vec3f p);
float distanciaCena(const std::vector<FormaCena>& fs, Vec3f p);
// Indices das formas a ate "raio" do foco, ordenados pela distancia ate a forma (no maximo "max").
std::vector<int> formasPerto(const std::vector<FormaCena>& fs, Vec3f foco, float raio, int max);
