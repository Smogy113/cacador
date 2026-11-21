#include "game/ui.h"
#include <algorithm>
#include <cstdio>
#include "core/bussola.h"
#include "core/noise.h"
#include "raylib.h"

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

static void textoCentro(const std::string& t, int cx, int y, int tam, Color c) {
    std::string s = semAcentos(t);
    DrawText(s.c_str(), cx - MeasureText(s.c_str(), tam) / 2, y, tam, c);
}

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
    DrawRectangle(0, 0, W, H, {0, 0, 0, 150});
    int pw = std::min(W - 40, 1100), ph = std::min(H - 40, 600);
    int px = (W - pw) / 2, py = (H - ph) / 2;
    DrawRectangle(px, py, pw, ph, {92, 64, 40, 255});            // madeira do quadro
    DrawRectangleLinesEx({float(px), float(py), float(pw), float(ph)}, 8, {60, 40, 25, 255});
    textoCentro("PROCURADOS  -  " + std::string("Xerife de ") + lugar, W / 2, py + 22, 30, {245, 225, 185, 255});
    if (cs.empty()) return;
    int n = int(cs.size());
    int cw = (pw - 80) / 3, ch = ph - 150;
    for (int k = -1; k <= 1; k++) {
        int idx = ((sel + k) % n + n) % n;
        const Cartaz& c = cs[idx];
        int cx = px + 40 + (k + 1) * cw + 10, cy = py + 70 + (k == 0 ? 0 : 14);
        int w = cw - 20, h = ch - (k == 0 ? 0 : 28);
        Color papel = k == 0 ? Color{236, 222, 184, 255} : Color{200, 186, 150, 255};
        DrawRectangle(cx + 6, cy + 6, w, h, {0, 0, 0, 90});
        DrawRectangle(cx, cy, w, h, papel);
        DrawCircle(cx + w / 2, cy + 8, 5, {90, 90, 95, 255});  // prego
        Color tinta{70, 45, 28, 255};
        int mx = cx + w / 2;
        textoCentro("PROCURADO", mx, cy + 18, 30, {130, 30, 22, 255});
        int rw = std::min(w - 60, 170), rh = int(rw * 0.95f);
        retrato(c.rosto, mx - rw / 2, cy + 56, rw, rh);
        DrawRectangleLines(mx - rw / 2, cy + 56, rw, rh, tinta);
        int y = cy + 64 + rh;
        textoCentro(c.nome, mx, y, 22, tinta);
        textoCentro("\"" + c.apelido + "\"", mx, y + 26, 18, tinta);
        textoCentro(c.crime, mx, y + 52, 18, {110, 40, 30, 255});
        char buf[64];
        std::snprintf(buf, sizeof buf, "VIVO: %d pratas", c.recompensaVivo);
        textoCentro(buf, mx, y + 84, 20, {40, 90, 40, 255});
        std::snprintf(buf, sizeof buf, "MORTO: %d pratas", c.recompensaMorto);
        textoCentro(buf, mx, y + 108, 18, {110, 40, 30, 255});
        textoCentro("Visto por ultimo: " + c.ultimoVisto, mx, y + 136, 16, tinta);
        textoCentro(c.pista, mx, y + 158, 16, {110, 40, 30, 255});
        if (idx == aceito) {  // carimbo
            DrawRectangleLinesEx({float(mx - 80), float(cy + 120), 160.f, 44.f}, 4, {150, 30, 25, 220});
            textoCentro("ACEITO", mx, cy + 128, 30, {150, 30, 25, 230});
        }
    }
    char buf[96];
    std::snprintf(buf, sizeof buf, "<  %d / %d  >      Setas: trocar      Enter: aceitar      E: fechar", sel + 1, n);
    textoCentro(buf, W / 2, py + ph - 42, 20, {240, 220, 180, 255});
}

void desenharAvisos(const std::vector<ErroMapa>& erros, float t) {
    if (erros.empty() || t > 10.f) return;
    int y = 40;
    DrawRectangle(10, y - 6, 760, 30 + 22 * int(std::min<size_t>(erros.size(), 6)), {90, 20, 15, 200});
    DrawText("Problemas no arquivo do mapa (o jogo continua sem essas linhas):", 20, y, 18, {255, 225, 200, 255});
    for (size_t k = 0; k < erros.size() && k < 6; k++) {
        char buf[300];
        std::snprintf(buf, sizeof buf, "linha %d: %s", erros[k].linha, semAcentos(erros[k].msg).c_str());
        DrawText(buf, 30, y + 24 + int(k) * 22, 18, {255, 235, 215, 255});
    }
}

void desenharDica(const char* t) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    int tw = MeasureText(t, 22);
    DrawRectangle(w / 2 - tw / 2 - 14, h - 92, tw + 28, 38, {20, 16, 12, 170});
    DrawText(t, w / 2 - tw / 2, h - 84, 22, {240, 225, 195, 255});
}

