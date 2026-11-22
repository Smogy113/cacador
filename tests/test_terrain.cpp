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
TEST_CASE("capim responde ao poco, trilha e cerca") {
    RegioesCapim regioes;
    regioes.pocos.push_back({0, 0, 0});
    regioes.cercas.push_back({{20, 0, 0}, {20, 0, 12}});
    AmbienteCapim campo = ambienteCapim(40, 20, 0.f, 0.f, regioes);
    AmbienteCapim poco = ambienteCapim(3.f, 0.f, 0.f, 0.f, regioes);
    AmbienteCapim trilha = ambienteCapim(40, 20, 1.f, 0.f, regioes);
    AmbienteCapim cerca = ambienteCapim(20.7f, 6.f, 0.f, 0.f, regioes);
    CHECK(campo.densidade == doctest::Approx(1.f));
    CHECK(campo.altura == doctest::Approx(1.f));
    CHECK(poco.densidade < campo.densidade);
    CHECK(poco.altura < campo.altura * .5f);
    CHECK(poco.inclinacao > campo.inclinacao);
    CHECK(trilha.densidade < .1f);
    CHECK(trilha.altura < .3f);
    CHECK(cerca.altura > campo.altura * 1.6f);
    CHECK(cerca.densidade == doctest::Approx(campo.densidade));
    AmbienteCapim cercaNaTrilha = ambienteCapim(20.7f, 6.f, 1.f, 0.f, regioes);
    CHECK(cercaNaTrilha.altura == doctest::Approx(trilha.altura));
    CHECK(cercaNaTrilha.densidade == doctest::Approx(trilha.densidade));
}

TEST_CASE("capim filtra regioes sem perder cercas que cruzam blocos") {
    RegioesCapim regioes;
    regioes.pocos = {{-7, 0, 0}, {100, 0, 100}};
    regioes.cercas = {{{-20, 0, 4}, {20, 0, 4}}, {{100, 0, 100}, {120, 0, 100}}};
    RegioesCapim perto = regioes.proximas(0, 0, 8, 8);
    REQUIRE(perto.pocos.size() == 1);
    REQUIRE(perto.cercas.size() == 1);
    AmbienteCapim original = ambienteCapim(4, 4, 0.f, 0.f, regioes);
    AmbienteCapim filtrado = ambienteCapim(4, 4, 0.f, 0.f, perto);
    CHECK(filtrado.altura == doctest::Approx(original.altura));
    CHECK(filtrado.densidade == doctest::Approx(original.densidade));
}

TEST_CASE("margem umida escurece o capim sem afetar campo distante") {
    Mapa m;
    Terreno t(m, 1);
    int margem = 0;
    for (int x = -20; x <= 80; x += 5)
        for (int z = -170; z <= -80; z++) {
            float y = t.altura(float(x), float(z));
            if (t.mascaraRio(float(x), float(z)) < .05f && y > t.nivelAgua() + .4f &&
                t.umidadeMargem(float(x), float(z), y) > .1f) ++margem;
        }
    CHECK(margem > 10);
    CHECK(t.umidadeMargem(500, 500, t.altura(500, 500)) == 0.f);
    CHECK(t.umidadeMargem(60, -110, t.nivelAgua() + 4.f) == 0.f);
    Rgb seca{.4f, .5f, .2f};
    Rgb umida = corCapimUmido(seca, 1.f);
    CHECK(umida.r < seca.r);
    CHECK(umida.g < seca.g);
    CHECK(umida.b < seca.b);
    CHECK(corCapimUmido(seca, 0.f).g == doctest::Approx(seca.g));
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
