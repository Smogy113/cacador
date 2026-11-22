#include "game/ui.h"
#include <algorithm>
#include <cstdio>
#include "core/bussola.h"
#include "core/noise.h"
#include "raylib.h"
#include "render/estilo.h"

std::string semAcentos(const std::string& s) {
    static const struct { const char* de; const char* para; } T[] = {
        {"á", "a"}, {"à", "a"}, {"ã", "a"}, {"â", "a"}, {"é", "e"}, {"ê", "e"}, {"í", "i"}, {"ó", "o"}, {"õ", "o"},
        {"ô", "o"}, {"ú", "u"}, {"ç", "c"}, {"Á", "A"}, {"À", "A"}, {"Ã", "A"}, {"Â", "A"}, {"É", "E"}, {"Ê", "E"},
        {"Í", "I"}, {"Ó", "O"}, {"Õ", "O"}, {"Ô", "O"}, {"Ú", "U"}, {"Ç", "C"}};
    std::string r = s;
    for (auto& t : T) {
        size_t p;
        std::string de = t.de;
        while ((p = r.find(de)) != std::string::npos) r.replace(p, de.size(), t.para);
    }
    return r;
}

using namespace estilo;

static void textoCentro(const std::string& t, int cx, int y, int tam, Color c) { texto(Fonte::Texto, t, float(cx), float(y), float(tam), c, CENTRO, false); }

static void retrato(uint32_t semente, int x, int y, int w, int h) {
    uint32_t r = semente;
    const Color peles[] = {{222, 172, 135, 255}, {190, 140, 105, 255}, {150, 105, 75, 255}, {110, 78, 55, 255}};
    const Color cabelos[] = {{40, 30, 25, 255}, {90, 60, 35, 255}, {150, 110, 60, 255}, {120, 115, 110, 255}, {140, 50, 30, 255}};
    Color pele = peles[int(aleatorio01(r) * 4) % 4], cab = cabelos[int(aleatorio01(r) * 5) % 5];
    Color tinta{60, 45, 30, 255};
    DrawRectangle(x, y, w, h, {205, 190, 155, 255});
    int cx = x + w / 2, cy = y + h / 2 + 6;
    int hw = int(w * (0.26f + aleatorio01(r) * 0.06f)), hh = int(h * 0.34f);
    DrawRectangle(cx - hw - 6, cy + hh - 2, hw * 2 + 12, h - (cy + hh - y), {110, 80, 55, 255});  // ombros
    DrawRectangle(cx - hw, cy - hh, hw * 2, hh * 2, pele);                                      // rosto
    DrawRectangle(cx - hw, cy - hh, hw * 2, hh / 3, cab);                                       // cabelo
    if (aleatorio01(r) < 0.55f) {                                                               // chapeu
        DrawRectangle(cx - hw - 10, cy - hh - 2, hw * 2 + 20, 7, tinta);
        DrawRectangle(cx - hw + 4, cy - hh - 20, hw * 2 - 8, 20, tinta);
    }
    int olhoY = cy - hh / 4;
    bool tapa = aleatorio01(r) < 0.18f;
    DrawRectangle(cx - hw / 2 - 3, olhoY, 6, 5, {30, 25, 20, 255});
    if (tapa) {
        DrawRectangle(cx + hw / 2 - 7, olhoY - 3, 14, 11, {20, 18, 15, 255});
        DrawLine(cx - hw, olhoY - 8, cx + hw, olhoY - 2, {20, 18, 15, 255});
    } else
        DrawRectangle(cx + hw / 2 - 3, olhoY, 6, 5, {30, 25, 20, 255});
    DrawRectangle(cx - hw / 2 - 5, olhoY - 7, 12, 3, cab);  // sobrancelhas
    DrawRectangle(cx + hw / 2 - 7, olhoY - 7, 12, 3, cab);
    DrawRectangle(cx - 3, olhoY + 4, 6, hh / 3, {pele.r * 0.85f > 0 ? (unsigned char)(pele.r * 0.85f) : (unsigned char)0, (unsigned char)(pele.g * 0.85f), (unsigned char)(pele.b * 0.85f), 255});
    if (aleatorio01(r) < 0.6f)  // barba
        DrawRectangle(cx - hw, cy + hh / 4, hw * 2, hh * 3 / 4, cab);
    DrawRectangle(cx - hw / 3, cy + hh / 2 - 2, hw * 2 / 3, 3, {90, 45, 40, 255});  // boca
    if (aleatorio01(r) < 0.3f) DrawLine(cx - hw + 4, cy - 4, cx - 4, cy + hh / 3, {150, 50, 45, 255});  // cicatriz
}

