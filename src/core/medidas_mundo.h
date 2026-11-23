#pragma once

// No mundo, uma unidade linear equivale a um metro em todos os eixos.
namespace medidas {
inline constexpr int MEIO_TERRENO = 750, MEIO_AREA_JOGAVEL = 700;
inline constexpr float ALTURA_ADULTO = 1.80f, ALTURA_CACADOR = 1.82f;
inline constexpr float RAIO_ADULTO = .32f;

inline constexpr float PORTA_LARGURA = 1.04f, PORTA_ALTURA = 2.12f, PORTA_ESPESSURA = .05f;
inline constexpr float ESTRADA_LARGURA = 4.f, RIO_LARGURA = 18.f;
inline constexpr float PONTE_LARGURA = 4.2f, PONTE_GUARDA_CORPO = 1.05f;
inline constexpr float CAPIM_ALTURA_MIN = .16f, CAPIM_ALTURA_MAX = .40f;

struct Arvore {
    float altura, raioCopa, raioTronco, alturaTronco;
};
inline constexpr Arvore PINHEIRO{12.f, 2.45f, .23f, 3.05f};
inline constexpr Arvore CARVALHO{10.f, 3.25f, .34f, 4.1f};
}
