// Combate reativo dos Circuitos de Queima no jogo: o pátio de patrulha ao lado da arena de teste.
//
//   - Inimigos de teste (manequins provisórios): patrulham, percebem as cápsulas marcadas largadas no
//     chão (vão investigar: estado Alerta) e reagem aos golpes (Atordoado, Congelado, empurrados...).
//   - Baia de cobertura: um Sentinela agachado atrás de uma cobertura baixa, entre paredes laterais a
//     45°: só a Espiral + Resina e Cera, ricocheteando na parede, o alcança.
//   - Pistas (engenharia reversa): equipamentos entalhados no chão ou largados pelos inimigos
//     derrotados; inspecionar (E) ensina um entalhe ou uma pasta na fogueira.
//   - Marcadores (sempre): losango de alerta e badges de gelo/atordoamento sobre os inimigos,
//     prompts flutuantes "[E] ..." sobre pistas e cápsulas, banner de receita descoberta.
//   - Depuração (F3, ligada por padrão): nome/vida/estado da IA sob cada marcador, cone de visão,
//     rota e o quadro do canto superior esquerdo (Circuitos + combate reativo).
//
// A regra fica no núcleo, sem raylib e sem a malha do personagem: core/inimigo_teste (cabeça dos
// inimigos), core/fisica_alvo (corpo), core/receitas (pistas e receitas), core/baia_cobertura
// (geometria) e core/rebote (gatilho). Este arquivo só monta, liga e desenha com primitivas: para
// trocar o visual, mexa em desenharCombateReativo() (3D), desenharRotulosCombate() (2D) e
// game/ui_oeste (paleta e ícones).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/terrain.h"
#include "game/jogo.h"
#include "render/estilo.h"
#include "rlgl.h"

namespace {
// ---- planta do pátio de patrulha (metros a partir da largada: frente, direita; ver arena_teste.h) ----
// O laço do Patrulheiro passa a ~4 m da fogueira, onde o caçador costuma atirar: é quem mais acha
// as cápsulas esquecidas no chão.
constexpr arena::Ponto ROTA_PATRULHEIRO[] = {{2.5f, 7.f}, {2.5f, 21.f}, {12.f, 21.f}, {12.f, 12.f}};
// A Dupla vai e volta lado a lado: os dois cabem no raio do Enxofre e Sal.
constexpr arena::Ponto DUPLA_INICIO{9.f, 26.f}, DUPLA_FIM{24.f, 26.f};
constexpr float DUPLA_AFASTAMENTO = 1.6f;
// O Sentinela fica agachado atrás da baia e anda um pouco de lado, sem sair da cobertura.
constexpr arena::Ponto SENTINELA{31.f, 15.f};
constexpr float SENTINELA_VAIVEM = 0.8f;
constexpr arena::Ponto PISTA_GRADE{8.5f, 4.5f};  // cartucheira entalhada no chão, perto dos bonecos
// Índices em inimigos_ (a ordem em que iniciarCombateReativo() os cria).
constexpr size_t PATRULHEIRO = 0, DUPLA_A = 1, DUPLA_B = 2, O_SENTINELA = 3;

constexpr float DIST_DESENHO = 90.f;  // além disso o pátio não é desenhado
constexpr float DIST_ROTULO = 45.f;   // rótulos de estado sobre os inimigos
constexpr float DIST_ROTULO_PISTA = 14.f;
constexpr float AFASTAR_DO_CACADOR = 0.7f;  // m: os inimigos não atravessam o caçador

Vec3f direitaDe(float yaw) { return {-std::cos(yaw), 0, std::sin(yaw)}; }
float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 v3(Vec3f p) { return {p.x, p.y, p.z}; }
unsigned char paraByte(float v) { return static_cast<unsigned char>(std::clamp(v, 0.f, 1.f) * 255.f + 0.5f); }
Color cor(Rgb c, float alfa = 1.f) { return {paraByte(c.r), paraByte(c.g), paraByte(c.b), paraByte(alfa)}; }

// O que o caçador vê no chão para cada coisa que uma pista ensina.
std::string objetoDaPista(const Descoberta& d) {
    if (d.tipo == Descoberta::Tipo::Entalhe) {
        switch (d.entalhe) {
        case Entalhe::Espiral: return "Bala alheia com entalhe em Espiral";
        case Entalhe::Grade: return "Cartucheira com balas entalhadas em Grade";
        case Entalhe::Onda: return "Cápsula queimada com entalhe em Onda";
        }
    }
    switch (d.pasta) {
    case Pasta::PolvoraFina: return "Saquinho de Pólvora Fina";
    case Pasta::ResinaECera: return "Pote de Resina e Cera";
    case Pasta::EnxofreESal: return "Frasco de Enxofre e Sal";
    case Pasta::PoDePrata: return "Bolsinha de Pó de Prata";
    }
    return "Equipamento entalhado";
}

// Cor de cada estado na depuração (rótulos e quadro).
Color corDoEstado(EstadoInimigo e) {
    switch (e) {
    case EstadoInimigo::Patrulha: return {150, 210, 140, 255};
    case EstadoInimigo::Alerta: return {240, 150, 60, 255};
    case EstadoInimigo::Congelado: return {150, 210, 255, 255};
    case EstadoInimigo::Atordoado: return {245, 215, 90, 255};
    case EstadoInimigo::Derrotado: return {150, 140, 130, 255};
    }
    return estilo::MARFIM;
}

// Golpe de verdade (não só a lentidão ou o fogo de uma zona): deixa o inimigo em alerta.
bool ehAtaque(const GolpeAlvo& g) {
    return g.dano > 0.f || g.empurrao.x != 0.f || g.empurrao.z != 0.f || g.levante > 0.f || g.derrubar > 0.f || g.atordoar > 0.f ||
           g.congelar > 0.f;
}

// Caixa girada da colisão desenhada como bloco (mesmo truque das paredes do corredor).
void desenharCaixaGirada(const CaixaGirada& w, Color corpo, Color aresta) {
    rlPushMatrix();
    rlTranslatef(w.centro.x, (w.y0 + w.y1) * 0.5f, w.centro.z);
    rlRotatef(w.rot * RAD2DEG, 0, 1, 0);
    DrawCube({0, 0, 0}, w.meiaX * 2.f, w.y1 - w.y0, w.meiaZ * 2.f, corpo);
    DrawCubeWires({0, 0, 0}, w.meiaX * 2.f, w.y1 - w.y0, w.meiaZ * 2.f, aresta);
    rlPopMatrix();
}
}  // namespace