void desenharQuadro(const std::vector<Cartaz>& cs, int sel, const std::string& lugar, int aceito) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangleGradientV(0, 0, W, H, Color{10, 7, 5, 200}, Color{10, 7, 5, 235});
    float cx = W * 0.5f;
    texto(Fonte::Titulo, "Procurados", cx, H * 0.06f, 46, MARFIM, CENTRO);
    texto(Fonte::Texto, "Xerife de " + lugar, cx, H * 0.06f + 50, 24, MARFIM_ESC, CENTRO);
    linha(cx, H * 0.06f + 84, 520, OURO);
    if (cs.empty()) return;
    int n = int(cs.size());
    float cw = std::min(330.f, W * 0.26f), ch = std::min(H * 0.66f, 520.f), y0 = H * 0.06f + 104;
    for (int k = -1; k <= 1; k++) {
        int idx = ((sel + k) % n + n) % n;
        const Cartaz& c = cs[idx];
        float escala = k == 0 ? 1.f : 0.86f;
        float w = cw * escala, h = ch * escala;
        float x = cx + k * (cw + 26) - w / 2, y = y0 + (k == 0 ? 0 : ch * 0.07f);
        // papel envelhecido com borda escura e sombra
        DrawRectangle(int(x + 8), int(y + 10), int(w), int(h), Color{0, 0, 0, 110});
        Color papel = k == 0 ? Color{228, 210, 168, 255} : Color{176, 160, 124, 255};
        DrawRectangle(int(x), int(y), int(w), int(h), papel);
        DrawRectangleGradientV(int(x), int(y), int(w), int(h * 0.25f), Color{120, 90, 50, 60}, Color{120, 90, 50, 0});
        DrawRectangleGradientV(int(x), int(y + h * 0.75f), int(w), int(h * 0.25f), Color{120, 90, 50, 0}, Color{120, 90, 50, 70});
        DrawRectangleLinesEx({x + 6, y + 6, w - 12, h - 12}, 1.5f, comAlfa(TINTA, 0.6f));
        DrawCircle(int(x + w / 2), int(y + 10), 4.5f * escala, Color{70, 70, 74, 255});  // prego
        float mx = x + w / 2, e = escala;
        texto(Fonte::Titulo, "PROCURADO", mx, y + 20 * e, 34 * e, SANGUE, CENTRO, false);
        texto(Fonte::Forte, "vivo ou morto", mx, y + 58 * e, 17 * e, comAlfa(TINTA, 0.8f), CENTRO, false);
        float rw = std::min(w - 70, 170.f * e), rh = rw * 0.95f;
        retrato(c.rosto, int(mx - rw / 2), int(y + 84 * e), int(rw), int(rh));
        DrawRectangleLinesEx({mx - rw / 2, y + 84 * e, rw, rh}, 2, TINTA);
        float ty = y + 92 * e + rh;
        texto(Fonte::Forte, c.nome, mx, ty, 25 * e, TINTA, CENTRO, false);
        texto(Fonte::Texto, "\"" + c.apelido + "\"", mx, ty + 28 * e, 20 * e, comAlfa(TINTA, 0.85f), CENTRO, false);
        texto(Fonte::Texto, c.crime, mx, ty + 54 * e, 19 * e, Color{120, 34, 24, 255}, CENTRO, false);
        char buf[64];
        std::snprintf(buf, sizeof buf, "%d pratas vivo", c.recompensaVivo);
        texto(Fonte::Titulo, buf, mx, ty + 84 * e, 22 * e, Color{40, 70, 36, 255}, CENTRO, false);
        std::snprintf(buf, sizeof buf, "%d pratas morto", c.recompensaMorto);
        texto(Fonte::Texto, buf, mx, ty + 112 * e, 18 * e, Color{110, 36, 26, 255}, CENTRO, false);
        texto(Fonte::Texto, "Visto por último: " + c.ultimoVisto, mx, ty + 138 * e, 17 * e, comAlfa(TINTA, 0.85f), CENTRO, false);
        texto(Fonte::Texto, c.pista, mx, ty + 160 * e, 16 * e, comAlfa(SANGUE, 0.9f), CENTRO, false);
        if (idx == aceito) {  // carimbo torto
            Rectangle r{mx - 80, y + h * 0.38f, 160, 46};
            DrawRectangleLinesEx(r, 3, comAlfa(SANGUE, 0.85f));
            texto(Fonte::Titulo, "ACEITO", mx, r.y + 7, 30, comAlfa(SANGUE, 0.9f), CENTRO, false);
        }
    }
    // rodape: teclas
    float fy = y0 + ch + 24, x = cx - 300;
    char cont[32];
    std::snprintf(cont, sizeof cont, "%d de %d", sel + 1, n);
    x += tecla("<  >", x, fy, 24) + 10;
    texto(Fonte::Texto, "trocar", x, fy - 1, 24, MARFIM);
    x += largura(Fonte::Texto, "trocar", 24) + 34;
    x += tecla("Enter", x, fy, 24) + 10;
    texto(Fonte::Texto, "aceitar", x, fy - 1, 24, MARFIM);
    x += largura(Fonte::Texto, "aceitar", 24) + 34;
    x += tecla("E", x, fy, 24) + 10;
    texto(Fonte::Texto, "fechar", x, fy - 1, 24, MARFIM);
    texto(Fonte::Texto, cont, cx + 300, fy - 1, 22, MARFIM_ESC, DIR);
}

