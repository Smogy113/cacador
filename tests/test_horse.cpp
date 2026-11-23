#include <doctest/doctest.h>
#include <cmath>
#include <memory>
#include "core/horse.h"
#include "core/terrain.h"

static const Terreno& planoH() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(lerMapa("lugar cidade A 0 0 0 300\n"), 1);
    return *t;
}
static Cavalo novoCavalo(float x = 0, float z = 0) {
    Cavalo c;
    c.corpo.pos = {x, planoH().altura(x, z), z};
    return c;
}

TEST_CASE("montar só perto") {
    Cavalo c = novoCavalo();
    Corpo perto{{2, 0, 0}, 0, 0}, longe{{5, 0, 0}, 0, 0};
    perto.pos.y = longe.pos.y = c.corpo.pos.y;
    CHECK_FALSE(tentarMontar(c, longe));
    CHECK_FALSE(c.montado);
    CHECK(tentarMontar(c, perto));
    CHECK(c.montado);
}
TEST_CASE("galope se estabiliza em 9 m/s e menos de 2,6 ciclos por segundo") {
    Cavalo c = novoCavalo(-200, 0);
    c.montado = true;
    Entrada e; e.frente = 1; e.correr = true;
    for (int i = 0; i < 240; i++) moverCavalo(c, e, 1 / 60.f, planoH(), MundoColisao{});
    CHECK(c.corpo.vel == doctest::Approx(9.f));
    CHECK(c.andadura == Andadura::Galope);
    float fase = c.passada;
    float z = c.corpo.pos.z;
    for (int i = 0; i < 60; ++i) moverCavalo(c, e, 1 / 60.f, planoH(), MundoColisao{});
    CHECK((c.passada - fase) / 6.283185307f == doctest::Approx(2.58f).epsilon(.01));
    CHECK(c.corpo.pos.z - z == doctest::Approx(9.f).epsilon(.01));
}
TEST_CASE("trote sem Shift") {
    Cavalo c = novoCavalo(-200, 0);
    c.montado = true;
    Entrada e; e.frente = 1;
    for (int i = 0; i < 240; i++) moverCavalo(c, e, 1 / 60.f, planoH(), MundoColisao{});
    CHECK(c.corpo.vel == doctest::Approx(5.f));
    CHECK(c.andadura == Andadura::Trote);
}
TEST_CASE("aceleração gradual") {
    Cavalo c = novoCavalo(-200, 0);
    c.montado = true;
    Entrada e; e.frente = 1; e.correr = true;
    for (int i = 0; i < 30; i++) moverCavalo(c, e, 1 / 60.f, planoH(), MundoColisao{});
    CHECK(c.corpo.vel < VEL_GALOPE * 0.5f);
    CHECK(c.corpo.vel > 0.f);
}
TEST_CASE("freia e para sem comando") {
    Cavalo c = novoCavalo(-200, 0);
    c.montado = true;
    c.corpo.vel = VEL_GALOPE;
    for (int i = 0; i < 300; i++) moverCavalo(c, Entrada{}, 1 / 60.f, planoH(), MundoColisao{});
    CHECK(c.corpo.vel == doctest::Approx(0));
    CHECK(c.andadura == Andadura::Parado);
}
TEST_CASE("cavalo sem cavaleiro não anda") {
    Cavalo c = novoCavalo(-200, 0);
    Entrada e; e.frente = 1;
    Vec3f antes = c.corpo.pos;
    for (int i = 0; i < 60; i++) moverCavalo(c, e, 1 / 60.f, planoH(), MundoColisao{});
    CHECK(len(c.corpo.pos - antes) < 0.01f);
}
TEST_CASE("vira mais devagar em galope") {
    Cavalo a = novoCavalo(-200, 0), b = novoCavalo(-200, 50);
    a.montado = b.montado = true;
    b.corpo.vel = VEL_GALOPE;
    Entrada ea; ea.lado = 1;
    Entrada eb; eb.lado = 1; eb.frente = 1; eb.correr = true;
    moverCavalo(a, ea, 0.5f, planoH(), MundoColisao{});
    moverCavalo(b, eb, 0.5f, planoH(), MundoColisao{});
    CHECK(std::abs(b.corpo.yaw) < std::abs(a.corpo.yaw));
    CHECK(std::abs(a.corpo.yaw) > 0.3f);
}
TEST_CASE("cruzar o mapa na diagonal a galope leva menos de 4 min") {
    float min = 1400.f * std::sqrt(2.f) / VEL_GALOPE / 60.f;
    CHECK(min > 3.5f);
    CHECK(min < 4.f);
}
TEST_CASE("desmontar não coloca jogador dentro de parede") {
    Cavalo c = novoCavalo();
    c.montado = true;
    float h = c.corpo.pos.y;
    MundoColisao col;
    col.adicionar({{-3, h - 1, -2}, {-0.95f, h + 3, 2}});  // parede colada no lado direito do cavalo (-x)
    Corpo j;
    desmontar(c, j, col);
    CHECK_FALSE(c.montado);
    CHECK(len(col.resolver(j.pos, RAIO_PESSOA) - j.pos) < 1e-4f);
    CHECK(len(j.pos - c.corpo.pos) < 3.f);
}
TEST_CASE("cavalo respeita a largura de perfil e o comprimento ao girar") {
    MundoColisao col;
    col.adicionar({{-2.f, -1.f, -2.f}, {-.54f, 3.f, 2.f}});
    col.adicionar({{.54f, -1.f, -2.f}, {2.f, 3.f, 2.f}});
    CHECK(len(resolverCavalo({}, 0.f, col)) < 1e-4f);
    CHECK(len(resolverCavalo({}, 1.5707963f, col)) > .2f);
}
TEST_CASE("cavalo montado precisa de espaço para o cavaleiro sob a verga") {
    MundoColisao col;
    col.adicionarGirada({{0, 0, .18f}, 1.f, .08f, 0.f, medidas::PORTA_ALTURA, 3.f});
    CHECK(len(resolverCavalo({}, 0.f, col)) < 1e-4f);
    CHECK(len(resolverCavalo({}, 0.f, col, true)) > .1f);
}
TEST_CASE("paredão é bloqueado, rampa suave não") {
    const Terreno& t = planoH();
    Vec3f a{0, t.altura(0, 0), 0};
    CHECK(inclinacaoPermitida(t, a, a + Vec3f{1, 0.3f, 0}));
    // inclinacaoPermitida olha o terreno real: monta um caso com a borda do mapa
    Mapa m;
    Terreno serra(m, 2);
    int bloqueios = 0;
    for (float x = 640; x < 745; x += 1) {
        Vec3f p{x, serra.altura(x, 0), 0}, q{x + 1, serra.altura(x + 1, 0), 0};
        if (!inclinacaoPermitida(serra, p, q)) bloqueios++;
    }
    CHECK(bloqueios > 0);
}
TEST_CASE("não escala a montanha") {
    Mapa m;
    Terreno serra(m, 2);
    Cavalo c;
    c.corpo.pos = {600, serra.altura(600, 0), 0};
    c.corpo.yaw = 3.14159265f / 2;  // +x
    c.montado = true;
    Entrada e; e.frente = 1; e.correr = true;
    for (int i = 0; i < 1200; i++) moverCavalo(c, e, 1 / 60.f, serra, MundoColisao{});
    CHECK(c.corpo.pos.y < serra.altura(745, 0) - 5);
    CHECK(serra.dentroDoJogavel(c.corpo.pos.x, c.corpo.pos.z));
}
TEST_CASE("anda em cima da ponte mesmo com barranco embaixo") {
    Mapa m;
    Terreno serra(m, 2);
    float topo = 0;
    for (float x = 640; x < 746; x += 1) topo = std::max(topo, serra.altura(x, 0));
    MundoColisao col;
    col.adicionarPiso({{640, topo + 0.5f, -3}, {746, topo + 1.f, 3}});  // tabuleiro acima do morro
    Cavalo c;
    c.corpo.pos = {642, topo + 1.f, 0};
    c.corpo.yaw = 3.14159265f / 2;  // +x
    c.montado = true;
    Entrada e; e.frente = 1;
    for (int i = 0; i < 660; i++) moverCavalo(c, e, 1 / 60.f, serra, col);
    CHECK(c.corpo.pos.x > 690.f);  // antes da correcao travava em ~673
    CHECK(c.corpo.pos.y == doctest::Approx(topo + 1.f));
}

