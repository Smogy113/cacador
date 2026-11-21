#include <doctest/doctest.h>
#include <algorithm>
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

static const Esqueleto& cav() {
    static Esqueleto e = esqueletoCavalo();
    return e;
}
TEST_CASE("cavalo parado com cascos no chão") {
    for (float t = 0; t < 5; t += 0.7f) CHECK(alturaDosCascos(cav(), poseCavalo(cav(), 0, 0, t)) == doctest::Approx(0).epsilon(0.01));
}
TEST_CASE("cavalo andando nunca afunda nem flutua") {
    for (int a = 1; a <= 3; a++)
        for (float f = 0; f < 6.3f; f += 0.4f) CHECK(alturaDosCascos(cav(), poseCavalo(cav(), f, a, 0)) == doctest::Approx(0).epsilon(0.01));
}
TEST_CASE("trote: diagonais juntas") {
    Pose p = poseCavalo(cav(), 1.2f, 2, 0);
    CHECK(p.rot[C_BRACO_DE].x == doctest::Approx(p.rot[C_COXA_TD].x).epsilon(0.05));
    CHECK(p.rot[C_BRACO_DE].x * p.rot[C_BRACO_DD].x < 0);
}
TEST_CASE("galope balança mais que passo") {
    float passo = 0, galope = 0;
    for (float f = 0; f < 6.3f; f += 0.1f) {
        passo = std::max(passo, std::abs(poseCavalo(cav(), f, 1, 0).rot[C_BRACO_DE].x));
        galope = std::max(galope, std::abs(poseCavalo(cav(), f, 3, 0).rot[C_BRACO_DE].x));
    }
    CHECK(galope > passo * 1.5f);
}

// ---------- juntas nao podem achatar ao dobrar ----------
static float espessuraRelativa(const MalhaPele& m, const Esqueleto& e, const Pose& p, int pai, int filho) {
    std::vector<Mat4> mt;
    calcularMatrizes(e, p, mt);
    Vec3f j = e.ossos[filho].inicio;                       // junta em repouso
    Vec3f jp = mt[pai].ponto(j);                           // junta na pose
    float soma = 0, somaRep = 0;
    int n = 0;
    for (auto& v : m.v) {
        bool doPar = (v.osso[0] == pai || v.osso[0] == filho) && (v.osso[1] == pai || v.osso[1] == filho);
        if (!doPar || std::abs(v.pos.y - j.y) > 0.03f) continue;
        Vec3f q = mt[v.osso[0]].ponto(v.pos) * v.peso + mt[v.osso[1]].ponto(v.pos) * (1 - v.peso);
        soma += len(q - jp);
        somaRep += len(v.pos - j);
        n++;
    }
    REQUIRE(n > 0);
    return soma / somaRep;
}
static Pose maisDobrado(const Esqueleto& e, int osso) {
    Pose melhor;
    float maior = -1;
    for (float f = 0; f < 6.3f; f += 0.1f) {
        Pose p = poseAndar(e, f, 5.f);
        if (std::abs(p.rot[osso].x) > maior) { maior = std::abs(p.rot[osso].x); melhor = p; }
    }
    return melhor;
}
TEST_CASE("joelho e cotovelo não achatam correndo (modelo leve e HD)") {
    Proporcoes pr;
    Esqueleto e = esqueletoHumano(pr);
    Visual v;
    v.sobretudo = false;
    for (int hd = 0; hd < 2; hd++) {
        MalhaPele m = hd ? corpoHumanoHD(pr, v) : corpoHumano(pr, v);
        CHECK_MESSAGE(espessuraRelativa(m, e, maisDobrado(e, CANELA_E), COXA_E, CANELA_E) > 0.88f, "joelho hd=" << hd);
        CHECK_MESSAGE(espessuraRelativa(m, e, maisDobrado(e, ANTEBRACO_E), BRACO_E, ANTEBRACO_E) > 0.88f, "cotovelo hd=" << hd);
    }
}

