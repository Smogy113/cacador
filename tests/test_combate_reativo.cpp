// Combate reativo dos Circuitos de Queima: inimigos de teste (patrulha, vestígios, golpes), baia de
// cobertura (ricochete), receitas por engenharia reversa e rebote na câmara.
#include <cmath>
#include <memory>
#include "core/baia_cobertura.h"
#include "core/inimigo_teste.h"
#include "core/places.h"
#include "core/projetil_encantado.h"
#include "core/rebote.h"
#include "core/receitas.h"
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
float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }

const Circuito ESPIRAL_RESINA{Entalhe::Espiral, Pasta::ResinaECera, Doador::Pena};
const Circuito ESPIRAL_POLVORA{Entalhe::Espiral, Pasta::PolvoraFina, Doador::Pena};
const Circuito GRADE_ENXOFRE{Entalhe::Grade, Pasta::EnxofreESal, Doador::PedraPesada};
const Circuito ONDA_PRATA{Entalhe::Onda, Pasta::PoDePrata, Doador::CarvaoEmBrasa};

// Patrulheiro andando de (0, 0) para (0, 20), olhando para +z.
InimigoTeste patrulheiro(const ParametrosInimigo& p) { return criarInimigo("Teste", {chao(0, 0), chao(0, 20)}, p); }

void passar(InimigoTeste& in, float seg, const MundoColisao& col, const ParametrosInimigo& p) {
    for (float t = 0; t < seg; t += DT) atualizar(in, DT, plano(), col, p);
}

// Roda os projéteis até não sobrar nenhum e junta os impactos e ricochetes.
EventosCircuito voar(CircuitosNoMundo& m, const MundoColisao& col, const std::vector<Alvo>& alvos) {
    EventosCircuito tudo;
    for (float t = 0; t < 4.f && !m.projeteis().empty(); t += DT) {
        EventosCircuito ev = m.atualizar(DT, plano(), col, alvos);
        tudo.impactos.insert(tudo.impactos.end(), ev.impactos.begin(), ev.impactos.end());
        tudo.ricochetes.insert(tudo.ricochetes.end(), ev.ricochetes.begin(), ev.ricochetes.end());
    }
    return tudo;
}
}  // namespace

// ---------------------------------------------------------------- colisão

TEST_CASE("colisao: primeiroContato da a normal exata de uma caixa girada") {
    MundoColisao col;
    const float rot = 0.6f;
    col.adicionarGirada({{0, 0, 0}, 0.2f, 3.f, rot, -1.f, 4.f});
    // face +x local da caixa: normal = (cos, 0, -sen) do giro
    const Vec3f n{std::cos(rot), 0, -std::sin(rot)};
    const Vec3f longe = n * 5.f + Vec3f{0, 1, 0};
    float t;
    Vec3f normal;
    REQUIRE(col.primeiroContato(longe, Vec3f{0, 1, 0}, t, normal));
    CHECK(dot(normal, n) > 0.9999f);
    CHECK(t == doctest::Approx((5.f - 0.2f) / 5.f).epsilon(0.001));
    CHECK_FALSE(col.primeiroContato(longe, longe + Vec3f{0, 3, 0}, t, normal));
}

// ---------------------------------------------------------------- receitas

TEST_CASE("receitas: o cacador comeca so com Espiral e Polvora Fina") {
    ReceitasConhecidas r;
    CHECK(r.conhece(Entalhe::Espiral));
    CHECK(r.conhece(Pasta::PolvoraFina));
    CHECK_FALSE(r.conhece(Entalhe::Grade));
    CHECK_FALSE(r.conhece(Entalhe::Onda));
    CHECK_FALSE(r.conhece(Pasta::ResinaECera));
    CHECK_FALSE(r.conhece(Pasta::EnxofreESal));
    CHECK_FALSE(r.conhece(Pasta::PoDePrata));
    CHECK(r.entalhesConhecidos() == 1);
    CHECK(r.pastasConhecidas() == 1);
    CHECK(r.permite(ESPIRAL_POLVORA));
    CHECK_FALSE(r.permite(ESPIRAL_RESINA));
}

