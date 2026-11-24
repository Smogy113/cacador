// Corrente da Cinza (core/cinza) e a explosao no bando (core/bando).
#include <cmath>
#include <memory>
#include "core/bando.h"
#include "core/cinza.h"
#include "core/places.h"
#include "core/terrain.h"
#include "doctest/doctest.h"

namespace {
constexpr float DT = 1.f / 60.f;

// Caçador parado na origem, olhando para +z.
EntradaCinza parado(bool segurar) {
    EntradaCinza e;
    e.segurar = segurar;
    e.pes = {0, 0, 0};
    e.peito = {0, 1.35f, 0};
    e.frente = {0, 0, 1};
    e.pontoMolde = e.peito + Vec3f{0, 0, 1.2f};  // sem mira: puxa para junto do corpo
    return e;
}

EntradaCinza mirandoLonge(bool segurar, Vec3f alvo) {
    EntradaCinza e = parado(segurar);
    e.pontoMolde = alvo;
    e.moldeMirado = true;
    return e;
}

// Roda 'segundos' de relogio com a mesma entrada; o mundo anda no fator de tempo atual.
EventosCinza rodar(CorrenteCinza& c, Vida& v, const EntradaCinza& e, float segundos) {
    EventosCinza tudo;
    for (float t = 0; t < segundos - 1e-5f; t += DT) {
        EventosCinza ev = c.atualizar(DT, DT * c.fatorTempo(), e, v);
        tudo.detonacoes.insert(tudo.detonacoes.end(), ev.detonacoes.begin(), ev.detonacoes.end());
        tudo.comecouCanalizar |= ev.comecouCanalizar;
        tudo.faiscaSolta |= ev.faiscaSolta;
        tudo.faiscaVazia |= ev.faiscaVazia;
        tudo.escudoErguido |= ev.escudoErguido;
        tudo.interrompida |= ev.interrompida;
        tudo.apagadasPelaAgua += ev.apagadasPelaAgua;
    }
    return tudo;
}

const Terreno& plano() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(lerMapa("lugar cidade A 0 0 0 300\n"), 1);
    return *t;
}
}  // namespace

TEST_CASE("cinza: cada disparo deixa uma nuvem que dissipa sozinha") {
    CorrenteCinza c;
    Vida v;
    c.emitirDisparo({0, 1.4f, 0}, {0, 0, 1});
    REQUIRE(c.nuvens().size() == 1);
    CHECK(c.nuvens()[0].pos.z == doctest::Approx(c.p.distNuvemDisparo));
    CHECK(c.nuvens()[0].raio == doctest::Approx(c.p.raioNuvemDisparo));
    rodar(c, v, parado(false), c.p.vidaNuvem - 0.5f);
    REQUIRE(c.nuvens().size() == 1);
    CHECK(c.nuvens()[0].raio > c.p.raioNuvemDisparo);  // a nuvem abre com o tempo
    rodar(c, v, parado(false), 1.f);
    CHECK(c.nuvens().empty());
}

TEST_CASE("cinza: a fogueira solta fumaca sem parar e o teto de nuvens segura a memoria") {
    CorrenteCinza c;
    c.p.maxNuvens = 6;
    Vida v;
    c.adicionarFonte({20, 0, 0});
    rodar(c, v, parado(false), c.p.intervaloFogueira * 3.5f);
    CHECK(c.nuvens().size() == 4);  // uma logo ao acender e mais uma a cada intervalo
    rodar(c, v, parado(false), 30.f);
    CHECK(c.nuvens().size() <= 6);
    CHECK_FALSE(c.nuvens().empty());
}

TEST_CASE("cinza: canalizar desacelera o mundo e captura so o que esta no alcance") {
    CorrenteCinza c;
    Vida v;
    c.emitir({0, 1.5f, 6}, 1, {});                    // perto
    c.emitir({0, 1.5f, c.p.raioAtracao + 8}, 1, {});  // longe demais
    EventosCinza ev = rodar(c, v, parado(true), 0.5f);
    CHECK(ev.comecouCanalizar);
    CHECK(c.estado() == EstadoConjuracao::Canalizando);
    CHECK(c.fatorTempo() == doctest::Approx(c.p.fatorTempo));
    CHECK(c.capturadas() == 1);
    // a capturada veio na direcao do corpo; a outra ficou onde estava
    rodar(c, v, parado(true), 1.5f);
    const NuvemFumaca* perto = nullptr;
    for (const NuvemFumaca& n : c.nuvens())
        if (n.capturada) perto = &n;
    REQUIRE(perto);
    CHECK(perto->pos.z < 3.f);
    CHECK(perto->controle == doctest::Approx(1.f));
    // soltar sem tempo acabar: o mundo volta a velocidade normal
    rodar(c, v, parado(false), 0.5f);
    CHECK(c.fatorTempo() == doctest::Approx(1.f));
}