// posicao no mundo (pose aplicada) do inicio/fim de um osso
static Vec3f juntaPose(const Pose& p, int osso, bool fim) {
    std::vector<Mat4> m;
    calcularMatrizes(esq(), p, m);
    const Osso& o = esq().ossos[osso];
    Vec3f r = m[osso].ponto(fim ? o.fim : o.inicio);
    return r + p.raiz * 0.f;  // a raiz ja entra nas matrizes
}
static float maisAlto(const Pose& p) {
    float y = -1e9f;
    for (int o = 0; o < N_OSSOS_HUMANO; o++) y = std::max({y, juntaPose(p, o, false).y, juntaPose(p, o, true).y});
    return y;
}
static float maisBaixo(const Pose& p) {
    float y = 1e9f;
    for (int o = 0; o < N_OSSOS_HUMANO; o++) y = std::min({y, juntaPose(p, o, false).y, juntaPose(p, o, true).y});
    return y;
}
TEST_CASE("mirar: mao direita a frente na altura do ombro") {
    Pose p = poseMirar(esq(), 0, 0);
    Vec3f mao = juntaPose(p, MAO_D, true), ombro = juntaPose(p, BRACO_D, false), peito = juntaPose(p, PEITO, true);
    CHECK(mao.z - peito.z > 0.5f);
    CHECK(std::abs(mao.y - ombro.y) < 0.1f);
    CHECK(std::abs(mao.x) < std::abs(ombro.x) + 0.02f);  // aponta para a frente do corpo, nao para fora
}
TEST_CASE("mirar: angulo levanta a mao e recuo levanta o antebraco") {
    CHECK(juntaPose(poseMirar(esq(), 0.5f, 0), MAO_D, true).y > juntaPose(poseMirar(esq(), 0, 0), MAO_D, true).y + 0.15f);
    CHECK(juntaPose(poseMirar(esq(), 0, 1), MAO_D, true).y > juntaPose(poseMirar(esq(), 0, 0), MAO_D, true).y + 0.05f);
}
TEST_CASE("cair: no fim deitado no chao") {
    Pose p = poseCair(esq(), 1.f);
    CHECK(maisAlto(p) < 0.45f);
    CHECK(maisBaixo(p) > -0.02f);
    CHECK(maisBaixo(p) < 0.15f);
    CHECK(maisAlto(poseCair(esq(), 0.f)) > 1.6f);  // comeca em pe
}
TEST_CASE("rendido: maos acima da cabeca e joelho no chao") {
    Pose p = poseRendido(esq(), 0.3f);
    float cab = juntaPose(p, CABECA, false).y;
    CHECK(juntaPose(p, MAO_E, true).y > cab + 0.1f);
    CHECK(juntaPose(p, MAO_D, true).y > cab + 0.1f);
    CHECK(std::min(juntaPose(p, CANELA_E, false).y, juntaPose(p, CANELA_D, false).y) < 0.15f);
    CHECK(maisBaixo(p) > -0.02f);
}
TEST_CASE("amarrado: sentado com as maos para tras") {
    Pose p = poseAmarrado(esq(), 0.3f);
    float zPeito = juntaPose(p, PEITO, false).z;
    CHECK(juntaPose(p, MAO_E, true).z < zPeito);
    CHECK(juntaPose(p, MAO_D, true).z < zPeito);
    CHECK(juntaPose(p, QUADRIL, false).y < 0.5f);
    CHECK(maisBaixo(p) > -0.02f);
}
TEST_CASE("recarregar e tranco mexem no corpo") {
    Pose a = poseRecarregar(esq(), 0.5f);
    CHECK(juntaPose(a, MAO_D, true).z > 0.15f);  // maos na frente do corpo
    Pose p = poseParado(esq(), 0), q = p;
    aplicarTranco(q, 1.f);
    CHECK(juntaPose(q, CABECA, true).z < juntaPose(p, CABECA, true).z - 0.03f);  // joga o tronco para tras
}
TEST_CASE("garupa: dobrado na cintura") {
    Pose p = poseGarupa(esq());
    // cabeca e pes abaixo do quadril (atravessado no cavalo)
    float q = juntaPose(p, QUADRIL, false).y;
    CHECK(juntaPose(p, CABECA, true).y < q);
    CHECK(juntaPose(p, PE_E, true).y < q);
}
