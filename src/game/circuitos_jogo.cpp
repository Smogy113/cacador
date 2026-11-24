// Circuitos de Queima no jogo: arena de teste, painel da fogueira, troca de munição, disparo com
// sacrifício do doador, impactos nos alvos, cápsulas de vestígio, desenho provisório e HUD.
//
// A regra fica em core/circuito (receitas, arsenal, pavio), core/projetil_encantado (voo, ricochete,
// Ferro Frio, zonas, cápsulas) e core/fisica_alvo (empurrão, arremesso, derrubada, gelo). Os
// números de ajuste ficam em ParametrosCircuito (circuitos_.p). Este arquivo só liga isso ao jogo e
// desenha com primitivas da raylib: para trocar o visual, mexa só em desenharCircuitos().
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/noise.h"
#include "core/terrain.h"
#include "game/jogo.h"
#include "render/estilo.h"
#include "rlgl.h"

namespace {
// Arena relativa ao ponto de partida do jogo (entrada de Pedrabranca, olhando para a ponte).
constexpr float INICIO_X = -40.f, INICIO_Z = 10.f, INICIO_YAW_GRAUS = 160.f;
constexpr float DIST_FOGUEIRA = 2.2f;       // alcance do E para abrir a fogueira
constexpr float DIST_DESENHO_ARENA = 90.f;  // além disso a arena não é montada (custo zero)
constexpr int MAX_FAISCAS = 220;
constexpr float ALTURA_PAREDE = 3.2f;
// Posições da arena em metros a partir da largada: para a frente do olhar inicial e para a direita.
struct PontoArena {
    float frente, direita;
};
constexpr PontoArena ARENA_FOGUEIRA{2.f, 3.f}, ARENA_CHUVA{3.f, -15.f}, ARENA_GRUPO{14.f, 7.f}, ARENA_FERRO{22.f, 2.f};
constexpr float CORREDOR_DIREITA = -6.f, CORREDOR_VAO = 5.f, CORREDOR_INICIO = 4.f, CORREDOR_FIM = 34.f;

Vec3f direitaDe(float yaw) { return {-std::cos(yaw), 0, std::sin(yaw)}; }
float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 v3(Vec3f p) { return {p.x, p.y, p.z}; }
unsigned char paraByte(float v) { return static_cast<unsigned char>(std::clamp(v, 0.f, 1.f) * 255.f + 0.5f); }
Color cor(Rgb c, float alfa = 1.f) { return {paraByte(c.r), paraByte(c.g), paraByte(c.b), paraByte(alfa)}; }
float h01(uint32_t s) { return float(hash32(s) & 0xFFFFu) / 65535.f; }  // 0..1 estável por semente
float angDif(float a) { return std::remainder(a, 6.2831853f); }

// Cor provisória de cada propriedade (projétil, rastro, impacto, cápsula).
Rgb corDe(Propriedade p) {
    switch (p) {
    case Propriedade::Velocidade: return {0.72f, 0.92f, 1.f};
    case Propriedade::Arrasto: return {0.66f, 0.52f, 0.95f};
    case Propriedade::Peso: return {0.92f, 0.72f, 0.42f};
    case Propriedade::Levante: return {0.55f, 1.f, 0.66f};
    case Propriedade::Calor: return {1.f, 0.48f, 0.14f};
    case Propriedade::Frio: return {0.62f, 0.86f, 1.f};
    }
    return {1, 1, 1};
}
const Rgb CINZA_CHUMBO{0.62f, 0.62f, 0.6f};

struct Sprite {
    Vec3f pos;
    float tam;
    Color cor;
};
}  // namespace

Vec3f Jogo::pontoChao(Vec3f p) const { return {p.x, terreno_->altura(p.x, p.z), p.z}; }

bool Jogo::naAgua(Vec3f p) const {
    // mesmo critério do som de passo na água (pisoSonoro), mais a zona de chuva da arena
    bool rio = p.y < terreno_->nivelAgua() - .06f && terreno_->mascaraRio(p.x, p.z) > .16f;
    bool chuva = arena_.pronta && std::hypot(p.x - arena_.chuva.x, p.z - arena_.chuva.z) <= arena_.raioChuva &&
                 p.y < arena_.chuva.y + arena_.alturaChuva;
    return rio || chuva;
}

// ---------------------------------------------------------------- arena