// ---------------------------------------------------------------- montagem

void Jogo::iniciarCombateReativo() {
    auto rota = [&](std::initializer_list<arena::Ponto> pontos) {
        std::vector<Vec3f> r;
        for (const arena::Ponto& q : pontos) r.push_back(pontoDaArena(q));
        return r;
    };
    // 0) Patrulheiro: laço perto da fogueira. Larga a Resina e Cera (abre o ricochete).
    InimigoTeste patrulheiro = criarInimigo(
        "Patrulheiro", rota({ROTA_PATRULHEIRO[0], ROTA_PATRULHEIRO[1], ROTA_PATRULHEIRO[2], ROTA_PATRULHEIRO[3]}), paramInimigo_);
    patrulheiro.espolio = Descoberta::de(Pasta::ResinaECera);
    inimigos_.push_back(patrulheiro);
    // 1, 2) Dupla: vão e voltam lado a lado. Largam o Enxofre e Sal e o Pó de Prata.
    for (int k = 0; k < 2; k++) {
        const float lado = float(k) * DUPLA_AFASTAMENTO;
        InimigoTeste in = criarInimigo(k == 0 ? "Dupla A" : "Dupla B",
                                       rota({{DUPLA_INICIO.frente, DUPLA_INICIO.direita + lado}, {DUPLA_FIM.frente, DUPLA_FIM.direita + lado}}),
                                       paramInimigo_);
        in.espolio = Descoberta::de(k == 0 ? Pasta::EnxofreESal : Pasta::PoDePrata);
        inimigos_.push_back(in);
    }
    // 3) Sentinela agachado atrás da baia, que abre para a largada. Larga o entalhe Onda.
    const float yawBaia = arena::INICIO_YAW_GRAUS * DEG2RAD + PI;
    const Vec3f posSentinela = pontoDaArena(SENTINELA);
    baia_ = montarBaia(posSentinela, yawBaia, *terreno_);
    col_.adicionarGirada(baia_.cobertura);
    for (const CaixaGirada& w : baia_.paredes) col_.adicionarGirada(w);
    baiaPronta_ = true;
    InimigoTeste sentinela = criarInimigo(
        "Sentinela", {pontoChao(posSentinela + baia_.lateral() * SENTINELA_VAIVEM), pontoChao(posSentinela - baia_.lateral() * SENTINELA_VAIVEM)},
        paramInimigo_);
    sentinela.agachado = sentinela.posto = true;
    sentinela.yawVigia = sentinela.corpo.yaw = yawBaia;
    sentinela.espolio = Descoberta::de(Entalhe::Onda);
    inimigos_.push_back(sentinela);
    // Pista espalhada no chão: a Grade (com o Enxofre da Dupla, fecha a onda de pressão).
    PistaEntalhada grade;
    grade.pos = pontoDaArena(PISTA_GRADE);
    grade.yaw = 0.7f;
    grade.ensina = Descoberta::de(Entalhe::Grade);
    grade.objeto = objetoDaPista(grade.ensina);
    pistas_.push_back(grade);

    // Conferência barata: ponto de rota dentro de uma colisão (casa, cerca) aparece no log.
    for (const InimigoTeste& in : inimigos_)
        for (Vec3f q : in.rota)
            if (distXZ(col_.resolver(q, paramInimigo_.raioCorpo), q) > 0.05f)
                std::printf("combate reativo: AVISO rota de %s dentro de colisao em (%.1f, %.1f)\n", in.nome.c_str(), q.x, q.z);
    std::printf("combate reativo: %zu inimigos de teste, sentinela=(%.1f, %.1f), pista Grade=(%.1f, %.1f)\n", inimigos_.size(), posSentinela.x,
                posSentinela.z, grade.pos.x, grade.pos.z);
}

// ---------------------------------------------------------------- quadro

void Jogo::atualizarCombateReativo(float dt) {
    for (InimigoTeste& in : inimigos_) {
        ::atualizar(in, dt, *terreno_, col_, paramInimigo_);  // corpo + cabeça (core/inimigo_teste; não Jogo::atualizar)
        if (!in.ativo()) continue;
        // não atravessa o caçador: empurra para fora de um círculo em volta dele
        const Vec3f fora = in.corpo.pos - jogador_.pos;
        const float n = std::hypot(fora.x, fora.z);
        if (montadoEm_ < 0 && n < AFASTAR_DO_CACADOR && n > 1e-3f)
            in.corpo.pos = pontoChao(col_.resolver(jogador_.pos + fora * (AFASTAR_DO_CACADOR / n), paramInimigo_.raioCorpo));
        // vestígios: uma cápsula marcada que ninguém viu ainda, perto ou no cone de visão
        const int c = vestigioPercebido(in, circuitos_.capsulas(), col_, paramInimigo_);
        if (c >= 0) {
            const Vec3f onde = circuitos_.capsulas()[size_t(c)].pos;
            circuitos_.marcarDescoberta(c);
            investigar(in, onde, MotivoAlerta::Vestigio, paramInimigo_);
            avisar(in.nome + " achou uma cápsula marcada no chão e foi investigar.");
            std::printf("combate reativo: %s percebeu a capsula em (%.1f, %.1f)\n", in.nome.c_str(), onde.x, onde.z);
        }
    }
}

