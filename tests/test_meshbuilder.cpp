#include <doctest/doctest.h>
#include <algorithm>
#include <limits>
#include "core/meshbuilder.h"

static float aoMaisProximo(const MeshBuilder& b, Vec3f p) {
    float melhor = std::numeric_limits<float>::max(), ao = -1;
    for (auto& v : b.verts()) {
        float d = len(v.pos - p);
        if (d < melhor) { melhor = d; ao = v.ao; }
    }
    return ao;
}

TEST_CASE("caixa tem 12 triângulos") {
    MeshBuilder b;
    b.caixa({0, 0, 0}, {1, 1, 1}, 0, {1, 1, 1});
    CHECK(b.triangulos() == 12);
}
TEST_CASE("normais unitárias") {
    MeshBuilder b;
    b.cilindro({0, 0, 0}, 1, 2, 8, {1, 1, 1});
    b.cone({3, 0, 0}, 1, 2, 6, {1, 1, 1});
    b.prisma({0, 3, 0}, {4, 1, 3}, 0.5f, {1, 1, 1});
    for (auto& v : b.verts()) CHECK(len(v.normal) == doctest::Approx(1));
}
TEST_CASE("normais apontam para fora da caixa") {
    MeshBuilder b;
    b.caixa({0, 0, 0}, {2, 2, 2}, 0.3f, {1, 1, 1});
    for (auto& v : b.verts()) CHECK(dot(v.normal, v.pos) > 0);
}
TEST_CASE("canto entre chão e parede é mais escuro que topo") {
    MeshBuilder b;
    b.caixa({0, 0.05f, 0}, {10, 0.1f, 10}, 0, {1, 1, 1});
    b.caixa({0, 1.1f, -4.5f}, {10, 2, 1}, 0, {1, 1, 1});
    b.calcularAO(nullptr);
    float canto = aoMaisProximo(b, {0, 0.1f, -3.95f}), aberto = aoMaisProximo(b, {0, 0.1f, 3});
    CHECK(canto < aberto - 0.2f);
}

TEST_CASE("tipo de material marca os vértices (chama, janela)") {
    MeshBuilder b;
    b.caixa({0, 0, 0}, {1, 1, 1}, 0, {1, 1, 1});
    b.tipo(TIPO_CHAMA);
    b.caixa({3, 0, 0}, {1, 1, 1}, 0, {1, .5f, .1f});
    b.tipo(TIPO_NORMAL);
    REQUIRE(b.verts().size() == 72);
    CHECK(b.verts()[0].tipo == TIPO_NORMAL);
    CHECK(b.verts()[40].tipo == TIPO_CHAMA);
}
TEST_CASE("anexar junta malhas") {
    MeshBuilder a, b;
    a.caixa({0, 0, 0}, {1, 1, 1}, 0, {1, 1, 1});
    b.cone({0, 0, 0}, 1, 2, 5, {1, 1, 1});
    size_t t = a.triangulos() + b.triangulos();
    a.anexar(b);
    CHECK(a.triangulos() == t);
}
TEST_CASE("aoVertical escurece a base") {
    MeshBuilder b;
    b.cilindro({0, 0, 0}, 0.3f, 4, 6, {1, 1, 1});
    b.aoVertical(0, 4);
    float baixo = 1, alto = 0;
    for (auto& v : b.verts()) {
        if (v.pos.y < 0.01f) baixo = std::min(baixo, v.ao);
        if (v.pos.y > 3.99f) alto = std::max(alto, v.ao);
    }
    CHECK(baixo < alto - 0.3f);
}

TEST_CASE("meshbuilder: oclusao respeita o giro (parede girada nao escurece o lado de dentro longe dela)") {
    // parede fina girada 45 graus; um piso a 1,5 m dela (dentro da caixa alinhada da parede, fora da parede)
    MeshBuilder m;
    m.caixa({0, 1.5f, 0}, {6, 3, 0.2f}, 0.785f, {0.5f, 0.5f, 0.5f});
    size_t n0 = m.verts().size();
    m.caixa({-1.2f, 0.05f, -1.2f}, {0.3f, 0.1f, 0.3f}, 0.f, {0.5f, 0.5f, 0.5f});  // azulejo no chao, ~1,7 m da parede
    m.calcularAO(nullptr, 16);
    float topo = 0;
    int n = 0;
    for (size_t i = n0; i < m.verts().size(); i++)
        if (m.verts()[i].normal.y > 0.9f) { topo += m.verts()[i].ao; n++; }
    REQUIRE(n > 0);
    CHECK(topo / n > 0.85f);
}
TEST_CASE("meshbuilder: caixa inclinada") {
    MeshBuilder m;
    m.caixaInclinada({0, 0, 0}, {2, 0.2f, 4}, 0.f, 0.5f, {1, 1, 1});
    REQUIRE(m.verts().size() >= 36);
    // a face de cima aponta para (0, cos, sen) da inclinacao
    bool achou = false;
    for (auto& v : m.verts())
        if (std::abs(v.normal.y - std::cos(0.5f)) < 1e-3f && std::abs(v.normal.z - std::sin(0.5f)) < 1e-3f) achou = true;
    CHECK(achou);
}
