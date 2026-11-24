// Corrente da Cinza no jogo: arena de teste, entrada, efeitos nos alvos, desenho provisório e HUD.
// A regra (fumaça, canalização, faísca, escudo, cadeia, Custo de Sangue e umidade) fica em
// core/cinza; os números de ajuste ficam em ParametrosCinza (cinza_.p).
//
// Tudo aqui é provisório de propósito: primitivas da raylib e sprites da textura de fumaça, sem
// depender da malha nem das animações do caçador. Para trocar o visual, mexa só em desenharCinza().
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/noise.h"
#include "game/jogo.h"
#include "render/estilo.h"
#include "rlgl.h"

namespace {
constexpr float ALTURA_PEITO = 1.35f, ALTURA_PEITO_MONTADO = 2.2f;
constexpr float DIST_FOGUEIRA = 2.2f;        // alcance do E para descansar
constexpr float DIST_DESENHO_ARENA = 90.f;   // além disso a fogueira e a chuva não são desenhadas
// Arena relativa ao ponto de partida do jogo (entrada de Pedrabranca, olhando para a ponte).
constexpr float INICIO_X = -40.f, INICIO_Z = 10.f, INICIO_YAW_GRAUS = 160.f;

Vec3f direitaDe(float yaw) { return {-std::cos(yaw), 0, std::sin(yaw)}; }
float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 v3(Vec3f p) { return {p.x, p.y, p.z}; }
unsigned char paraByte(float v) { return static_cast<unsigned char>(std::clamp(v, 0.f, 1.f) * 255.f + 0.5f); }
Color cor(Rgb c, float alfa) { return {paraByte(c.r), paraByte(c.g), paraByte(c.b), paraByte(alfa)}; }
float h01(uint32_t s) { return float(hash32(s) & 0xFFFFu) / 65535.f; }  // 0..1 estável por semente

// Um sprite da textura de fumaça, ordenado por profundidade antes de desenhar.
struct Sprite {
    Vec3f pos;
    float tam;
    Color cor;
};
}  // namespace

Vec3f Jogo::peitoJogador() const {
    return jogador_.pos + Vec3f{0, montadoEm_ >= 0 ? ALTURA_PEITO_MONTADO : ALTURA_PEITO, 0};
}

// Procura em espiral um chão seco, plano, fora de casas e de caixas de colisão.
Vec3f Jogo::pontoLivreArena(Vec3f desejado, float folga) const {
    const Terreno& t = *terreno_;
    for (int k = 0; k < 96; k++) {
        float ang = float(k) * 0.5236f, r = float(k / 12) * 2.f;
        Vec3f q = col_.resolver(desejado + Vec3f{std::sin(ang) * r, 0, std::cos(ang) * r}, folga);
        q.y = t.altura(q.x, q.z);
        bool seco = t.mascaraRio(q.x, q.z) < 0.05f && q.y > t.nivelAgua() + 0.5f;
        if (seco && t.normal(q.x, q.z).y > 0.93f && t.dentroDoJogavel(q.x, q.z) && dentroDeInterior(cenario_.interiores(), q) < 0)
            return q;
    }
    desejado.y = t.altura(desejado.x, desejado.z);
    return desejado;
}

bool Jogo::naAgua(Vec3f p) const {
    // mesmo critério do som de passo na água (pisoSonoro)
    bool rio = p.y < terreno_->nivelAgua() - .06f && terreno_->mascaraRio(p.x, p.z) > .16f;
    return rio || (arena_.pronta && ZonaUmida{arena_.chuva, arena_.raioChuva, arena_.alturaChuva}.contem(p));
}