bool Jogo::golpearInimigo(int k, const GolpeAlvo& g) {
    if (k < 0 || k >= int(inimigos_.size())) return false;
    InimigoTeste& in = inimigos_[size_t(k)];
    if (!in.ativo()) return false;
    if (golpear(in, g)) {
        std::printf("combate reativo: %s derrotado\n", in.nome.c_str());
        soltarEspolio(in);
        return true;
    }
    // Atacado de verdade: fica em alerta e vai até de onde o caçador atirou (o Sentinela só vira).
    const bool jaProcura = in.conduta == Conduta::Investigando && in.motivo == MotivoAlerta::Atacado && distXZ(in.interesse, jogador_.pos) < 1.f;
    if (ehAtaque(g) && !jaProcura) investigar(in, jogador_.pos, MotivoAlerta::Atacado, paramInimigo_);
    return false;
}

void Jogo::soltarEspolio(InimigoTeste& in) {
    if (!in.espolio) return;  // só na primeira derrota: ao reaparecer não carrega outro
    PistaEntalhada p;
    p.pos = pontoChao(in.corpo.pos + frenteDe(in.corpo.yaw) * 0.45f);
    p.yaw = in.corpo.yaw;
    p.ensina = *in.espolio;
    p.objeto = objetoDaPista(p.ensina);
    pistas_.push_back(p);
    in.espolio.reset();
    avisar(in.nome + " caiu e largou: " + p.objeto + ". Chegue perto e aperte E para inspecionar.");
}

bool Jogo::interagirPista(Entrada& e) {
    if (pistaPerto_ < 0 || !e.interagir) return false;
    PistaEntalhada& p = pistas_[size_t(pistaPerto_)];
    const bool aprendeu = inspecionar(p, receitas_);
    avisar(aprendeu ? "Engenharia reversa: " + p.objeto + ". Novo na fogueira: " + nome(p.ensina) + "."
                    : "Você examinou " + p.objeto + ": nada que já não soubesse.");
    if (aprendeu) {  // banner no topo: "NOVO ENTALHE DESCOBERTO: GRADE"
        const bool entalhe = p.ensina.tipo == Descoberta::Tipo::Entalhe;
        banner_.mostrar(std::string(entalhe ? "NOVO ENTALHE DESCOBERTO: " : "NOVA PASTA DESCOBERTA: ") +
                            oeste::maiusculas(entalhe ? nome(p.ensina.entalhe) : nome(p.ensina.pasta)),
                        "Engenharia reversa: " + p.objeto + ". Já vale na fogueira.", tempo_);
    }
    std::printf("combate reativo: pista inspecionada (%s)%s\n", nome(p.ensina).c_str(), aprendeu ? " aprendeu" : "");
    pistas_.erase(std::remove_if(pistas_.begin(), pistas_.end(), [](const PistaEntalhada& q) { return q.inspecionada; }), pistas_.end());
    pistaPerto_ = -1;
    e.interagir = false;
    return true;
}

// ---------------------------------------------------------------- capturas (--roteiro-circuito 6..10)

void Jogo::prepararRoteiroReativo(int modo) {
    if (inimigos_.size() <= O_SENTINELA || !baiaPronta_) return;
    auto olharPara = [&](Vec3f alvo) { jogador_.yaw = rig_.yaw = std::atan2(alvo.x - jogador_.pos.x, alvo.z - jogador_.pos.z); };
    switch (modo) {
    case 6:  // Grade + Enxofre na Dupla: os dois ficam atordoados
        jogador_.pos = pontoDaArena({6.f, 19.f});
        olharPara(inimigos_[DUPLA_A].corpo.pos);
        break;
    case 7:  // Onda + Prata no caminho do Patrulheiro: ele atravessa a zona devagar
        jogador_.pos = pontoDaArena({-8.f, 11.f});
        olharPara(inimigos_[PATRULHEIRO].corpo.pos);
        break;
    case 8: {  // Espiral + Resina na parede lateral da baia: ricocheteia no Sentinela
        // o cano fica ~0,3 m à direita do caçador; a direita de quem olha para a baia é +lateral()
        const Vec3f mira = baia_.miraRicochete(0, 0.75f);
        jogador_.pos = pontoChao(mira + baia_.frente() * 14.f - baia_.lateral() * 0.3f);
        olharPara(mira);
        break;
    }
    case 9:  // rebote: a bala do Preparo Rápido estoura na câmara (sorteio fixo)
        arsenal_.preparar(0, {Entalhe::Espiral, Pasta::PolvoraFina, Doador::Pena}, Preparo::Rapido);
        arsenal_.selecionar(0);
        sorteioRebote_ = 0.f;
        break;
    case 10:  // vestígio: atira (Pólvora Fina, sem ricochete) no caminho do Patrulheiro; ele acha a cápsula
        arsenal_.preparar(0, {Entalhe::Espiral, Pasta::PolvoraFina, Doador::Pena});
        arsenal_.selecionar(0);
        jogador_.pos = pontoDaArena({2.5f, 14.f});
        olharPara(alvoRoteiroReativo(10));
        break;
    default: break;
    }
}

