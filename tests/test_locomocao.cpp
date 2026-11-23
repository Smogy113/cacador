#include <doctest/doctest.h>
#include <cmath>
#include <vector>
#include "core/locomocao.h"

static const Esqueleto& esq() {
    static Esqueleto e = esqueletoHumano({});
    return e;
}
static Vec3f cabeca(const Pose& p) {
    std::vector<Mat4> m;
    calcularMatrizes(esq(), p, m);
    return m[CABECA].ponto(esq().ossos[CABECA].fim);
}
// anda em linha reta (+z) por 'seg' segundos a 'v' m/s, em quadros de 1/60 s
static void andarReto(Locomocao& l, float v, float seg) {
    for (int i = 0; i < int(seg * 60); i++) l.atualizar(1.f / 60, {0, 0, v / 60}, 0.f);
}

TEST_CASE("locomoção: parado fica na pose parada") {
    Locomocao l;
    andarReto(l, 0.f, 1.f);
    CHECK(l.peso == 0.f);
    CHECK(l.fase == 0.f);
}
TEST_CASE("locomoção: andando a fase avança e a pose de andar assume") {
    Locomocao l;
    andarReto(l, 1.6f, 1.f);
    CHECK(l.peso == doctest::Approx(1.f));
    CHECK(l.vel == doctest::Approx(1.6f).epsilon(0.02));
    float f0 = l.fase;
    andarReto(l, 1.6f, 1.f);  // em regime: a fase anda o que a passada pede para 1,6 m
    CHECK(l.fase - f0 == doctest::Approx(1.6f * fasePorMetro(1.6f)).epsilon(0.02));
}
TEST_CASE("locomoção: um passo a cada toque de calcanhar") {
    Locomocao l;
    andarReto(l, 1.6f, 0.5f);
    int p0 = l.passo();
    float ciclo = 6.2831853f / fasePorMetro(1.6f);  // metros por ciclo (dois passos)
    andarReto(l, 1.6f, ciclo / 1.6f);
    CHECK(l.passo() - p0 == 2);
}
TEST_CASE("locomoção: arrancar inclina para a frente e frear para trás") {
    Locomocao l;
    for (int i = 0; i < 12; i++) l.atualizar(1.f / 60, {0, 0, (0.3f + i * 0.12f) / 60}, 0.f);  // acelerando
    CHECK(l.inclFrente > 0.03f);
    Pose em = poseLocomocao(esq(), l, 0.f);
    andarReto(l, 1.6f, 1.f);
    CHECK(std::abs(l.inclFrente) < 0.01f);  // velocidade constante: tronco volta
    for (int i = 0; i < 10; i++) l.atualizar(1.f / 60, {0, 0, (1.6f - i * 0.15f) / 60}, 0.f);  // freando
    CHECK(l.inclFrente < -0.03f);
    (void)em;
}
TEST_CASE("locomoção: na curva o corpo tomba para dentro") {
    Locomocao l;
    andarReto(l, 5.f, 1.f);
    float yaw = 0;
    for (int i = 0; i < 20; i++) {  // virando para a esquerda (yaw crescente) a 2 rad/s
        yaw += 2.f / 60;
        l.atualizar(1.f / 60, {std::sin(yaw) * 5.f / 60, 0, std::cos(yaw) * 5.f / 60}, yaw);
    }
    CHECK(l.inclLado > 0.05f);
    // a cabeca vai para a esquerda do personagem (+x) em relacao a pose sem inclinacao
    Pose p = poseAndar(esq(), l.fase, l.vel), q = p;
    inclinarTronco(q, 0.f, l.inclLado);
    CHECK(cabeca(q).x > cabeca(p).x + 0.02f);
    // mirando (yaw da camera) nao tomba
    Locomocao m = l;
    for (int i = 0; i < 30; i++) {
        yaw += 2.f / 60;
        m.atualizar(1.f / 60, {std::sin(yaw) * 5.f / 60, 0, std::cos(yaw) * 5.f / 60}, yaw, false);
    }
    CHECK(std::abs(m.inclLado) < 0.02f);
}
TEST_CASE("locomoção: inclinar para a frente leva a cabeça para a frente") {
    Pose p = poseParado(esq(), 0.f), q = p;
    inclinarTronco(q, 0.15f, 0.f);
    CHECK(cabeca(q).z > cabeca(p).z + 0.05f);
}
TEST_CASE("locomoção: teletransporte não vira passo") {
    Locomocao l;
    andarReto(l, 1.6f, 0.5f);
    float fase = l.fase;
    l.atualizar(1.f / 60, {50.f, 0, 80.f}, 2.f);  // montou/desmontou/reapareceu
    CHECK(l.fase == fase);
    CHECK(std::abs(l.inclLado) < 0.01f);
}
TEST_CASE("locomoção: andando para trás a passada corre ao contrário") {
    Locomocao l;
    for (int i = 0; i < 30; i++) l.atualizar(1.f / 60, {0, 0, -1.6f / 60}, 0.f);  // olhando +z, indo para -z
    CHECK(l.fase < 0.f);
}
