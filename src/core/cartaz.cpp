#include "core/cartaz.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float GRAUS = 3.14159265f / 180.f;
constexpr float ANGULO_BASE = -2.f;       // torto, como quem pregou com pressa
constexpr float BALANCO = .35f;           // graus para cada lado
constexpr float PERIODO_BALANCO = 6.f;    // segundos
}  // namespace

Cartaz cartazNaTela(float larguraTela, float alturaTela, float tempo) {
    // ~86% da altura, mas nunca mais largo que metade da tela (janelas estreitas). A margem cobre a
    // folha girada: o canto de um papel torto sai um pouco do retângulo reto.
    const float proporcao = CARTAZ_LARGURA / CARTAZ_ALTURA;
    float altura = alturaTela * .86f;
    altura = std::min(altura, larguraTela * .52f / proporcao);
    const float largura = altura * proporcao;
    const float margem = altura * .06f;
    const float cx = std::max(larguraTela * .30f, largura * .5f + margem);
    const float angulo = ANGULO_BASE + BALANCO * std::sin(tempo * 2.f * 3.14159265f / PERIODO_BALANCO);
    return {cx, alturaTela * .5f, largura, altura, angulo};
}

float escalaDoCartaz(const Cartaz& c) { return c.altura / CARTAZ_ALTURA; }

Vec2 noCartaz(const Cartaz& c, float u, float v) {
    const float s = escalaDoCartaz(c);
    const float dx = (u - CARTAZ_LARGURA * .5f) * s, dy = (v - CARTAZ_ALTURA * .5f) * s;
    const float a = c.angulo * GRAUS, cs = std::cos(a), sn = std::sin(a);
    return {c.cx + dx * cs - dy * sn, c.cy + dx * sn + dy * cs};
}

Vec2 paraCartaz(const Cartaz& c, float x, float y) {
    const float s = escalaDoCartaz(c);
    const float dx = x - c.cx, dy = y - c.cy;
    const float a = c.angulo * GRAUS, cs = std::cos(a), sn = std::sin(a);
    return {(dx * cs + dy * sn) / s + CARTAZ_LARGURA * .5f, (-dx * sn + dy * cs) / s + CARTAZ_ALTURA * .5f};
}
