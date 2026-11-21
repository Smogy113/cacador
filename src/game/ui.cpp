#include "game/ui.h"
#include <algorithm>
#include <cstdio>
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

void desenharQuadro(const std::vector<Cartaz>& cs, int sel, const std::string& lugar) {
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
    }
    char buf[96];
    std::snprintf(buf, sizeof buf, "<  %d / %d  >      Setas: trocar cartaz      E: fechar", sel + 1, n);
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