void Jogo::iniciarCircuitos() {
    const float yaw = INICIO_YAW_GRAUS * DEG2RAD;
    const Vec3f s{INICIO_X, 0, INICIO_Z}, f = frenteDe(yaw), d = direitaDe(yaw);
    auto em = [&](PontoArena q) { return s + f * q.frente + d * q.direita; };
    arena_.fogueira = pontoChao(em(ARENA_FOGUEIRA));
    arena_.chuva = pontoChao(em(ARENA_CHUVA));
    // Corredor de ricochete à esquerda: duas paredes paralelas ao olhar inicial. Fica fora da linha
    // de tiro da largada até os bonecos e o Ferro Frio, que ficam à direita.
    for (float lado : {-CORREDOR_VAO * 0.5f, CORREDOR_VAO * 0.5f}) {
        float meio = (CORREDOR_INICIO + CORREDOR_FIM) * 0.5f;
        Vec3f c = pontoChao(s + f * meio + d * (CORREDOR_DIREITA + lado));
        CaixaGirada w{c, 0.15f, (CORREDOR_FIM - CORREDOR_INICIO) * 0.5f, yaw, c.y - 0.5f, c.y + ALTURA_PAREDE};
        arena_.paredes.push_back(w);
        col_.adicionarGirada(w);
    }
    // Bonecos de treino: quatro juntos (para área e onda de pressão).
    const Vec3f grupo = em(ARENA_GRUPO);
    for (float k : {-2.25f, -0.75f, 0.75f, 2.25f}) {
        AlvoTreino b;
        b.base = b.pos = pontoChao(grupo + d * k + f * (0.4f * std::sin(k * 3.f)));
        b.yaw = yaw + 3.14159f;  // de frente para quem chega
        bonecos_.push_back(b);
    }
    // Barreira de Ferro Frio mais adiante, com um boneco atrás dela.
    const Vec3f centroFerro = pontoChao(em(ARENA_FERRO));
    arena_.ferro = {{centroFerro.x - 3.f, centroFerro.y - 0.5f, centroFerro.z - 0.6f}, {centroFerro.x + 3.f, centroFerro.y + 3.2f, centroFerro.z + 0.6f}};
    circuitos_.definirFerro({ZonaFerroFrio{arena_.ferro}});
    {
        AlvoTreino b;
        b.base = b.pos = pontoChao(centroFerro + f * 5.f);
        b.yaw = yaw + 3.14159f;
        bonecos_.push_back(b);
    }
    arena_.pronta = true;
    std::printf("circuitos: fogueira=(%.1f, %.1f) chuva=(%.1f, %.1f) ferro=(%.1f, %.1f) bonecos=(%.1f, %.1f)\n", arena_.fogueira.x,
                arena_.fogueira.z, arena_.chuva.x, arena_.chuva.z, centroFerro.x, centroFerro.z, grupo.x, grupo.z);

    if (op_.abrirFogueira) {  // capturas do painel: uma receita inválida na tela, para mostrar o aviso
        arsenal_.preparar(0, {Entalhe::Espiral, Pasta::ResinaECera, Doador::Pena});
        painel_.aberto = true;
        painel_.linha = 3;
        painel_.slot = 1;
        painel_.circuito = {Entalhe::Onda, Pasta::PoDePrata, Doador::PedraPesada};
    }
    // Capturas: já entra com as três receitas da especificação gravadas.
    if (op_.roteiroCircuito > 0) {
        arsenal_.preparar(0, {Entalhe::Espiral, Pasta::ResinaECera, Doador::Pena});
        arsenal_.preparar(1, {Entalhe::Grade, Pasta::EnxofreESal, Doador::PedraPesada});
        arsenal_.preparar(2, {Entalhe::Onda, Pasta::PoDePrata, Doador::CarvaoEmBrasa});
        const int slot[] = {0, 0, 1, 2, 1, 2};
        arsenal_.selecionar(slot[std::clamp(op_.roteiroCircuito, 0, 5)]);
        if (op_.roteiroCircuito == 4) {  // dentro da chuva
            jogador_.pos = pontoChao(arena_.chuva + d * 1.5f);
            jogador_.yaw = rig_.yaw = std::atan2(grupo.x - jogador_.pos.x, grupo.z - jogador_.pos.z);
        }
        if (op_.roteiroCircuito == 1)  // na boca do corredor de ricochete
            jogador_.pos = pontoChao(s + f * (CORREDOR_INICIO - 3.f) + d * CORREDOR_DIREITA);
    }
}

// Entrada simulada (--screenshot ... --roteiro-circuito N): mira a câmera no alvo do roteiro e
// atira no quadro 25.
void Jogo::roteiroCircuitos(Entrada& e) {
    const int q = quadroRoteiro_++;
    const int modo = op_.roteiroCircuito;
    if (bonecos_.size() < 5 || arena_.paredes.size() < 2) return;
    const float yaw = INICIO_YAW_GRAUS * DEG2RAD;
    const Vec3f f = frenteDe(yaw), d = direitaDe(yaw);
    const Vec3f grupo = (bonecos_[1].base + bonecos_[2].base) * 0.5f;
    Vec3f alvo = grupo + Vec3f{0, 1.f, 0};
    if (modo == 1) {  // face interna da parede direita, na altura da boca: quica pelo corredor
        Vec3f s{INICIO_X, 0, INICIO_Z};
        float faceDireita = CORREDOR_DIREITA + CORREDOR_VAO * 0.5f - 0.2f;
        alvo = pontoChao(s + f * (CORREDOR_INICIO + 2.f) + d * faceDireita) + Vec3f{0, 1.45f, 0};
    } else if (modo == 3) {
        alvo = grupo - f * 1.f;  // o chão na frente do grupo
    } else if (modo == 5) {
        alvo = bonecos_[4].base + Vec3f{0, 1.f, 0};  // o boneco atrás do Ferro Frio
    }
    // corrige a câmera um pouco a cada quadro até o centro da tela apontar para o alvo
    Vec3f olho{cam_.position.x, cam_.position.y, cam_.position.z};
    Vec3f atual = normalizar(Vec3f{cam_.target.x, cam_.target.y, cam_.target.z} - olho), quer = normalizar(alvo - olho);
    if (q > 0) {
        rig_.yaw += angDif(std::atan2(quer.x, quer.z) - std::atan2(atual.x, atual.z));
        rig_.pitch += std::asin(std::clamp(-quer.y, -1.f, 1.f)) - std::asin(std::clamp(-atual.y, -1.f, 1.f));
    }
    e.mirar = true;
    e.atirar = q == 25;
}

// ---------------------------------------------------------------- interação

bool Jogo::interagirCircuitos(Entrada& e) {
    pertoFogueira_ = arena_.pronta && distXZ(jogador_.pos, arena_.fogueira) < DIST_FOGUEIRA;
    capsulaPerto_ = circuitos_.capsulaPerto(jogador_.pos, circuitos_.p.distRecolher);
    if (!e.interagir) return false;
    if (pertoFogueira_) {
        painel_.aberto = true;
        painel_.retorno.clear();
        if (arsenal_.slot(painel_.slot).cheio) painel_.circuito = arsenal_.slot(painel_.slot).circuito;
        EnableCursor();
        e.interagir = false;
        return true;
    }
    if (capsulaPerto_ >= 0) {
        int n = circuitos_.recolher(jogador_.pos, circuitos_.p.distRecolher);
        char buf[96];
        std::snprintf(buf, sizeof buf, n == 1 ? "Cápsula recolhida: um rastro a menos." : "%d cápsulas recolhidas: rastros apagados.", n);
        avisar(buf);
        capsulaPerto_ = -1;
        e.interagir = false;
        return true;
    }
    return false;
}

