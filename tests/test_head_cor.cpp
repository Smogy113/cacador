#include <doctest/doctest.h>
#include <cmath>
#include <cstdio>
#include "core/body.h"
#include "core/head_cor.h"
#include "core/sdf_head.h"

#ifndef REF_N
#define REF_N 37935
#define REF_LAB 160
#define REF_SOMA 157656.1492
#endif
static Visual visHeroi() {
    Visual r;
    r.pele = {0.78f, 0.58f, 0.45f};
    r.cabelo = {0.27f, 0.2f, 0.14f};
    r.heroi = true;
    r.cabeloLongo = true;
    r.chapeuAba = false;
    return r;
}
static bool peleDoRosto(const VertPele& v) { return (v.material == MAT_PELE || v.material == MAT_LABIO) && v.tang.z == 1.f; }
// primeiro ponto da superficie andando em -z a partir da frente
static Vec3f naSuperficie(float x, float y) {
    for (float z = 0.16f; z > -0.05f; z -= 0.0002f)
        if (sdf::cabeca({x, y, z}) < 0) return {x, y, z};
    return {x, y, 0};
}

TEST_CASE("cor da pele: malha do heroi nao mudou (referencia antes da refatoracao)") {
    auto m = corpoHumanoHD({}, visHeroi());
    double soma = 0;
    int n = 0, lab = 0;
    for (auto& v : m.v)
        if (peleDoRosto(v)) {
            soma += v.cor.r + 2.0 * v.cor.g + 3.0 * v.cor.b + 5.0 * v.pelos;
            n++;
            lab += v.material == MAT_LABIO;
        }
    char buf[96];
    std::snprintf(buf, sizeof buf, "n=%d lab=%d soma=%.4f", n, lab, soma);
    MESSAGE(buf);
    CHECK(n == REF_N);
    CHECK(lab == REF_LAB);
    CHECK(soma == doctest::Approx(REF_SOMA).epsilon(1e-6));
}
#ifndef SO_REFERENCIA

TEST_CASE("cor da pele: igual a malha") {
    Visual vis = visHeroi();
    auto m = corpoHumanoHD({}, vis);
    int n = 0;
    for (size_t i = 0; i < m.v.size() && n < 500; i += 37) {
        const VertPele& v = m.v[i];
        if (!peleDoRosto(v)) continue;
        n++;
        CorPele c = corDaPele(v.pos, v.normal, vis);
        CHECK(std::abs(c.cor.r - v.cor.r) < 0.002f);
        CHECK(std::abs(c.cor.g - v.cor.g) < 0.002f);
        CHECK(std::abs(c.cor.b - v.cor.b) < 0.002f);
        CHECK(c.labio == (v.material == MAT_LABIO));
    }
    CHECK(n > 300);
}
TEST_CASE("cor da pele: labio no meio da boca") {
    Vec3f p = naSuperficie(0, 1.6366f);
    CHECK(corDaPele(p, sdf::gradiente(p), visHeroi()).labio);
}
TEST_CASE("cor da pele: sobrancelha tem pelos") {
    Vec3f p = naSuperficie(0.032f, 1.719f);
    CHECK(corDaPele(p, sdf::gradiente(p), visHeroi()).pelos > 0.3f);
}
TEST_CASE("cor da pele: sem barba a bochecha nao tem pelos") {
    Vec3f p = naSuperficie(0.045f, 1.66f);
    CHECK(corDaPele(p, sdf::gradiente(p), visHeroi()).pelos < 0.02f);
}
#endif