TEST_CASE("receitas: a bancada recusa o que o cacador nao conhece") {
    ReceitasConhecidas r;
    Arsenal a;
    CHECK(gravar(a, r, 0, GRADE_ENXOFRE) == ResultadoPreparo::EntalheDesconhecido);
    CHECK(gravar(a, r, 0, ESPIRAL_RESINA) == ResultadoPreparo::PastaDesconhecida);
    CHECK_FALSE(a.slot(0).cheio);
    CHECK(a.estoque(Doador::Pena) == ParametrosCircuito{}.estoqueInicialDoador);  // nada saiu do estoque
    CHECK(gravar(a, r, 0, ESPIRAL_POLVORA) == ResultadoPreparo::Ok);
    // a regra do doador continua valendo depois do conhecimento
    r.aprenderTudo();
    CHECK(gravar(a, r, 1, {Entalhe::Grade, Pasta::EnxofreESal, Doador::Pena}) == ResultadoPreparo::ReceitaInvalida);
}

TEST_CASE("receitas: inspecionar uma pista ensina uma vez") {
    ReceitasConhecidas r;
    std::vector<PistaEntalhada> pistas(2);
    pistas[0].pos = chao(3, 0);
    pistas[0].ensina = Descoberta::de(Pasta::ResinaECera);
    pistas[1].pos = chao(10, 0);
    pistas[1].ensina = Descoberta::de(Entalhe::Grade);
    CHECK(pistaPerto(pistas, chao(2.5f, 0), 1.6f) == 0);
    CHECK(pistaPerto(pistas, chao(6.5f, 0), 1.6f) == -1);
    CHECK(inspecionar(pistas[0], r));
    CHECK(r.conhece(Pasta::ResinaECera));
    CHECK(r.permite(ESPIRAL_RESINA));
    CHECK_FALSE(inspecionar(pistas[0], r));                    // já inspecionada
    CHECK(pistaPerto(pistas, chao(2.5f, 0), 1.6f) == -1);      // e some da busca
    CHECK(nome(pistas[1].ensina) == "entalhe Grade");
    // uma pista de algo já conhecido não ensina nada (mas é marcada)
    PistaEntalhada repetida;
    repetida.ensina = Descoberta::de(Entalhe::Espiral);
    CHECK_FALSE(inspecionar(repetida, r));
    CHECK(repetida.inspecionada);
}

// ---------------------------------------------------------------- rebote

TEST_CASE("rebote: a decisao do gatilho segue a bala e o pavio") {
    ParametrosCircuito p;
    SlotMunicao cuidadosa{true, ESPIRAL_POLVORA, Preparo::Cuidadoso};
    SlotMunicao rapida{true, ESPIRAL_POLVORA, Preparo::Rapido};
    Pavio seco;
    CHECK(decidirGatilho(nullptr, seco, false, 0.f, p).gatilho == Gatilho::Comum);
    CHECK(decidirGatilho(&cuidadosa, seco, false, 0.f, p).gatilho == Gatilho::Acende);  // chance 0
    CHECK(chanceRebote(cuidadosa, seco, p) == 0.f);
    // preparo rápido: rebote abaixo da chance, acende acima
    CHECK(rapida.imperfeita());
    CHECK(chanceRebote(rapida, seco, p) == doctest::Approx(p.chanceReboteRapido));
    CHECK(decidirGatilho(&rapida, seco, false, p.chanceReboteRapido - 0.01f, p).gatilho == Gatilho::Rebote);
    CHECK(decidirGatilho(&rapida, seco, false, p.chanceReboteRapido + 0.01f, p).gatilho == Gatilho::Acende);
    // na água: falha sempre, nem forçando
    Pavio agua;
    agua.atualizar(DT, true, false, p.tempoSecagem);
    CHECK(decidirGatilho(&cuidadosa, agua, true, 0.99f, p).gatilho == Gatilho::Falha);
    // Ferro Frio: não acende (sai chumbo), sem risco de rebote
    Pavio ferro;
    ferro.atualizar(DT, false, true, p.tempoSecagem);
    CHECK(decidirGatilho(&rapida, ferro, false, 0.f, p).gatilho == Gatilho::ApagaNoFerro);
}

