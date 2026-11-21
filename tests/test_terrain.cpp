#include <doctest/doctest.h>
#include <algorithm>
#include "core/noise.h"
#include "core/terrain.h"

TEST_CASE("ruido determinístico e limitado") {
    for (float x = -50; x < 50; x += 3.7f) {
        float v = ruido2D(x, x * 0.7f, 9);
        CHECK(v == ruido2D(x, x * 0.7f, 9));
        CHECK(v >= -1.f);
        CHECK(v <= 1.f);
    }
}
TEST_CASE("determinístico") {
    Mapa m;
    Terreno a(m, 42), b(m, 42);
    CHECK(a.altura(123, -45) == b.altura(123, -45));
}
TEST_CASE("borda é montanha") {
    Mapa m;
    Terreno t(m, 1);
    CHECK(t.altura(745, 0) > t.altura(0, 0) + 50);
}
TEST_CASE("plano onde tem lugar") {
    auto m = lerMapa("lugar cidade A 100 100 0 60\n");
    Terreno t(m, 1);
    float h = t.altura(100, 100);
    for (float d : {10.f, 30.f, 55.f}) CHECK(t.altura(100 + d, 100) == doctest::Approx(h).epsilon(0.02));
}
TEST_CASE("estrada entre lugares") {
    auto m = lerMapa("lugar cidade A -300 0 0 40\nlugar vilarejo B 300 0 0 40\nestrada A B\n");
    Terreno t(m, 1);
    int na = 0;
    for (float x = -250; x <= 250; x += 10) {
        float best = 0;
        for (float z = -30; z <= 30; z += 1) best = std::max(best, t.mascaraEstrada(x, z));
        if (best > 0.5f) na++;
    }
    CHECK(na == 51);
}
TEST_CASE("rio tem água") {
    Mapa m;
    Terreno t(m, 1);
    int agua = 0;
    for (float x = -700; x <= 700; x += 5)
        for (float z = -700; z <= 700; z += 5)
            if (t.altura(x, z) < t.nivelAgua()) agua++;
    CHECK(agua > 500);
}
TEST_CASE("normal aponta pra cima no plano") {
    auto m = lerMapa("lugar cidade A 0 0 0 60\n");
    Terreno t(m, 1);
    CHECK(t.normal(0, 0).y > 0.99f);
}
TEST_CASE("ponte do mapa real fica sobre o rio") {
    auto m = carregarMapa(DATA_DIR "/lugares.txt");
    Terreno t(m, 7);
    auto* p = m.achar("Ponte_Velha");
    REQUIRE(p);
    CHECK(t.mascaraRio(p->x, p->z) > 0.5f);
}
TEST_CASE("lugares do mapa real ficam em terra seca") {
    auto m = carregarMapa(DATA_DIR "/lugares.txt");
    Terreno t(m, 7);
    for (auto& l : m.lugares)
        if (l.tipo != TipoLugar::Ponte) CHECK_MESSAGE(t.altura(l.x, l.z) > t.nivelAgua() + 0.5f, l.nome);
}