void Jogo::atualizarPainelFogueira() {
    PainelFogueira& pn = painel_;
    pn.retornoT = std::max(0.f, pn.retornoT - GetFrameTime());
    if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ESCAPE)) {
        pn.aberto = false;
        if (!op_.semEntrada) DisableCursor();
        return;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) pn.linha = (pn.linha + 3) % 4;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) pn.linha = (pn.linha + 1) % 4;
    int passo = (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) - (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A));
    if (passo != 0) {
        auto girar = [&](int v, int n) { return (v + passo + n) % n; };
        Circuito& c = pn.circuito;
        if (pn.linha == 0) {
            pn.slot = girar(pn.slot, arsenal_.slots());
            if (arsenal_.slot(pn.slot).cheio) c = arsenal_.slot(pn.slot).circuito;  // mostra o que está gravado
        }
        if (pn.linha == 1) c.entalhe = Entalhe(girar(int(c.entalhe), N_ENTALHES));
        if (pn.linha == 2) c.pasta = Pasta(girar(int(c.pasta), N_PASTAS));
        if (pn.linha == 3) c.doador = Doador(girar(int(c.doador), N_DOADORES));
    }
    auto responder = [&](const std::string& t) {
        pn.retorno = t;
        pn.retornoT = 3.f;
    };
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        ResultadoPreparo r = arsenal_.preparar(pn.slot, pn.circuito);
        if (r == ResultadoPreparo::Ok) responder("Slot " + std::to_string(pn.slot + 2) + ": " + nomeCombinacao(pn.circuito));
        else if (r == ResultadoPreparo::ReceitaInvalida)
            responder(std::string("Receita inválida: ") + nome(pn.circuito.entalhe) + " exige " + nome(doadorDaReceita(pn.circuito.entalhe)));
        else responder(nome(r));
    }
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) {
        arsenal_.esvaziar(pn.slot);
        responder("Slot " + std::to_string(pn.slot + 2) + " esvaziado; o doador voltou ao estoque.");
    }
    if (IsKeyPressed(KEY_R)) {
        arsenal_.reporEstoque();
        responder("Estoque de doadores reposto (teste).");
    }
}

// ---------------------------------------------------------------- disparo

bool Jogo::balaGravadaFalhou(Vec3f boca) {
    if (!arsenal_.preparadaNaVez() || pavio_.estado() != EstadoPavio::Molhado) return false;
    arma_.espera = Revolver::CADENCIA;  // o cão bate, nada acende
    soltarFaiscas(boca, 5, Rgb{0.55f, 0.55f, 0.55f}, 0.6f, 0.7f, 0.07f, false, -0.4f);
    avisar("Pavio molhado: a bala gravada falhou. Troque para a comum (1) ou espere secar.");
    return true;
}

bool Jogo::dispararCircuito(Vec3f boca, Vec3f dir) {
    if (!arsenal_.preparadaNaVez()) return false;
    const Circuito c = arsenal_.consumirNaVez();
    // Vestígio: a cápsula marcada cai ao lado do caçador em todo disparo de bala gravada.
    Vec3f casca = jogador_.pos + direitaJogador() * 0.55f - frenteDe(jogador_.yaw) * 0.25f;
    circuitos_.soltarCapsula(pontoChao(casca), jogador_.yaw + h01(uint32_t(tempo_ * 1000.f)) * 6.28f, c);
    if (pavio_.estado() == EstadoPavio::BloqueadoFerro || circuitos_.noFerro(boca)) {
        soltarFaiscas(boca, 8, CINZA_CHUMBO, 1.5f, 0.4f, 0.05f, true, 3.f);
        avisar("Ferro Frio: o circuito não acendeu. Saiu chumbo comum.");
        return false;
    }
    circuitos_.disparar(boca, dir, c);
    // Sacrifício: o doador queima na boca do cano.
    switch (c.doador) {
    case Doador::Pena: soltarFaiscas(boca, 10, Rgb{0.96f, 0.95f, 0.9f}, 0.8f, 1.3f, 0.07f, false, 0.25f); break;
    case Doador::PedraPesada: soltarFaiscas(boca, 10, Rgb{0.45f, 0.43f, 0.4f}, 1.6f, 0.8f, 0.05f, false, 9.f); break;
    case Doador::CarvaoEmBrasa: soltarFaiscas(boca, 14, Rgb{1.f, 0.45f, 0.12f}, 1.8f, 0.7f, 0.05f, true, -0.5f); break;
    }
    soltarFaiscas(boca, 6, corDe(resolverEfeito(c, circuitos_.p).propriedade), 2.5f, 0.25f, 0.09f, true, 0.f);
    avisar(std::string(nome(c.doador)) + " queimou: " + nomeCombinacao(c));
    return true;
}

std::vector<Alvo> Jogo::alvosCircuito() const {
    std::vector<Alvo> r = cacada_.bando().vazio() ? std::vector<Alvo>{} : cacada_.bando().alvos();
    for (const AlvoTreino& b : bonecos_) {
        Alvo a{b.pos + Vec3f{0, b.fisica.voo, 0}, 1.75f};
        if (!b.ativo()) a.pes.y = -1e6f;  // destruído: fora do alcance
        r.push_back(a);
    }
    return r;
}