void desenharAvisos(const std::vector<ErroMapa>& erros, float t) {
    if (erros.empty() || t > 10.f) return;
    float y = 40;
    faixa(400, y - 8, 900, 34 + 24 * float(std::min<size_t>(erros.size(), 6)), 1.f);
    texto(Fonte::Forte, "Problemas no arquivo do mapa (o jogo continua sem essas linhas):", 30, y, 20, Color{255, 210, 180, 255});
    for (size_t k = 0; k < erros.size() && k < 6; k++) {
        char buf[300];
        std::snprintf(buf, sizeof buf, "linha %d: %s", erros[k].linha, erros[k].msg.c_str());
        texto(Fonte::Texto, buf, 40, y + 26 + float(k) * 24, 19, MARFIM);
    }
}

void desenharDica(const char* t, const std::string& acao) {
    float W = float(GetScreenWidth()), H = float(GetScreenHeight()), tam = 24;
    float wt = (t && *t) ? std::max(tam * 1.1f, largura(Fonte::Forte, t, tam * 0.8f) + tam * 0.6f) + 12 : 0;
    float w = wt + largura(Fonte::Texto, acao, tam), x = W / 2 - w / 2, y = H - 92;
    faixa(W / 2, y - 4, w + 46, 35, 0.68f);
    if (wt > 0) tecla(t, x, y, tam);
    texto(Fonte::Texto, acao, x + wt, y - 1, tam, MARFIM);
}

