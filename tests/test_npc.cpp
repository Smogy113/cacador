#include <doctest/doctest.h>
#include <memory>
#include "core/npc.h"
#include "core/terrain.h"

static const Terreno& planoN() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(lerMapa("lugar cidade A 0 0 0 300\n"), 1);
    return *t;
}

static std::vector<std::vector<Vec3f>> grupos() {
    std::vector<Vec3f> a, b;
    for (int k = 0; k < 6; k++) a.push_back({float(k * 6), 0, 0});
    for (int k = 0; k < 6; k++) b.push_back({-50.f, 0, float(k * 6)});
    return {a, b};
}

TEST_CASE("chega ao ponto e segue para o próximo") {
    Populacao p({{{0, 0, 0}, {5, 0, 0}, {5, 0, 5}}}, 1, 1);
    MundoColisao col;
    DayClock dia(12);
    REQUIRE(p.npcs().size() == 1);
    CHECK(p.npcs()[0].alvo == 1);
    for (int i = 0; i < 60 * 5; i++) p.atualizar(1 / 60.f, dia, planoN(), col, {0, 0, 0});
    CHECK(p.npcs()[0].alvo == 2);
    CHECK(std::hypot(p.npcs()[0].corpo.pos.x - 5, p.npcs()[0].corpo.pos.z) < 1.f);
}
TEST_CASE("à noite a cidade esvazia (longe do jogador)") {
    Populacao p(grupos(), 7, 40);
    MundoColisao col;
    p.atualizar(0.1f, DayClock(23), planoN(), col, {1000, 0, 1000});
    CHECK(p.ativos() < 0.3f * 40);
    CHECK(p.ativos() > 0);  // alguns notivagos
}
TEST_CASE("de dia volta") {
    Populacao p(grupos(), 7, 40);
    MundoColisao col;
    p.atualizar(0.1f, DayClock(23), planoN(), col, {1000, 0, 1000});
    p.atualizar(0.1f, DayClock(12), planoN(), col, {1000, 0, 1000});
    CHECK(p.ativos() > 0.9f * 40);
}
TEST_CASE("perto do jogador não some na frente dele: vai para casa andando") {
    Populacao p(grupos(), 7, 40);
    MundoColisao col;
    for (int i = 0; i < 10; i++) p.atualizar(0.1f, DayClock(12), planoN(), col, {15, 0, 0});
    int antes = p.ativos();
    p.atualizar(0.1f, DayClock(23), planoN(), col, {15, 0, 0});
    // os que estao no grupo perto do jogador (x 0..30) nao somem de uma vez
    int perto = 0;
    for (auto& n : p.npcs())
        if (n.ativo && std::hypot(n.corpo.pos.x - 15, n.corpo.pos.z) < DIST_FORA_DE_VISTA) perto++;
    CHECK(perto > 0);
    CHECK(p.ativos() < antes);
}
TEST_CASE("NPC fica no chão e fora das paredes") {
    MundoColisao col;
    float h = planoN().altura(2.5f, 0);
    col.adicionar({{2, h - 1, -3}, {3, h + 3, 3}});  // parede no meio do caminho
    Populacao p({{{0, 0, 0}, {5, 0, 0}}}, 3, 1);
    for (int i = 0; i < 60 * 6; i++) {
        p.atualizar(1 / 60.f, DayClock(12), planoN(), col, {100, 0, 100});
        Vec3f q = p.npcs()[0].corpo.pos;
        CHECK(len(col.resolver(q, RAIO_PESSOA) - q) < 1e-3f);
    }
    CHECK(p.npcs()[0].corpo.pos.y == doctest::Approx(planoN().altura(p.npcs()[0].corpo.pos.x, p.npcs()[0].corpo.pos.z)));
}
TEST_CASE("determinístico pela semente") {
    Populacao a(grupos(), 9, 20), b(grupos(), 9, 20);
    MundoColisao col;
    for (int i = 0; i < 100; i++) {
        a.atualizar(0.05f, DayClock(12), planoN(), col, {0, 0, 0});
        b.atualizar(0.05f, DayClock(12), planoN(), col, {0, 0, 0});
    }
    for (size_t k = 0; k < 20; k++) CHECK(len(a.npcs()[k].corpo.pos - b.npcs()[k].corpo.pos) < 1e-5f);
}
