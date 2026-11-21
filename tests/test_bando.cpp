#include <cmath>
#include <memory>
#include "core/bando.h"
#include "core/places.h"
#include "core/terrain.h"
#include "doctest/doctest.h"

static const Terreno& plano() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(lerMapa("lugar cidade A 0 0 0 300\n"), 1);
    return *t;
}
static Vec3f chao(float x, float z) { return {x, plano().altura(x, z), z}; }
static float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
// simula ate a condicao ou o tempo acabar
template <class F>
static bool simular(Bando& b, Vec3f jog, float seg, F parar, const MundoColisao& col = MundoColisao{}, float vel = 0,
                    bool montado = false) {
    for (float t = 0; t < seg; t += 1 / 30.f) {
        b.atualizar(1 / 30.f, jog, vel, montado, false, plano(), col);
        if (parar()) return true;
    }
    return false;
}

TEST_CASE("bando: capangas 2 ou 3 e um chefe") {
    Bando a(chao(0, 0), Temperamento::Teimoso, 2, 1, plano(), MundoColisao{});
    Bando b(chao(0, 0), Temperamento::Teimoso, 3, 1, plano(), MundoColisao{});
    CHECK(a.bandidos().size() == 3);
    CHECK(b.bandidos().size() == 4);
    int chefes = 0;
    for (auto& x : b.bandidos()) chefes += x.chefe;
    CHECK(chefes == 1);
    CHECK(b.chefe()->temperamento == Temperamento::Teimoso);
}
TEST_CASE("bando: nasce livre de colisao") {
    MundoColisao col;
    col.adicionar({{-3, -10, -3}, {3, 50, 3}});  // casa em cima do esconderijo
    Bando b(chao(0, 0), Temperamento::Covarde, 3, 7, plano(), col);
    for (auto& x : b.bandidos()) {
        Vec3f r = col.resolver(x.corpo.pos, RAIO_PESSOA);
        CHECK(distXZ(r, x.corpo.pos) < 0.01f);
        CHECK(x.corpo.pos.y == doctest::Approx(plano().altura(x.corpo.pos.x, x.corpo.pos.z)).epsilon(0.01));
    }
}
TEST_CASE("bando: ve com linha de visao e nao atras de parede") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    auto alerta = [&] { return b.chefe()->estado != EstadoBandido::Vigiando; };
    CHECK(simular(b, chao(0, 25), 0.5f, alerta));
    Bando c(chao(0, 0), Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    MundoColisao parede;
    parede.adicionar({{-40, -10, 14}, {40, 60, 15}});
    auto alertaC = [&] { for (auto& x : c.bandidos()) if (x.estado != EstadoBandido::Vigiando) return true; return false; };
    CHECK_FALSE(simular(c, chao(0, 25), 2.f, alertaC, parede));
    Bando d(chao(0, 0), Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    auto alertaD = [&] { return d.chefe()->estado != EstadoBandido::Vigiando; };
    CHECK_FALSE(simular(d, chao(0, 60), 2.f, alertaD));  // longe demais para ver
}
TEST_CASE("bando: ouve tiro a 80 m") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    b.atualizar(0.1f, chao(0, 70), 0, false, true, plano(), MundoColisao{});
    CHECK(b.chefe()->estado != EstadoBandido::Vigiando);
    Bando c(chao(0, 0), Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    c.atualizar(0.1f, chao(0, 95), 0, false, true, plano(), MundoColisao{});
    CHECK(c.chefe()->estado == EstadoBandido::Vigiando);
}
TEST_CASE("bando: no maximo 2 atirando e o jogador leva tiros") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 3, 5, plano(), MundoColisao{});
    int maxAt = 0, tiros = 0;
    for (float t = 0; t < 12; t += 1 / 30.f) {
        EventosBando e = b.atualizar(1 / 30.f, chao(0, 15), 0, false, false, plano(), MundoColisao{});
        tiros += e.tirosNoJogador;
        maxAt = std::max(maxAt, b.atirando());
    }
    CHECK(maxAt <= 2);
    CHECK(maxAt >= 1);
    CHECK(tiros >= 1);
}
TEST_CASE("bando: precisao cai com distancia e velocidade") {
    CHECK(chanceAcerto(30, 0) < chanceAcerto(10, 0));
    CHECK(chanceAcerto(10, 5) < chanceAcerto(10, 0));
    CHECK(chanceAcerto(10, 0) <= 0.75f);
    CHECK(chanceAcerto(60, 5) > 0.f);
}
TEST_CASE("chefe covarde se rende abaixo de 35%") {
    Bando b(chao(0, 0), Temperamento::Covarde, 2, 3, plano(), MundoColisao{});
    int c = int(b.chefe() - &b.bandidos()[0]);
    b.levarTiro(c, Parte::Corpo);
    CHECK(b.chefe()->estado != EstadoBandido::Rendido);
    b.levarTiro(c, Parte::Corpo);
    CHECK(b.chefe()->estado == EstadoBandido::Rendido);
    EventosBando e = b.atualizar(0.1f, chao(0, 15), 0, false, false, plano(), MundoColisao{});
    CHECK(e.chefeRendeu);
}
TEST_CASE("chefe teimoso nunca se rende") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    int c = int(b.chefe() - &b.bandidos()[0]);
    b.levarTiro(c, Parte::Corpo);
    b.levarTiro(c, Parte::Corpo);
    CHECK(b.chefe()->estado != EstadoBandido::Rendido);
    CHECK(b.levarTiro(c, Parte::Corpo));
    CHECK(b.chefe()->estado == EstadoBandido::Morto);
}
TEST_CASE("chefe fujao foge mais devagar que VEL_CORRER e se rende a 4 m") {
    for (bool montado : {false, true}) {
        Bando b(chao(0, 0), Temperamento::Fujao, 2, 3, plano(), MundoColisao{});
        int c = int(b.chefe() - &b.bandidos()[0]);
        b.levarTiro(c, Parte::Corpo);
        b.levarTiro(c, Parte::Corpo);
        CHECK(b.chefe()->estado == EstadoBandido::Fugindo);
        Vec3f jog = chao(0, 20);
        Vec3f antes = b.chefe()->corpo.pos;
        simular(b, jog, 2.f, [] { return false; });
        CHECK(distXZ(b.chefe()->corpo.pos, antes) < VEL_CORRER * 2.f);
        CHECK(b.chefe()->estado == EstadoBandido::Fugindo);
        // o jogador alcanca
        Vec3f perto = b.chefe()->corpo.pos + Vec3f{3, 0, 0};
        b.atualizar(0.05f, perto, 3, montado, false, plano(), MundoColisao{});
        CHECK(b.chefe()->estado == EstadoBandido::Rendido);
    }
}
TEST_CASE("rendido que leva tiro morre") {
    Bando b(chao(0, 0), Temperamento::Covarde, 2, 3, plano(), MundoColisao{});
    int c = int(b.chefe() - &b.bandidos()[0]);
    b.levarTiro(c, Parte::Corpo);
    b.levarTiro(c, Parte::Corpo);
    REQUIRE(b.chefe()->estado == EstadoBandido::Rendido);
    CHECK(b.levarTiro(c, Parte::Corpo));
    CHECK(b.chefe()->estado == EstadoBandido::Morto);
}
TEST_CASE("tiro na cabeca mata") {
    Bando b(chao(0, 0), Temperamento::Covarde, 2, 3, plano(), MundoColisao{});
    CHECK(b.levarTiro(0, Parte::Cabeca));
    CHECK(b.bandidos()[0].estado == EstadoBandido::Morto);
    CHECK_FALSE(b.levarTiro(0, Parte::Cabeca));  // ja morto
}
TEST_CASE("capangas fogem e somem quando o chefe cai") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 3, 3, plano(), MundoColisao{});
    int c = int(b.chefe() - &b.bandidos()[0]);
    b.levarTiro(c, Parte::Cabeca);
    Vec3f jog = chao(0, 15);
    simular(b, jog, 0.2f, [] { return false; });
    for (auto& x : b.bandidos())
        if (!x.chefe) CHECK(x.estado == EstadoBandido::Fugindo);
    // fugindo e longe do jogador: somem
    for (auto& x : b.bandidos())
        if (!x.chefe) x.corpo.pos = chao(0, -130);
    simular(b, jog, 0.2f, [] { return false; });
    for (auto& x : b.bandidos())
        if (!x.chefe) CHECK(x.estado == EstadoBandido::Sumiu);
}
TEST_CASE("alvos: mesmo indice dos bandidos, mortos nao levam tiro") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    b.levarTiro(1, Parte::Cabeca);
    auto al = b.alvos();
    REQUIRE(al.size() == b.bandidos().size());
    Vec3f p = b.bandidos()[1].corpo.pos;
    Acerto a = lancarTiro(p + Vec3f{0, 1, -5}, {0, 0, 1}, 20, al, plano(), MundoColisao{});
    CHECK(a.indice != 1);
}
TEST_CASE("curarTodos nao ressuscita mortos nem solta amarrado") {
    Bando b(chao(0, 0), Temperamento::Covarde, 2, 3, plano(), MundoColisao{});
    int c = int(b.chefe() - &b.bandidos()[0]);
    int cap = c == 0 ? 1 : 0;
    b.levarTiro(cap, Parte::Cabeca);
    b.levarTiro(c, Parte::Corpo);
    b.levarTiro(c, Parte::Corpo);
    b.chefe()->estado = EstadoBandido::Amarrado;
    b.curarTodos();
    CHECK(b.bandidos()[cap].estado == EstadoBandido::Morto);
    CHECK(b.chefe()->estado == EstadoBandido::Amarrado);
}
TEST_CASE("colisao: caixasPerto acha so as caixas do raio") {
    MundoColisao col;
    col.adicionar({{5, 0, 5}, {6, 3, 6}});
    col.adicionar({{80, 0, 80}, {81, 3, 81}});
    auto v = col.caixasPerto({0, 0, 0}, 15);
    REQUIRE(v.size() == 1);
    CHECK(v[0].min.x == 5);
}
TEST_CASE("bando: sem linha de visao sai da cobertura e se aproxima") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    MundoColisao parede;
    parede.adicionar({{-6, -10, 9}, {6, 60, 10}});  // muro entre o bando e o jogador
    Vec3f jog = chao(0, 22);
    b.atualizar(0.05f, jog, 0, false, true, plano(), parede);
    float antes = distXZ(b.chefe()->corpo.pos, jog);
    int atirou = 0;
    for (float t = 0; t < 12; t += 1 / 30.f) atirou += int(b.atualizar(1 / 30.f, jog, 0, false, false, plano(), parede).claroes.size());
    CHECK(distXZ(b.chefe()->corpo.pos, jog) < antes - 2.f);
    CHECK(atirou > 0);  // contornou o muro e voltou a atirar
}
TEST_CASE("bando: capangas nao se amontoam na mesma cobertura") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 3, 4, plano(), MundoColisao{});
    MundoColisao col;
    col.adicionar({{-1, -10, 3}, {1, 60, 4}});  // uma unica caixa por perto
    Vec3f jog = chao(0, 20);
    b.atualizar(0.05f, jog, 0, false, true, plano(), col);
    for (float t = 0; t < 8; t += 1 / 30.f) b.atualizar(1 / 30.f, jog, 0, false, false, plano(), col);
    auto& v = b.bandidos();
    for (size_t i = 0; i < v.size(); i++)
        for (size_t j = i + 1; j < v.size(); j++) CHECK(distXZ(v[i].corpo.pos, v[j].corpo.pos) > 1.2f);
}