// ---------------- HUD do tiroteio ----------------
void desenharHud(float vida01, int balas, bool recarregando, int dinheiro, bool mirando, float marca, bool matou, Vector2 mini, float raio) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    // A vida fica junto da navegacao; a municao tem seu proprio canto.
    float x = mini.x - raio, y = mini.y + raio + 8;
    DrawRectangle(int(x - 1), int(y - 1), int(2 * raio + 2), 5, Color{24, 21, 18, 220});
    DrawRectangle(int(x), int(y), int(2 * raio * std::clamp(vida01, 0.f, 1.f)), 3,
                  vida01 < 0.35f ? SANGUE : MARFIM);
    float direita = W - 30.f, base = H - 30.f;
    DrawRectangleGradientH(int(direita - 164), int(base - 59), 164, 58,
                           Color{20, 18, 15, 0}, Color{20, 18, 15, 158});
    char buf[48];
    std::snprintf(buf, sizeof buf, "%d pratas", dinheiro);
    texto(Fonte::Texto, buf, direita - 9, base - 62, 20, MARFIM, DIR);
    std::snprintf(buf, sizeof buf, "%d", balas);
    texto(Fonte::Forte, buf, direita - 44, base - 41, 36, MARFIM, DIR);
    texto(Fonte::Texto, "/ 6", direita - 9, base - 32, 22, MARFIM, DIR);
    if (recarregando) texto(Fonte::Forte, "Recarregando", direita, base - 88, 21, MARFIM, DIR);
    float cx = W * 0.5f, cy = H * 0.5f;
    if (mirando) {
        DrawCircleV({cx, cy}, 2.6f, Color{25, 22, 18, 190});
        DrawCircleV({cx, cy}, 1.5f, MARFIM);
    }
    if (marca > 0) {
        Color c = matou ? SANGUE : MARFIM;
        for (int s = -1; s <= 1; s += 2)
            for (int t = -1; t <= 1; t += 2) DrawLineEx({cx + 6.f * s, cy + 6.f * t}, {cx + 13.f * s, cy + 13.f * t}, 2.2f, c);
    }
}

