// Circuitos de Queima: receitas, arsenal, pavio, projétil encantado, golpes e física dos alvos.
#include <cmath>
#include <memory>
#include "core/bando.h"
#include "core/circuito.h"
#include "core/fisica_alvo.h"
#include "core/places.h"
#include "core/projetil_encantado.h"
#include "core/terrain.h"
#include "doctest/doctest.h"

namespace {
constexpr float DT = 1.f / 60.f;

const Terreno& plano() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(lerMapa("lugar cidade A 0 0 0 300\n"), 1);
    return *t;
}
Vec3f chao(float x, float z) { return {x, plano().altura(x, z), z}; }

const Circuito ESPIRAL_RESINA{Entalhe::Espiral, Pasta::ResinaECera, Doador::Pena};
const Circuito GRADE_ENXOFRE{Entalhe::Grade, Pasta::EnxofreESal, Doador::PedraPesada};
const Circuito ONDA_PRATA{Entalhe::Onda, Pasta::PoDePrata, Doador::CarvaoEmBrasa};

// Roda o mundo até não sobrar projétil (ou o tempo acabar) e junta os eventos.
EventosCircuito voar(CircuitosNoMundo& m, const MundoColisao& col, const std::vector<Alvo>& alvos = {}, float seg = 4.f) {
    EventosCircuito tudo;
    for (float t = 0; t < seg && !m.projeteis().empty(); t += DT) {
        EventosCircuito ev = m.atualizar(DT, plano(), col, alvos);
        tudo.impactos.insert(tudo.impactos.end(), ev.impactos.begin(), ev.impactos.end());
        tudo.ricochetes.insert(tudo.ricochetes.end(), ev.ricochetes.begin(), ev.ricochetes.end());
        tudo.desencantos.insert(tudo.desencantos.end(), ev.desencantos.begin(), ev.desencantos.end());
        tudo.sumidos.insert(tudo.sumidos.end(), ev.sumidos.begin(), ev.sumidos.end());
    }
    return tudo;
}
}  // namespace

TEST_CASE("circuitos: cada entalhe exige o seu doador") {
    CHECK(doadorDaReceita(Entalhe::Espiral) == Doador::Pena);
    CHECK(doadorDaReceita(Entalhe::Grade) == Doador::PedraPesada);
    CHECK(doadorDaReceita(Entalhe::Onda) == Doador::CarvaoEmBrasa);
    CHECK(ESPIRAL_RESINA.valido());
    CHECK_FALSE((Circuito{Entalhe::Onda, Pasta::PolvoraFina, Doador::Pena}.valido()));
    CHECK(nomeCombinacao(ESPIRAL_RESINA) == "Espiral + Resina e Cera + Pena");
}

TEST_CASE("circuitos: a pasta decide como a propriedade sai") {
    ParametrosCircuito p;
    EfeitoCircuito e = resolverEfeito(ESPIRAL_RESINA, p);
    CHECK(e.propriedade == Propriedade::Velocidade);
    CHECK(e.gravidadeMult == 0.f);
    CHECK(e.ricochetes == p.ricochetes);
    CHECK(e.velocidade == doctest::Approx(p.velEspiral));

    e = resolverEfeito(GRADE_ENXOFRE, p);
    CHECK(e.propriedade == Propriedade::Peso);
    CHECK(e.raio == doctest::Approx(p.raioEnxofre));
    CHECK(e.ricochetes == 0);

    e = resolverEfeito(ONDA_PRATA, p);  // Pó de Prata inverte: calor vira frio
    CHECK(e.propriedade == Propriedade::Frio);
    CHECK(e.duracaoZona == doctest::Approx(p.duracaoZona));

    e = resolverEfeito({Entalhe::Grade, Pasta::PoDePrata, Doador::PedraPesada}, p);
    CHECK(e.propriedade == Propriedade::Levante);
    e = resolverEfeito({Entalhe::Espiral, Pasta::PoDePrata, Doador::Pena}, p);
    CHECK(e.propriedade == Propriedade::Arrasto);
    CHECK(e.gravidadeMult > 1.f);

    e = resolverEfeito({Entalhe::Onda, Pasta::ResinaECera, Doador::CarvaoEmBrasa}, p);
    CHECK(e.duracaoZona == doctest::Approx(p.duracaoZona * p.multPersistente));  // persistente dura mais
    e = resolverEfeito({Entalhe::Onda, Pasta::PolvoraFina, Doador::CarvaoEmBrasa}, p);
    CHECK(e.duracaoZona == 0.f);  // pólvora: tudo no ponto, sem zona
    CHECK(e.intensidade == doctest::Approx(p.intensidadeConcentrada));
    CHECK(descreverEfeito({Entalhe::Onda, Pasta::PolvoraFina, Doador::Pena}, p).rfind("Receita inválida", 0) == 0);
}

