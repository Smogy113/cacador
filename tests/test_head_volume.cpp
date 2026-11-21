#include <doctest/doctest.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include "core/head_volume.h"
#include "core/pescoco.h"
#include "core/sdf_head.h"

static const VolumeCabeca& vol() {
    static VolumeCabeca v = [] {
        Visual r;
        r.heroi = true;
        r.cabeloLongo = true;
        r.chapeuAba = false;
        return gerarVolume(0.0013f, 0.002f, r);
    }();
    return v;
}
static const sdf::MalhaSup& esc() {
    static sdf::MalhaSup m = sdf::malhar(0.0026f);
    return m;
}

TEST_CASE("volume: reproduz a escultura perto da superficie") {
    const auto& v = vol();
    int n = 0, bons = 0;
    float pior = 0;
    for (size_t i = 0; i < esc().v.size() && n < 4000; i += 9)
        for (float d : {-0.003f, 0.f, 0.003f}) {
            Vec3f p = esc().v[i].pos + esc().v[i].normal * d;
            float e = std::abs(v.amostra(p) - sdf::cabeca(p));
            pior = std::max(pior, e);
            bons += e < 0.0004f;
            n++;
        }
    MESSAGE("pior erro " << pior * 1000 << " mm, bons " << bons << "/" << n);
    CHECK(bons >= n * 0.98f);
    CHECK(pior < 0.0015f);
}
TEST_CASE("volume: nunca superestima longe") {
    const auto& v = vol();
    uint32_t h = 7;
    auto r01 = [&]() { h = h * 1664525u + 1013904223u; return float(h >> 8) / 16777216.f; };
    int ruins = 0;
    for (int i = 0; i < 2000; i++) {
        Vec3f p{lerp(v.lo.x, v.hi.x, r01()), lerp(v.lo.y, v.hi.y, r01()), lerp(v.lo.z, v.hi.z, r01())};
        float real = sdf::cabeca(p);
        if (real < 0) continue;
        ruins += v.amostra(p) > real + 0.0005f;
    }
    CHECK(ruins == 0);
}
TEST_CASE("volume: caixa contem a escultura") {
    const auto& v = vol();
    float folga = 2 * v.h;
    for (auto& e : esc().v) {
        CHECK(e.pos.x > v.lo.x + folga * 0.99f);
        CHECK(e.pos.x < v.hi.x - folga * 0.99f);
        CHECK(e.pos.y > v.lo.y + folga * 0.99f);
        CHECK(e.pos.y < v.hi.y - folga * 0.99f);
        CHECK(e.pos.z > v.lo.z + folga * 0.99f);
        CHECK(e.pos.z < v.hi.z - folga * 0.99f);
    }
}
TEST_CASE("volume: atlas no layout certo") {
    const auto& v = vol();
    auto a = atlasMeiaPrecisao(v);
    REQUIRE(a.size() == size_t(v.colunas) * v.nx * size_t(v.linhas) * v.ny);
    REQUIRE(v.colunas * v.linhas >= v.nz);
    size_t W = size_t(v.colunas) * v.nx;
    for (int k : {0, v.nz / 3, v.nz - 1})
        for (int j : {0, v.ny / 2, v.ny - 1})
            for (int i : {0, v.nx / 2 + 3, v.nx - 1}) {
                size_t x = size_t(k % v.colunas) * v.nx + i, y = size_t(k / v.colunas) * v.ny + j;
                float d = v.dist[(size_t(k) * v.ny + j) * v.nx + i];
                CHECK(meiaParaFloat(a[y * W + x]) == doctest::Approx(d).epsilon(0.002));
            }
    auto c = atlasCor(v);
    CHECK(c.size() == size_t(v.colunasCor) * v.cx * size_t(v.linhasCor) * v.cy * 4);
}
TEST_CASE("volume: cor do labio e da sobrancelha marcadas no alfa") {
    const auto& v = vol();
    auto naSup = [](float x, float y) {
        for (float z = 0.16f; z > -0.05f; z -= 0.0002f)
            if (sdf::cabeca({x, y, z}) < 0) return Vec3f{x, y, z};
        return Vec3f{x, y, 0};
    };
    CHECK(v.amostraCor(naSup(0, 1.6366f)).a > 0.8f);         // labio
    CHECK(v.amostraCor(naSup(0.032f, 1.719f)).a < 0.35f);    // sobrancelha (pelos)
    float testa = v.amostraCor(naSup(0, 1.745f)).a;
    CHECK(testa == doctest::Approx(0.5f).epsilon(0.03));      // pele lisa
}
TEST_CASE("volume: gera em menos de 2 s") {
    Visual r;
    r.heroi = true;
    auto t0 = std::chrono::steady_clock::now();
    auto v = gerarVolume(0.0013f, 0.002f, r);
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    MESSAGE("gerou em " << s << " s, " << v.nx << "x" << v.ny << "x" << v.nz);
    CHECK(s < 2.0);
}
TEST_CASE("caixa da cabeca: contem a escultura e segue o pescoco") {
    const auto& v = vol();
    MalhaPele c = caixaCabeca(v, 1.f, 1.f);
    REQUIRE(c.v.size() >= 8);
    REQUIRE(c.idx.size() % 3 == 0);
    Vec3f lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
    for (auto& x : c.v) {
        lo = {std::min(lo.x, x.pos.x), std::min(lo.y, x.pos.y), std::min(lo.z, x.pos.z)};
        hi = {std::max(hi.x, x.pos.x), std::max(hi.y, x.pos.y), std::max(hi.z, x.pos.z)};
        CHECK(x.peso == doctest::Approx(pesoCabeca(x.pos.y)));  // mesma mistura de ossos da malha
    }
    for (auto& e : esc().v) {
        CHECK(e.pos.x > lo.x + 0.005f);
        CHECK(e.pos.x < hi.x - 0.005f);
        CHECK(e.pos.y > lo.y);
        CHECK(e.pos.y < hi.y - 0.005f);
        CHECK(e.pos.z > lo.z + 0.005f);
        CHECK(e.pos.z < hi.z - 0.005f);
    }
    // faces para fora: a normal de cada triangulo aponta para longe do centro
    Vec3f centro = (lo + hi) * 0.5f;
    for (size_t t = 0; t < c.idx.size(); t += 3) {
        Vec3f a = c.v[c.idx[t]].pos, b = c.v[c.idx[t + 1]].pos, d = c.v[c.idx[t + 2]].pos;
        CHECK(dot(cross(b - a, d - a), (a + b + d) * (1.f / 3.f) - centro) > 0);
    }
}