TEST_CASE("rebote: o pavio umido falha no primeiro clique e forcar arrisca mais quanto mais umido") {
    ParametrosCircuito p;
    SlotMunicao bala{true, ESPIRAL_POLVORA, Preparo::Cuidadoso};
    Pavio pv;
    pv.atualizar(DT, true, false, p.tempoSecagem);
    pv.atualizar(DT, false, false, p.tempoSecagem);  // acabou de sair da água
    REQUIRE(pv.estado() == EstadoPavio::Umido);
    DecisaoGatilho primeiro = decidirGatilho(&bala, pv, false, 0.f, p);
    CHECK(primeiro.gatilho == Gatilho::Falha);
    CHECK(primeiro.pedeForcar);
    CHECK(primeiro.chanceRebote == doctest::Approx(p.chanceReboteUmidoMax).epsilon(0.01));
    CHECK(decidirGatilho(&bala, pv, true, p.chanceReboteUmidoMax - 0.05f, p).gatilho == Gatilho::Rebote);
    CHECK(decidirGatilho(&bala, pv, true, 0.99f, p).gatilho == Gatilho::Acende);
    const float molhadoMuito = chanceRebote(bala, pv, p);
    pv.atualizar(p.tempoSecagem * 0.9f, false, false, p.tempoSecagem);  // quase seco
    CHECK(chanceRebote(bala, pv, p) < molhadoMuito);
    CHECK(chanceRebote(bala, pv, p) >= p.chanceReboteUmidoMin);
    // os dois riscos somam: bala rápida com pavio úmido arrisca mais que qualquer um sozinho
    SlotMunicao rapida{true, ESPIRAL_POLVORA, Preparo::Rapido};
    CHECK(chanceRebote(rapida, pv, p) > chanceRebote(bala, pv, p));
    CHECK(chanceRebote(rapida, pv, p) > p.chanceReboteRapido);
}

TEST_CASE("rebote: o tremor da mira da um tranco para cima e some") {
    TremorMira tm;
    CHECK_FALSE(tm.ativo());
    tm.disparar(1.4f, 0.09f, 0.035f);
    float dy, dp;
    tm.deslocamento(dy, dp);
    CHECK(dp == doctest::Approx(0.09f));  // o tranco inteiro no começo
    float maior = 0;
    for (float t = 0; t < 1.5f; t += DT) {
        tm.atualizar(DT);
        tm.deslocamento(dy, dp);
        maior = std::max(maior, std::abs(dy));
    }
    CHECK(maior > 0.f);  // balançou de lado
    CHECK_FALSE(tm.ativo());
    CHECK(dp == doctest::Approx(0.f));
    CHECK(dy == doctest::Approx(0.f));
}

TEST_CASE("arsenal: o preparo rapido fica marcado na bala") {
    Arsenal a;
    CHECK(a.preparar(0, ESPIRAL_POLVORA, Preparo::Rapido) == ResultadoPreparo::Ok);
    CHECK(a.preparar(1, ESPIRAL_POLVORA) == ResultadoPreparo::Ok);
    CHECK(a.slot(0).imperfeita());
    CHECK_FALSE(a.slot(1).imperfeita());
    a.selecionar(0);
    REQUIRE(a.slotNaVez());
    CHECK(a.slotNaVez()->imperfeita());
}

// ---------------------------------------------------------------- inimigos: patrulha e vestígios

TEST_CASE("inimigo: anda a rota em laco") {
    ParametrosInimigo p;
    MundoColisao col;
    InimigoTeste in = patrulheiro(p);
    CHECK(estado(in) == EstadoInimigo::Patrulha);
    passar(in, 5.f, col, p);
    CHECK(in.corpo.pos.z == doctest::Approx(p.velPatrulha * 5.f).epsilon(0.05));
    passar(in, 20.f / p.velPatrulha, col, p);  // chegou ao fim e voltou um pedaço
    CHECK(in.proximo == 0);
    CHECK(in.corpo.pos.z < 20.f);
}

TEST_CASE("inimigo: percebe a capsula na frente e perto, mas nao atras de longe") {
    ParametrosInimigo p;
    MundoColisao col;
    InimigoTeste in = patrulheiro(p);
    CHECK(percebe(in, chao(0.5f, 6), col, p));                         // na frente, dentro do cone
    CHECK_FALSE(percebe(in, chao(0, p.raioVisaoVestigio + 1), col, p));  // longe demais
    CHECK_FALSE(percebe(in, chao(0, -4), col, p));                     // atrás, fora do raio próximo
    CHECK(percebe(in, chao(0, -2), col, p));                           // atrás, mas bem perto
    CHECK_FALSE(percebe(in, chao(7, 1), col, p));                      // de lado, fora do cone
    // parede no meio esconde
    const float y0 = chao(0, 3).y;
    col.adicionar({{-3, y0 - 5, 3}, {3, y0 + 10, 3.5f}});
    CHECK_FALSE(percebe(in, chao(0, 6), col, p));
}