Vec3f Jogo::alvoRoteiroReativo(int modo) const {
    if (inimigos_.size() <= O_SENTINELA || !baiaPronta_) return pontoDaArena({12.f, 0.f});
    switch (modo) {
    case 6: return inimigos_[DUPLA_A].pes() + Vec3f{0, 1.f, 0};
    case 7: {  // o chão 2 m à frente do Patrulheiro (já cai dentro da zona)
        const InimigoTeste& in = inimigos_[PATRULHEIRO];
        return pontoChao(in.corpo.pos + frenteDe(in.corpo.yaw) * 2.f);
    }
    case 8: return baia_.miraRicochete(0, 0.75f);
    case 10: return pontoDaArena({6.f, 6.5f});  // o chão à esquerda, com o Patrulheiro chegando no quadro
    default: return pontoDaArena({12.f, 0.f}) + Vec3f{0, 1.f, 0};
    }
}

// ---------------------------------------------------------------- desenho 3D

void Jogo::desenharEstadoDoAlvo(Vec3f pes, float altura, const FisicaAlvo& f, Transparencias& tr) const {
    const float t = tempo_;
    if (f.congelado > 0.f)  // gelo de verdade: bloco translúcido em volta
        tr.vidros.push_back({pes + Vec3f{0, altura * 0.5f, 0}, {0.85f, altura + 0.1f, 0.85f}, Color{170, 225, 255, 95}});
    if (f.queimando > 0.f)
        for (int k = 0; k < 3; k++)
            tr.brilho.push_back({pes + Vec3f{0, 0.5f + 0.45f * float(k) + 0.1f * std::sin(t * 9.f + float(k)), 0}, 0.55f, Color{255, 120, 40, 150}});
    if (f.lento > 0.f) {  // zona congelante: geada nos pés e cristais subindo pelas pernas
        DrawCircle3D(v3(pes + Vec3f{0, 0.05f, 0}), 0.45f, {1, 0, 0}, 90.f, Color{190, 230, 255, 220});
        for (int k = 0; k < 6; k++) {
            const float a = float(k) * 1.047f + t * 0.6f;
            tr.brilho.push_back({pes + Vec3f{std::cos(a) * 0.3f, 0.12f + 0.22f * float(k % 3), std::sin(a) * 0.3f}, 0.13f, Color{200, 235, 255, 170}});
        }
    }
    if (f.atordoado > 0.f || f.derrubado > 0.f) {  // zonzo: três estrelas girando sobre a cabeça
        const Vec3f cabeca = pes + Vec3f{0, f.derrubado > 0.f ? 0.45f : altura + 0.18f, 0};
        for (int k = 0; k < 3; k++) {
            const float a = t * 5.f + float(k) * 2.094f;
            tr.brilho.push_back({cabeca + Vec3f{std::cos(a) * 0.3f, 0.05f * std::sin(t * 7.f + float(k)), std::sin(a) * 0.3f}, 0.17f, Color{255, 225, 90, 230}});
        }
    }
}

