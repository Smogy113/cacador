// Painel de depuracao (F1): informacoes, tempo, graficos, jogador, recompensa e teleporte.
// Controles desenhados na hora (botao, caixa de marcar, barra deslizante) no estilo da interface.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "game/jogo.h"
#include "render/estilo.h"
#include "versao.h"

using namespace estilo;

namespace {
const Color FUNDO{14, 10, 7, 228}, CAIXA{40, 30, 21, 255}, CAIXA_HOVER{64, 48, 32, 255};
float px, py, colW;  // cursor de layout da coluna atual

bool sobre(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

void titulo(const char* t) {
    py += 8;
    texto(Fonte::Titulo, t, px, py, 24, OURO);
    py += 30;
    DrawRectangleGradientH(int(px), int(py), int(colW), 1, comAlfa(OURO, 0.8f), comAlfa(OURO, 0.f));
    py += 8;
}
void info(const char* rotulo, const char* valor) {
    texto(Fonte::Texto, rotulo, px, py, 19, MARFIM_ESC, ESQ, false);
    texto(Fonte::Forte, valor, px + colW, py, 19, MARFIM, DIR, false);
    py += 22;
}
bool botao(const char* t, float x, float y, float w) {
    Rectangle r{x, y, w, 28};
    bool h = sobre(r);
    DrawRectangleRounded(r, 0.2f, 4, h ? CAIXA_HOVER : CAIXA);
    DrawRectangleRoundedLinesEx(r, 0.2f, 4, 1.f, comAlfa(OURO, h ? 0.9f : 0.45f));
    texto(Fonte::Forte, t, x + w / 2, y + 3, 19, MARFIM, CENTRO, false);
    return h && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
bool botaoLinha(const char* t) {
    bool r = botao(t, px, py, colW);
    py += 34;
    return r;
}
bool caixa(const char* t, bool& v) {
    Rectangle r{px, py + 2, 18, 18};
    bool h = sobre({px, py, colW, 22});
    DrawRectangleRec(r, CAIXA);
    DrawRectangleLinesEx(r, 1.f, comAlfa(OURO, h ? 1.f : 0.6f));
    if (v) DrawRectangle(int(r.x + 4), int(r.y + 4), 10, 10, OURO);
    texto(Fonte::Texto, t, px + 28, py, 19, h ? MARFIM : MARFIM_ESC, ESQ, false);
    py += 25;
    if (h && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { v = !v; return true; }
    return false;
}
bool barra(const char* t, float& v, float mn, float mx, const char* fmt) {
    char buf[48];
    std::snprintf(buf, sizeof buf, fmt, v);
    texto(Fonte::Texto, t, px, py, 19, MARFIM_ESC, ESQ, false);
    texto(Fonte::Forte, buf, px + colW, py, 19, MARFIM, DIR, false);
    py += 22;
    Rectangle r{px, py + 4, colW, 8};
    DrawRectangleRounded(r, 1.f, 4, CAIXA);
    float f = std::clamp((v - mn) / (mx - mn), 0.f, 1.f);
    DrawRectangleRounded({r.x, r.y, r.width * f, r.height}, 1.f, 4, comAlfa(OURO, 0.8f));
    DrawCircleV({r.x + r.width * f, r.y + 4}, 7, MARFIM);
    py += 20;
    Rectangle area{r.x - 6, r.y - 8, r.width + 12, r.height + 16};
    if (sobre(area) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float nv = mn + std::clamp((GetMousePosition().x - r.x) / r.width, 0.f, 1.f) * (mx - mn);
        if (nv != v) { v = nv; return true; }
    }
    return false;
}
}  // namespace

void Jogo::desenharDepuracao() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    const float pw = 960, ph = std::min(float(H) - 40.f, 690.f), x0 = (W - pw) / 2, y0 = (H - ph) / 2;
    DrawRectangleRounded({x0, y0, pw, ph}, 0.02f, 6, FUNDO);
    DrawRectangleRoundedLinesEx({x0, y0, pw, ph}, 0.02f, 6, 1.f, comAlfa(OURO, 0.6f));
    texto(Fonte::Titulo, "Depuração", x0 + 24, y0 + 12, 34, MARFIM);
    texto(Fonte::Texto, std::string("F1 fecha   ·   versão ") + VERSAO_JOGO, x0 + pw - 24, y0 + 22, 19, MARFIM_ESC, DIR, false);
    char b[128];
    colW = 280;

    // ---------------- coluna 1: informacoes, tempo ----------------
    px = x0 + 24, py = y0 + 56;
    titulo("Informações");
    std::snprintf(b, sizeof b, "%d  (%.1f ms)", GetFPS(), dbg_.msQuadro);
    info("FPS", b);
    std::snprintf(b, sizeof b, "%zu mil", tris_ / 1000);
    info("Triângulos", b);
    std::snprintf(b, sizeof b, "%d", chamadas_);
    info("Chamadas de desenho", b);
    std::snprintf(b, sizeof b, "%.0f, %.1f, %.0f", jogador_.pos.x, jogador_.pos.y, jogador_.pos.z);
    info("Posição", b);
    std::snprintf(b, sizeof b, "%.2f h", relogio_.hora());
    info("Hora", b);
    std::snprintf(b, sizeof b, "%d ativos", populacao_.ativos());
    info("Moradores", b);
    const char* fases[] = {"nenhuma", "indo", "combate", "coletar", "levando", "entregue"};
    info("Recompensa", fases[int(cacada_.fase())]);
    if (!cacada_.bando().vazio()) {
        const char* est[] = {"vigia", "alerta", "combate", "foge", "rendido", "amarrado", "morto", "sumiu"};
        std::string s;
        for (auto& x : cacada_.bando().bandidos()) s += std::string(x.chefe ? "*" : "") + est[int(x.estado)] + " ";
        info("Bando", s.c_str());
    }
    titulo("Tempo");
    float hora = relogio_.hora();
    if (barra("Hora do dia", hora, 0.f, 23.99f, "%.1f h")) relogio_ = DayClock(hora);
    barra("Velocidade do tempo", dbg_.velTempo, 0.f, 60.f, "x%.1f");
    caixa("Congelar a hora", op_.congelarHora);
    caixa("Pausar o mundo", dbg_.pausado);

    // ---------------- coluna 2: graficos ----------------
    px = x0 + 24 + 310, py = y0 + 56;
    titulo("Gráficos");
    const char* niv[] = {"Baixo", "Médio", "Alto"};
    for (int k = 0; k < 3; k++) {
        bool atual = int(luz_.nivel()) == k;
        if (botao(atual ? (std::string("> ") + niv[k]).c_str() : niv[k], px + k * 95.f, py, 88)) trocarNivel(Nivel(k));
    }
    py += 36;
    auto& d = luz_.dbg;
    const Efeitos& ef = luz_.efeitosNivel();
    bool sombras = !d.semSombras, ssao = !d.semSSAO, bloom = !d.semBloom, raios = !d.semRaios, nuvens = !d.semNuvens, nebl = !d.semNeblina;
    if (caixa(ef.sombras ? "Sombras" : "Sombras (não há neste nível)", sombras)) d.semSombras = !sombras;
    if (caixa(ef.ssao ? "Oclusão de ambiente (SSAO)" : "SSAO (só no Alto)", ssao)) d.semSSAO = !ssao;
    if (caixa(ef.bloom ? "Brilho (bloom)" : "Bloom (não há neste nível)", bloom)) d.semBloom = !bloom;
    if (caixa(ef.raiosDeLuz ? "Raios de sol" : "Raios (não há neste nível)", raios)) d.semRaios = !raios;
    if (caixa("Nuvens", nuvens)) d.semNuvens = !nuvens;
    if (caixa("Neblina", nebl)) d.semNeblina = !nebl;
    float pn = d.passosNuvem < 0 ? (luz_.nivel() == Nivel::Alto ? 22.f : (luz_.nivel() == Nivel::Medio ? 12.f : 0.f)) : d.passosNuvem;
    if (barra("Passos das nuvens", pn, 0.f, 24.f, "%.0f")) d.passosNuvem = std::round(pn);
    barra("Exposição", d.exposicao, 0.4f, 2.f, "x%.2f");
    barra("Campo de visão", dbg_.fov, 40.f, 100.f, "%.0f°");
    float rg = grama_.raio(), dg = dbg_.densGrama < 0 ? (luz_.nivel() == Nivel::Alto ? 20.f : 12.f) : dbg_.densGrama;
    bool mudou = barra("Raio da grama", rg, 0.f, 60.f, "%.0f m");
    mudou |= barra("Folhas por m²", dg, 0.f, 40.f, "%.0f");
    if (mudou) { dbg_.densGrama = dg; grama_.configurar(std::round(rg), std::round(dg)); }

    // ---------------- coluna 3: jogador, recompensa, teleporte ----------------
    px = x0 + 24 + 620, py = y0 + 56;
    colW = 300;
    titulo("Jogador");
    caixa("Invencível", dbg_.invencivel);
    caixa("Munição infinita", dbg_.municaoInf);
    barra("Velocidade", dbg_.velJogador, 0.5f, 6.f, "x%.1f");
    if (botao("Curar", px, py, 94)) vida_.curar();
    if (botao("+100 pratas", px + 103, py, 94)) dinheiro_ += 100;
    if (botao("Cavalo aqui", px + 206, py, 94) && !cavalos_.empty() && montadoEm_ < 0) {
        Cavalo& c = cavalos_[cavaloPerto_ >= 0 ? cavaloPerto_ : 0];
        c.corpo.pos = col_.resolver(jogador_.pos + direitaJogador() * 2.f, RAIO_CAVALO);
        c.corpo.pos.y = terreno_->altura(c.corpo.pos.x, c.corpo.pos.z);
    }
    py += 36;
    titulo("Recompensa");
    if (botao("Aceitar 1º cartaz", px, py, 146)) aceitarCartaz(0, 0);
    if (botao("Ir ao tiroteio", px + 154, py, 146)) prepararTiroteio(true);
    py += 34;
    if (botao("Render chefe", px, py, 146) && !cacada_.bando().vazio()) {
        Bandido* c = cacada_.bando().chefe();
        if (c && c->estado != EstadoBandido::Morto && c->estado != EstadoBandido::Amarrado) c->estado = EstadoBandido::Rendido;
    }
    if (botao("Matar bando", px + 154, py, 146))
        for (size_t i = 0; i < cacada_.bando().bandidos().size(); i++) cacada_.bando().levarTiro(int(i), Parte::Cabeca);
    py += 34;
    if (botao("Cancelar", px, py, 146)) cacada_.cancelar();
    if (botao("Ir ao objetivo", px + 154, py, 146) && cacada_.ativa()) {
        Vec3f o = cacada_.objetivo(jogador_.pos, xerifes_);
        teleportar(o.x + 6.f, o.z + 6.f, 0, relogio_.hora());
    }
    py += 40;
    titulo("Ir para");
    int k = 0;
    for (auto& l : mapa_.lugares) {
        std::string nome = nomeLegivel(l.nome);
        if (nome.size() > 16) nome = nome.substr(0, 15) + ".";
        float bx = px + (k % 2) * 154.f;
        if (botao(nome.c_str(), bx, py, 146)) teleportar(l.x + 10.f, l.z + 10.f, 0, relogio_.hora());
        if (k % 2 == 1) py += 32;
        k++;
    }
}
