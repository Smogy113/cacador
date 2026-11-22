#pragma once
#include <array>
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
    bool parede = false;  // caixa de parede/alicerce (tem luz guardada por face)
    int grupo = -1;       // predio a que pertence (-1: solta)
    bool grossa = false;  // volume grosso do predio (so para as faces dos vizinhos)
};
// formas que importam para a luz de um predio: as suas todas, dos outros predios so o volume grosso
std::vector<int> formasParaLuz(const std::vector<FormaCena>& fs, Vec3f foco, int grupo, float raio, int max);
// janela (acende a noite): centro e direcao para fora da parede
struct JanelaLuz {
    Vec3f pos, normal;
};
// Distancia com sinal (< 0 dentro). Caixa e cilindro exatos; prisma: limite inferior (nunca maior que a real).
float distanciaForma(const FormaCena& f, Vec3f p);
float distanciaCena(const std::vector<FormaCena>& fs, Vec3f p);
// Indices das formas a ate "raio" do foco, ordenados pela distancia ate a forma (no maximo "max").
std::vector<int> formasPerto(const std::vector<FormaCena>& fs, Vec3f foco, float raio, int max);

// Face vertical de uma caixa de parede: origem no canto de baixo, eixoU horizontal ao longo da face.
struct FaceParede {
    Vec3f origem, eixoU, normal;
    float largura, altura;
};
FaceParede faceDe(const FormaCena& caixa, int k);  // k: 0 +x, 1 -x, 2 +z, 3 -z (no espaco da caixa)
// Lugar de cada face num atlas quadrado (texels, com 1 texel de borda em volta); w == 0: sem lugar.
struct RetAtlas {
    int x = 0, y = 0, w = 0, h = 0;
};
std::vector<std::array<RetAtlas, 4>> alocarAtlas(const std::vector<FormaCena>& fs, float metrosPorTexel, int tam);