// ---------------- HUD do tiroteio ----------------
void desenharHud(float vida01, int balas, bool recarregando, int dinheiro, bool mirando, float marca, bool matou) {
    int H = GetScreenHeight(), W = GetScreenWidth();
    int x = 24, y = H - 92;
    DrawRectangle(x - 10, y - 10, 270, 82, {0, 0, 0, 110});
    // vida
    DrawRectangle(x, y, 240, 14, {40, 20, 18, 220});
    Color cv = vida01 < 0.35f ? Color{220, 60, 45, 255} : Color{200, 70, 55, 255};
    DrawRectangle(x, y, int(240 * std::clamp(vida01, 0.f, 1.f)), 14, cv);
    DrawRectangleLines(x, y, 240, 14, {235, 215, 180, 200});
    // balas
    for (int k = 0; k < 6; k++) {
        Color c = k < balas ? Color{220, 190, 110, 255} : Color{90, 80, 65, 160};
        DrawRectangle(x + k * 16, y + 24, 9, 20, c);
        DrawCircle(x + k * 16 + 4, y + 24, 4.5f, c);
    }
    if (recarregando) DrawText("recarregando", x + 104, y + 28, 16, {235, 215, 180, 230});
    char buf[48];
    std::snprintf(buf, sizeof buf, "%d pratas", dinheiro);
    DrawText(buf, x, y + 52, 18, {235, 215, 160, 255});
    // mira
    int cx = W / 2, cy = H / 2;
    if (mirando) {
        Color c{245, 240, 230, 220};
        DrawRectangle(cx - 1, cy - 9, 2, 6, c);
        DrawRectangle(cx - 1, cy + 3, 2, 6, c);
        DrawRectangle(cx - 9, cy - 1, 6, 2, c);
        DrawRectangle(cx + 3, cy - 1, 6, 2, c);
    }
    if (marca > 0) {  // marca de acerto
        Color c = matou ? Color{230, 50, 40, 255} : Color{250, 245, 235, 255};
        for (int s : {-1, 1}) {
            DrawLineEx({float(cx + 5 * s), float(cy + 5)}, {float(cx + 12 * s), float(cy + 12)}, 2, c);
            DrawLineEx({float(cx + 5 * s), float(cy - 5)}, {float(cx + 12 * s), float(cy - 12)}, 2, c);
        }
    }
}

void desenharBussola(float yaw, bool temObj, float pos, float dist) {
    int W = GetScreenWidth();
    const int largura = 460, alt = 30, x0 = W / 2 - largura / 2, y0 = 14;
    const float campo = 1.6f;
    DrawRectangle(x0, y0, largura, alt, {0, 0, 0, 110});
    DrawRectangleLines(x0, y0, largura, alt, {235, 215, 180, 120});
    const struct { const char* t; float rumo; } P[] = {{"N", 3.14159265f}, {"L", 1.5707963f}, {"S", 0.f}, {"O", -1.5707963f}};
    for (auto& p : P) {
        float f = 0.5f + anguloRelativo(yaw, p.rumo) / campo;
        if (f < 0.03f || f > 0.97f) continue;
        int px = x0 + int(f * largura);
        Color c = p.t[0] == 'N' ? Color{240, 120, 90, 255} : Color{235, 215, 180, 255};
        DrawText(p.t, px - MeasureText(p.t, 20) / 2, y0 + 5, 20, c);
    }
    for (int k = -8; k <= 8; k++) {  // riscos a cada 22,5 graus
        float f = 0.5f + anguloRelativo(yaw, float(k) * 0.3927f) / campo;
        if (f < 0.02f || f > 0.98f || k % 4 == 0) continue;
        int px = x0 + int(f * largura);
        DrawLine(px, y0 + 22, px, y0 + 28, {235, 215, 180, 140});
    }
    if (!temObj) return;
    float f = std::clamp(pos, 0.f, 1.f);
    int px = x0 + int(f * largura);
    Color c{250, 200, 80, 255};
    if (pos < 0 || pos > 1) {  // fora: seta na borda
        int s = pos < 0 ? -1 : 1;
        DrawTriangle({float(px - 8 * s), float(y0 + 8)}, {float(px - 8 * s), float(y0 + 22)}, {float(px + 2 * s), float(y0 + 15)}, c);
        DrawTriangle({float(px - 8 * s), float(y0 + 8)}, {float(px + 2 * s), float(y0 + 15)}, {float(px - 8 * s), float(y0 + 22)}, c);
    } else {
        DrawPoly({float(px), float(y0 + alt + 8)}, 4, 7, 0, c);
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d m", int(dist));
    DrawText(buf, px - MeasureText(buf, 18) / 2, y0 + alt + 18, 18, c);
}

void desenharMensagem(const std::string& texto, float t) {
    if (t <= 0 || texto.empty()) return;
    int W = GetScreenWidth(), H = GetScreenHeight();
    unsigned char a = (unsigned char)(255 * std::clamp(t / 0.5f, 0.f, 1.f));
    std::string s = semAcentos(texto);
    int tw = MeasureText(s.c_str(), 24);
    DrawRectangle(W / 2 - tw / 2 - 16, H / 4 - 10, tw + 32, 44, {0, 0, 0, (unsigned char)(a * 0.55f)});
    DrawText(s.c_str(), W / 2 - tw / 2, H / 4, 24, {245, 230, 200, a});
}

void desenharBordaDano(float vida01) {
    if (vida01 >= 0.35f) return;
    int W = GetScreenWidth(), H = GetScreenHeight();
    unsigned char a = (unsigned char)(170 * (1.f - vida01 / 0.35f));
    Color c{150, 10, 5, a}, z{150, 10, 5, 0};
    int e = H / 5;
    DrawRectangleGradientV(0, 0, W, e, c, z);
    DrawRectangleGradientV(0, H - e, W, e, z, c);
    DrawRectangleGradientH(0, 0, e, H, c, z);
    DrawRectangleGradientH(W - e, 0, e, H, z, c);
}

void desenharTelaMorte(float t) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, {0, 0, 0, (unsigned char)(255 * std::clamp(t / 1.5f, 0.f, 1.f))});
    if (t > 0.8f) textoCentro("Voce apagou...", W / 2, H / 2 - 20, 36, {200, 60, 45, 255});
}