void Jogo::iniciarCinza() {
    const float yaw = INICIO_YAW_GRAUS * DEG2RAD;
    const Vec3f inicio{INICIO_X, 0, INICIO_Z}, f = frenteDe(yaw), d = direitaDe(yaw);
    arena_.fogueira = pontoLivreArena(inicio + f * 6.f + d * 2.5f, 1.2f);
    arena_.chuva = pontoLivreArena(inicio + f * 9.f - d * 7.f, 2.f);
    arena_.pronta = true;
    std::printf("cinza: fogueira=(%.1f, %.1f) chuva=(%.1f, %.1f) raio=%.1f\n", arena_.fogueira.x, arena_.fogueira.z, arena_.chuva.x,
                arena_.chuva.z, arena_.raioChuva);  // para --pos nas capturas
    cinza_.adicionarFonte(arena_.fogueira);
    cinza_.definirZonasUmidas({ZonaUmida{arena_.chuva, arena_.raioChuva, arena_.alturaChuva}});
    // A fumaça que desce até a lâmina do rio também se apaga.
    cinza_.definirTesteAgua([this](Vec3f p) {
        return terreno_->mascaraRio(p.x, p.z) > .16f && p.y < terreno_->nivelAgua() + .25f;
    });
}

bool Jogo::interagirFogueira(Entrada& e) {
    pertoFogueira_ = arena_.pronta && montadoEm_ < 0 && distXZ(jogador_.pos, arena_.fogueira) < DIST_FOGUEIRA;
    if (!pertoFogueira_ || !e.interagir) return false;
    float devolvido = cinza_.sangue().desgaste;
    cinza_.restaurarDesgaste(vida_);
    char buf[120];
    if (devolvido > 0.5f) std::snprintf(buf, sizeof buf, "Você descansa junto ao fogo: +%.0f de vida máxima.", devolvido);
    else std::snprintf(buf, sizeof buf, "Junto ao fogo. Seu corpo não carrega desgaste.");
    avisar(buf);
    e.interagir = false;
    return true;
}

// Entrada simulada para conferir o visual sem jogar (--screenshot ... --roteiro-cinza N).
//   1: atira duas vezes e segura a canalização (nuvens capturadas, tempo lento)
//   2: atira três vezes mirando, lança a fumaça ao ponto mirado e solta a faísca (detonação)
//   3: atira duas vezes e puxa a fumaça para o corpo antes de soltar (Escudo de Fuligem)
void Jogo::roteiroCinza(Entrada& e) {
    const int q = quadroRoteiro_++;
    const int modo = op_.roteiroCinza;
    const int tiros[] = {2, 26, 50};
    const int nTiros = modo == 2 ? 3 : 2;
    for (int i = 0; i < nTiros; i++) e.atirar |= q == tiros[i];
    if (modo == 1) e.conjurar = q >= 30;
    if (modo == 2) {
        e.mirar = true;
        e.conjurar = q >= 55 && q < 85;
    }
    if (modo == 3) e.conjurar = q >= 30 && q < 62;
}