void Jogo::aplicarImpacto(const Impacto& im) {
    const ParametrosCircuito& p = circuitos_.p;
    bool acertou = false, matou = false;
    Bando& bando = cacada_.bando();
    const int nB = bando.vazio() ? 0 : int(bando.bandidos().size());
    for (int i = 0; i < nB; i++) {
        const Bandido& b = bando.bandidos()[size_t(i)];
        Vec3f centro = b.corpo.pos + Vec3f{0, b.fisica.voo + 0.9f, 0};
        const bool direto = im.alvo == i;
        if (direto) {  // o chumbo pega como um tiro comum (cabeça continua valendo)
            matou |= bando.levarTiro(i, im.parte);
            acertou = true;
        }
        GolpeAlvo g = golpeDoImpacto(im, centro, direto, p);
        if (!g.vazio()) {
            matou |= bando.aplicarGolpe(i, g);
            acertou = true;
        }
    }
    for (size_t j = 0; j < bonecos_.size(); j++) {
        AlvoTreino& b = bonecos_[j];
        if (!b.ativo()) continue;
        const bool direto = im.alvo == nB + int(j);
        GolpeAlvo g = golpeDoImpacto(im, b.pos + Vec3f{0, b.fisica.voo + 0.9f, 0}, direto, p);
        if (direto) g.dano += p.danoBala;
        if (!g.vazio()) {
            matou |= golpear(b, g);
            acertou = true;
        }
    }
    std::printf("circuitos: impacto %s em (%.1f, %.1f, %.1f)%s%s\n", nomeCombinacao(im.circuito).c_str(), im.ponto.x, im.ponto.y, im.ponto.z,
                im.encantado ? "" : " como chumbo", acertou ? " atingiu alvo" : "");
    if (acertou) {
        marcaAcerto_ = 0.2f;
        marcaMatou_ = matou;
    }

    // ---- visual provisório do impacto ----
    if (!im.encantado) {
        soltarFaiscas(im.ponto, 5, Rgb{0.55f, 0.48f, 0.38f}, 1.2f, 0.45f, 0.08f, false, 2.f, im.normal * 0.8f);
        return;
    }
    const EfeitoCircuito& e = im.efeito;
    const Rgb c = corDe(e.propriedade);
    const Vec3f chao = pontoChao(im.ponto);
    switch (e.propriedade) {
    case Propriedade::Velocidade:
    case Propriedade::Arrasto:
        soltarFaiscas(im.ponto, 16, c, 6.f, 0.35f, 0.07f, true, 4.f, im.normal * 2.f);
        break;
    case Propriedade::Peso:  // anel de pressão + poeira rasteira
        ondas_.push_back({chao, e.raio, 0.f, 0.55f, c, false});
        for (int k = 0; k < 18; k++) {
            float a = float(k) / 18.f * 6.2832f;
            soltarFaiscas(chao + Vec3f{std::cos(a) * 0.4f, 0.15f, std::sin(a) * 0.4f}, 1, Rgb{0.55f, 0.47f, 0.36f}, 0.3f, 0.8f, 0.45f,
                          false, 0.f, Vec3f{std::cos(a), 0.15f, std::sin(a)} * (e.raio * 1.6f));
        }
        break;
    case Propriedade::Levante:  // coluna que sobe
        ondas_.push_back({chao, e.raio, 0.f, 0.7f, c, true});
        soltarFaiscas(chao + Vec3f{0, 0.2f, 0}, 22, c, 1.2f, 0.9f, 0.08f, true, -2.f, Vec3f{0, 6.f, 0});
        break;
    case Propriedade::Calor:
        soltarFaiscas(im.ponto, e.duracaoZona > 0 ? 12 : 30, c, e.duracaoZona > 0 ? 2.5f : 5.f, 0.6f, 0.14f, true, -1.5f);
        break;
    case Propriedade::Frio:
        soltarFaiscas(im.ponto, e.duracaoZona > 0 ? 14 : 26, Rgb{0.85f, 0.95f, 1.f}, 3.f, 0.6f, 0.08f, true, 2.f);
        break;
    }
    if (sons_.ok() && (e.propriedade == Propriedade::Peso || e.propriedade == Propriedade::Levante)) {
        float dist = len(im.ponto - jogador_.pos);
        sons_.tiro(AcusticaTiro::Externa, 0.7f / (1.f + dist * .05f), panSonoro(im.ponto, .38f));
    }
}

void Jogo::soltarFaiscas(Vec3f p, int n, Rgb c, float vel, float vida, float tam, bool aditiva, float gravidade, Vec3f base) {
    for (int k = 0; k < n; k++) {
        Vec3f r{aleatorio01(rngEfeito_) - 0.5f, aleatorio01(rngEfeito_) - 0.3f, aleatorio01(rngEfeito_) - 0.5f};
        float v = vida * (0.6f + 0.6f * aleatorio01(rngEfeito_));
        faiscas_.push_back({p, base + normalizar(r) * (vel * (0.4f + 0.8f * aleatorio01(rngEfeito_))), v, v, tam, gravidade, c, aditiva});
    }
    if (faiscas_.size() > size_t(MAX_FAISCAS)) faiscas_.erase(faiscas_.begin(), faiscas_.end() - MAX_FAISCAS);
}

// ---------------------------------------------------------------- quadro