TEST_CASE("cinza: mirando, captura tambem em volta do ponto mirado") {
    CorrenteCinza c;
    Vida v;
    Vec3f alvo{0, 1.5f, 25};
    c.emitir(alvo + Vec3f{2, 0, 0}, 1, {});
    rodar(c, v, mirandoLonge(true, alvo), 0.2f);
    CHECK(c.capturadas() == 1);
}

TEST_CASE("custo de sangue: canalizar gasta vida atual e desgasta a vida maxima") {
    CorrenteCinza c;
    Vida v;
    rodar(c, v, parado(true), 2.f);
    CHECK(v.atual == doctest::Approx(100.f - 2.f * c.p.custoVidaPorSegundo).epsilon(0.02));
    CHECK(c.sangue().desgaste == doctest::Approx(2.f * c.p.desgastePorSegundo).epsilon(0.02));
    CHECK(v.max == doctest::Approx(c.sangue().vidaMaxima()));
    // a regeneracao natural nao devolve o que o desgaste tirou
    rodar(c, v, parado(false), 1.f);
    v.atualizar(60.f, true);
    CHECK(v.atual == doctest::Approx(v.max));
    CHECK(v.max < 100.f);
    // descanso na fogueira
    c.restaurarDesgaste(v);
    CHECK(v.max == doctest::Approx(100.f));
    CHECK(c.sangue().desgaste == doctest::Approx(0.f));
}

TEST_CASE("custo de sangue: nunca mata e o desgaste tem piso") {
    CorrenteCinza c;
    c.p.custoVidaPorSegundo = 500.f;
    c.p.desgastePorSegundo = 500.f;
    Vida v;
    EventosCinza ev = rodar(c, v, parado(true), 2.f);
    CHECK(v.atual >= 1.f);
    CHECK_FALSE(v.morto());
    CHECK(v.max == doctest::Approx(c.p.vidaMaximaMinima));
    CHECK(ev.interrompida);  // a vida caiu abaixo do minimo e a canalizacao parou
    CHECK(c.estado() == EstadoConjuracao::SemSangue);
}

TEST_CASE("faisca: fumaca junto ao corpo vira escudo que barra tiro pela frente") {
    CorrenteCinza c;
    Vida v;
    c.emitir({0.3f, 1.4f, 1.2f}, 1, {});
    c.emitir({-0.3f, 1.4f, 1.4f}, 1, {});
    c.emitir({0.f, 1.6f, 1.0f}, 1, {});
    rodar(c, v, parado(true), 0.4f);
    REQUIRE(c.capturadas() == 3);
    float vidaAntes = v.atual, maxAntes = v.max;
    EventosCinza ev = rodar(c, v, parado(false), DT);
    CHECK(ev.faiscaSolta);
    CHECK(ev.escudoErguido);
    CHECK(ev.detonacoes.empty());
    CHECK(c.escudo().ativo());
    CHECK(c.escudo().absorcao == doctest::Approx(c.p.absorcaoEscudoMax));
    CHECK(v.atual < vidaAntes);  // a faisca tambem cobra
    CHECK(v.max < maxAntes);
    Vec3f peito{0, 1.35f, 0};
    CHECK(c.fatorDano({0, 1.4f, 20}, peito) == doctest::Approx(1.f - c.p.absorcaoEscudoMax));
    CHECK(c.fatorDano({0, 1.4f, -20}, peito) == doctest::Approx(1.f));  // pelas costas
    CHECK(c.fatorDano({20, 1.4f, 0.5f}, peito) == doctest::Approx(1.f));  // de lado, fora do retangulo
    // dura o tempo configurado (tempo do mundo)
    rodar(c, v, parado(false), c.p.duracaoEscudo + 0.3f);
    CHECK_FALSE(c.escudo().ativo());
}

TEST_CASE("faisca: fumaca lancada longe detona e a explosao corre em cadeia") {
    CorrenteCinza c;
    Vida v;
    Vec3f alvo{0, 1.5f, 15};
    c.p.raioAtracao = 3.f;                       // so a primeira cabe no alcance da captura
    c.emitir(alvo, 1, {});                       // sera capturada (perto do ponto mirado)
    c.emitir(alvo + Vec3f{6.5f, 0, 0}, 1, {});   // 2o elo: fora da captura e longe demais da primeira
    c.emitir(alvo + Vec3f{10.f, 0, 0}, 1, {});   // 3o elo
    c.emitir(alvo + Vec3f{40, 0, 0}, 1, {});     // isolada: nao pega fogo
    rodar(c, v, mirandoLonge(true, alvo), 0.3f);
    REQUIRE(c.capturadas() == 1);
    // ponte: a 3 m da capturada e a 3,5 m da segunda (raioPropagacao = 4,5 m)
    c.emitir(alvo + Vec3f{3.f, 0, 0}, 1, {});
    EventosCinza soltar = rodar(c, v, mirandoLonge(false, alvo), DT);
    REQUIRE(soltar.detonacoes.size() == 1);
    CHECK(soltar.detonacoes[0].primeira);
    CHECK_FALSE(soltar.escudoErguido);
    EventosCinza cadeia = rodar(c, v, parado(false), 1.f);
    CHECK(cadeia.detonacoes.size() == 3);  // ponte, segunda e terceira
    for (const GolpeDetonacao& g : cadeia.detonacoes) CHECK_FALSE(g.primeira);
    CHECK(c.nuvens().size() == 1);  // a isolada continua no ar
}