void Jogo::atualizarCinza(float dtReal, float dtMundo, const Entrada& e) {
    if (montadoEm_ >= 0) pertoFogueira_ = false;  // interagirFogueira() so roda a pe
    EntradaCinza ec;
    ec.segurar = e.conjurar;
    ec.podeConjurar = montadoEm_ < 0 && morteT_ < 0 && quadroAberto_ < 0 && !op_.estudio;
    ec.pes = jogador_.pos;
    ec.peito = peitoJogador();
    ec.frente = frenteDe(rig_.yaw);
    ec.jogadorUmido = naAgua(jogador_.pos) || naAgua(ec.peito);
    if (mirando_ && e.conjurar) {  // o raio da mira so e lancado enquanto o botao da Cinza esta segurado
        // Mirando, a fumaça vai para onde a câmera aponta (no mínimo distMinimaMira à frente).
        Vec3f camPos{cam_.position.x, cam_.position.y, cam_.position.z};
        Vec3f camDir = normalizar(Vec3f{cam_.target.x, cam_.target.y, cam_.target.z} - camPos);
        Acerto a = lancarTiro(camPos, camDir, cinza_.p.alcanceMira, {}, *terreno_, col_);
        Vec3f alvo = a.ponto - camDir * 0.4f;  // um pouco antes da superfície, não dentro dela
        Vec3f rel = alvo - ec.peito;
        if (len(rel) < cinza_.p.distMinimaMira) alvo = ec.peito + normalizar(rel) * cinza_.p.distMinimaMira;
        ec.pontoMolde = alvo;
        ec.moldeMirado = true;
    } else {
        ec.pontoMolde = ec.peito + ec.frente * cinza_.p.distMoldeCorpo;
    }
    moldeAtual_ = ec.pontoMolde;

    EventosCinza ev = cinza_.atualizar(dtReal, dtMundo, ec, vida_);
    if (cinza_.estado() == EstadoConjuracao::Canalizando) jogador_.yaw = rig_.yaw;  // o corpo encara a magia

    // avisos curtos
    const bool apertou = e.conjurar && !conjurarAntes_;
    conjurarAntes_ = e.conjurar;
    if (apertou && cinza_.estado() == EstadoConjuracao::Bloqueada) avisar("Molhado: a Cinza não acende.");
    if (apertou && cinza_.estado() == EstadoConjuracao::SemSangue) avisar("Sangue fraco demais para conjurar.");
    if (ev.interrompida) avisar(ec.jogadorUmido ? "A água apagou a Cinza." : "O sangue não sustenta mais a Cinza.");
    if (ev.faiscaVazia) avisar("A faísca não achou fumaça.");
    if (ev.escudoErguido) avisar("Escudo de Fuligem.");

    // detonações: dano e empurrão no bando; som de estouro pelo canal de tiro
    for (const GolpeDetonacao& g : ev.detonacoes) {
        int mortos = 0;
        int atingidos = cacada_.bando().vazio() ? 0 : cacada_.bando().explosao(g.centro, g.raio, g.dano, g.empurrao, &mortos);
        if (atingidos > 0) {
            marcaAcerto_ = 0.25f;
            marcaMatou_ = mortos > 0;
            std::printf("cinza: detonacao atingiu %d (mortos %d)%s\n", atingidos, mortos, g.primeira ? "" : " em cadeia");
        }
        if (cinza_.p.fracaoDanoJogador > 0.f && !dbg_.invencivel) {
            float dano = danoNaDistancia(g, len(ec.peito - g.centro)) * cinza_.p.fracaoDanoJogador;
            if (dano > 0.f) vida_.dano(dano);
        }
        if (sons_.ok()) {
            float dist = len(g.centro - ec.peito);
            float ganho = (g.primeira ? 1.f : 0.8f) / (1.f + dist * .05f);
            sons_.tiro(AcusticaTiro::Externa, ganho, panSonoro(g.centro, .38f));
        }
    }
}