void Jogo::atualizarCircuitos(float dt, const Entrada& e) {
    const ParametrosCircuito& p = circuitos_.p;
    // troca rápida de munição
    if (e.escolherMunicao >= -1 || e.proximaMunicao) {
        if (e.proximaMunicao) arsenal_.proximo();
        else arsenal_.selecionar(e.escolherMunicao);
        const Circuito* c = arsenal_.circuitoNaVez();
        if (c) avisar("Na vez: " + nomeCombinacao(*c));
        else if (arsenal_.selecionado() >= 0) avisar("Slot " + std::to_string(arsenal_.selecionado() + 2) + " vazio: atira comum.");
        else avisar("Na vez: munição comum.");
    }
    if (montadoEm_ >= 0) pertoFogueira_ = false, capsulaPerto_ = -1;  // interagirCircuitos() só roda a pé

    // pavio
    const Vec3f peito = jogador_.pos + Vec3f{0, montadoEm_ >= 0 ? 2.2f : 1.3f, 0};
    const EstadoPavio antes = pavio_.estado();
    pavio_.atualizar(dt, naAgua(jogador_.pos) || naAgua(peito), circuitos_.noFerro(peito), p.tempoSecagem);
    if (pavio_.estado() != antes) {
        if (pavio_.estado() == EstadoPavio::Molhado && antes == EstadoPavio::Seco) avisar("O pavio molhou: as balas gravadas vão falhar.");
        if (pavio_.estado() == EstadoPavio::BloqueadoFerro) avisar("Ferro Frio: nenhum circuito acende aqui.");
        if (pavio_.estado() == EstadoPavio::Seco && antes == EstadoPavio::Molhado) avisar("O pavio secou.");
    }

    for (AlvoTreino& b : bonecos_) ::atualizar(b, dt, *terreno_, col_);  // a do core, nao Jogo::atualizar

    // projéteis, impactos e zonas
    EventosCircuito ev = circuitos_.atualizar(dt, *terreno_, col_, alvosCircuito());
    for (Vec3f r : ev.ricochetes) {
        soltarFaiscas(r, 10, Rgb{1.f, 0.85f, 0.55f}, 4.f, 0.3f, 0.05f, true, 6.f);
        std::printf("circuitos: ricochete em (%.1f, %.1f, %.1f)\n", r.x, r.y, r.z);
    }
    for (Vec3f r : ev.desencantos) soltarFaiscas(r, 14, Rgb{0.7f, 0.78f, 0.9f}, 3.f, 0.5f, 0.06f, true, 5.f);
    if (!ev.desencantos.empty()) avisar("O Ferro Frio apagou o circuito: a bala seguiu como chumbo.");
    for (const Impacto& im : ev.impactos) aplicarImpacto(im);
    Bando& bando = cacada_.bando();
    for (const ZonaElemental& z : circuitos_.zonas()) {
        for (int i = 0; !bando.vazio() && i < int(bando.bandidos().size()); i++) {
            GolpeAlvo g = golpeDaZona(z, bando.bandidos()[size_t(i)].corpo.pos, dt, p);
            if (!g.vazio()) bando.aplicarGolpe(i, g);
        }
        for (AlvoTreino& b : bonecos_) {
            GolpeAlvo g = golpeDaZona(z, b.pos, dt, p);
            if (!g.vazio()) golpear(b, g);
        }
    }

    // efeitos visuais
    for (Faisca& f : faiscas_) {
        f.vida -= dt;
        f.vel.y -= f.gravidade * dt;
        f.vel = f.vel * std::exp(-1.2f * dt);
        f.pos = f.pos + f.vel * dt;
    }
    faiscas_.erase(std::remove_if(faiscas_.begin(), faiscas_.end(), [](const Faisca& f) { return f.vida <= 0.f; }), faiscas_.end());
    for (OndaVisual& o : ondas_) o.idade += dt;
    ondas_.erase(std::remove_if(ondas_.begin(), ondas_.end(), [](const OndaVisual& o) { return o.idade >= o.duracao; }), ondas_.end());
}

// ---------------------------------------------------------------- desenho

