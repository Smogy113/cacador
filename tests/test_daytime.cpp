#include <doctest/doctest.h>
#include <cmath>
#include "core/daytime.h"

TEST_CASE("um dia dura 1200s") {
    DayClock c(0);
    c.avancar(600);
    CHECK(c.hora() == doctest::Approx(12));
    c.avancar(600);
    CHECK(c.hora() == doctest::Approx(0));
}
TEST_CASE("sol alto ao meio-dia, abaixo do horizonte à meia-noite") {
    DayClock m(12), n(0);
    CHECK(m.direcaoSol().y > 0.8f);
    CHECK(n.direcaoSol().y < 0);
}
TEST_CASE("fases") {
    CHECK(DayClock(12).fase() == Fase::Dia);
    CHECK(DayClock(18.5f).fase() == Fase::PorDoSol);
    CHECK(DayClock(23).fase() == Fase::Noite);
}
TEST_CASE("tochas") {
    CHECK(DayClock(12).intensidadeTochas() == 0);
    CHECK(DayClock(23).intensidadeTochas() == 1);
    float p = DayClock(18.5f).intensidadeTochas();
    CHECK(p > 0);
    CHECK(p < 1);
}
TEST_CASE("noite navegável: ambiente tem piso") {
    auto p = DayClock(2).paleta();
    CHECK(p.ambienteCeu.r + p.ambienteCeu.g + p.ambienteCeu.b > 0.03f);
}
TEST_CASE("paleta contínua: sem saltos entre minutos") {
    for (float h = 0; h < 24; h += 1 / 60.f) {
        auto a = DayClock(h).paleta(), b = DayClock(h + 1 / 60.f).paleta();
        CHECK(std::abs(a.neblina.r - b.neblina.r) < 0.02f);
    }
}
TEST_CASE("pôr do sol é quente") {
    auto p = DayClock(18.5f).paleta();
    CHECK(p.neblina.r > p.neblina.b);
}
