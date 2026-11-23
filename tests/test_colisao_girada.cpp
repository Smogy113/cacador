#include <doctest/doctest.h>
#include <cmath>
#include "core/collision.h"

static Vec3f girar(Vec3f p, float a) {  // como rotY/girarY
    float c = std::cos(a), s = std::sin(a);
    return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
}

TEST_CASE("colisao girada: empurra para fora pela face mais proxima") {
    MundoColisao m;
    CaixaGirada w{{10, 0, 10}, 2.f, 0.1f, 0.6f, 0.f, 3.f};  // parede 4 x 0,2
    m.adicionarGirada(w);
    Vec3f n = girar({0, 0, 1}, 0.6f);
    Vec3f p = w.centro + n * 0.25f;  // encostado (raio 0,3)
    Vec3f r = m.resolver(p, 0.3f);
    float dist = dot(r - w.centro, n);
    CHECK(dist >= 0.1f + 0.3f - 1e-3f);
    // centro dentro da parede: sai pelo lado mais perto
    Vec3f r2 = m.resolver(w.centro + n * 0.05f, 0.3f);
    CHECK(dot(r2 - w.centro, n) >= 0.4f - 1e-3f);
}
TEST_CASE("colisao girada: vao entre duas paredes deixa passar") {
    MundoColisao m;
    float a = 0.9f;
    Vec3f u = girar({1, 0, 0}, a);
    // duas paredes de 2 m com um vao de 1,0 m entre elas (porta)
    m.adicionarGirada({u * -1.5f, 1.f, 0.1f, a, 0.f, 3.f});
    m.adicionarGirada({u * 1.5f, 1.f, 0.1f, a, 0.f, 3.f});
    Vec3f n = girar({0, 0, 1}, a);
    for (float t = -1.f; t <= 1.f; t += 0.1f) {
        Vec3f p = n * t;  // atravessando pelo meio do vao
        Vec3f r = m.resolver(p, 0.3f);
        CHECK(len(r - p) < 1e-4f);
    }
}
TEST_CASE("colisao girada: raio bloqueado e livre") {
    MundoColisao m;
    m.adicionarGirada({{0, 0, 0}, 2.f, 0.1f, 0.7f, 0.f, 3.f});
    Vec3f n = girar({0, 0, 1}, 0.7f);
    CHECK_FALSE(m.raioLivre(n * -2.f + Vec3f{0, 1, 0}, n * 2.f + Vec3f{0, 1, 0}));
    CHECK(m.raioLivre(n * -2.f + Vec3f{0, 3.5f, 0}, n * 2.f + Vec3f{0, 3.5f, 0}));   // por cima
    Vec3f u = girar({1, 0, 0}, 0.7f);
    CHECK(m.raioLivre(u * 3.f + n * -2.f + Vec3f{0, 1, 0}, u * 3.f + n * 2.f + Vec3f{0, 1, 0}));  // ao lado
}
TEST_CASE("colisao girada: fora da faixa de altura nao bloqueia") {
    MundoColisao m;
    m.adicionarGirada({{0, 0, 0}, 2.f, 0.1f, 0.3f, 2.5f, 3.f});  // verga acima da porta
    Vec3f p{0, 0, 0.05f};
    CHECK(len(m.resolver(p, 0.3f) - p) < 1e-4f);  // passa por baixo
}
TEST_CASE("piso girado: so dentro do retangulo girado") {
    MundoColisao m;
    m.adicionarPisoGirado({{0, 0.35f, 0}, 3.f, 2.f, 0.785f, 0.f, 0.35f});
    CHECK(m.alturaPiso(0, 0, 0.f) == doctest::Approx(0.35f));
    CHECK(m.alturaPiso(2.9f, 1.9f, 0.f) < -1e8f);  // canto da caixa alinhada, fora do girado
    CHECK(m.alturaPiso(0, 0, -1.f) < -1e8f);       // alto demais para subir num passo
}
TEST_CASE("piso inclinado conserva largura e altura locais após a rotação") {
    MundoColisao m;
    Vec3f centro{13.f, 0, -18.f};
    constexpr float rot = .71f, meiaLargura = medidas::PONTE_LARGURA * .5f;
    m.adicionarPisoGirado({centro, meiaLargura, 2.f, rot, 3.8f, 4.f}, .2f);
    auto piso = [&](float x, float z, float y) {
        Vec3f p = centro + rotY({x, 0, z}, rot);
        return m.alturaPiso(p.x, p.z, y);
    };
    CHECK(piso(0.f, -1.5f, 4.f) == doctest::Approx(3.7f));
    CHECK(piso(meiaLargura - .01f, 1.5f, 4.f) == doctest::Approx(4.3f));
    CHECK(piso(meiaLargura + .01f, 1.5f, 4.f) < -1e8f);
    CHECK(piso(0.f, 1.5f, 3.f) < -1e8f);
}