TEST_CASE("arsenal: gravar prende o doador, regravar e esvaziar devolvem") {
    ParametrosCircuito p;
    p.estoqueInicialDoador = 2;
    Arsenal a(p);
    CHECK(a.slots() == 3);
    CHECK(a.preparar(0, ESPIRAL_RESINA) == ResultadoPreparo::Ok);
    CHECK(a.estoque(Doador::Pena) == 1);
    CHECK(a.preparar(1, {Entalhe::Espiral, Pasta::PolvoraFina, Doador::Pena}) == ResultadoPreparo::Ok);
    CHECK(a.preparar(2, ESPIRAL_RESINA) == ResultadoPreparo::SemDoador);
    CHECK(a.preparar(2, {Entalhe::Grade, Pasta::PolvoraFina, Doador::Pena}) == ResultadoPreparo::ReceitaInvalida);
    CHECK(a.preparar(3, GRADE_ENXOFRE) == ResultadoPreparo::SlotInvalido);
    // regravar o slot 0 com a mesma pena: não precisa de estoque
    CHECK(a.preparar(0, {Entalhe::Espiral, Pasta::EnxofreESal, Doador::Pena}) == ResultadoPreparo::Ok);
    CHECK(a.estoque(Doador::Pena) == 0);
    // regravar com outro entalhe devolve a pena
    CHECK(a.preparar(0, GRADE_ENXOFRE) == ResultadoPreparo::Ok);
    CHECK(a.estoque(Doador::Pena) == 1);
    CHECK(a.estoque(Doador::PedraPesada) == 1);
    a.esvaziar(1);
    CHECK(a.estoque(Doador::Pena) == 2);
    CHECK_FALSE(a.slot(1).cheio);
}

TEST_CASE("arsenal: troca rapida e consumo no disparo") {
    Arsenal a;
    a.preparar(0, ESPIRAL_RESINA);
    a.preparar(2, ONDA_PRATA);
    CHECK(a.selecionado() == -1);  // começa na munição comum
    a.proximo();
    CHECK(a.selecionado() == 0);
    a.proximo();
    CHECK(a.selecionado() == 2);  // pula o slot vazio
    a.proximo();
    CHECK(a.selecionado() == -1);
    a.selecionar(1);
    CHECK_FALSE(a.preparadaNaVez());  // slot vazio atira comum
    a.selecionar(2);
    REQUIRE(a.circuitoNaVez());
    Circuito c = a.consumirNaVez();
    CHECK(c.entalhe == Entalhe::Onda);
    CHECK_FALSE(a.slot(2).cheio);
    CHECK(a.selecionado() == -1);
    CHECK(a.estoque(Doador::CarvaoEmBrasa) == ParametrosCircuito{}.estoqueInicialDoador - 1);  // queimou
}

TEST_CASE("pavio: molha na agua, fica umido depois de sair, seca e o Ferro Frio bloqueia") {
    Pavio pv;
    CHECK(pv.estado() == EstadoPavio::Seco);
    pv.atualizar(DT, true, false, 5.f);
    CHECK(pv.estado() == EstadoPavio::Molhado);  // dentro da agua
    pv.atualizar(DT, false, false, 5.f);
    CHECK(pv.estado() == EstadoPavio::Umido);  // acabou de sair: da para forcar (core/rebote)
    CHECK(pv.fracaoUmidade(5.f) > 0.99f);
    pv.atualizar(4.8f, false, false, 5.f);
    CHECK(pv.estado() == EstadoPavio::Umido);
    pv.atualizar(0.2f, false, false, 5.f);
    CHECK(pv.estado() == EstadoPavio::Seco);
    pv.atualizar(DT, false, true, 5.f);
    CHECK(pv.estado() == EstadoPavio::BloqueadoFerro);
}

TEST_CASE("projetil: a Espiral ignora a gravidade e as outras caem") {
    MundoColisao col;
    CircuitosNoMundo m;
    Vec3f boca = chao(0, -100) + Vec3f{0, 3.f, 0};
    m.disparar(boca, {0, 0, 1}, {Entalhe::Espiral, Pasta::PolvoraFina, Doador::Pena});
    m.disparar(boca + Vec3f{2, 0, 0}, {0, 0, 1}, {Entalhe::Grade, Pasta::PolvoraFina, Doador::PedraPesada});
    for (int i = 0; i < 15; i++) m.atualizar(DT, plano(), col, {});
    REQUIRE(m.projeteis().size() == 2);
    CHECK(m.projeteis()[0].pos.y == doctest::Approx(boca.y));
    CHECK(m.projeteis()[1].pos.y < boca.y - 0.2f);
    CHECK(m.projeteis()[0].pos.z > m.projeteis()[1].pos.z);  // e é bem mais rápida
}

