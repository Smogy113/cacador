#include <doctest/doctest.h>

#include "core/hair_physics.h"

TEST_CASE("cabelo: pontas atrasam na corrida, avancam na parada e descansam") {
    FisicaCabelo fisica;
    constexpr float dt = 1.f / 60.f;
    const Mat4 repouso = Mat4::id();
    for (int i = 0; i < 60; ++i) fisica.atualizar(dt, repouso, repouso, 1.f, {});

    Mat4 corpo{};
    for (int i = 1; i <= 18; ++i) {
        corpo = Mat4::trans({0, 0, .065f * i});
        fisica.atualizar(dt, corpo, corpo, 1.f, {});
    }
    CHECK(fisica.deformacao().ponta[2].z < -.025f);
    for (int i = 0; i < 4; ++i) fisica.atualizar(dt, corpo, corpo, 1.f, {});
    CHECK(fisica.deformacao().ponta[2].z > .015f);

    for (int i = 0; i < 120; ++i) fisica.atualizar(dt, corpo, corpo, 1.f, {});
    for (Vec3f p : fisica.deformacao().ponta) CHECK(len(p) < .012f);
}

TEST_CASE("cabelo: lados reagem separadamente ao giro e guias reiniciam no teleporte") {
    FisicaCabelo fisica;
    constexpr float dt = 1.f / 60.f;
    const Mat4 id = Mat4::id();
    fisica.atualizar(dt, id, id, 1.f, {});
    const Vec3f eixo{0, 1.6f, .05f};
    for (int i = 1; i <= 9; ++i) {
        Mat4 cabeca = Mat4::trans(eixo) * Mat4::rotY(i * .13f) * Mat4::trans(eixo * -1.f);
        fisica.atualizar(dt, cabeca, id, 1.f, {});
    }
    const auto& pontas = fisica.deformacao().ponta;
    CHECK(pontas[0].z > .015f);
    CHECK(pontas[1].z < -.015f);
    CHECK(pontas[2].x > .02f);

    Mat4 destino = Mat4::trans({42, 4, -30});
    fisica.atualizar(dt, destino, destino, 1.f, {});
    for (size_t i = 0; i < pontas.size(); ++i) {
        CHECK(pontas[i] == Vec3f{});
        CHECK(fisica.deformacao().meio[i] == Vec3f{});
    }
}

TEST_CASE("cabelo: subpassos limitam o desvio quando a taxa de quadros cai") {
    for (float dt : {1.f / 120.f, 1.f / 60.f, 1.f / 30.f, 1.f / 15.f}) {
        FisicaCabelo fisica;
        for (int i = 0; i < 80; ++i) {
            float tempo = i * dt;
            Mat4 movimento = Mat4::trans({.42f * std::sin(tempo * 12.f), 0, tempo * 3.3f}) *
                             Mat4::rotY(.65f * std::sin(tempo * 10.f));
            fisica.atualizar(dt, movimento, movimento, 1.f, {1.4f, 0, -.8f});
            for (Vec3f p : fisica.deformacao().ponta) CHECK(len(p) < .065f);
        }
    }
}