TEST_CASE("batidas do cavalo seguem a cadência de cada andadura") {
    constexpr float ciclo = 6.283185307f;
    CHECK(contatosCavalo(Andadura::Parado, 0, ciclo).total == 0);
    CHECK(contatosCavalo(Andadura::Passo, 0, ciclo).total == 4);
    CHECK(contatosCavalo(Andadura::Trote, 0, ciclo).total == 2);
    CHECK(contatosCavalo(Andadura::Galope, 0, ciclo).total == 4);
    CHECK(contatosCavalo(Andadura::Galope, 0, TOQUES_GALOPE.back()).total == 4);
    CHECK(contatosCavalo(Andadura::Galope, TOQUES_GALOPE.back(), ciclo).total == 0);
}

TEST_CASE("batidas do cavalo cruzam quadros e ciclos sem se duplicar") {
    constexpr float arco = 2.f * 6.283185307f;
    for (Andadura andadura : {Andadura::Passo, Andadura::Trote, Andadura::Galope}) {
        int total = 0;
        float fase = 0;
        while (fase < arco) {
            float depois = std::min(arco, fase + .039f);
            total += contatosCavalo(andadura, fase, depois).total;
            fase = depois;
        }
        CHECK(total == (andadura == Andadura::Trote ? 4 : 8));
        CHECK(contatosCavalo(andadura, arco, arco).total == 0);
        CHECK(contatosCavalo(andadura, 0, 10.f * arco).total <= 4);
    }
}