TEST_CASE("projetil: Espiral + Resina + Pena ricocheteia 3 vezes nas paredes") {
    MundoColisao col;
    Vec3f c = chao(0, 0);
    col.adicionar({{-30, c.y - 5, 10}, {30, c.y + 20, 11}});   // parede ao norte
    col.adicionar({{-30, c.y - 5, -11}, {30, c.y + 20, -10}});  // parede ao sul
    CircuitosNoMundo m;
    m.disparar(c + Vec3f{0, 1.5f, 0}, {0.05f, 0, 1}, ESPIRAL_RESINA);
    EventosCircuito ev = voar(m, col);
    CHECK(ev.ricochetes.size() == 3);
    REQUIRE(ev.impactos.size() == 1);
    CHECK(ev.impactos[0].encantado);
    CHECK(ev.impactos[0].ponto.y == doctest::Approx(c.y + 1.5f).epsilon(0.02));  // sem cair entre as quicadas
    CHECK(std::abs(ev.impactos[0].normal.z) > 0.9f);  // bateu numa parede, não no chão
}

TEST_CASE("projetil: sem Resina nao ricocheteia") {
    MundoColisao col;
    Vec3f c = chao(0, 0);
    col.adicionar({{-30, c.y - 5, 10}, {30, c.y + 20, 11}});
    CircuitosNoMundo m;
    m.disparar(c + Vec3f{0, 1.5f, 0}, {0, 0, 1}, {Entalhe::Espiral, Pasta::PolvoraFina, Doador::Pena});
    EventosCircuito ev = voar(m, col);
    CHECK(ev.ricochetes.empty());
    CHECK(ev.impactos.size() == 1);
}

TEST_CASE("projetil: atravessar o Ferro Frio vira chumbo e nao deixa zona") {
    MundoColisao col;
    Vec3f c = chao(0, 0);
    CircuitosNoMundo m;
    m.definirFerro({ZonaFerroFrio{{{-3, c.y - 1, 4}, {3, c.y + 4, 5}}}});
    // Onda + Pó de Prata atirada para o chão do outro lado da barreira
    m.disparar(c + Vec3f{0, 1.5f, 0}, normalizar(Vec3f{0, -0.12f, 1}), ONDA_PRATA);
    EventosCircuito ev = voar(m, col);
    CHECK(ev.desencantos.size() == 1);
    REQUIRE(ev.impactos.size() == 1);
    CHECK_FALSE(ev.impactos[0].encantado);
    CHECK(m.zonas().empty());
    CHECK(m.noFerro(c + Vec3f{0, 1, 4.5f}));
}

TEST_CASE("projetil: Onda + Po de Prata + Carvao cria zona congelante onde atinge") {
    MundoColisao col;
    Vec3f c = chao(0, 0);
    CircuitosNoMundo m;
    m.disparar(c + Vec3f{0, 1.5f, 0}, normalizar(Vec3f{0, -0.2f, 1}), ONDA_PRATA);
    EventosCircuito ev = voar(m, col);
    REQUIRE(ev.impactos.size() == 1);
    REQUIRE(m.zonas().size() == 1);
    const ZonaElemental z = m.zonas()[0];
    CHECK(z.frio);
    CHECK(z.raio == doctest::Approx(m.p.raioPadrao));
    CHECK(z.centro.y == doctest::Approx(plano().altura(z.centro.x, z.centro.z)));
    // dentro deixa lento (sem parar por completo), fora nada
    const GolpeAlvo dentro = golpeDaZona(z, z.centro + Vec3f{1, 0, 0}, DT, m.p);
    CHECK(dentro.lentidao > 0.f);
    CHECK(dentro.fatorLento == doctest::Approx(m.p.fatorLentidaoGelo));
    CHECK(dentro.congelar == 0.f);
    CHECK(golpeDaZona(z, z.centro + Vec3f{z.raio + 1, 0, 0}, DT, m.p).vazio());
    // e some depois da duração
    for (float t = 0; t < m.p.duracaoZona + 0.1f; t += DT) m.atualizar(DT, plano(), col, {});
    CHECK(m.zonas().empty());
}

TEST_CASE("projetil: acerta o alvo no caminho") {
    MundoColisao col;
    Vec3f c = chao(0, 0);
    CircuitosNoMundo m;
    std::vector<Alvo> alvos{{chao(0, 15), 1.8f}};
    m.disparar(c + Vec3f{0, 1.3f, 0}, {0, 0, 1}, GRADE_ENXOFRE);
    EventosCircuito ev = voar(m, col, alvos);
    REQUIRE(ev.impactos.size() == 1);
    CHECK(ev.impactos[0].alvo == 0);
    CHECK(ev.impactos[0].parte == Parte::Corpo);
}