void Jogo::desenharCombateReativo(Transparencias& tr) {
    if (!baiaPronta_) return;
    const float t = tempo_;
    const Vec3f olho{cam_.position.x, cam_.position.y, cam_.position.z};
    const Vec3f frenteCam = normalizar(Vec3f{cam_.target.x, cam_.target.y, cam_.target.z} - olho);
    const float yawCamera = std::atan2(frenteCam.x, frenteCam.z);
    const ParametrosInimigo& pi = paramInimigo_;

    // ---- baia de cobertura: cobertura de pedra baixa e paredes laterais de tábua ----
    if (distXZ(olho, baia_.alvo) < DIST_DESENHO) {
        desenharCaixaGirada(baia_.cobertura, Color{128, 122, 112, 255}, Color{86, 80, 72, 255});
        for (const CaixaGirada& w : baia_.paredes) desenharCaixaGirada(w, Color{112, 84, 58, 255}, Color{70, 50, 34, 255});
    }

    // ---- pistas: caixinha de couro com marca dourada, brilho e um facho fraco para achar de longe ----
    for (const PistaEntalhada& p : pistas_) {
        if (p.inspecionada || distXZ(olho, p.pos) > DIST_DESENHO) continue;
        rlPushMatrix();
        rlTranslatef(p.pos.x, p.pos.y + 0.09f, p.pos.z);
        rlRotatef(p.yaw * RAD2DEG, 0, 1, 0);
        DrawCube({0, 0, 0}, 0.34f, 0.18f, 0.22f, Color{92, 62, 38, 255});
        DrawCubeWires({0, 0, 0}, 0.34f, 0.18f, 0.22f, Color{200, 160, 80, 255});
        DrawCube({0, 0.095f, 0}, 0.2f, 0.012f, 0.08f, Color{214, 176, 98, 255});  // o entalhe na tampa
        rlPopMatrix();
        const float pulso = 0.5f + 0.5f * std::sin(t * 3.f + p.pos.x);
        tr.brilho.push_back({p.pos + Vec3f{0, 0.35f, 0}, 0.4f + 0.12f * pulso, Color{240, 200, 110, 115}});
        DrawLine3D(v3(p.pos + Vec3f{0, 0.2f, 0}), v3(p.pos + Vec3f{0, 3.f, 0}), Color{230, 190, 100, 110});
    }

    // ---- vestígios descobertos: dois anéis vermelhos em volta da cápsula ----
    for (const CapsulaVestigio& c : circuitos_.capsulas()) {
        if (!c.descoberta || distXZ(olho, c.pos) > 60.f) continue;
        const float pulso = 0.06f * std::sin(t * 6.f);
        DrawCircle3D(v3(c.pos + Vec3f{0, 0.04f, 0}), 0.32f + pulso, {1, 0, 0}, 90.f, Color{220, 60, 40, 230});
        DrawCircle3D(v3(c.pos + Vec3f{0, 0.05f, 0}), 0.42f + pulso, {1, 0, 0}, 90.f, Color{220, 60, 40, 140});
    }

    // ---- inimigos de teste: manequins de pano azul-ardósia (os bonecos de treino são de palha) ----
    for (const InimigoTeste& in : inimigos_) {
        if (distXZ(olho, in.corpo.pos) > DIST_DESENHO) continue;
        const FisicaAlvo& f = in.corpo.fisica;
        const EstadoInimigo est = estado(in);
        if (depuracaoCombate_ && in.rota.size() >= 2) {  // a rota da patrulha, fraca no chão (fecha o laço se tiver 3+ pontos)
            const size_t trechos = in.rota.size() > 2 ? in.rota.size() : 1;
            for (size_t i = 0; i < trechos; i++)
                DrawLine3D(v3(in.rota[i] + Vec3f{0, 0.05f, 0}), v3(in.rota[(i + 1) % in.rota.size()] + Vec3f{0, 0.05f, 0}), Color{120, 140, 170, 130});
        }
        if (!in.ativo()) continue;
        const Vec3f pes = in.pes(), frente = frenteDe(in.corpo.yaw), lado = direitaDe(in.corpo.yaw);
        const float escala = in.agachado ? 0.64f : 1.f;  // agachado: tudo mais baixo
        // derrubado: deitado na direção do empurrão (ou para trás); zonzo: balança de lado
        const Vec3f para = len(f.empurrao) > 0.1f ? normalizar(f.empurrao) : frente * -1.f;
        const float tomba = f.derrubado > 0.f ? std::min(1.f, f.derrubado * 3.f) : 0.f;
        const float balanca = 0.1f * in.corpo.tranco * std::sin(t * 40.f) + (f.atordoado > 0.f && tomba <= 0.f ? 0.09f * std::sin(t * 6.f) : 0.f);
        const Vec3f cima = normalizar(Vec3f{0, 1, 0} * (1.f - tomba) + para * (tomba * 1.2f) + lado * balanca);
        Rgb pano{0.28f, 0.31f, 0.38f};
        if (est == EstadoInimigo::Alerta) pano = lerp(pano, Rgb{0.55f, 0.3f, 0.18f}, 0.35f);
        if (f.lento > 0.f) pano = lerp(pano, Rgb{0.62f, 0.8f, 0.95f}, 0.45f);
        if (f.congelado > 0.f) pano = lerp(pano, Rgb{0.75f, 0.9f, 1.f}, 0.65f);
        if (f.queimando > 0.f) pano = lerp(pano, Rgb{0.2f, 0.12f, 0.08f}, 0.5f);
        if (in.corpo.tranco > 0.5f) pano = lerp(pano, Rgb{0.9f, 0.3f, 0.25f}, in.corpo.tranco - 0.5f);
        // pernas: passada pela distância andada (some quando caído)
        const float fase = in.passada * 6.2832f, abre = std::min(1.f, in.vel / 1.2f) * 0.32f * (1.f - tomba);
        const Vec3f quadril = pes + cima * (0.85f * escala);
        for (float s : {-1.f, 1.f}) {
            const Vec3f pe = quadril - cima * (0.85f * escala) + lado * (0.13f * s) + frente * (std::sin(fase + (s > 0.f ? 0.f : PI)) * abre);
            DrawCylinderEx(v3(quadril + lado * (0.11f * s)), v3(pe), 0.075f, 0.06f, 6, cor(pano * 0.72f));
        }
        // tronco, braços e cabeça com a "viseira" dourada apontando para onde olha
        const Vec3f ombros = quadril + cima * (0.6f * escala);
        DrawCylinderEx(v3(quadril), v3(ombros), 0.2f, 0.17f, 8, cor(pano));
        for (float s : {-1.f, 1.f}) {
            const Vec3f mao = ombros + lado * (0.27f * s) - cima * (0.52f * escala) - frente * (std::sin(fase + (s > 0.f ? PI : 0.f)) * abre * 0.6f);
            DrawCylinderEx(v3(ombros + lado * (0.22f * s)), v3(mao), 0.055f, 0.045f, 5, cor(pano * 0.85f));
        }
        const Vec3f cabeca = ombros + cima * (0.17f * escala + 0.03f);
        DrawSphereEx(v3(cabeca), 0.14f, 6, 8, cor(pano * 1.12f));
        DrawCylinderEx(v3(cabeca + frente * 0.07f), v3(cabeca + frente * 0.19f), 0.05f, 0.025f, 6, Color{230, 196, 110, 255});
        // vida: barra de frente para a câmera (o x local fica deitado na tela; +x local = esquerda da câmera)
        const float vida = clamp01(in.corpo.vida.atual / in.corpo.vida.max);
        const Vec3f topo = pes + Vec3f{0, (in.agachado ? pi.alturaAgachado : pi.alturaEmPe) + 0.22f, 0};
        rlPushMatrix();
        rlTranslatef(topo.x, topo.y, topo.z);
        rlRotatef(yawCamera * RAD2DEG, 0, 1, 0);
        DrawCube({0, 0, 0}, 0.62f, 0.06f, 0.06f, Color{30, 26, 22, 230});
        DrawCube({0.3f * (1.f - vida), 0, 0}, 0.6f * vida, 0.07f, 0.07f, vida < 0.35f ? Color{200, 50, 40, 255} : Color{230, 220, 190, 255});
        rlPopMatrix();
        desenharEstadoDoAlvo(pes, in.agachado ? pi.alturaAgachado : pi.alturaEmPe, f, tr);

        if (!depuracaoCombate_) continue;
        // depuração: cone de visão para vestígios (linhas no chão) e a linha até o ponto investigado
        const Color c = corDoEstado(est);
        const Color linha{c.r, c.g, c.b, 150};
        const Vec3f o = pes + Vec3f{0, 0.07f, 0};
        const float r = pi.raioVisaoVestigio, a0 = in.corpo.yaw - pi.meioConeVisao;
        auto noChao = [&](float ang, float raio) { Vec3f q = pontoChao(o + Vec3f{std::sin(ang) * raio, 0, std::cos(ang) * raio}); return q + Vec3f{0, 0.07f, 0}; };
        DrawLine3D(v3(o), v3(noChao(a0, r)), linha);
        DrawLine3D(v3(o), v3(noChao(a0 + 2.f * pi.meioConeVisao, r)), linha);
        for (int i = 0; i < 12; i++) {
            const float a = a0 + 2.f * pi.meioConeVisao * float(i) / 12.f, b = a0 + 2.f * pi.meioConeVisao * float(i + 1) / 12.f;
            DrawLine3D(v3(noChao(a, r)), v3(noChao(b, r)), linha);
        }
        DrawCircle3D(v3(o), pi.raioPercepcaoProxima, {1, 0, 0}, 90.f, Color{c.r, c.g, c.b, 90});
        if (in.conduta == Conduta::Investigando) {
            DrawLine3D(v3(cabeca), v3(in.interesse + Vec3f{0, 0.1f, 0}), Color{240, 150, 60, 220});
            DrawCircle3D(v3(in.interesse + Vec3f{0, 0.06f, 0}), 0.55f, {1, 0, 0}, 90.f, Color{240, 150, 60, 200});
        }
    }
}

