#include <doctest/doctest.h>
#include <cmath>
#include <memory>
#include "core/camrig.h"
#include "core/motion.h"
#include "core/terrain.h"

// terreno plano em volta da origem (um lugar grande aplaina tudo)
static const Terreno& plano() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(lerMapa("lugar cidade A 0 0 0 300\n"), 1);
    return *t;
}

static float dist2D(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }

TEST_CASE("anda na direção da câmera") {
    MundoColisao col;
    Corpo c{{0, 0, 0}, 0, 0};
    Entrada e; e.frente = 1;
    moverAPe(c, e, 0, 1.f, plano(), col);  // embala
    float z0 = c.pos.z;
    moverAPe(c, e, 0, 1.f, plano(), col);  // ja em velocidade de cruzeiro
    CHECK(c.pos.z - z0 == doctest::Approx(VEL_ANDAR).epsilon(0.05));
    CHECK(c.pos.x == doctest::Approx(0).epsilon(0.01));
}
TEST_CASE("acelera e freia em vez de saltar de velocidade") {
    MundoColisao col;
    Corpo c{{0, 0, 0}, 0, 0};
    Entrada e; e.frente = 1;
    moverAPe(c, e, 0, 1.f / 60, plano(), col);
    CHECK(c.vel < 0.15f);  // um quadro depois ainda esta embalando
    for (int i = 0; i < 30; i++) moverAPe(c, e, 0, 1.f / 60, plano(), col);
    CHECK(c.vel == doctest::Approx(VEL_ANDAR));  // meio segundo: ja no passo normal
    // solta o comando: continua em frente, freando, e para em pouco espaco
    Entrada solto;
    float z0 = c.pos.z;
    moverAPe(c, solto, 0, 1.f / 60, plano(), col);
    CHECK(c.vel > 0.f);
    CHECK(c.pos.z > z0);
    for (int i = 0; i < 30; i++) moverAPe(c, solto, 0, 1.f / 60, plano(), col);
    CHECK(c.vel == 0.f);
    CHECK(c.pos.z - z0 < 0.3f);
}
TEST_CASE("meia-volta: freia antes de girar o corpo") {
    MundoColisao col;
    Corpo c{{0, 0, 0}, 0, 0};
    Entrada e; e.frente = 1;
    for (int i = 0; i < 60; i++) moverAPe(c, e, 0, 1.f / 60, plano(), col);
    Entrada volta; volta.frente = -1;
    moverAPe(c, volta, 0, 1.f / 60, plano(), col);
    CHECK(c.vel < VEL_ANDAR);  // ninguem da meia-volta em velocidade cheia
    for (int i = 0; i < 120; i++) moverAPe(c, volta, 0, 1.f / 60, plano(), col);
    CHECK(c.vel == doctest::Approx(VEL_ANDAR));  // ja virado, volta ao passo normal
    CHECK(std::abs(std::cos(c.yaw) + 1.f) < 0.01f);  // olhando para -z
}
TEST_CASE("mirando anda para trás sem frear nem girar") {
    MundoColisao col;
    Corpo c{{0, 0, 0}, 0, 0};
    Entrada e; e.frente = -1; e.mirar = true;
    for (int i = 0; i < 60; i++) moverAPe(c, e, 0, 1.f / 60, plano(), col);
    CHECK(c.pos.z < -1.f);
    CHECK(c.vel == doctest::Approx(VEL_ANDAR));
}
TEST_CASE("direita da câmera") {
    MundoColisao col;
    Corpo c{{0, 0, 0}, 0, 0};
    Entrada e; e.lado = 1;
    moverAPe(c, e, 0, 1.f, plano(), col);
    CHECK(c.pos.x < -1.f);  // camera olhando +z: a direita e -x
}
TEST_CASE("correr é mais rápido") {
    MundoColisao col;
    Corpo a{{0, 0, 0}, 0, 0}, b{{0, 0, 0}, 0, 0};
    Entrada ea; ea.frente = 1;
    Entrada eb = ea; eb.correr = true;
    moverAPe(a, ea, 0, 1.f, plano(), col);  // embala
    moverAPe(b, eb, 0, 1.f, plano(), col);
    Vec3f a0 = a.pos, b0 = b.pos;
    moverAPe(a, ea, 0, 1.f, plano(), col);
    moverAPe(b, eb, 0, 1.f, plano(), col);
    CHECK(dist2D(b.pos, b0) > 2.5f * dist2D(a.pos, a0));
}
TEST_CASE("fica no chão") {
    MundoColisao col;
    Corpo c{{10, 50, 10}, 0, 0};
    Entrada e; e.frente = 1;
    moverAPe(c, e, 0, 0.5f, plano(), col);
    CHECK(c.pos.y == doctest::Approx(plano().altura(c.pos.x, c.pos.z)));
}
TEST_CASE("não atravessa parede mesmo com dt enorme") {
    MundoColisao col;
    float h = plano().altura(0, 0);
    col.adicionar({{-5, h - 1, 2}, {5, h + 30, 3}});
    Corpo c{{0, h, 0}, 0, 0};
    Entrada e; e.frente = 1; e.correr = true;
    moverAPe(c, e, 0, limitarDt(5.f), plano(), col);
    moverAPe(c, e, 0, limitarDt(5.f), plano(), col);
    CHECK(c.pos.z < 2.f);
}
TEST_CASE("não sai do mapa") {
    MundoColisao col;
    Corpo c{{699, 0, 0}, 0, 0};
    Entrada e; e.lado = -1; e.correr = true;  // esquerda da camera = +x
    for (int i = 0; i < 100; i++) moverAPe(c, e, 0, 0.1f, plano(), col);
    CHECK(plano().dentroDoJogavel(c.pos.x, c.pos.z));
}
TEST_CASE("limitarDt") {
    CHECK(limitarDt(5.f) == 0.1f);
    CHECK(limitarDt(0.016f) == 0.016f);
}
TEST_CASE("resolver empurra para fora da caixa") {
    MundoColisao col;
    col.adicionar({{-1, 0, -1}, {1, 3, 1}});
    Vec3f p = col.resolver({0.9f, 0, 0}, 0.4f);
    CHECK(p.x >= 1.4f - 1e-4f);
}
TEST_CASE("raioLivre") {
    MundoColisao col;
    col.adicionar({{-1, 0, -1}, {1, 3, 1}});
    CHECK_FALSE(col.raioLivre({-5, 1, 0}, {5, 1, 0}));
    CHECK(col.raioLivre({-5, 5, 0}, {5, 5, 0}));
}
TEST_CASE("câmera nunca fica dentro do morro") {
    Mapa m;  // morros naturais
    Terreno t(m, 3);
    MundoColisao col;
    RigCamera r;
    for (float x = -400; x <= 400; x += 37)
        for (float pitch : {-0.35f, 0.f, 0.5f}) {
            r.pitch = pitch;
            Vec3f pes{x, t.altura(x, x * 0.5f), x * 0.5f};
            r.posicionar(pes + Vec3f{0, 1.6f, 0}, 4.f, .45f, t, col, 0.f);
            Vec3f c = r.posicao();
            CHECK(c.y >= t.altura(c.x, c.z) + 0.5f - 1e-3f);
        }
}
TEST_CASE("câmera recua quando tem parede atrás") {
    Terreno const& t = plano();
    MundoColisao col;
    float h = t.altura(0, 0);
    col.adicionar({{-5, h - 1, -4}, {5, h + 10, -3}});  // parede atras (camera olha +z, fica em -z)
    RigCamera r;
    r.pitch = 0.1f;
    r.posicionar({0, h + 1.6f, 0}, 4.f, .45f, t, col, 0.f);
    CHECK(r.posicao().z > -3.f);
    CHECK_FALSE(r.primeiraPessoa());
}
TEST_CASE("pitch limitado") {
    RigCamera r;
    r.girar(0, 100000.f, false);
    CHECK(r.pitch <= RigCamera::PITCH_MAX + 1e-5f);
    r.girar(0, -100000.f, false);
    CHECK(r.pitch >= RigCamera::PITCH_MIN - 1e-5f);
}

