#include <doctest/doctest.h>
#include "core/skeleton.h"

// esqueleto de teste: tronco (raiz) e braco pendurado no ombro
static Esqueleto braco() {
    Esqueleto e;
    e.ossos.push_back({-1, {0, 1, 0}, {0, 1.5f, 0}});        // 0 tronco
    e.ossos.push_back({0, {0.2f, 1.45f, 0}, {0.2f, 1.15f, 0}});  // 1 braco
    e.ossos.push_back({1, {0.2f, 1.15f, 0}, {0.2f, 0.9f, 0}});   // 2 antebraco
    return e;
}
static bool perto(Vec3f a, Vec3f b) { return len(a - b) < 1e-4f; }

TEST_CASE("pose de repouso não mexe em nada") {
    Esqueleto e = braco();
    std::vector<Mat4> m;
    calcularMatrizes(e, poseRepouso(e), m);
    REQUIRE(m.size() == 3);
    for (auto& x : m) CHECK(perto(x.ponto({0.3f, 0.7f, -0.2f}), {0.3f, 0.7f, -0.2f}));
}
TEST_CASE("girar o braço leva a mão para a frente") {
    Esqueleto e = braco();
    Pose p = poseRepouso(e);
    p.rot[1].x = -1.5707963f;
    std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    Vec3f mao = m[2].ponto({0.2f, 0.9f, 0});
    CHECK(mao.z == doctest::Approx(0.55f).epsilon(0.01));
    CHECK(mao.y == doctest::Approx(1.45f).epsilon(0.01));
    CHECK(perto(m[1].ponto({0.2f, 1.45f, 0}), {0.2f, 1.45f, 0}));  // o ombro fica no lugar
}
TEST_CASE("filho herda a rotação do pai") {
    Esqueleto e = braco();
    Pose p = poseRepouso(e);
    p.rot[0].y = 1.5707963f;  // gira o tronco 90 graus
    std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    Vec3f cotovelo = m[2].ponto({0.2f, 1.15f, 0});
    CHECK(cotovelo.x == doctest::Approx(0).epsilon(0.01));
    CHECK(cotovelo.z == doctest::Approx(-0.2f).epsilon(0.01));
}
TEST_CASE("deslocar a raiz move tudo") {
    Esqueleto e = braco();
    Pose p = poseRepouso(e);
    p.raiz = {1, 2, 3};
    std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    for (auto& x : m) CHECK(perto(x.ponto({0, 0, 0}), {1, 2, 3}));
}
TEST_CASE("Mat4: a*b aplica b e depois a") {
    Mat4 t = Mat4::trans({1, 0, 0}), r = Mat4::rotY(1.5707963f);
    CHECK(perto((t * r).ponto({0, 0, 1}), {2, 0, 0}));
    CHECK(perto((r * t).ponto({0, 0, 1}), {1, 0, -1}));
}