// ---------------------------------------------------------------- marcadores, prompts e depuração 2D

namespace {
// Ponto do mundo na tela; false se atrás da câmera ou longe da borda.
bool naTela(const Camera3D& cam, Vec3f p, Vector2& s) {
    const Vec3f olho{cam.position.x, cam.position.y, cam.position.z};
    const Vec3f frente = normalizar(Vec3f{cam.target.x, cam.target.y, cam.target.z} - olho);
    if (dot(p - olho, frente) < 0.5f) return false;
    s = GetWorldToScreen(v3(p), cam);
    return s.x > -80.f && s.x < float(GetScreenWidth()) + 80.f && s.y > -40.f && s.y < float(GetScreenHeight()) + 40.f;
}
}  // namespace

void Jogo::desenharRotulosCombate() {
    using namespace estilo;
    const Vec3f olho{cam_.position.x, cam_.position.y, cam_.position.z};
    // Um marcador por inimigo perto: losango com olho (investigando: amarelo se foi um vestígio,
    // vermelho se foi atacado) e badges de gelo/atordoamento. Com F3, logo abaixo, o nome, a vida e
    // o estado da IA. Os mais próximos ficam no lugar; um que cairia em cima de outro sobe até ficar
    // livre (a Dupla anda colada).
    constexpr float ICONE = 28.f;  // faixa reservada aos ícones, acima da caixa de texto
    struct Rotulo {
        Rectangle caixa;  // ícones + texto de depuração (usada para não sobrepor)
        const InimigoTeste* in;
        float tam, dist;
    };
    std::vector<Rotulo> rotulos;
    for (const InimigoTeste& in : inimigos_) {
        const float d = distXZ(olho, in.pes());
        Vector2 s;
        const float alt = in.agachado ? paramInimigo_.alturaAgachado : paramInimigo_.alturaEmPe;
        if (d > DIST_ROTULO || !naTela(cam_, in.pes() + Vec3f{0, alt + 0.55f, 0}, s)) continue;
        const float tam = d < 15.f ? 16.f : 14.f;
        float w = 60.f, h = ICONE;
        if (depuracaoCombate_) {
            w = std::max(w, std::max(largura(Fonte::Forte, in.nome + " 100/100", 14), largura(Fonte::Forte, descreverEstado(in), tam)) + 18.f);
            h += 40.f;
        }
        rotulos.push_back({{s.x - w * 0.5f, s.y - h, w, h}, &in, tam, d});
    }
    std::sort(rotulos.begin(), rotulos.end(), [](const Rotulo& a, const Rotulo& b) { return a.dist < b.dist; });
    char hp[32];
    for (size_t i = 0; i < rotulos.size(); i++) {
        for (int tentativa = 0; tentativa < 8; tentativa++) {
            bool bateu = false;
            for (size_t j = 0; j < i; j++)
                if (CheckCollisionRecs(rotulos[i].caixa, rotulos[j].caixa)) {
                    rotulos[i].caixa.y = rotulos[j].caixa.y - rotulos[i].caixa.height - 3.f;
                    bateu = true;
                }
            if (!bateu) break;
        }
        const Rotulo& r = rotulos[i];
        const InimigoTeste& in = *r.in;
        const EstadoInimigo e = estado(in);
        const float cx = r.caixa.x + r.caixa.width * 0.5f, iy = r.caixa.y + ICONE * 0.5f;
        // ícones: alerta no centro, badges à direita
        float bx = cx;
        if (e != EstadoInimigo::Derrotado && in.conduta == Conduta::Investigando) {
            oeste::losangoAlerta({cx, iy}, 11.f, in.motivo == MotivoAlerta::Atacado ? oeste::PERIGO : oeste::ALERTA);
            bx += 22.f;
        }
        if (e == EstadoInimigo::Congelado) oeste::badgeGelo({bx, iy}, 9.f);
        if (e == EstadoInimigo::Atordoado) oeste::badgeAtordoado({bx, iy}, 9.f, tempo_);
        if (!depuracaoCombate_) continue;
        const Rectangle txt{r.caixa.x, r.caixa.y + ICONE, r.caixa.width, 40.f};
        DrawRectangleRounded(txt, 0.3f, 6, Color{14, 12, 10, 175});
        std::snprintf(hp, sizeof hp, " %.0f/%.0f", std::max(0.f, in.corpo.vida.atual), in.corpo.vida.max);
        texto(Fonte::Forte, in.nome + hp, cx, txt.y + 2.f, 14, MARFIM_ESC, CENTRO, false);
        texto(Fonte::Forte, descreverEstado(in), cx, txt.y + 18.f, r.tam, corDoEstado(e), CENTRO, false);
    }
    // bandidos da recompensa: alerta (amarelo) e combate (vermelho)
    if (!cacada_.bando().vazio())
        for (const Bandido& b : cacada_.bando().bandidos()) {
            if (b.estado != EstadoBandido::Alerta && b.estado != EstadoBandido::Combate) continue;
            Vector2 s;
            if (distXZ(olho, b.corpo.pos) > 60.f || !naTela(cam_, b.corpo.pos + Vec3f{0, 2.3f, 0}, s)) continue;
            oeste::losangoAlerta(s, 11.f, b.estado == EstadoBandido::Combate ? oeste::PERIGO : oeste::ALERTA);
        }
    // pistas: losango de latão sempre; o nome do objeto com F3 (o prompt de perto fica em desenharPromptNoChao)
    for (size_t k = 0; k < pistas_.size(); k++) {
        const PistaEntalhada& p = pistas_[k];
        Vector2 s;
        if (p.inspecionada || int(k) == pistaPerto_ || distXZ(olho, p.pos) > DIST_ROTULO_PISTA || !naTela(cam_, p.pos + Vec3f{0, 0.75f, 0}, s)) continue;
        DrawPoly(s, 4, 7.f, 0, comAlfa(oeste::LATAO, 0.5f + 0.3f * std::sin(tempo_ * 3.f)));
        DrawPolyLinesEx(s, 4, 7.f, 0, 1.5f, oeste::PERGAMINHO);
        if (depuracaoCombate_) texto(Fonte::Forte, "Pista: " + p.objeto, s.x, s.y - 28.f, 15, OURO, CENTRO);
    }
    if (!depuracaoCombate_) return;
    for (const CapsulaVestigio& c : circuitos_.capsulas()) {
        Vector2 s;
        if (!c.descoberta || distXZ(olho, c.pos) > 25.f || !naTela(cam_, c.pos + Vec3f{0, 0.4f, 0}, s)) continue;
        texto(Fonte::Forte, "vestígio descoberto", s.x, s.y - 8.f, 14, Color{230, 90, 70, 255}, CENTRO);
    }
}

