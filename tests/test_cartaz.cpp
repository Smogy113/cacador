#include <doctest/doctest.h>

#include <cmath>
#include <initializer_list>

#include "core/cartaz.h"

// O menu inicial é um cartaz pregado e levemente torto: o desenho e o mouse usam esta geometria.

TEST_CASE("cartaz: cabe na tela") {
    const float telas[][2] = {{1280, 720}, {1920, 1080}, {3840, 2160}, {800, 900}};
    for (const auto& t : telas) {
        CAPTURE(t[0]);
        CAPTURE(t[1]);
        for (float tempo : {0.f, 1.5f, 3.f}) {
            const Cartaz c = cartazNaTela(t[0], t[1], tempo);
            for (Vec2 canto : {noCartaz(c, 0, 0), noCartaz(c, CARTAZ_LARGURA, 0), noCartaz(c, 0, CARTAZ_ALTURA),
                               noCartaz(c, CARTAZ_LARGURA, CARTAZ_ALTURA)}) {
                CHECK(canto.x >= 0.f);
                CHECK(canto.x <= t[0]);
                CHECK(canto.y >= 0.f);
                CHECK(canto.y <= t[1]);
            }
            CHECK(escalaDoCartaz(c) > 0.f);
        }
    }
}

TEST_CASE("cartaz: ida e volta entre a tela e o papel") {
    const Cartaz c = cartazNaTela(1920, 1080, 0.f);
    for (Vec2 p : {Vec2{0, 0}, Vec2{320, 450}, Vec2{600, 820}, Vec2{40, 700}}) {
        const Vec2 tela = noCartaz(c, p.x, p.y);
        const Vec2 volta = paraCartaz(c, tela.x, tela.y);
        CHECK(volta.x == doctest::Approx(p.x).epsilon(.0001));
        CHECK(volta.y == doctest::Approx(p.y).epsilon(.0001));
    }
}

TEST_CASE("cartaz: torto de leve e balanco pequeno") {
    for (float tempo = 0.f; tempo <= 20.f; tempo += .25f) {
        const Cartaz c = cartazNaTela(1280, 720, tempo);
        CHECK(std::abs(c.angulo + 2.f) <= .36f);
    }
    // o giro aparece no clique: o canto de baixo fica deslocado em x em relação ao de cima
    const Cartaz c = cartazNaTela(1280, 720, 0.f);
    CHECK(std::abs(noCartaz(c, 0, CARTAZ_ALTURA).x - noCartaz(c, 0, 0).x) > 5.f);
}
