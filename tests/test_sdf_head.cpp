#include <doctest/doctest.h>
#include <cmath>
#include <map>
#include "core/sdf_head.h"

TEST_CASE("SDF: dentro da cabeça é negativo, fora é positivo") {
    CHECK(sdf::cabeca({0, 1.70f, 0}) < -0.03f);
    CHECK(sdf::cabeca({0, 1.70f, 0.3f}) > 0.1f);
    CHECK(sdf::cabeca({0.3f, 1.70f, 0}) > 0.1f);
    CHECK(sdf::cabeca({0, 1.90f, 0}) > 0.05f);
}
TEST_CASE("SDF: topo da cabeça a 1,80 m e ponta do nariz à frente") {
    float topo = 0;
    for (float y = 1.75f; y < 1.85f; y += 0.0005f) if (sdf::cabeca({0, y, -0.01f}) < 0) topo = y;
    CHECK(topo == doctest::Approx(1.80f).epsilon(0.004));
    float nariz = 0;
    for (float z = 0.05f; z < 0.16f; z += 0.0005f) if (sdf::cabeca({0, 1.666f, z}) < 0) nariz = z;
    CHECK(nariz > 0.11f);
    CHECK(nariz < 0.13f);
}
TEST_CASE("SDF: a abertura do olho deixa o globo à mostra") {
    // no centro da abertura, a pele termina antes da frente do globo
    float pele = -1;
    for (float z = 0.12f; z > 0.06f; z -= 0.0002f)
        if (sdf::cabeca({sdf::OLHO_X, sdf::OLHO_Y, z}) < 0) { pele = z; break; }
    CHECK(pele < sdf::OLHO_Z + sdf::OLHO_R - 0.0005f);
    // logo acima da abertura, a palpebra cobre a frente do globo
    float palp = -1;
    for (float z = 0.12f; z > 0.06f; z -= 0.0002f)
        if (sdf::cabeca({sdf::OLHO_X, sdf::OLHO_Y + 0.0075f, z}) < 0) { palp = z; break; }
    CHECK(palp > sdf::OLHO_Z + 0.008f);
}
TEST_CASE("malha da escultura: fechada, lisa e dentro do orçamento") {
    auto m = sdf::malhar(0.0026f);
    CHECK(m.v.size() > 8000);
    CHECK(m.v.size() < 40000);
    REQUIRE(m.idx.size() % 3 == 0);
    // arestas de borda so na abertura do pescoco
    std::map<std::pair<uint32_t, uint32_t>, int> arestas;
    for (size_t t = 0; t < m.idx.size(); t += 3)
        for (int k = 0; k < 3; k++) {
            uint32_t a = m.idx[t + k], b = m.idx[t + (k + 1) % 3];
            arestas[{std::min(a, b), std::max(a, b)}]++;
        }
    int borda = 0, bordaFora = 0;
    for (auto& [e, n] : arestas)
        if (n == 1) {
            borda++;
            if (m.v[e.first].pos.y > 1.505f) bordaFora++;
        }
    CHECK(bordaFora <= borda / 20 + 2);
    // normais unitarias apontando para fora; superficie lisa (vizinhos quase paralelos)
    for (auto& v : m.v) CHECK(len(v.normal) == doctest::Approx(1).epsilon(0.01));
    int lisos = 0, total = 0;
    for (size_t t = 0; t < m.idx.size(); t += 3) {
        Vec3f na = m.v[m.idx[t]].normal, nb = m.v[m.idx[t + 1]].normal, nc = m.v[m.idx[t + 2]].normal;
        total++;
        if (dot(na, nb) > 0.9f && dot(nb, nc) > 0.9f) lisos++;
    }
    CHECK(lisos > total * 0.95f);  // vincos reais (boca, narinas, palpebras) sao marcados
}
TEST_CASE("oclusão: narina e concha da orelha mais escuras que a testa") {
    auto m = sdf::malhar(0.0026f);
    float testa = 0, narina = 1, concha = 1;
    int nt = 0;
    for (auto& v : m.v) {
        if (std::abs(v.pos.x) < 0.02f && v.pos.y > 1.74f && v.pos.y < 1.76f && v.pos.z > 0.06f) { testa += v.ao; nt++; }
        if (std::abs(std::abs(v.pos.x) - 0.0072f) < 0.003f && std::abs(v.pos.y - 1.657f) < 0.003f && v.pos.z > 0.095f) narina = std::min(narina, v.ao);
        if (std::abs(std::abs(v.pos.x) - 0.083f) < 0.006f && std::abs(v.pos.y - 1.686f) < 0.008f) concha = std::min(concha, v.ao);
    }
    REQUIRE(nt > 0);
    testa /= nt;
    CHECK(testa > 0.85f);
    CHECK(narina < testa - 0.25f);
    CHECK(concha < testa - 0.2f);
}