TEST_CASE("câmera aproxima pela rodinha e sai da primeira pessoa ao afastar") {
    RigCamera r;
    CHECK(r.distanciaZoom(4.f) == 4.f);
    CHECK(r.distanciaZoom(6.f) == 6.f);
    r.ajustarZoom(1.f, 4.f);
    CHECK(r.distanciaZoom(4.f) < 4.f);
    CHECK(r.distanciaZoom(6.f) == r.distanciaZoom(4.f));
    CHECK_FALSE(r.primeiraPessoa());

    r.ajustarZoom(100.f, 4.f);
    REQUIRE(r.primeiraPessoa());
    CHECK(r.distanciaZoom(6.f) == 0.f);
    Vec3f olhos{3.f, plano().altura(3, 4) + 1.7f, 4.f};
    r.girar(0, -100000.f, true);
    r.posicionar(olhos, r.distanciaZoom(4.f), .45f, plano(), MundoColisao{}, 1.f / 60.f);
    CHECK(r.posicao() == olhos);
    CHECK(r.olhar().y > olhos.y);
    CHECK(len(r.olhar() - olhos) == doctest::Approx(1.f));

    r.ajustarZoom(-1.f, 6.f);
    CHECK_FALSE(r.primeiraPessoa());
    CHECK(r.distanciaZoom(6.f) > 1.f);
    r.ajustarZoom(-100.f, 6.f);
    CHECK(r.distanciaZoom(6.f) == doctest::Approx(9.f));
}