bool Jogo::desenharPromptNoChao() {
    Vector2 s;
    const float alfa = 0.85f + 0.15f * std::sin(tempo_ * 4.f);  // pulsa de leve
    if (pistaPerto_ >= 0) {
        const PistaEntalhada& p = pistas_[size_t(pistaPerto_)];
        if (!naTela(cam_, p.pos + Vec3f{0, 0.9f, 0}, s)) return false;
        oeste::prompt("E", p.ensina.tipo == Descoberta::Tipo::Entalhe ? "Inspecionar Entalhe" : "Inspecionar Pasta", s.x, s.y - 40.f, alfa);
        estilo::texto(estilo::Fonte::Texto, p.objeto, s.x, s.y - 4.f, 15, oeste::CINZA, estilo::CENTRO);
        return true;
    }
    if (capsulaPerto_ >= 0 && capsulaPerto_ < int(circuitos_.capsulas().size())) {
        if (!naTela(cam_, circuitos_.capsulas()[size_t(capsulaPerto_)].pos + Vec3f{0, 0.6f, 0}, s)) return false;
        oeste::prompt("E", "Recolher Cápsula", s.x, s.y - 40.f, alfa);
        estilo::texto(estilo::Fonte::Texto, "apaga o rastro", s.x, s.y - 4.f, 15, oeste::CINZA, estilo::CENTRO);
        return true;
    }
    return false;
}