TEST_CASE("inimigo: ao perceber a capsula fica em Alerta, investiga e volta a patrulha") {
    ParametrosInimigo p;
    MundoColisao col;
    CircuitosNoMundo m;
    m.soltarCapsula(chao(0.4f, 7), 0, ESPIRAL_POLVORA);
    InimigoTeste in = patrulheiro(p);
    int k = vestigioPercebido(in, m.capsulas(), col, p);
    REQUIRE(k == 0);
    investigar(in, m.capsulas()[0].pos, MotivoAlerta::Vestigio, p);
    m.marcarDescoberta(k);
    CHECK(vestigioPercebido(in, m.capsulas(), col, p) == -1);  // já descoberta: não chama de novo
    CHECK(estado(in) == EstadoInimigo::Alerta);
    CHECK(descreverEstado(in) == "Alerta: investigando vestígio");
    // vai até a cápsula (mais rápido que a patrulha) e para perto dela
    passar(in, 7.f / p.velInvestigar + 0.5f, col, p);
    CHECK(distXZ(in.corpo.pos, m.capsulas()[0].pos) <= p.distInspecao + 0.05f);
    CHECK(estado(in) == EstadoInimigo::Alerta);
    passar(in, p.tempoInspecao + 0.2f, col, p);
    CHECK(estado(in) == EstadoInimigo::Patrulha);
}

TEST_CASE("inimigo: o sentinela de posto so vira para o ponto, nao sai do lugar") {
    ParametrosInimigo p;
    MundoColisao col;
    InimigoTeste in = criarInimigo("Sentinela", {chao(-0.8f, 0), chao(0.8f, 0)}, p);
    in.posto = in.agachado = true;
    in.yawVigia = 0;
    investigar(in, chao(-10, 0), MotivoAlerta::Atacado, p);
    const Vec3f antes = in.corpo.pos;
    passar(in, 1.f, col, p);
    CHECK(distXZ(in.corpo.pos, antes) < 0.01f);
    CHECK(std::sin(in.corpo.yaw) < -0.9f);  // olhando para -x
    CHECK(in.alvo(p).altura == doctest::Approx(p.alturaAgachado));
}

TEST_CASE("inimigo: derrotado larga o espolio uma vez e reaparece patrulhando") {
    ParametrosInimigo p;
    MundoColisao col;
    InimigoTeste in = patrulheiro(p);
    in.espolio = Descoberta::de(Pasta::EnxofreESal);
    investigar(in, chao(5, 5), MotivoAlerta::Atacado, p);
    GolpeAlvo g;
    g.dano = 500.f;
    CHECK(golpear(in, g));
    CHECK(estado(in) == EstadoInimigo::Derrotado);
    CHECK(in.alvo(p).pes.y < -1e5f);  // fora do alcance dos tiros
    CHECK_FALSE(golpear(in, g));       // não derrota de novo
    passar(in, p.tempoReaparecer + 0.1f, col, p);
    CHECK(estado(in) == EstadoInimigo::Patrulha);
    CHECK(distXZ(in.corpo.pos, in.rota[0]) < 0.5f);
}

// ---------------------------------------------------------------- inimigos: golpes mágicos

TEST_CASE("Grade + Enxofre: empurra e atordoa os inimigos no raio") {
    ParametrosInimigo pi;
    ParametrosCircuito pc;
    MundoColisao col;
    InimigoTeste a = criarInimigo("A", {chao(-1, 0), chao(-1, 20)}, pi);
    InimigoTeste b = criarInimigo("B", {chao(1, 0), chao(1, 20)}, pi);
    InimigoTeste longe = criarInimigo("C", {chao(15, 0), chao(15, 20)}, pi);
    Impacto im;
    im.ponto = chao(0, 0.5f) + Vec3f{0, 0.9f, 0};
    im.circuito = GRADE_ENXOFRE;
    im.efeito = resolverEfeito(GRADE_ENXOFRE, pc);
    for (InimigoTeste* in : {&a, &b, &longe}) golpear(*in, golpeDoImpacto(im, in->pes() + Vec3f{0, 0.9f, 0}, false, pc));
    CHECK(estado(a) == EstadoInimigo::Atordoado);
    CHECK(estado(b) == EstadoInimigo::Atordoado);
    CHECK(estado(longe) == EstadoInimigo::Patrulha);
    CHECK(a.corpo.fisica.empurrao.x < 0.f);  // para fora do impacto
    CHECK(b.corpo.fisica.empurrao.x > 0.f);
    // zonzo: depois do empurrão não anda a rota
    passar(a, 1.f, col, pi);
    const float zParado = a.corpo.pos.z;
    passar(a, 0.8f, col, pi);
    CHECK(a.corpo.pos.z == doctest::Approx(zParado).epsilon(0.001));
    CHECK(estado(a) == EstadoInimigo::Atordoado);
    passar(a, pc.tempoAtordoado, col, pi);  // passa e volta à patrulha
    CHECK(estado(a) == EstadoInimigo::Patrulha);
    CHECK(a.ativo());
}

