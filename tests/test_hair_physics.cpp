#include <doctest/doctest.h>
#include <algorithm>

#include "core/hair_physics.h"
#include "core/loft.h"

namespace {
PerfilCabelo perfilDeTeste() {
    PerfilCabelo perfil;
    perfil.guias[0].pontos = {{{ .071f, 1.748f, .113f}, { .103f, 1.686f, .093f}, { .117f, 1.638f, .061f}}};
    perfil.guias[1].pontos = {{{-.071f, 1.748f, .113f}, {-.103f, 1.686f, .093f}, {-.117f, 1.638f, .061f}}};
    perfil.guias[2].pontos = {{{ 0, 1.735f, -.054f}, {0, 1.654f, -.070f}, {0, 1.572f, -.075f}}};
    perfil.quantidade = 3;
    return perfil;
}
}  // namespace

TEST_CASE("cabelo: pontas atrasam na corrida, avancam na parada e descansam") {
    FisicaCabelo fisica(perfilDeTeste());
    constexpr float dt = 1.f / 60.f;
    const Mat4 repouso = Mat4::id();
    fisica.atualizar(dt, repouso, repouso, repouso, {});

    Mat4 corpo{};
    for (int i = 1; i <= 18; ++i) {
        corpo = Mat4::trans({0, 0, .065f * i});
        fisica.atualizar(dt, corpo, corpo, corpo, {});
    }
    CHECK(fisica.deformacao().ponta[2].z < -.025f);
    float avanco = 0.f;
    for (int i = 0; i < 12; ++i) {
        fisica.atualizar(dt, corpo, corpo, corpo, {});
        avanco = std::max(avanco, fisica.deformacao().ponta[2].z);
    }
    CHECK(avanco > .015f);

    for (int i = 0; i < 120; ++i) fisica.atualizar(dt, corpo, corpo, corpo, {});
    for (Vec3f p : fisica.deformacao().ponta) CHECK(len(p) < .012f);
}

TEST_CASE("cabelo: lados reagem separadamente ao giro e guias reiniciam no teleporte") {
    FisicaCabelo fisica(perfilDeTeste());
    constexpr float dt = 1.f / 60.f;
    const Mat4 id = Mat4::id();
    fisica.atualizar(dt, id, id, id, {});
    const Vec3f eixo{0, 1.6f, .05f};
    for (int i = 1; i <= 9; ++i) {
        Mat4 cabeca = Mat4::trans(eixo) * Mat4::rotY(i * .13f) * Mat4::trans(eixo * -1.f);
        fisica.atualizar(dt, cabeca, id, id, {});
    }
    const auto& pontas = fisica.deformacao().ponta;
    CHECK(pontas[0].z > .015f);
    CHECK(pontas[1].z < -.015f);
    CHECK(pontas[2].x > .02f);

    Mat4 destino = Mat4::trans({42, 4, -30});
    fisica.atualizar(dt, destino, destino, destino, {});
    for (size_t i = 0; i < pontas.size(); ++i) {
        CHECK(pontas[i] == Vec3f{});
        CHECK(fisica.deformacao().meio[i] == Vec3f{});
    }
}

TEST_CASE("cabelo: subpassos limitam o desvio quando a taxa de quadros cai") {
    for (float dt : {1.f / 120.f, 1.f / 60.f, 1.f / 30.f, 1.f / 15.f}) {
        FisicaCabelo fisica(perfilDeTeste());
        for (int i = 0; i < 80; ++i) {
            float tempo = i * dt;
            Mat4 movimento = Mat4::trans({.42f * std::sin(tempo * 12.f), 0, tempo * 3.3f}) *
                             Mat4::rotY(.65f * std::sin(tempo * 10.f));
            fisica.atualizar(dt, movimento, movimento, movimento, {1.4f, 0, -.8f});
            for (Vec3f p : fisica.deformacao().ponta) CHECK(len(p) < .105f);
        }
    }
}

