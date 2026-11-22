#include "render/estilo.h"
#include <algorithm>
#include <vector>

namespace estilo {
namespace {
Font fontes[3];
bool ok = false;
Font& fonte(Fonte f) { return fontes[int(f)]; }
}  // namespace

void carregar() {
    if (ok) return;
    std::vector<int> cps;
    for (int c = 32; c < 256; c++) cps.push_back(c);  // latim com acentos
    const char* arq[3] = {"data/fontes/Rye.ttf", "data/fontes/EBGaramond-Medium.ttf", "data/fontes/EBGaramond-SemiBold.ttf"};
    const int tam[3] = {72, 64, 64};
    for (int k = 0; k < 3; k++) {
        fontes[k] = LoadFontEx(arq[k], tam[k], cps.data(), int(cps.size()));
        if (fontes[k].texture.id == 0) fontes[k] = GetFontDefault();
        else SetTextureFilter(fontes[k].texture, TEXTURE_FILTER_BILINEAR);
    }
    ok = true;
}
void descarregar() {
    if (!ok) return;
    for (auto& f : fontes)
        if (f.texture.id != GetFontDefault().texture.id) UnloadFont(f);
    ok = false;
}
Color comAlfa(Color c, float a) { return Color{c.r, c.g, c.b, (unsigned char)std::clamp(a * c.a, 0.f, 255.f)}; }
float largura(Fonte f, const std::string& s, float tam) { return MeasureTextEx(fonte(f), s.c_str(), tam, tam * 0.02f).x; }

void texto(Fonte f, const std::string& s, float x, float y, float tam, Color c, int alinha, bool sombra) {
    float w = largura(f, s, tam);
    if (alinha == CENTRO) x -= w * 0.5f;
    if (alinha == DIR) x -= w;
    if (sombra) {  // sombra suave (dois deslocamentos) em vez de caixa
        Color s1 = comAlfa(SOMBRA, c.a / 255.f * 0.8f), s2 = comAlfa(SOMBRA, c.a / 255.f * 0.4f);
        DrawTextEx(fonte(f), s.c_str(), {x + 1.5f, y + 2.f}, tam, tam * 0.02f, s1);
        DrawTextEx(fonte(f), s.c_str(), {x + 0.f, y + 3.f}, tam, tam * 0.02f, s2);
    }
    DrawTextEx(fonte(f), s.c_str(), {x, y}, tam, tam * 0.02f, c);
}

float tecla(const std::string& t, float x, float y, float tam) {
    float w = std::max(tam * 1.1f, largura(Fonte::Forte, t, tam * 0.8f) + tam * 0.6f), h = tam * 1.1f;
    Rectangle r{x, y, w, h};
    DrawRectangleRounded(r, 0.25f, 6, Color{20, 14, 10, 170});
    DrawRectangleRoundedLinesEx(r, 0.25f, 6, 1.5f, comAlfa(MARFIM, 0.85f));
    texto(Fonte::Forte, t, x + w * 0.5f, y + h * 0.5f - tam * 0.45f, tam * 0.8f, MARFIM, CENTRO, false);
    return w;
}

void faixa(float cx, float y, float w, float h, float a) {
    Color c{8, 6, 4, (unsigned char)(170 * a)}, z{8, 6, 4, 0};
    DrawRectangleGradientH(int(cx - w / 2), int(y), int(w / 2), int(h), z, c);
    DrawRectangleGradientH(int(cx), int(y), int(w / 2), int(h), c, z);
}

void linha(float cx, float y, float w, Color c) {
    Color z = comAlfa(c, 0.f);
    DrawRectangleGradientH(int(cx - w / 2), int(y), int(w / 2), 1, z, c);
    DrawRectangleGradientH(int(cx), int(y), int(w / 2), 1, c, z);
}
}  // namespace estilo