void Jogo::desenharCinza() {
    if (!arena_.pronta) return;
    const float t = tempo_;
    const Vec3f fg = arena_.fogueira;
    const float noite = relogio_.intensidadeTochas();

    // ---- opaco: pedras, lenha e brasas da fogueira; chão molhado e chuva ----
    // A arena só é montada perto da câmera; longe dela o custo é zero.
    const Vec3f olhoArena{cam_.position.x, cam_.position.y, cam_.position.z};
    const bool arenaPerto = distXZ(olhoArena, fg) < DIST_DESENHO_ARENA || distXZ(olhoArena, arena_.chuva) < DIST_DESENHO_ARENA;
    if (arenaPerto) {
        for (int k = 0; k < 8; k++) {
            float a = float(k) * 0.7854f;
            float tom = 0.36f + 0.1f * h01(uint32_t(k) + 11u);
            DrawSphereEx(v3(fg + Vec3f{std::cos(a) * 0.55f, 0.07f, std::sin(a) * 0.55f}), 0.13f, 4, 6, cor(Rgb{tom, tom * 0.97f, tom * 0.92f}, 1.f));
        }
        for (int k = 0; k < 3; k++) {
            float a = float(k) * 2.094f + 0.3f;
            Vec3f base = fg + Vec3f{std::cos(a) * 0.42f, 0.05f, std::sin(a) * 0.42f};
            Vec3f topo = fg + Vec3f{-std::cos(a) * 0.08f, 0.34f, -std::sin(a) * 0.08f};
            DrawCylinderEx(v3(base), v3(topo), 0.06f, 0.045f, 6, Color{58, 40, 26, 255});
        }
        DrawSphereEx(v3(fg + Vec3f{0, 0.04f, 0}), 0.2f, 3, 6, Color{150, 46, 18, 255});

        const Vec3f ch = arena_.chuva;
        const float r = arena_.raioChuva;
        DrawCylinder(v3(ch + Vec3f{0, 0.03f, 0}), r, r, 0.02f, 32, Color{70, 95, 120, 70});
        DrawCircle3D(v3(ch + Vec3f{0, 0.06f, 0}), r, {1, 0, 0}, 90.f, Color{150, 185, 220, 160});
        for (int i = 0; i < 160; i++) {  // gotas: posição fixa por índice, queda em ciclo
            float raio = r * std::sqrt(h01(uint32_t(i) * 3u + 1u)), ang = h01(uint32_t(i) * 3u + 2u) * 6.2832f;
            float queda = std::fmod(t * 9.f + h01(uint32_t(i) * 3u + 3u) * arena_.alturaChuva, arena_.alturaChuva);
            Vec3f p = ch + Vec3f{std::cos(ang) * raio, arena_.alturaChuva - queda, std::sin(ang) * raio};
            DrawLine3D(v3(p), v3(p + Vec3f{0.04f, -0.5f, 0.02f}), Color{170, 200, 230, 150});
        }
    }  // arenaPerto

    // ---- transparente: fumaça (alfa) e fogo, brasa e explosões (aditivo) ----
    std::vector<Sprite> fumaca, brilho;
    fumaca.reserve(cinza_.nuvens().size() * 3 + 32);
    for (const NuvemFumaca& n : cinza_.nuvens()) {
        float a = n.alfaVida(), c = n.controle;
        float pulso = 1.f + 0.08f * c * std::sin(t * 9.f + float(n.semente % 97u));
        // Capturada: a fumaça escurece, adensa e pulsa (sinal de que está sob controle).
        Rgb tom = lerp(Rgb{0.63f, 0.62f, 0.59f}, Rgb{0.19f, 0.15f, 0.13f}, c);
        float alfa = (0.30f + 0.32f * c) * a;
        for (int k = 0; k < 3; k++) {
            uint32_t s = n.semente * 7u + uint32_t(k);
            Vec3f desvio{h01(s) - 0.5f, (h01(s + 101u) - 0.5f) * 0.6f, h01(s + 202u) - 0.5f};
            fumaca.push_back({n.pos + desvio * (0.9f * n.raio), n.raio * (1.6f + 0.5f * h01(s + 303u)) * pulso, cor(tom, alfa)});
        }
        if (c > 0.01f) brilho.push_back({n.pos, n.raio * 1.1f, cor(Rgb{1.f, 0.42f, 0.16f}, 0.30f * c * a)});
        if (n.pavio >= 0.f) brilho.push_back({n.pos, n.raio * 1.6f, cor(Rgb{1.f, 0.6f, 0.25f}, 0.6f)});  // vai pegar fogo
    }
    if (cinza_.estado() == EstadoConjuracao::Canalizando && cinza_.capturadas() > 0)
        brilho.push_back({moldeAtual_, 0.35f + 0.06f * std::sin(t * 14.f), Color{255, 150, 60, 200}});  // destino do puxão
    const EscudoFuligem& es = cinza_.escudo();
    if (es.ativo()) {
        float fade = std::min(1.f, es.restante / 0.5f) * std::min(1.f, (es.duracao - es.restante) / 0.2f + 0.2f);
        Vec3f lado = direitaDe(es.yaw);
        const float larg = cinza_.p.larguraEscudo, alt = cinza_.p.alturaEscudo;
        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 4; j++) {
                uint32_t s = uint32_t(i * 4 + j);
                float u = (float(i) + 0.5f) / 5.f - 0.5f, v = (float(j) + 0.5f) / 4.f - 0.5f;
                Vec3f onda{0.05f * std::sin(t * 3.f + float(s)), 0.05f * std::cos(t * 2.3f + float(s)), 0};
                Vec3f p = es.centro + lado * (u * larg) + Vec3f{0, v * alt, 0} + onda;
                fumaca.push_back({p, 0.95f + 0.15f * h01(s), cor(Rgb{0.15f, 0.13f, 0.12f}, (0.45f + 0.3f * es.absorcao) * fade)});
            }
        for (int k = 0; k < 8; k++) {  // brasas presas na fuligem
            uint32_t s = uint32_t(k) + 40u;
            float u = h01(s) - 0.5f, v = h01(s + 9u) - 0.5f;
            float pisca = 0.5f + 0.5f * std::sin(t * (7.f + float(k)) + float(k));
            brilho.push_back({es.centro + lado * (u * larg) + Vec3f{0, v * alt, 0}, 0.12f, cor(Rgb{1.f, 0.45f, 0.15f}, 0.8f * pisca * fade)});
        }
    }
    for (const ExplosaoCinza& x : cinza_.explosoes()) {
        float k = clamp01(x.idade / std::max(1e-3f, x.duracao));
        float abre = 1.f - (1.f - k) * (1.f - k);  // abre rápido e desacelera
        float tam = x.raio * 2.f * (0.35f + 0.65f * abre);
        brilho.push_back({x.pos, tam * 0.6f, cor(Rgb{1.f, 0.8f, 0.5f}, 0.7f * (1.f - k) * (1.f - k))});
        brilho.push_back({x.pos, tam, cor(Rgb{1.f, 0.36f, 0.1f}, 0.55f * (1.f - k))});
        fumaca.push_back({x.pos + Vec3f{0, 0.4f * k, 0}, tam * 1.2f, cor(Rgb{0.12f, 0.1f, 0.09f}, 1.4f * k * (1.f - k))});
    }
    for (int i = 0; i < 5 && arenaPerto; i++) {  // chamas da fogueira
        float fi = float(i);
        float y = 0.22f + 0.13f * fi + 0.04f * std::sin(t * 11.f + fi * 1.7f);
        Vec3f p = fg + Vec3f{0.05f * std::sin(t * 6.f + fi), y, 0.05f * std::cos(t * 5.f + fi * 2.f)};
        brilho.push_back({p, 0.5f - 0.07f * fi + 0.06f * std::sin(t * 13.f + fi), cor(Rgb{1.f, 0.55f + 0.08f * fi, 0.22f}, 0.65f + 0.2f * noite)});
    }

    Vec3f olho{cam_.position.x, cam_.position.y, cam_.position.z};
    Vec3f frente = normalizar(Vec3f{cam_.target.x, cam_.target.y, cam_.target.z} - olho);
    std::sort(fumaca.begin(), fumaca.end(), [&](const Sprite& a, const Sprite& b) { return dot(a.pos - olho, frente) > dot(b.pos - olho, frente); });
    rlDrawRenderBatchActive();
    rlDisableDepthMask();  // transparências testam a profundidade, mas não escrevem nela
    for (const Sprite& s : fumaca) DrawBillboard(cam_, texturaFumaca_, v3(s.pos), s.tam, s.cor);
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Sprite& s : brilho) DrawBillboard(cam_, texturaFumaca_, v3(s.pos), s.tam, s.cor);
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