TEST_CASE("cabelo: cortes com silhuetas diferentes geram o proprio perfil") {
    Visual visual;
    visual.semCabelo = true;
    const auto careca = corpoHumanoBase({}, visual);
    CHECK(careca.cabelo.quantidade == 0);

    visual.cabeloOndulado = true;
    const auto ondulado = corpoHumanoBase({}, visual);
    MalhaPele comprido;
    loft::cabeloComprido(comprido, 1.f, 1.f, {}, true);

    const MalhaPele* cortes[] = {&ondulado, &comprido};
    for (const auto* malha : cortes) {
        const auto& perfil = malha->cabelo;
        REQUIRE(perfil.quantidade > 0);
        REQUIRE(perfil.quantidade <= MAX_GUIAS_CABELO);
        size_t dinamicos = 0;
        for (const auto& v : malha->v) {
            if (v.guiaCabelo[0] == SEM_GUIA_CABELO) continue;
            ++dinamicos;
            REQUIRE(v.guiaCabelo[0] < perfil.quantidade);
            REQUIRE(v.guiaCabelo[1] < perfil.quantidade);
            REQUIRE(v.osso[0] == CABECA);
        }
        CHECK(dinamicos > 0);
    }
    CHECK(comprido.cabelo.guias[0].pontos[2].y < ondulado.cabelo.guias[0].pontos[2].y);
}

TEST_CASE("cabelo: mechas curtas e rigidas cedem menos ao mesmo vento") {
    auto gerarPerfil = [](float comprimento, float flex) {
        MalhaPele malha;
        loft::ConstrutorCabelo corte(malha, {0, 1.7f, .02f}, 1.f, 1.f);
        corte.fita({{{.1f, 1.76f, -.1f}, 0.f}, {{.1f, 1.76f - comprimento, -.1f}, flex}}, .01f, {}, 1.f);
        corte.finalizar();
        return malha.cabelo;
    };
    const auto rente = gerarPerfil(.01f, .10f);
    const auto curto = gerarPerfil(.055f, .42f);
    const auto longo = gerarPerfil(.24f, 1.f);
    CHECK(rente.quantidade == 0);
    REQUIRE(curto.quantidade == 1);
    REQUIRE(longo.quantidade == 1);
    FisicaCabelo fisicaCurta(curto), fisicaLonga(longo);
    const Mat4 id = Mat4::id();
    for (int i = 0; i < 60; ++i) {
        fisicaCurta.atualizar(1.f / 60.f, id, id, id, {4.f, 0, 0});
        fisicaLonga.atualizar(1.f / 60.f, id, id, id, {4.f, 0, 0});
    }
    CHECK(fisicaCurta.deformacao().ponta[0].x > 0.f);
    CHECK(fisicaLonga.deformacao().ponta[0].x > fisicaCurta.deformacao().ponta[0].x * 1.5f);
}

TEST_CASE("cabelo: a ponta desliza pelo colisor do ombro sem atravessa-lo") {
    PerfilCabelo perfil;
    perfil.quantidade = 1;
    perfil.guias[0].pontos = {{{.12f, 1.72f, .102f}, {.12f, 1.58f, .102f}, {.12f, 1.44f, .102f}}};
    perfil.colisores.ombroE = {-.17f, 1.44f, 0};
    perfil.colisores.ombroD = { .17f, 1.44f, 0};
    FisicaCabelo livre(perfil);
    perfil.colisores.raioOmbros = .09f;
    FisicaCabelo comColisao(perfil);

    const Mat4 id = Mat4::id();
    constexpr float raio = .09f;
    for (int i = 0; i < 90; ++i) {
        livre.atualizar(1.f / 60.f, id, id, id, {0, 0, -9.f});
        comColisao.atualizar(1.f / 60.f, id, id, id, {0, 0, -9.f});
        auto desvio = comColisao.deformacao().ponta[0];
        CHECK(len({0, desvio.y, .102f + desvio.z}) >= raio - 1e-5f);
    }
    CHECK(comColisao.deformacao().ponta[0].z > livre.deformacao().ponta[0].z + .02f);
}