TEST_CASE("golpe: a onda de pressao da Grade empurra para fora e derruba quem esta no raio") {
    ParametrosCircuito p;
    Impacto im;
    im.ponto = {0, 0, 0};
    im.circuito = GRADE_ENXOFRE;
    im.efeito = resolverEfeito(GRADE_ENXOFRE, p);
    GolpeAlvo perto = golpeDoImpacto(im, {2, 0.9f, 0}, false, p);
    CHECK(perto.empurrao.x > 0.f);
    CHECK(perto.derrubar > 0.f);
    CHECK(perto.atordoar > perto.derrubar);  // levanta zonzo
    CHECK(perto.dano > 0.f);
    CHECK(golpeDoImpacto(im, {p.raioEnxofre + 1, 0.9f, 0}, false, p).vazio());
    // Pó de Prata na Grade: levanta em vez de esmagar
    im.circuito = {Entalhe::Grade, Pasta::PoDePrata, Doador::PedraPesada};
    im.efeito = resolverEfeito(im.circuito, p);
    GolpeAlvo sobe = golpeDoImpacto(im, {1, 0.9f, 0}, false, p);
    CHECK(sobe.levante > 0.f);
    CHECK(sobe.derrubar == 0.f);
    // chumbo não tem efeito mágico
    im.encantado = false;
    CHECK(golpeDoImpacto(im, {1, 0.9f, 0}, true, p).vazio());
}

TEST_CASE("fisica do alvo: arremessado sobe, fica sem controle e cai com dano") {
    MundoColisao col;
    FisicaAlvo f;
    Vec3f pos = chao(0, 0);
    GolpeAlvo g;
    g.levante = 14.f;
    f.aplicar(g);
    CHECK(f.ocupado());
    float alturaMax = 0, dano = 0;
    for (int i = 0; i < 240; i++) {
        dano += f.atualizar(DT, pos, plano(), col, 0.3f);
        alturaMax = std::max(alturaMax, f.voo);
    }
    CHECK(alturaMax > 8.f);
    CHECK(dano > 0.f);  // caiu de mais de 10 m
    CHECK_FALSE(f.noAr());
    CHECK_FALSE(f.ocupado());  // já levantou
}

TEST_CASE("boneco de treino: e destruido e reaparece na base") {
    MundoColisao col;
    AlvoTreino b;
    b.base = b.pos = chao(5, 5);
    GolpeAlvo g;
    g.dano = 150.f;
    g.empurrao = {5, 0, 0};
    CHECK(golpear(b, g));
    CHECK_FALSE(b.ativo());
    for (float t = 0; t < b.tempoReaparecer + 0.1f; t += DT) atualizar(b, DT, plano(), col);
    CHECK(b.ativo());
    CHECK(b.pos.x == doctest::Approx(b.base.x));
}

TEST_CASE("bando: congelado nao atira e arremessado sobe no alvo") {
    Bando b(chao(0, 0), Temperamento::Teimoso, 2, 5, plano(), MundoColisao{});
    Vec3f jog = chao(0, 15);
    for (int i = 0; i < 120; i++) b.atualizar(DT, jog, 0, false, true, plano(), MundoColisao{});  // entra em combate
    GolpeAlvo gelo;
    gelo.congelar = 2.f;
    for (int i = 0; i < int(b.bandidos().size()); i++) b.aplicarGolpe(i, gelo);
    int tiros = 0;
    for (int i = 0; i < 100; i++) tiros += int(b.atualizar(DT, jog, 0, false, false, plano(), MundoColisao{}).claroes.size());
    CHECK(tiros == 0);
    GolpeAlvo sobe;
    sobe.levante = 9.f;
    b.aplicarGolpe(0, sobe);
    b.atualizar(0.2f, jog, 0, false, false, plano(), MundoColisao{});
    CHECK(b.alvos()[0].pes.y > b.bandidos()[0].corpo.pos.y + 0.5f);
}

TEST_CASE("vestigios: capsulas marcadas podem ser recolhidas e tem teto") {
    CircuitosNoMundo m;
    m.p.maxCapsulas = 3;
    for (int i = 0; i < 5; i++) m.soltarCapsula(chao(float(i), 0), 0, ESPIRAL_RESINA);
    CHECK(m.capsulas().size() == 3);
    CHECK(m.capsulaPerto(chao(3.2f, 0), 1.f) >= 0);
    CHECK(m.capsulaPerto(chao(30, 0), 1.f) == -1);
    CHECK(m.recolher(chao(3.5f, 0), 0.6f) == 2);  // as de x = 3 e x = 4 (sobraram x = 2, 3, 4)
    CHECK(m.capsulas().size() == 1);
}