void Jogo::desenharHudCinza() {
    using namespace estilo;
    const CustoSangue& s = cinza_.sangue();
    const EstadoConjuracao est = cinza_.estado();
    const float x = 18.f, y = 44.f, w = 250.f;

    // véu quente nas bordas enquanto o tempo está dilatado
    float lento = clamp01((1.f - cinza_.fatorTempo()) / std::max(1e-3f, 1.f - cinza_.p.fatorTempo));
    if (lento > 0.f) {
        int W = GetScreenWidth(), H = GetScreenHeight();
        Color borda{60, 26, 10, paraByte(0.35f * lento)}, meio{60, 26, 10, 0};
        DrawRectangleGradientV(0, 0, W, H / 4, borda, meio);
        DrawRectangleGradientV(0, H - H / 4, W, H / 4, meio, borda);
    }

    DrawRectangleRounded({x - 10, y - 8, w + 20, 152}, 0.08f, 6, Color{18, 16, 14, 165});
    texto(Fonte::Forte, "Corrente da Cinza", x, y, 20, OURO);

    // barra: vida atual | vazio até a vida máxima | desgaste até a base (hachurado)
    const float yb = y + 28, hb = 10;
    const float fracMax = vida_.max / s.vidaBase, fracAtual = vida_.atual / s.vidaBase;
    DrawRectangle(int(x), int(yb), int(w), int(hb), Color{34, 30, 26, 235});
    float xMax = x + w * fracMax;
    if (fracMax < 1.f) {
        DrawRectangle(int(xMax), int(yb), int(x + w - xMax + 0.5f), int(hb), Color{70, 24, 20, 220});
        for (float hx = xMax; hx < x + w - 2; hx += 6) DrawLineEx({hx, yb + hb}, {std::min(hx + 6, x + w), yb}, 1.f, Color{150, 58, 44, 230});
    }
    DrawRectangle(int(x), int(yb), int(w * fracAtual), int(hb), vida_.fracao() < 0.35f ? SANGUE : MARFIM);
    DrawRectangle(int(xMax) - 1, int(yb) - 3, 2, int(hb) + 6, OURO);

    char buf[160];
    std::snprintf(buf, sizeof buf, "Vida %.0f  ·  Máxima %.0f de %.0f", vida_.atual, vida_.max, s.vidaBase);
    texto(Fonte::Texto, buf, x, yb + 14, 18, MARFIM);
    if (s.desgaste > 0.05f) std::snprintf(buf, sizeof buf, "Desgaste: -%.1f (descanse na fogueira)", s.desgaste);
    else std::snprintf(buf, sizeof buf, "Desgaste: nenhum");
    texto(Fonte::Texto, buf, x, yb + 34, 16, MARFIM_ESC);

    Color corEstado = MARFIM;
    if (est == EstadoConjuracao::Canalizando) corEstado = OURO;
    else if (est == EstadoConjuracao::Bloqueada) corEstado = Color{140, 180, 225, 255};
    else if (est == EstadoConjuracao::SemSangue) corEstado = Color{214, 80, 58, 255};
    else if (est == EstadoConjuracao::Indisponivel) corEstado = MARFIM_ESC;
    if (est == EstadoConjuracao::Canalizando)
        std::snprintf(buf, sizeof buf, "Canalizando  ·  tempo %.1fx  ·  %d presa%s", cinza_.fatorTempo(), cinza_.capturadas(),
                      cinza_.capturadas() == 1 ? "" : "s");
    else std::snprintf(buf, sizeof buf, "Conjuração: %s", nomeEstado(est));
    texto(Fonte::Forte, buf, x, yb + 54, 18, corEstado);

    const EscudoFuligem& es = cinza_.escudo();
    if (es.ativo()) std::snprintf(buf, sizeof buf, "Escudo de Fuligem %.1f s  ·  barra %.0f%%", es.restante, es.absorcao * 100.f);
    else std::snprintf(buf, sizeof buf, "Fumaça no ar: %d", int(cinza_.nuvens().size()));
    texto(Fonte::Texto, buf, x, yb + 76, 16, es.ativo() ? OURO : MARFIM_ESC);
    texto(Fonte::Texto, "F: canalizar  ·  com mira: lançar  ·  solte: faísca", x, yb + 96, 15, MARFIM_ESC);
}
