#include <doctest/doctest.h>
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
    moverAPe(c, e, 0, 1.f, plano(), col);
    CHECK(c.pos.z == doctest::Approx(VEL_ANDAR).epsilon(0.05));
    CHECK(c.pos.x == doctest::Approx(0).epsilon(0.01));
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
    moverAPe(a, ea, 0, 1.f, plano(), col);
    moverAPe(b, eb, 0, 1.f, plano(), col);
    CHECK(dist2D(b.pos, {0, 0, 0}) > 2.5f * dist2D(a.pos, {0, 0, 0}));
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
            for (int k = 0; k < 30; k++) r.atualizar(pes, 0, 0, t, col, 0.05f);
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
    for (int k = 0; k < 60; k++) r.atualizar({0, t.altura(0, 0), 0}, 0, 0, t, col, 0.05f);
    CHECK(r.posicao().z > -3.f);
}
TEST_CASE("pitch limitado") {
    RigCamera r;
    MundoColisao col;
    r.atualizar({0, plano().altura(0, 0), 0}, 0, 100000.f, plano(), col, 0.016f);
    CHECK(r.pitch <= RigCamera::PITCH_MAX + 1e-5f);
    r.atualizar({0, plano().altura(0, 0), 0}, 0, -100000.f, plano(), col, 0.016f);
    CHECK(r.pitch >= RigCamera::PITCH_MIN - 1e-5f);
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