void Jogo::desenharCircuitos() {
    if (!arena_.pronta) return;
    const float t = tempo_;
    const Vec3f olho{cam_.position.x, cam_.position.y, cam_.position.z};
    const Vec3f frente = normalizar(Vec3f{cam_.target.x, cam_.target.y, cam_.target.z} - olho);
    std::vector<Sprite> alfa, brilho;
    struct Caixa {
        Vec3f c, tam;
        Color cor;
    };
    std::vector<Caixa> vidros;  // gelo e Ferro Frio, translúcidos

    // ---- arena (só perto) ----
    const bool arenaPerto = distXZ(olho, arena_.fogueira) < DIST_DESENHO_ARENA;
    if (arenaPerto) {
        const Vec3f fg = arena_.fogueira;
        for (int k = 0; k < 8; k++) {
            float a = float(k) * 0.7854f, tom = 0.36f + 0.1f * h01(uint32_t(k) + 11u);
            DrawSphereEx(v3(fg + Vec3f{std::cos(a) * 0.55f, 0.07f, std::sin(a) * 0.55f}), 0.13f, 4, 6, cor(Rgb{tom, tom * 0.97f, tom * 0.92f}));
        }
        for (int k = 0; k < 3; k++) {
            float a = float(k) * 2.094f + 0.3f;
            DrawCylinderEx(v3(fg + Vec3f{std::cos(a) * 0.42f, 0.05f, std::sin(a) * 0.42f}),
                           v3(fg + Vec3f{-std::cos(a) * 0.08f, 0.34f, -std::sin(a) * 0.08f}), 0.06f, 0.045f, 6, Color{58, 40, 26, 255});
        }
        DrawSphereEx(v3(fg + Vec3f{0, 0.04f, 0}), 0.2f, 3, 6, Color{150, 46, 18, 255});
        for (int i = 0; i < 5; i++) {
            float fi = float(i);
            Vec3f pc = fg + Vec3f{0.05f * std::sin(t * 6.f + fi), 0.22f + 0.13f * fi + 0.04f * std::sin(t * 11.f + fi), 0.05f * std::cos(t * 5.f + fi)};
            brilho.push_back({pc, 0.5f - 0.07f * fi + 0.06f * std::sin(t * 13.f + fi), cor(Rgb{1.f, 0.55f + 0.08f * fi, 0.22f}, 0.7f)});
        }
        // chuva forte
        const Vec3f ch = arena_.chuva;
        const float r = arena_.raioChuva;
        DrawCylinder(v3(ch + Vec3f{0, 0.03f, 0}), r, r, 0.02f, 32, Color{70, 95, 120, 70});
        DrawCircle3D(v3(ch + Vec3f{0, 0.06f, 0}), r, {1, 0, 0}, 90.f, Color{150, 185, 220, 160});
        for (int i = 0; i < 140; i++) {
            float raio = r * std::sqrt(h01(uint32_t(i) * 3u + 1u)), ang = h01(uint32_t(i) * 3u + 2u) * 6.2832f;
            float queda = std::fmod(t * 9.f + h01(uint32_t(i) * 3u + 3u) * arena_.alturaChuva, arena_.alturaChuva);
            Vec3f pg = ch + Vec3f{std::cos(ang) * raio, arena_.alturaChuva - queda, std::sin(ang) * raio};
            DrawLine3D(v3(pg), v3(pg + Vec3f{0.04f, -0.5f, 0.02f}), Color{170, 200, 230, 150});
        }
        // paredes do corredor de ricochete (caixas giradas)
        for (const CaixaGirada& w : arena_.paredes) {
            rlPushMatrix();
            rlTranslatef(w.centro.x, (w.y0 + w.y1) * 0.5f, w.centro.z);
            rlRotatef(w.rot * RAD2DEG, 0, 1, 0);
            DrawCube({0, 0, 0}, w.meiaX * 2.f, w.y1 - w.y0, w.meiaZ * 2.f, Color{112, 84, 58, 255});
            DrawCubeWires({0, 0, 0}, w.meiaX * 2.f, w.y1 - w.y0, w.meiaZ * 2.f, Color{70, 50, 34, 255});
            rlPopMatrix();
        }
        // barreira de Ferro Frio: postes e travessas de ferro, volume azulado translúcido
        const AABB& fe = arena_.ferro;
        const Color ferro{64, 72, 84, 255};
        for (float x = fe.min.x; x <= fe.max.x + 0.01f; x += (fe.max.x - fe.min.x) / 6.f)
            for (float z : {fe.min.z, fe.max.z}) DrawCube({x, (fe.min.y + fe.max.y) * 0.5f, z}, 0.12f, fe.max.y - fe.min.y, 0.12f, ferro);
        for (float y : {fe.min.y + 1.1f, fe.max.y - 0.1f})
            for (float z : {fe.min.z, fe.max.z}) DrawCube({(fe.min.x + fe.max.x) * 0.5f, y, z}, fe.max.x - fe.min.x, 0.08f, 0.08f, ferro);
        vidros.push_back({(fe.min + fe.max) * 0.5f, fe.max - fe.min, Color{120, 150, 200, 38}});
    }

    // ---- bonecos de treino ----
    for (const AlvoTreino& b : bonecos_) {
        if (distXZ(olho, b.base) > DIST_DESENHO_ARENA) continue;
        DrawCylinder(v3(b.base), 0.05f, 0.05f, 0.25f, 6, Color{60, 44, 30, 255});  // toco da base
        if (!b.ativo()) continue;
        const FisicaAlvo& f = b.fisica;
        Vec3f pes = b.pos + Vec3f{0, f.voo, 0};
        // derrubado: deitado na direção do empurrão (ou para trás); sacudida no golpe
        Vec3f para = len(f.empurrao) > 0.1f ? normalizar(f.empurrao) : frenteDe(b.yaw) * -1.f;
        float tomba = f.derrubado > 0.f ? std::min(1.f, f.derrubado * 3.f) : 0.f;
        float sacode = 0.12f * b.tranco * std::sin(t * 40.f);
        Vec3f cima = normalizar(Vec3f{0, 1, 0} * (1.f - tomba) + (para + Vec3f{sacode, 0, 0}) * (tomba * 1.2f + std::abs(sacode)));
        Rgb pano{0.66f, 0.55f, 0.38f};
        if (f.queimando > 0.f) pano = lerp(pano, Rgb{0.35f, 0.18f, 0.08f}, 0.6f + 0.2f * std::sin(t * 20.f));
        if (f.congelado > 0.f) pano = lerp(pano, Rgb{0.75f, 0.9f, 1.f}, 0.65f);
        if (b.tranco > 0.5f) pano = lerp(pano, Rgb{0.9f, 0.3f, 0.25f}, b.tranco - 0.5f);
        Vec3f base = pes + cima * 0.1f;
        DrawCylinderEx(v3(base), v3(base + cima * 1.35f), 0.24f, 0.2f, 8, cor(pano));
        DrawSphereEx(v3(base + cima * 1.52f), 0.16f, 5, 8, cor(pano * 0.92f));
        DrawCylinderEx(v3(base + cima * 1.0f - direitaDe(b.yaw) * 0.42f), v3(base + cima * 1.0f + direitaDe(b.yaw) * 0.42f), 0.05f, 0.05f, 5,
                       cor(pano * 0.8f));  // braços
        // vida
        float vida = clamp01(b.vida.atual / b.vida.max);
        Vec3f topo = pes + Vec3f{0, 2.05f, 0};
        DrawCube(v3(topo), 0.62f, 0.06f, 0.06f, Color{30, 26, 22, 230});
        DrawCube(v3(topo - direitaDe(b.yaw) * (0.3f * (1.f - vida))), 0.6f * vida, 0.07f, 0.07f, vida < 0.35f ? Color{200, 50, 40, 255} : Color{230, 220, 190, 255});
        if (f.congelado > 0.f) vidros.push_back({pes + Vec3f{0, 0.9f, 0}, {0.85f, 1.95f, 0.85f}, Color{170, 225, 255, 95}});
        if (f.queimando > 0.f)
            for (int k = 0; k < 3; k++)
                brilho.push_back({pes + Vec3f{0, 0.5f + 0.45f * float(k) + 0.1f * std::sin(t * 9.f + float(k)), 0}, 0.55f, Color{255, 120, 40, 150}});
    }
    // bandidos congelados ganham o mesmo bloco de gelo
    if (!cacada_.bando().vazio())
        for (const Bandido& b : cacada_.bando().bandidos())
            if (b.fisica.congelado > 0.f) vidros.push_back({b.corpo.pos + Vec3f{0, b.fisica.voo + 0.92f, 0}, {0.9f, 2.f, 0.9f}, Color{170, 225, 255, 95}});

    // ---- cápsulas de vestígio ----
    for (const CapsulaVestigio& c : circuitos_.capsulas()) {
        if (distXZ(olho, c.pos) > 60.f) continue;
        Rgb marca = corDe(resolverEfeito(c.circuito, circuitos_.p).propriedade);
        Vec3f a = c.pos + Vec3f{0, 0.035f, 0};
        Vec3f eixo{std::sin(c.yaw) * 0.1f, 0, std::cos(c.yaw) * 0.1f};  // deitada no chão
        DrawCylinderEx(v3(a - eixo * 0.5f), v3(a + eixo * 0.5f), 0.032f, 0.032f, 8, Color{184, 148, 72, 255});
        DrawCylinderEx(v3(a + eixo * 0.15f), v3(a + eixo * 0.32f), 0.035f, 0.035f, 8, cor(marca));
        brilho.push_back({a + Vec3f{0, 0.12f, 0}, 0.18f + 0.04f * std::sin(t * 4.f + c.yaw), cor(marca, 0.55f)});
    }

    // ---- zonas elementais ----
    for (const ZonaElemental& z : circuitos_.zonas()) {
        float vida = clamp01(z.restante / std::max(0.01f, z.duracao)), surge = clamp01((z.duracao - z.restante) / 0.3f);
        float a = std::min(1.f, vida * 4.f) * surge;
        Rgb c = z.frio ? Rgb{0.72f, 0.9f, 1.f} : Rgb{1.f, 0.45f, 0.12f};
        DrawCylinder(v3(z.centro + Vec3f{0, 0.04f, 0}), z.raio, z.raio, 0.03f, 28, cor(c, (z.frio ? 0.45f : 0.3f) * a));
        DrawCircle3D(v3(z.centro + Vec3f{0, 0.08f, 0}), z.raio, {1, 0, 0}, 90.f, cor(c, 0.8f * a));
        const uint32_t s0 = uint32_t(std::abs(z.centro.x * 13.f + z.centro.z * 7.f));
        for (int k = 0; k < 12; k++) {
            float r = z.raio * std::sqrt(h01(s0 + uint32_t(k) * 2u)), ang = h01(s0 + uint32_t(k) * 2u + 1u) * 6.2832f;
            Vec3f q = pontoChao(z.centro + Vec3f{std::cos(ang) * r, 0, std::sin(ang) * r});
            if (z.frio) {  // cristais de gelo
                float alt = (0.25f + 0.35f * h01(s0 + uint32_t(k) * 5u)) * surge;
                DrawCylinderEx(v3(q), v3(q + Vec3f{0.08f * std::sin(ang), alt, 0.08f * std::cos(ang)}), 0.07f, 0.f, 4, cor(Rgb{0.85f, 0.96f, 1.f}, 0.85f * a));
            } else {  // labaredas
                float h = 0.3f + 0.25f * std::sin(t * 9.f + float(k) * 1.7f);
                brilho.push_back({q + Vec3f{0, h, 0}, 0.55f + 0.2f * std::sin(t * 13.f + float(k)), cor(c, 0.55f * a)});
            }
        }
    }

    // ---- ondas de pressão e colunas de levante ----
    for (const OndaVisual& o : ondas_) {
        float k = clamp01(o.idade / o.duracao), abre = 1.f - (1.f - k) * (1.f - k);
        if (!o.sobe) {
            for (int anel = 0; anel < 3; anel++)
                DrawCircle3D(v3(o.centro + Vec3f{0, 0.1f + 0.05f * float(anel), 0}), o.raio * abre * (1.f - 0.06f * float(anel)), {1, 0, 0}, 90.f,
                             cor(o.cor, (1.f - k) * (0.9f - 0.25f * float(anel))));
        } else {
            for (int n = 0; n < 6; n++)
                brilho.push_back({o.centro + Vec3f{0, 0.4f + 3.f * abre * float(n) / 5.f, 0}, o.raio * (0.9f - 0.1f * float(n)), cor(o.cor, 0.35f * (1.f - k))});
        }
    }

    // ---- projéteis e rastro ----
    for (const ProjetilEncantado& pr : circuitos_.projeteis()) {
        Rgb c = pr.encantado ? corDe(pr.efeito.propriedade) : CINZA_CHUMBO;
        for (int k = 1; k < pr.nRastro; k++) {
            DrawLine3D(v3(pr.rastro[size_t(k - 1)]), v3(pr.rastro[size_t(k)]), cor(c, 0.8f * (1.f - float(k) / ProjetilEncantado::RASTRO)));
            brilho.push_back({pr.rastro[size_t(k)], 0.16f * (1.f - float(k) / ProjetilEncantado::RASTRO), cor(c, 0.5f)});
        }
        brilho.push_back({pr.pos, pr.encantado ? 0.34f : 0.12f, cor(c, 0.95f)});
    }

    // ---- faíscas ----
    for (const Faisca& f : faiscas_) {
        float v = clamp01(f.vida / f.duracao);
        (f.aditiva ? brilho : alfa).push_back({f.pos, f.tam * (f.aditiva ? 1.f : 1.f + (1.f - v)), cor(f.cor, (f.aditiva ? 0.9f : 0.6f) * v)});
    }

    // ---- transparências: ordenadas de trás para a frente, sem escrever profundidade ----
    std::sort(alfa.begin(), alfa.end(), [&](const Sprite& a, const Sprite& b) { return dot(a.pos - olho, frente) > dot(b.pos - olho, frente); });
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const Caixa& c : vidros) {
        DrawCubeV(v3(c.c), v3(c.tam), c.cor);
        DrawCubeWiresV(v3(c.c), v3(c.tam), Color{c.cor.r, c.cor.g, c.cor.b, 150});
    }
    for (const Sprite& s : alfa) DrawBillboard(cam_, texturaFumaca_, v3(s.pos), s.tam, s.cor);
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Sprite& s : brilho) DrawBillboard(cam_, texturaFumaca_, v3(s.pos), s.tam, s.cor);
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