TEST_CASE("faisca: soltar sem fumaca nao faz nada e nao cobra") {
    CorrenteCinza c;
    Vida v;
    rodar(c, v, parado(true), 0.2f);
    float vida = v.atual, max = v.max;
    EventosCinza ev = rodar(c, v, parado(false), DT);
    CHECK(ev.faiscaVazia);
    CHECK(v.atual == doctest::Approx(vida));
    CHECK(v.max == doctest::Approx(max));
}

TEST_CASE("detonacao: dano e empurrao caem ate a borda") {
    GolpeDetonacao g{{0, 0, 0}, 4.f, 60.f, 8.f, true};
    CHECK(danoNaDistancia(g, 0.f) == doctest::Approx(60.f));
    CHECK(danoNaDistancia(g, 2.f) == doctest::Approx(30.f));
    CHECK(danoNaDistancia(g, 4.f) == doctest::Approx(0.f));
    CHECK(empurraoNaDistancia(g, 1.f) == doctest::Approx(6.f));
}

TEST_CASE("umidade: a agua apaga a fumaca e bloqueia a conjuracao") {
    CorrenteCinza c;
    Vida v;
    c.definirZonasUmidas({ZonaUmida{{10, 0, 0}, 3.f, 10.f}});
    c.emitir({10, 1.5f, 0}, 1, {});
    c.emitir({0, 1.5f, 3}, 1, {});
    EventosCinza ev = rodar(c, v, parado(false), DT);
    CHECK(ev.apagadasPelaAgua == 1);
    CHECK(c.nuvens().size() == 1);
    // molhado nao canaliza
    EntradaCinza molhado = parado(true);
    molhado.jogadorUmido = true;
    rodar(c, v, molhado, 0.3f);
    CHECK(c.estado() == EstadoConjuracao::Bloqueada);
    CHECK(c.fatorTempo() == doctest::Approx(1.f));
    CHECK(v.atual == doctest::Approx(100.f));
    // canalizando e entrando na chuva: para sem faisca e solta a fumaca
    rodar(c, v, parado(true), 0.3f);
    REQUIRE(c.estado() == EstadoConjuracao::Canalizando);
    ev = rodar(c, v, molhado, DT);
    CHECK(ev.interrompida);
    CHECK(c.capturadas() == 0);
    CHECK(ev.detonacoes.empty());
}

TEST_CASE("umidade: o escudo se desfaz quando o cacador entra na agua") {
    CorrenteCinza c;
    Vida v;
    c.emitir({0, 1.4f, 1.2f}, 1, {});
    rodar(c, v, parado(true), 0.3f);
    rodar(c, v, parado(false), DT);
    REQUIRE(c.escudo().ativo());
    EntradaCinza molhado = parado(false);
    molhado.jogadorUmido = true;
    rodar(c, v, molhado, DT);
    CHECK_FALSE(c.escudo().ativo());
}

TEST_CASE("bando: explosao fere, empurra para fora e alerta") {
    Vec3f esc{0, plano().altura(0, 0), 0};
    Bando b(esc, Temperamento::Teimoso, 2, 3, plano(), MundoColisao{});
    Bandido& alvo = b.bandidos()[0];
    Vec3f antes = alvo.corpo.pos;
    Vec3f centro = antes + Vec3f{-1.f, 0.9f, 0};  // 1 m a oeste do meio do corpo
    int mortos = -1;
    int atingidos = b.explosao(centro, 3.f, 30.f, 8.f, &mortos);
    CHECK(atingidos >= 1);
    CHECK(mortos == 0);
    CHECK(alvo.vida.atual == doctest::Approx(100.f - 30.f * (2.f / 3.f)).epsilon(0.01));
    CHECK(alvo.empurrao.x > 0.f);
    // jogador longe em +z: o passo de combate vai em z e nao mascara o empurrao em x
    for (int i = 0; i < 30; i++) b.atualizar(DT, {antes.x, 0, antes.z + 60}, 0, false, false, plano(), MundoColisao{});
    CHECK(alvo.corpo.pos.x > antes.x + 0.3f);  // deslizou para longe do centro
    CHECK(alvo.estado != EstadoBandido::Vigiando);
    // dano bastante mata
    b.explosao(alvo.corpo.pos + Vec3f{0, 0.9f, 0}, 3.f, 500.f, 0.f, &mortos);
    CHECK(alvo.estado == EstadoBandido::Morto);
    CHECK(mortos >= 1);
}