TEST_CASE("Onda + Po de Prata: quem pisa na zona congelante anda bem mais devagar") {
    ParametrosInimigo pi;
    MundoColisao col;
    CircuitosNoMundo m;
    // zona no caminho do patrulheiro (Onda + Prata atirada no chão à frente dele)
    m.disparar(chao(0, 2) + Vec3f{0, 1.5f, 0}, normalizar(Vec3f{0, -1.5f, 3.f}), ONDA_PRATA);
    voar(m, col, {});
    REQUIRE(m.zonas().size() == 1);
    const ZonaElemental z = m.zonas()[0];
    InimigoTeste dentro = criarInimigo("Dentro", {z.centro, z.centro + Vec3f{0, 0, 20}}, pi);
    InimigoTeste fora = criarInimigo("Fora", {chao(30, 0), chao(30, 20)}, pi);
    float andouDentro = 0, andouFora = 0;
    for (float t = 0; t < 1.f; t += DT) {
        for (InimigoTeste* in : {&dentro, &fora}) {
            GolpeAlvo g = golpeDaZona(z, in->corpo.pos, DT, m.p);
            if (!g.vazio()) golpear(*in, g);
        }
        const float zd = dentro.corpo.pos.z, zf = fora.corpo.pos.z;
        atualizar(dentro, DT, plano(), col, pi);
        atualizar(fora, DT, plano(), col, pi);
        andouDentro += dentro.corpo.pos.z - zd;
        andouFora += fora.corpo.pos.z - zf;
    }
    CHECK(estado(dentro) == EstadoInimigo::Congelado);
    CHECK(estado(fora) == EstadoInimigo::Patrulha);
    CHECK(andouDentro > 0.f);  // lento, não parado
    CHECK(andouDentro == doctest::Approx(andouFora * m.p.fatorLentidaoGelo).epsilon(0.1));
}

// ---------------------------------------------------------------- baia de cobertura (ricochete)

TEST_CASE("Espiral + Resina: so o ricochete na parede lateral pega quem esta atras da cobertura") {
    ParametrosInimigo pi;
    // a geometria não pode depender de como a baia está virada no mapa
    for (float yaw : {0.f, 2.79f, 0.65f, -2.2f}) {
        CAPTURE(yaw);
        MundoColisao col;
        const Vec3f alvo = chao(5, -3);
        const BaiaCobertura baia = montarBaia(alvo, yaw, plano());
        col.adicionarGirada(baia.cobertura);
        for (const CaixaGirada& w : baia.paredes) col.adicionarGirada(w);
        InimigoTeste sentinela = criarInimigo("Sentinela", {alvo}, pi);
        sentinela.agachado = sentinela.posto = true;
        const std::vector<Alvo> alvos{sentinela.alvo(pi)};
        const Vec3f u = baia.frente();
        const float altArma = 1.45f;

        // tiro reto na cabeça do agachado: a cobertura segura
        CircuitosNoMundo reto;
        const Vec3f boca = alvo + u * 14.f + Vec3f{0, altArma, 0};
        reto.disparar(boca, normalizar(alvo + Vec3f{0, 1.f, 0} - boca), ESPIRAL_RESINA);
        EventosCircuito evReto = voar(reto, col, alvos);
        bool pegouReto = false;
        for (const Impacto& im : evReto.impactos) pegouReto |= im.alvo == 0;
        CHECK_FALSE(pegouReto);

        // pela parede lateral (as duas): ricocheteia de lado e acerta
        for (int lado = 0; lado < 2; lado++) {
            CAPTURE(lado);
            CircuitosNoMundo m;
            const Vec3f mira = baia.miraRicochete(lado, 0.7f);
            const Vec3f bocaLado = mira + u * 14.f + Vec3f{0, altArma - 0.7f, 0};
            m.disparar(bocaLado, normalizar(mira - bocaLado), ESPIRAL_RESINA);
            EventosCircuito ev = voar(m, col, alvos);
            REQUIRE(ev.impactos.size() == 1);
            CHECK(ev.impactos[0].alvo == 0);
            CHECK(ev.impactos[0].encantado);
            CHECK(ev.ricochetes.size() >= 1);
            // o mesmo tiro sem Resina para na parede
            CircuitosNoMundo semResina;
            semResina.disparar(bocaLado, normalizar(mira - bocaLado), ESPIRAL_POLVORA);
            EventosCircuito ev2 = voar(semResina, col, alvos);
            REQUIRE(ev2.impactos.size() == 1);
            CHECK(ev2.impactos[0].alvo == -1);
        }
    }
}
