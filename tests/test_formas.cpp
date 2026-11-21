#include <doctest/doctest.h>
#include <cmath>
#include "core/formas.h"
#include "core/meshbuilder.h"

static Vec3f girar(Vec3f p, float a) {  // mesmo giro do MeshBuilder
    float c = std::cos(a), s = std::sin(a);
    return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
}

TEST_CASE("formas: caixa girada") {
    FormaCena f{TipoForma::Caixa, {3, 2, -1}, {2, 1.5f, 1}, 0.7f, {}};
    for (int k = 0; k < 8; k++) {
        Vec3f c = f.centro + girar({(k & 1) ? 2.f : -2.f, (k & 2) ? 1.5f : -1.5f, (k & 4) ? 1.f : -1.f}, 0.7f);
        CHECK(std::abs(distanciaForma(f, c)) < 1e-4f);
    }
    Vec3f n = girar({0, 0, 1}, 0.7f);
    CHECK(distanciaForma(f, f.centro + n * 2.f) == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(distanciaForma(f, f.centro) == doctest::Approx(-1.f).epsilon(1e-4));
}
TEST_CASE("formas: prisma") {
    FormaCena f{TipoForma::Prisma, {0, 5, 0}, {3, 1, 2}, 0.3f, {}};
    Vec3f cume = f.centro + Vec3f{0, 1, 0};
    CHECK(std::abs(distanciaForma(f, cume)) < 1e-4f);
    float acima = distanciaForma(f, cume + Vec3f{0, 1, 0});
    CHECK(acima <= 1.f);
    CHECK(acima > 0.6f);
    CHECK(distanciaForma(f, f.centro + Vec3f{0, -0.5f, 0}) < 0.f);
}
TEST_CASE("formas: cilindro") {
    FormaCena f{TipoForma::Cilindro, {1, 2, 1}, {0.5f, 2, 0.5f}, 0, {}};
    CHECK(distanciaForma(f, {2.5f, 2, 1}) == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(distanciaForma(f, {1, 5, 1}) == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(distanciaForma(f, {1, 2, 1}) < 0.f);
}
TEST_CASE("formas: o meshbuilder registra o que desenha") {
    std::vector<FormaCena> fs;
    MeshBuilder m;
    m.registrar(&fs);
    m.caixa({1, 2, 3}, {4, 3, 2}, 0.7f, {0.5f, 0.5f, 0.5f});
    m.prisma({0, 6, 0}, {6, 2, 4}, 1.1f, {0.4f, 0.3f, 0.2f});
    m.cilindro({5, 0, 5}, 0.6f, 3.f, 8, {0.3f, 0.3f, 0.3f});
    REQUIRE(fs.size() == 3);
    CHECK(fs[1].cor.r == doctest::Approx(0.4f));
    int ruins = 0;
    for (auto& v : m.verts()) ruins += std::abs(distanciaCena(fs, v.pos)) > 0.002f;
    CHECK(ruins == 0);
}
TEST_CASE("formas: sem registrar nao muda nada") {
    std::vector<FormaCena> fs;
    MeshBuilder a, b;
    b.registrar(&fs);
    for (MeshBuilder* m : {&a, &b}) {
        m->caixa({1, 2, 3}, {4, 3, 2}, 0.7f, {0.5f, 0.5f, 0.5f});
        m->prisma({0, 6, 0}, {6, 2, 4}, 1.1f, {0.4f, 0.3f, 0.2f});
    }
    REQUIRE(a.verts().size() == b.verts().size());
    for (size_t i = 0; i < a.verts().size(); i++) CHECK(len(a.verts()[i].pos - b.verts()[i].pos) == 0.f);
}