// ---------------------------------------------------------------- HUD e painel

void Jogo::desenharHudCircuitos() {
    using namespace estilo;
    const float x = 18.f, y = 44.f, w = 300.f;
    DrawRectangleRounded({x - 10, y - 8, w + 20, 198}, 0.07f, 6, Color{18, 16, 14, 165});
    texto(Fonte::Forte, "Circuitos de Queima", x, y, 20, OURO);
    const Circuito* naVez = arsenal_.circuitoNaVez();
    std::string vez = naVez ? nomeCombinacao(*naVez) : "Munição comum";
    texto(Fonte::Forte, "Ativa: " + vez, x, y + 26, 17, naVez ? MARFIM : MARFIM_ESC);
    // slots: 1 = comum, 2..4 = balas gravadas
    float ly = y + 48;
    texto(Fonte::Texto, std::string(arsenal_.selecionado() < 0 ? "> " : "   ") + "1  Comum", x, ly, 16, arsenal_.selecionado() < 0 ? OURO : MARFIM_ESC);
    for (int i = 0; i < arsenal_.slots(); i++) {
        const SlotMunicao& s = arsenal_.slot(i);
        bool sel = arsenal_.selecionado() == i;
        std::string linha = std::string(sel ? "> " : "   ") + std::to_string(i + 2) + "  " + (s.cheio ? nomeCombinacao(s.circuito) : "vazio");
        texto(Fonte::Texto, linha, x, ly + 18.f * float(i + 1), 16, sel ? OURO : (s.cheio ? MARFIM : MARFIM_ESC));
    }
    ly += 18.f * float(arsenal_.slots() + 1) + 4;
    char buf[128];
    Color corPavio = MARFIM;
    switch (pavio_.estado()) {
    case EstadoPavio::Seco: std::snprintf(buf, sizeof buf, "Pavio: Seco"); break;
    case EstadoPavio::Molhado:
        std::snprintf(buf, sizeof buf, "Pavio: Molhado (seca em %.1f s)", pavio_.umidade);
        corPavio = Color{140, 180, 225, 255};
        break;
    case EstadoPavio::BloqueadoFerro:
        std::snprintf(buf, sizeof buf, "Pavio: Bloqueado por Ferro Frio");
        corPavio = Color{170, 180, 200, 255};
        break;
    }
    texto(Fonte::Forte, buf, x, ly, 17, corPavio);
    std::snprintf(buf, sizeof buf, "Doadores: Pena %d · Pedra %d · Carvão %d   ·   Vestígios no chão: %d", arsenal_.estoque(Doador::Pena),
                  arsenal_.estoque(Doador::PedraPesada), arsenal_.estoque(Doador::CarvaoEmBrasa), int(circuitos_.capsulas().size()));
    texto(Fonte::Texto, buf, x, ly + 21, 14, MARFIM_ESC);
    texto(Fonte::Texto, "1 comum · 2-4 gravadas · Tab troca · E: fogueira / cápsula", x, ly + 38, 14, MARFIM_ESC);
}