TEST_CASE("em primeira pessoa anda para trás e de lado mantendo a direção do olhar") {
    Corpo c{{0, plano().altura(0, 0), 0}, 1.f, 0.f};
    Entrada e;
    e.frente = -1.f;
    e.lado = 1.f;
    moverAPe(c, e, 0.f, 1.f, plano(), MundoColisao{}, true);
    CHECK(c.pos.x < -.5f);
    CHECK(c.pos.z < -.5f);
    CHECK(c.yaw == 0.f);
    CHECK(c.vel == doctest::Approx(VEL_ANDAR));
}

TEST_CASE("anda sobre a ponte (piso) em vez de afundar no rio") {
    MundoColisao col;
    float h = plano().altura(0, 0);
    col.adicionarPiso({{-2, h, 0}, {2, h + 0.5f, 40}});  // tabuleiro meio metro acima do chao
    Corpo c{{0, h, -3}, 0, 0};
    Entrada e; e.frente = 1;
    moverAPe(c, e, 0, 3.f, plano(), col);  // sobe no tabuleiro
    CHECK(c.pos.z > 0.5f);
    CHECK(c.pos.y == doctest::Approx(h + 0.5f));
}
TEST_CASE("piso alto demais não teletransporta para cima") {
    MundoColisao col;
    float h = plano().altura(0, 0);
    col.adicionarPiso({{-2, h, 0}, {2, h + 5.0f, 40}});  // telhado, nao e degrau
    CHECK(col.alturaPiso(0, 5, h) < h + 1.f);
}

TEST_CASE("câmera não entra na parede colada no lado direito do jogador") {
    Terreno const& t = plano();
    float h = t.altura(0, 0);
    for (float yawOff : {0.f, 0.6f}) {
        MundoColisao col;
        col.adicionar({{-3, h - 1, -6}, {-0.4f, h + 8, 6}});  // parede a direita (camera olhando +z: direita = -x)
        RigCamera r;
        r.yaw = yawOff;
        r.pitch = 0.15f;
        r.posicionar({0, h + 1.6f, 0}, 4.f, .45f, t, col, 0.f);
        Vec3f c = r.posicao();
        CHECK_MESSAGE(c.x > -0.4f, "camera dentro da parede: x=" << c.x);
    }
}
