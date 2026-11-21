#include <doctest/doctest.h>
#include <cmath>
#include "core/anim.h"

static const Esqueleto& esq() {
    static Esqueleto e = esqueletoHumano({});
    return e;
}

TEST_CASE("parado: pernas paradas, mas respira") {
    Pose a = poseParado(esq(), 0.f), b = poseParado(esq(), 0.8f);
    for (int o : {COXA_E, COXA_D, CANELA_E, CANELA_D}) CHECK(std::abs(a.rot[o].x) < 0.05f);
    CHECK(std::abs(a.rot[PEITO].x - b.rot[PEITO].x) > 0.005f);
}
TEST_CASE("andar: pernas em oposição") {
    Pose p = poseAndar(esq(), 1.5707963f, 1.6f);
    CHECK(p.rot[COXA_E].x * p.rot[COXA_D].x < 0);
    CHECK(p.rot[BRACO_E].x * p.rot[COXA_E].x < 0);  // braco oposto a perna do mesmo lado
}
TEST_CASE("correr balança mais que andar") {
    Pose a = poseAndar(esq(), 1.5707963f, 1.6f), c = poseAndar(esq(), 1.5707963f, 5.f);
    CHECK(std::abs(c.rot[COXA_E].x) > std::abs(a.rot[COXA_E].x) * 1.3f);
}
TEST_CASE("montado: coxas para a frente") {
    for (int and_ = 0; and_ < 4; and_++) {
        Pose p = poseMontado(esq(), 0.7f, and_);
        CHECK(p.rot[COXA_E].x < -1.05f);
        CHECK(p.rot[COXA_D].x < -1.05f);
    }
}
TEST_CASE("pés no chão em todas as fases") {
    for (float v : {0.f, 1.6f, 5.f})
        for (float f = 0; f < 6.3f; f += 0.3f) {
            Pose p = v > 0 ? poseAndar(esq(), f, v) : poseParado(esq(), f);
            CHECK(alturaDosPes(esq(), p) == doctest::Approx(0).epsilon(0.01));
        }
}
TEST_CASE("proporções diferentes também ficam no chão") {
    Esqueleto baixo = esqueletoHumano({1.6f, 0.9f, 1.1f, 1.f});
    for (float f = 0; f < 6.3f; f += 0.7f) CHECK(alturaDosPes(baixo, poseAndar(baixo, f, 1.6f)) == doctest::Approx(0).epsilon(0.01));
}