void desenharBussola(float yaw, bool temObj, float pos, float dist) {
    float W = float(GetScreenWidth()), largura_ = std::min(340.f, W * 0.44f), cx = W / 2, y = 34;
    const float campo = 1.6f;
    DrawLineEx({cx, y + 8}, {cx, y + 13}, 2.f, MARFIM);
    linha(cx, y + 8, largura_, comAlfa(MARFIM, 0.7f));
    for (int k = -16; k <= 16; k++) {  // riscos a cada 11,25 graus, mais fracos nas pontas
        float f = 0.5f + anguloRelativo(yaw, float(k) * 0.19635f) / campo;
        if (f < 0.02f || f > 0.98f || k % 8 == 0) continue;
        float a = 1.f - std::abs(f - 0.5f) * 2.f;
        float x = cx - largura_ / 2 + f * largura_;
        DrawLineEx({x, y + 3}, {x, y + (k % 2 ? 6.f : 8.f)}, 1.f, comAlfa(MARFIM, 0.6f * a));
    }
    const struct { const char* t; float rumo; } P[] = {{"N", 3.14159265f}, {"L", 1.5707963f}, {"S", 0.f}, {"O", -1.5707963f}};
    for (auto& p : P) {
        float f = 0.5f + anguloRelativo(yaw, p.rumo) / campo;
        if (f < 0.03f || f > 0.97f) continue;
        float a = 1.f - std::abs(f - 0.5f) * 1.6f;
        Color c = p.t[0] == 'N' ? Color{214, 80, 58, 255} : MARFIM;
        texto(Fonte::Forte, p.t, cx - largura_ / 2 + f * largura_, y - 18, 20, comAlfa(c, a), CENTRO);
    }
    if (!temObj) return;
    float f = std::clamp(pos, 0.f, 1.f), x = cx - largura_ / 2 + f * largura_;
    if (pos < 0 || pos > 1) {
        float s = pos < 0 ? -1.f : 1.f;
        DrawTriangle({x + 6 * s, y + 8}, {x - 4 * s, y + 1}, {x - 4 * s, y + 15}, OURO);
        DrawTriangle({x + 6 * s, y + 8}, {x - 4 * s, y + 15}, {x - 4 * s, y + 1}, OURO);
    } else {
        DrawPoly({x, y + 8}, 4, 6, 45, OURO);
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d m", int(dist));
    texto(Fonte::Texto, buf, x, y + 16, 20, comAlfa(OURO, 0.95f), CENTRO);
}

void desenharMensagem(const std::string& t, float tempo) {
    if (tempo <= 0 || t.empty()) return;
    float W = float(GetScreenWidth()), H = float(GetScreenHeight()), a = std::clamp(tempo / 0.5f, 0.f, 1.f);
    float w = largura(Fonte::Texto, t, 28), y = H * 0.2f;
    faixa(W / 2, y - 12, w + 260, 56, a);
    linha(W / 2, y - 12, w + 120, comAlfa(OURO, 0.8f * a));
    linha(W / 2, y + 43, w + 120, comAlfa(OURO, 0.8f * a));
    texto(Fonte::Texto, t, W / 2, y, 28, comAlfa(MARFIM, a), CENTRO);
}

void desenharBordaDano(float vida01) {
    if (vida01 >= 0.35f) return;
    int W = GetScreenWidth(), H = GetScreenHeight();
    unsigned char a = (unsigned char)(170 * (1.f - vida01 / 0.35f));
    Color c{120, 8, 4, a}, z{120, 8, 4, 0};
    int e = H / 4;
    DrawRectangleGradientV(0, 0, W, e, c, z);
    DrawRectangleGradientV(0, H - e, W, e, z, c);
    DrawRectangleGradientH(0, 0, e, H, c, z);
    DrawRectangleGradientH(W - e, 0, e, H, z, c);
}

void desenharTelaMorte(float t) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, {0, 0, 0, (unsigned char)(255 * std::clamp(t / 1.5f, 0.f, 1.f))});
    if (t > 0.8f) {
        float a = std::clamp((t - 0.8f) / 0.6f, 0.f, 1.f);
        texto(Fonte::Titulo, "Você apagou", W * 0.5f, H * 0.5f - 40, 54, comAlfa(SANGUE, a), CENTRO);
        texto(Fonte::Texto, "e acorda na cidade mais próxima...", W * 0.5f, H * 0.5f + 24, 26, comAlfa(MARFIM_ESC, a), CENTRO);
    }
}

void desenharPausa(int nivel, const char* versao) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangleGradientV(0, 0, W, H, Color{8, 6, 4, 150}, Color{8, 6, 4, 230});
    float cx = W * 0.5f, cy = H * 0.5f;
    texto(Fonte::Titulo, "Pausado", cx, cy - 120, 64, MARFIM, CENTRO);
    linha(cx, cy - 40, 460, OURO);
    texto(Fonte::Texto, "Qualidade gráfica", cx, cy - 18, 26, MARFIM_ESC, CENTRO);
    const char* nomes[] = {"Baixo", "Médio", "Alto"};
    for (int k = 0; k < 3; k++) {
        float x = cx + (k - 1) * 150.f;
        bool atual = nivel == k;
        char n[8];
        std::snprintf(n, sizeof n, "%d", k + 1);
        float wt = tecla(n, x - 52, cy + 24, 22);
        texto(Fonte::Forte, nomes[k], x - 52 + wt + 8, cy + 22, 26, atual ? OURO : MARFIM_ESC);
        if (atual) linha(x, cy + 56, 110, OURO);
    }
    linha(cx, cy + 86, 460, OURO);
    float x = cx - 190;
    x += tecla("Clique", x, cy + 110, 22) + 8;
    texto(Fonte::Texto, "continuar", x, cy + 108, 24, MARFIM);
    x += largura(Fonte::Texto, "continuar", 24) + 36;
    x += tecla("Q", x, cy + 110, 22) + 8;
    texto(Fonte::Texto, "sair do jogo", x, cy + 108, 24, MARFIM);
    texto(Fonte::Texto, std::string("versão ") + versao, W - 20.f, H - 34.f, 20, MARFIM_ESC, DIR);
}
