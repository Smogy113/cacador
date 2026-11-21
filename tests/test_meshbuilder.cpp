#include <doctest/doctest.h>
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
