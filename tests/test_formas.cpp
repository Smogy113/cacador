#include <doctest/doctest.h>
#include <cmath>
#include "core/formas.h"
#include "core/meshbuilder.h"

static Vec3f girar(Vec3f p, float a) {  // mesmo giro do MeshBuilder
    float c = std::cos(a), s = std::sin(a);
    return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
}

TEST_CASE("formas: caixa girada") {
    FormaCena f{TipoForma::Caixa, {3, 2, -1}, {2, 1.5f, 1}, 0.7f, {}};
    for (int k = 0; k < 8; k++) {
        Vec3f c = f.centro + girar({(k & 1) ? 2.f : -2.f, (k & 2) ? 1.5f : -1.5f, (k & 4) ? 1.f : -1.f}, 0.7f);
        CHECK(std::abs(distanciaForma(f, c)) < 1e-4f);
    }
    Vec3f n = girar({0, 0, 1}, 0.7f);
    CHECK(distanciaForma(f, f.centro + n * 2.f) == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(distanciaForma(f, f.centro) == doctest::Approx(-1.f).epsilon(1e-4));
}
TEST_CASE("formas: prisma") {
    FormaCena f{TipoForma::Prisma, {0, 5, 0}, {3, 1, 2}, 0.3f, {}};
    Vec3f cume = f.centro + Vec3f{0, 1, 0};
    CHECK(std::abs(distanciaForma(f, cume)) < 1e-4f);
    float acima = distanciaForma(f, cume + Vec3f{0, 1, 0});
    CHECK(acima <= 1.f);
    CHECK(acima > 0.6f);
    CHECK(distanciaForma(f, f.centro + Vec3f{0, -0.5f, 0}) < 0.f);
}
TEST_CASE("formas: cilindro") {
    FormaCena f{TipoForma::Cilindro, {1, 2, 1}, {0.5f, 2, 0.5f}, 0, {}};
    CHECK(distanciaForma(f, {2.5f, 2, 1}) == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(distanciaForma(f, {1, 5, 1}) == doctest::Approx(1.f).epsilon(1e-4));
    CHECK(distanciaForma(f, {1, 2, 1}) < 0.f);
}
TEST_CASE("formas: o meshbuilder registra o que desenha") {
    std::vector<FormaCena> fs;
    MeshBuilder m;
    m.registrar(&fs);
    m.caixa({1, 2, 3}, {4, 3, 2}, 0.7f, {0.5f, 0.5f, 0.5f});
    m.prisma({0, 6, 0}, {6, 2, 4}, 1.1f, {0.4f, 0.3f, 0.2f});
    m.cilindro({5, 0, 5}, 0.6f, 3.f, 8, {0.3f, 0.3f, 0.3f});
    REQUIRE(fs.size() == 3);
    CHECK(fs[1].cor.r == doctest::Approx(0.4f));
    int ruins = 0;
    for (auto& v : m.verts()) ruins += std::abs(distanciaCena(fs, v.pos)) > 0.002f;
    CHECK(ruins == 0);
}
TEST_CASE("formas: sem registrar nao muda nada") {
    std::vector<FormaCena> fs;
    MeshBuilder a, b;
    b.registrar(&fs);
    for (MeshBuilder* m : {&a, &b}) {
        m->caixa({1, 2, 3}, {4, 3, 2}, 0.7f, {0.5f, 0.5f, 0.5f});
        m->prisma({0, 6, 0}, {6, 2, 4}, 1.1f, {0.4f, 0.3f, 0.2f});
    }
    REQUIRE(a.verts().size() == b.verts().size());
    for (size_t i = 0; i < a.verts().size(); i++) CHECK(len(a.verts()[i].pos - b.verts()[i].pos) == 0.f);
}
TEST_CASE("formas perto: ordem e limite") {
    std::vector<FormaCena> fs;
    for (int i = 9; i >= 0; i--) fs.push_back({TipoForma::Caixa, {float(i) * 5.f, 0, 0}, {1, 1, 1}, 0, {}});  // fora de ordem
    auto a = formasPerto(fs, {-3, 0, 0}, 100.f, 4);
    REQUIRE(a.size() == 4);
    for (int k = 0; k < 4; k++) CHECK(fs[a[k]].centro.x == doctest::Approx(5.f * k));
    CHECK(formasPerto(fs, {-2.9f, 0, 0}, 100.f, 4) == a);                // anda 10 cm: mesma lista
    CHECK(formasPerto(fs, {-3, 0, 0}, 8.f, 32).size() == 2);             // raio corta
}

// ---------------- faces de parede e atlas de luz ----------------
TEST_CASE("faces: cantos batem com a caixa") {
    FormaCena f{TipoForma::Caixa, {3, 2, -1}, {2, 1.5f, 1}, 0.7f, {}};
    f.parede = true;
    for (int k = 0; k < 4; k++) {
        FaceParede fc = faceDe(f, k);
        Vec3f cs[4] = {fc.origem, fc.origem + fc.eixoU * fc.largura, fc.origem + Vec3f{0, fc.altura, 0},
                       fc.origem + fc.eixoU * fc.largura + Vec3f{0, fc.altura, 0}};
        for (auto& c : cs) CHECK(std::abs(distanciaForma(f, c)) < 1e-4f);
        CHECK(dot(fc.normal, (fc.origem + fc.eixoU * (fc.largura * 0.5f)) - f.centro) > 0.f);  // para fora
        CHECK(std::abs(dot(fc.normal, fc.eixoU)) < 1e-5f);
        CHECK(fc.altura == doctest::Approx(3.f));
    }
    CHECK(faceDe(f, 0).largura == doctest::Approx(2.f));  // faces +-x tem a largura em z
    CHECK(faceDe(f, 2).largura == doctest::Approx(4.f));
}
TEST_CASE("atlas: sem sobreposicao e com borda") {
    std::vector<FormaCena> fs;
    for (int i = 0; i < 20; i++) {
        FormaCena f{TipoForma::Caixa, {float(i) * 10.f, 1, 0}, {3.f + i % 3, 1.3f, 2.f}, 0.3f * i, {}};
        f.parede = i % 4 != 3;
        fs.push_back(f);
    }
    fs.push_back({TipoForma::Prisma, {0, 5, 0}, {3, 1, 2}, 0, {}});
    auto at = alocarAtlas(fs, 0.25f, 512);
    REQUIRE(at.size() == fs.size());
    std::vector<RetAtlas> rs;
    for (size_t i = 0; i < fs.size(); i++)
        for (int k = 0; k < 4; k++) {
            const RetAtlas& r = at[i][k];
            if (!fs[i].parede) { CHECK(r.w == 0); continue; }
            REQUIRE(r.w > 0);
            FaceParede fc = faceDe(fs[i], k);
            CHECK(r.w == int(std::ceil(fc.largura / 0.25f)) + 2);
            CHECK(r.h == int(std::ceil(fc.altura / 0.25f)) + 2);
            CHECK(r.x >= 0);
            CHECK(r.y >= 0);
            CHECK(r.x + r.w <= 512);
            CHECK(r.y + r.h <= 512);
            rs.push_back(r);
        }
    for (size_t a = 0; a < rs.size(); a++)
        for (size_t b = a + 1; b < rs.size(); b++) {
            bool separados = rs[a].x + rs[a].w <= rs[b].x || rs[b].x + rs[b].w <= rs[a].x || rs[a].y + rs[a].h <= rs[b].y ||
                             rs[b].y + rs[b].h <= rs[a].y;
            CHECK(separados);
        }
}
TEST_CASE("atlas: cheio nao inventa lugar") {
    std::vector<FormaCena> fs;
    for (int i = 0; i < 40; i++) {
        FormaCena f{TipoForma::Caixa, {0, 0, 0}, {4, 1.5f, 3}, 0, {}};
        f.parede = true;
        fs.push_back(f);
    }
    auto at = alocarAtlas(fs, 0.25f, 128);
    int sem = 0;
    for (auto& f4 : at)
        for (auto& r : f4) {
            if (r.w == 0) { sem++; continue; }
            CHECK(r.x + r.w <= 128);
            CHECK(r.y + r.h <= 128);
        }
    CHECK(sem > 0);
}
TEST_CASE("meshbuilder: vertices de parede levam o indice da forma") {
    std::vector<FormaCena> fs;
    MeshBuilder m;
    m.caixa({0, 0, 0}, {1, 1, 1}, 0, {});  // antes de registrar: sem forma
    size_t n0 = m.verts().size();
    m.registrar(&fs);
    m.tipo(TIPO_REBOCO);
    m.caixa({5, 1, 0}, {4, 3, 2}, 0.2f, {0.8f, 0.7f, 0.6f});
    size_t n1 = m.verts().size();
    m.tipo(TIPO_NORMAL);
    m.prisma({5, 4, 0}, {4, 1, 2}, 0.2f, {});
    REQUIRE(fs.size() == 2);
    CHECK(fs[0].parede);
    CHECK_FALSE(fs[1].parede);
    for (size_t i = 0; i < m.verts().size(); i++) CHECK(m.verts()[i].forma == (i < n0 ? -1 : (i < n1 ? 0 : 1)));
}
TEST_CASE("meshbuilder: pecas presas levam o indice da forma fixa") {
    std::vector<FormaCena> fs;
    MeshBuilder m;
    m.registrar(&fs);
    m.tipo(TIPO_REBOCO);
    m.caixa({0, 1, 0}, {4, 3, 2}, 0, {});
    m.registrar(nullptr);
    m.formaFixa(0);
    size_t n0 = m.verts().size();
    m.caixa({0, 1, 1.05f}, {0.2f, 3, 0.1f}, 0, {});  // viga
    size_t n1 = m.verts().size();
    m.formaFixa(-1);
    m.caixa({5, 0, 0}, {1, 1, 1}, 0, {});
    REQUIRE(fs.size() == 1);
    for (size_t i = n0; i < n1; i++) CHECK(m.verts()[i].forma == 0);
    for (size_t i = n1; i < m.verts().size(); i++) CHECK(m.verts()[i].forma == -1);
}