void Jogo::desenharDepuracaoCombate() {
    // Quadro F3 (ligado por padrão), canto superior esquerdo: os números crus para testar.
    // Monta as linhas primeiro (cada uma com trechos de cores diferentes), mede e só então desenha.
    using namespace estilo;
    struct Trecho {
        std::string t;
        Color cor;
    };
    struct Linha {
        std::vector<Trecho> trechos;
        bool titulo = false;
    };
    std::vector<Linha> L;
    const Color BLOQUEADO{170, 96, 80, 255}, LARANJA{232, 150, 80, 255}, AZUL_UMIDO{150, 185, 225, 255};
    auto titulo = [&](const std::string& t) { L.push_back({{{t, OURO}}, true}); };
    auto linha = [&](const std::string& t, Color c = MARFIM_ESC) { L.push_back({{{t, c}}}); };
    char buf[200];
    const ParametrosCircuito& p = circuitos_.p;

    // ---- Circuitos de Queima: munição, pavio, rebote, estoque e receitas
    titulo("Circuitos de Queima · depuração (F3)");
    const Circuito* naVez = arsenal_.circuitoNaVez();
    linha("Ativa: " + (naVez ? nomeCombinacao(*naVez) : std::string("Munição comum")), naVez ? MARFIM : MARFIM_ESC);
    linha(std::string(arsenal_.selecionado() < 0 ? "> " : "   ") + "1  Comum", arsenal_.selecionado() < 0 ? OURO : MARFIM_ESC);
    for (int i = 0; i < arsenal_.slots(); i++) {
        const SlotMunicao& s = arsenal_.slot(i);
        const bool sel = arsenal_.selecionado() == i;
        linha(std::string(sel ? "> " : "   ") + std::to_string(i + 2) + "  " +
                  (s.cheio ? nomeCombinacao(s.circuito) + (s.imperfeita() ? "  (imperfeita)" : "") : "vazio"),
              sel ? OURO : (s.imperfeita() ? LARANJA : (s.cheio ? MARFIM : MARFIM_ESC)));
    }
    Color corPavio = MARFIM;
    switch (pavio_.estado()) {
    case EstadoPavio::Seco: std::snprintf(buf, sizeof buf, "Pavio: Seco"); break;
    case EstadoPavio::Umido:
        if (janelaForcar_ > 0.f) {
            std::snprintf(buf, sizeof buf, "Pavio: Úmido · atire agora para FORÇAR (%.1f s)", janelaForcar_);
            corPavio = LARANJA;
        } else {
            std::snprintf(buf, sizeof buf, "Pavio: Úmido (seca em %.1f s) · 2 cliques forçam", pavio_.umidade);
            corPavio = AZUL_UMIDO;
        }
        break;
    case EstadoPavio::Molhado:
        std::snprintf(buf, sizeof buf, "Pavio: Molhado (na água: a gravada falha)");
        corPavio = Color{140, 180, 225, 255};
        break;
    case EstadoPavio::BloqueadoFerro:
        std::snprintf(buf, sizeof buf, "Pavio: Bloqueado por Ferro Frio");
        corPavio = Color{170, 180, 200, 255};
        break;
    }
    linha(buf, corPavio);
    const SlotMunicao* bala = arsenal_.slotNaVez();
    const float risco = bala ? chanceRebote(*bala, pavio_, p) : 0.f;
    if (!bala) std::snprintf(buf, sizeof buf, "Gatilho: munição comum (sem risco de rebote)");
    else
        std::snprintf(buf, sizeof buf, "Gatilho: %s · pavio %s · rebote %.0f%%", bala->imperfeita() ? "bala imperfeita" : "bala cuidadosa",
                      nome(pavio_.estado()), risco * 100.f);
    linha(buf, tremor_.ativo() || risco > 0.f ? LARANJA : MARFIM_ESC);
    std::snprintf(buf, sizeof buf, "Doadores: Pena %d · Pedra %d · Carvão %d", arsenal_.estoque(Doador::Pena), arsenal_.estoque(Doador::PedraPesada),
                  arsenal_.estoque(Doador::CarvaoEmBrasa));
    linha(buf);
    // receitas: o que já se aprendeu (claro) e o que falta (apagado, com "?")
    std::snprintf(buf, sizeof buf, "Receitas: %d/%d entalhes · %d/%d pastas (inspecione pistas)", receitas_.entalhesConhecidos(), N_ENTALHES,
                  receitas_.pastasConhecidas(), N_PASTAS);
    linha(buf);
    auto listar = [&](const char* rotulo, int n, auto conhece, auto nomeDe) {
        Linha l{{{rotulo, MARFIM_ESC}}};
        for (int i = 0; i < n; i++) l.trechos.push_back({std::string(nomeDe(i)) + (conhece(i) ? "" : "?"), conhece(i) ? MARFIM : BLOQUEADO});
        L.push_back(l);
    };
    listar("   Entalhes:", N_ENTALHES, [&](int i) { return receitas_.conhece(Entalhe(i)); }, [](int i) { return nome(Entalhe(i)); });
    listar("   Pastas:", N_PASTAS, [&](int i) { return receitas_.conhece(Pasta(i)); }, [](int i) { return nome(Pasta(i)); });

    // ---- Combate reativo: estado e vida de cada inimigo de teste, vestígios e pistas
    titulo("Combate reativo");
    for (const InimigoTeste& in : inimigos_) {
        std::snprintf(buf, sizeof buf, "  %.0f/%.0f", std::max(0.f, in.corpo.vida.atual), in.corpo.vida.max);
        L.push_back({{{in.nome, MARFIM}, {descreverEstado(in), corDoEstado(estado(in))}, {buf, MARFIM_ESC}}});
    }
    int descobertas = 0;
    for (const CapsulaVestigio& c : circuitos_.capsulas()) descobertas += c.descoberta;
    std::snprintf(buf, sizeof buf, "Vestígios: %d cápsulas no chão (%d descobertas) · pistas: %d", int(circuitos_.capsulas().size()), descobertas,
                  int(pistas_.size()));
    linha(buf, descobertas > 0 ? Color{230, 110, 80, 255} : MARFIM_ESC);
    linha("1 comum · 2-4 gravadas · Tab troca · E: fogueira / pista / cápsula · F3 esconde");

    // ---- desenho: caixa discreta e linhas
    constexpr float X = 14.f, Y = 12.f, TAM = 14.f, ALT = 17.f, ESPACO = 8.f;
    float w = 0, h = 0;
    for (const Linha& l : L) {
        float lw = 0;
        for (const Trecho& t : l.trechos) lw += largura(Fonte::Texto, t.t, TAM) + ESPACO;
        w = std::max(w, lw);
        h += l.titulo ? ALT + 6.f : ALT;
    }
    DrawRectangleRounded({X - 8, Y - 6, w + 16, h + 12}, 0.03f, 6, Color{10, 8, 7, 165});
    DrawRectangleRoundedLinesEx({X - 8, Y - 6, w + 16, h + 12}, 0.03f, 6, 1.f, comAlfa(oeste::LATAO, 0.35f));
    float y = Y;
    for (const Linha& l : L) {
        if (l.titulo && y > Y) y += 6.f;
        float x = X;
        for (const Trecho& t : l.trechos) {
            texto(l.titulo ? Fonte::Forte : Fonte::Texto, t.t, x, y, TAM, t.cor, ESQ, false);
            x += largura(Fonte::Texto, t.t, TAM) + ESPACO;
        }
        y += ALT;
    }
}
