#include <doctest/doctest.h>
#include <cmath>
#include "core/body.h"
#include "core/pescoco.h"

static Vec3f deformar(const VertPele& v, const std::vector<Mat4>& m) {
    // como o vertex shader: mistura linear das matrizes dos dois ossos
    Vec3f a = m[v.osso[0]].ponto(v.pos), b = m[v.osso[1]].ponto(v.pos);
    return a * v.peso + b * (1.f - v.peso);
}
static void conferir(const Pose& pose, float tol) {
    Visual vis;
    vis.heroi = true;
    vis.chapeuAba = false;
    auto malha = corpoHumanoHD({}, vis);
    Esqueleto esq = esqueletoHumano({});
    std::vector<Mat4> m;
    calcularMatrizes(esq, pose, m);
    Mat4 ic = inversaRigida(m[CABECA]), ip = inversaRigida(m[PESCOCO]);
    int n = 0;
    float pior = 0;
    for (size_t i = 0; i < malha.v.size() && n < 300; i += 53) {
        const VertPele& v = malha.v[i];
        if (v.material != MAT_PELE || v.tang.z != 1.f) continue;
        n++;
        Vec3f r = paraRepouso(deformar(v, m), ic, ip);
        pior = std::max(pior, len(r - v.pos));
    }
    MESSAGE("pior " << pior * 1000 << " mm em " << n);
    CHECK(n > 200);
    CHECK(pior < tol);
}

TEST_CASE("inversa rigida: M * inv(M) = identidade") {
    Mat4 M = Mat4::trans({0.3f, 1.2f, -0.4f}) * Mat4::rotY(0.7f) * Mat4::rotX(-0.3f) * Mat4::rotZ(0.2f);
    Mat4 I = M * inversaRigida(M);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) CHECK(I.a[i][j] == doctest::Approx(i == j ? 1.f : 0.f).epsilon(1e-5));
}
TEST_CASE("pescoco: peso da cabeca pela altura") {
    CHECK(pesoCabeca(1.50f) == 0.f);
    CHECK(pesoCabeca(1.60f) == 1.f);
    CHECK(pesoCabeca(1.5575f) == doctest::Approx(0.5f).epsilon(0.01));
}
TEST_CASE("pescoco: pose de repouso nao muda nada") {
    conferir(poseRepouso(esqueletoHumano({})), 1e-5f);
}
TEST_CASE("pescoco: volta ao repouso com a cabeca girada") {
    Esqueleto esq = esqueletoHumano({});
    for (float sx : {1.f, -1.f}) {
        Pose p = poseRepouso(esq);
        p.rot[CABECA] = {0.4f * sx, 0.4f, 0};
        p.rot[PESCOCO] = {0.2f, -0.2f * sx, 0.1f};
        p.rot[PEITO] = {0.1f, 0.3f, 0};
        p.raiz = {0.2f, -0.1f, 0.3f};
        conferir(p, 0.0015f);
    }
}
