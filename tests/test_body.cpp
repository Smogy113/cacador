#include <doctest/doctest.h>
#include <algorithm>
#include "core/body.h"

static Visual semChapeu() {
    Visual v;
    v.chapeuAba = false;
    return v;
}

static float distSegmento(Vec3f p, Vec3f a, Vec3f b, Vec3f& maisPerto) {
    Vec3f ab = b - a;
    float t = std::clamp(dot(p - a, ab) / std::max(dot(ab, ab), 1e-8f), 0.f, 1.f);
    maisPerto = a + ab * t;
    return len(p - maisPerto);
}

TEST_CASE("corpo humano tem entre 1000 e 1600 triângulos") {
    for (bool sob : {false, true})
        for (bool ch : {false, true}) {
            Visual v;
            v.sobretudo = sob;
            v.chapeuAba = ch;
            auto m = corpoHumano({}, v);
            CHECK(m.triangulos() >= 1000);
            CHECK(m.triangulos() <= 1600);
        }
}
TEST_CASE("ossos e pesos válidos") {
    auto m = corpoHumano({}, Visual{});
    for (auto& v : m.v) {
        CHECK(v.osso[0] < N_OSSOS_HUMANO);
        CHECK(v.osso[1] < N_OSSOS_HUMANO);
        CHECK(v.peso >= 0.f);
        CHECK(v.peso <= 1.f);
    }
    for (auto i : m.idx) CHECK(i < m.v.size());
}
TEST_CASE("altura e pés no chão") {
    for (float alt : {1.6f, 1.8f, 1.95f}) {
        Proporcoes p;
        p.altura = alt;
        auto m = corpoHumano(p, semChapeu());
        float topo = -1e9f, base = 1e9f;
        for (auto& v : m.v) { topo = std::max(topo, v.pos.y); base = std::min(base, v.pos.y); }
        CHECK(topo == doctest::Approx(alt).epsilon(0.03));
        CHECK(base == doctest::Approx(0).epsilon(0.01));
        CHECK(base > -0.01f);
    }
}
TEST_CASE("normais apontam para fora do osso") {
    Proporcoes p;
    auto m = corpoHumano(p, semChapeu());
    auto e = esqueletoHumano(p);
    std::vector<bool> usado(m.v.size(), false);
    for (auto i : m.idx) usado[i] = true;
    int bons = 0, total = 0;
    for (size_t i = 0; i < m.v.size(); i++) {
        if (!usado[i]) continue;
        const auto& v = m.v[i];
        const Osso& o = e.ossos[v.osso[0]];
        Vec3f q;
        distSegmento(v.pos, o.inicio, o.fim, q);
        total++;
        if (dot(v.normal, v.pos - q) > 0) bons++;
        CHECK(len(v.normal) == doctest::Approx(1).epsilon(0.01));
    }
    CHECK(bons >= int(total * 0.95f));
}
TEST_CASE("cotovelo e joelho misturam dois ossos") {
    auto m = corpoHumano({}, Visual{});
    bool cotovelo = false, joelho = false;
    for (auto& v : m.v) {
        auto par = [&](int a, int b) {
            return ((v.osso[0] == a && v.osso[1] == b) || (v.osso[0] == b && v.osso[1] == a)) && v.peso > 0.2f && v.peso < 0.8f;
        };
        if (par(BRACO_E, ANTEBRACO_E)) cotovelo = true;
        if (par(COXA_D, CANELA_D)) joelho = true;
    }
    CHECK(cotovelo);
    CHECK(joelho);
}
TEST_CASE("esqueleto: pais antes dos filhos e 17 ossos") {
    auto e = esqueletoHumano({});
    REQUIRE(e.ossos.size() == N_OSSOS_HUMANO);
    for (size_t i = 0; i < e.ossos.size(); i++) CHECK(e.ossos[i].pai < int(i));
}
TEST_CASE("lado esquerdo do personagem fica em +x (ele olha para +z)") {
    auto e = esqueletoHumano({});
    CHECK(e.ossos[MAO_E].inicio.x > 0);
    CHECK(e.ossos[MAO_D].inicio.x < 0);
}

TEST_CASE("cavalo: até 2000 triângulos, ossos válidos") {
    for (bool sel : {false, true}) {
        VisualCavalo v;
        v.selado = sel;
        auto m = corpoCavalo(v);
        CHECK(m.triangulos() <= 2000);
        CHECK(m.triangulos() >= 600);
        for (auto& x : m.v) {
            CHECK(x.osso[0] < N_OSSOS_CAVALO);
            CHECK(x.osso[1] < N_OSSOS_CAVALO);
        }
    }
    auto e = esqueletoCavalo();
    REQUIRE(e.ossos.size() == N_OSSOS_CAVALO);
    CHECK(N_OSSOS_CAVALO <= 17);
    for (size_t i = 0; i < e.ossos.size(); i++) CHECK(e.ossos[i].pai < int(i));
}
TEST_CASE("cavalo: cascos no chão e olha para +z") {
    auto m = corpoCavalo({});
    float base = 1e9f, zCabeca = -1e9f;
    for (auto& v : m.v) {
        base = std::min(base, v.pos.y);
        if (v.osso[0] == C_CABECA) zCabeca = std::max(zCabeca, v.pos.z);
    }
    CHECK(base == doctest::Approx(0).epsilon(0.01));
    CHECK(zCabeca > 1.2f);
    CHECK(assentoCavalo().y > 1.6f);
}
