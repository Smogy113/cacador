#include <cmath>
#include <memory>
#include "core/combat.h"
#include "core/places.h"
#include "core/terrain.h"
#include "doctest/doctest.h"

static const Terreno& plano() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(lerMapa("lugar cidade A 0 0 0 300\n"), 1);
    return *t;
}

TEST_CASE("revolver: 6 tiros e depois precisa recarregar") {
    Revolver r;
    for (int i = 0; i < 6; i++) {
        REQUIRE(r.podeAtirar());
        r.atirar();
        r.atualizar(0.4f);
    }
    CHECK_FALSE(r.podeAtirar());
    r.recarregar();
    r.atualizar(1.9f);
    CHECK_FALSE(r.podeAtirar());
    r.atualizar(0.11f);
    CHECK(r.balas == 6);
    CHECK(r.podeAtirar());
}
TEST_CASE("revolver: cadencia") {
    Revolver r;
    r.atirar();
    r.atualizar(0.3f);
    CHECK_FALSE(r.podeAtirar());
    r.atualizar(0.06f);
    CHECK(r.podeAtirar());
}
TEST_CASE("vida: regenera so depois de 5 s") {
    Vida v;
    v.dano(50);
    v.atualizar(4.9f, true);
    CHECK(v.atual == doctest::Approx(50));
    v.atualizar(1.f, true);
    CHECK(v.atual > 55.f);
    CHECK(v.atual < 70.f);
    v.dano(200);
    CHECK(v.morto());
    v.atualizar(10.f, true);
    CHECK(v.morto());  // morto nao regenera
}
TEST_CASE("tiro: acerta cabeca e corpo") {
    float y0 = plano().altura(0, 10);
    std::vector<Alvo> al{{Vec3f{0, y0, 10}, 1.8f}};
    MundoColisao col;
    CHECK(lancarTiro({0, y0 + 1.656f, 0}, {0, 0, 1}, 100, al, plano(), col).parte == Parte::Cabeca);
    Acerto c = lancarTiro({0, y0 + 1.0f, 0}, {0, 0, 1}, 100, al, plano(), col);
    CHECK(c.parte == Parte::Corpo);
    CHECK(c.indice == 0);
    CHECK(lancarTiro({1, y0 + 1.0f, 0}, {0, 0, 1}, 100, al, plano(), col).indice == -1);
}
TEST_CASE("tiro: caixa bloqueia") {
    float y0 = plano().altura(0, 10);
    std::vector<Alvo> al{{Vec3f{0, y0, 10}, 1.8f}};
    MundoColisao col;
    col.adicionar({{-2, y0 - 1, 4}, {2, y0 + 3, 5}});
    Acerto a = lancarTiro({0, y0 + 1.0f, 0}, {0, 0, 1}, 100, al, plano(), col);
    CHECK(a.indice == -1);
    CHECK(a.dist == doctest::Approx(4).epsilon(0.08));
}
TEST_CASE("tiro: escolhe o mais perto") {
    float y0 = plano().altura(0, 10);
    std::vector<Alvo> al{{Vec3f{0, y0, 20}, 1.8f}, {Vec3f{0, y0, 8}, 1.8f}};
    CHECK(lancarTiro({0, y0 + 1.f, 0}, {0, 0, 1}, 100, al, plano(), MundoColisao{}).indice == 1);
}
TEST_CASE("tiro: para no terreno") {
    float y0 = plano().altura(5, 5);
    Vec3f d = normalizar(Vec3f{0, -1, 1});
    Acerto a = lancarTiro({5, y0 + 3.f, 2}, d, 100, {}, plano(), MundoColisao{});
    CHECK(a.indice == -1);
    CHECK(a.ponto.y == doctest::Approx(plano().altura(a.ponto.x, a.ponto.z)).epsilon(0.02));
    CHECK(a.dist < 6.f);
}
TEST_CASE("espalhamento: mirar parado e mais preciso") {
    CHECK(espalhamento(true, 0) < espalhamento(false, 0));
    CHECK(espalhamento(true, 0) < espalhamento(true, 5));
}
TEST_CASE("final: linhaLivre e bloqueada pelo relevo") {
    static std::unique_ptr<Terreno> real;
    if (!real) real = std::make_unique<Terreno>(carregarMapa(DATA_DIR "/lugares.txt"), 1337);
    const Terreno& t = *real;
    MundoColisao col;
    // procura dois pontos a 20 m com um morro no meio
    bool achou = false;
    for (float x = -600; x < 560 && !achou; x += 7)
        for (float z = -600; z < 600 && !achou; z += 7) {
            Vec3f a{x, t.altura(x, z) + 1.5f, z}, b{x + 40, t.altura(x + 40, z) + 1.4f, z};
            float m = t.altura(x + 20, z);
            if (m > std::max(a.y, b.y) + 0.5f) {
                achou = true;
                CHECK(col.raioLivre(a, b));
                CHECK_FALSE(linhaLivre(a, b, t, col));
            }
        }
    REQUIRE(achou);
    Vec3f a{0, plano().altura(0, 0) + 1.5f, 0}, b{0, plano().altura(0, 15) + 1.5f, 15};
    CHECK(linhaLivre(a, b, plano(), col));
}