void Jogo::desenharPainelFogueira() {
    using namespace estilo;
    const PainelFogueira& pn = painel_;
    const float W = float(GetScreenWidth()), H = float(GetScreenHeight());
    const float pw = 620, ph = 370, px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
    DrawRectangle(0, 0, int(W), int(H), Color{0, 0, 0, 110});
    DrawRectangleRounded({px, py, pw, ph}, 0.04f, 8, Color{24, 20, 16, 235});
    DrawRectangleRoundedLinesEx({px, py, pw, ph}, 0.04f, 8, 1.5f, comAlfa(OURO, 0.7f));
    texto(Fonte::Titulo, "Fogueira", px + pw * 0.5f, py + 14, 34, OURO, CENTRO);
    texto(Fonte::Texto, "Grave um circuito numa bala: entalhe + pasta + objeto doador", px + pw * 0.5f, py + 56, 17, MARFIM_ESC, CENTRO);

    const Circuito& c = pn.circuito;
    const SlotMunicao& s = arsenal_.slot(pn.slot);
    std::string valores[4] = {"Slot " + std::to_string(pn.slot + 2) + (s.cheio ? "  (" + nomeCombinacao(s.circuito) + ")" : "  (vazio)"),
                              nome(c.entalhe), nome(c.pasta),
                              std::string(nome(c.doador)) + "  (estoque " + std::to_string(arsenal_.estoque(c.doador)) + ")"};
    const char* rotulos[4] = {"Bala", "Entalhe", "Pasta", "Doador"};
    for (int i = 0; i < 4; i++) {
        float ly = py + 92 + 40.f * float(i);
        bool sel = pn.linha == i;
        if (sel) DrawRectangleRounded({px + 24, ly - 4, pw - 48, 32}, 0.3f, 6, Color{70, 56, 36, 200});
        texto(Fonte::Forte, rotulos[i], px + 44, ly, 20, sel ? OURO : MARFIM_ESC);
        texto(Fonte::Texto, std::string(sel ? "<  " : "") + valores[i] + (sel ? "  >" : ""), px + 170, ly, 20, MARFIM);
    }
    // o que a receita faz, ou por que não vale
    std::string desc = descreverEfeito(c, circuitos_.p);
    texto(Fonte::Texto, desc, px + pw * 0.5f, py + 258, 17, c.valido() ? MARFIM : Color{214, 80, 58, 255}, CENTRO);
    if (pn.retornoT > 0.f) texto(Fonte::Forte, pn.retorno, px + pw * 0.5f, py + 286, 18, comAlfa(OURO, std::min(1.f, pn.retornoT)), CENTRO);
    texto(Fonte::Texto, "Setas: escolher · Enter: gravar · X: esvaziar · R: repor doadores (teste) · E: fechar", px + pw * 0.5f, py + ph - 34,
          15, MARFIM_ESC, CENTRO);
}
