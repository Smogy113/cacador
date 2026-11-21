#include "render/mapa_tela.h"
#include <algorithm>
#include <cmath>
#include "core/bounty.h"
#include "core/motion.h"
#include "core/terrain.h"
#include "render/shaders.h"
#include "render/terrain_mesh.h"

namespace {
const char* FS_REDONDO = R"(
uniform sampler2D texture0;
uniform vec2 centro;      // jogador no mapa (uv)
uniform vec2 direita;     // direcao da tela para a direita, no mundo (x, z)
uniform vec2 frente;      // direcao da tela para cima, no mundo (x, z)
uniform float escala;     // raio do minimapa em uv
void main() {
    vec2 p = (fragTexCoord - 0.5) * 2.0;
    float r = length(p);
    if (r > 1.0) discard;
    vec2 mundo = direita * p.x - frente * p.y;
    vec3 c = TEX(texture0, centro + mundo * escala).rgb;
    c *= 1.0 - 0.45 * smoothstep(0.82, 1.0, r);   // borda escurece
    FRAG = vec4(c, 0.93);
}
)";
Vector2 v2(float x, float y) { return {x, y}; }
}  // namespace

void MapaTela::carregar(const Terreno& t, const Mapa& m) {
    meta_ = Terreno::META;
    lugares_ = m.lugares;
    const int N = 512;
    Image img = GenImageColor(N, N, BLACK);
    Color* px = (Color*)img.data;
    Vec3f luz = normalizar(Vec3f{-0.6f, 0.7f, -0.4f});  // luz vinda do noroeste (relevo)
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            float x = -meta_ + (i + 0.5f) * 2 * meta_ / N, z = -meta_ + (j + 0.5f) * 2 * meta_ / N;
            Vec3f n = t.normal(x, z);
            Rgb c = corDoChao(t, x, z, n.y);
            float relevo = std::clamp(0.55f + 0.6f * dot(n, luz), 0.45f, 1.2f);
            c = c * relevo;
            float rio = t.mascaraRio(x, z), est = t.mascaraEstrada(x, z);
            c = lerp(c, Rgb{0.27f, 0.42f, 0.52f}, std::clamp(rio * 1.4f, 0.f, 1.f));
            c = lerp(c, Rgb{0.56f, 0.44f, 0.3f}, std::clamp((est - 0.3f) * 3.f, 0.f, 1.f));
            if (!t.dentroDoJogavel(x, z)) c = c * 0.6f;
            c = lerp(c, Rgb{0.86f, 0.79f, 0.63f}, 0.28f);  // papel
            auto c8 = [](float v) { return (unsigned char)std::clamp(v * 255.f, 0.f, 255.f); };
            px[j * N + i] = Color{c8(c.r), c8(c.g), c8(c.b), 255};
        }
    tex_ = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex_, TEXTURE_FILTER_BILINEAR);
    redondo_ = carregarShaderPos(FS_REDONDO);
}

void MapaTela::descarregar() {
    UnloadTexture(tex_);
    UnloadShader(redondo_);
}

void MapaTela::desenharMini(Vec3f jog, float yawJ, float yawC, const std::vector<Vec3f>& rota, bool temObj, Vec3f obj) {
    const float R = 105.f, RAIO_MUNDO = 170.f;  // pixels e metros
    float cx = GetScreenWidth() - R - 26.f, cy = GetScreenHeight() - R - 26.f;
    Vec3f fr = frenteDe(yawC), dir{-std::cos(yawC), 0, std::sin(yawC)};
    float centro[2] = {(jog.x + meta_) / (2 * meta_), (jog.z + meta_) / (2 * meta_)};
    float vd[2] = {dir.x, dir.z}, vf[2] = {fr.x, fr.z}, esc = RAIO_MUNDO / (2 * meta_);
    DrawCircle(int(cx), int(cy), R + 5, Color{30, 22, 14, 210});
    BeginShaderMode(redondo_);
    SetShaderValue(redondo_, GetShaderLocation(redondo_, "centro"), centro, SHADER_UNIFORM_VEC2);
    SetShaderValue(redondo_, GetShaderLocation(redondo_, "direita"), vd, SHADER_UNIFORM_VEC2);
    SetShaderValue(redondo_, GetShaderLocation(redondo_, "frente"), vf, SHADER_UNIFORM_VEC2);
    SetShaderValue(redondo_, GetShaderLocation(redondo_, "escala"), &esc, SHADER_UNIFORM_FLOAT);
    DrawTexturePro(tex_, {0, 0, float(tex_.width), float(tex_.height)}, {cx - R, cy - R, 2 * R, 2 * R}, {0, 0}, 0, WHITE);
    EndShaderMode();
    // mundo -> tela do minimapa (a frente da camera fica para cima)
    auto tela = [&](Vec3f p, bool& dentro) {
        Vec3f d = p - jog;
        float sx = dot(d, dir), sy = -dot(d, fr);
        float k = R / RAIO_MUNDO;
        Vector2 q{cx + sx * k, cy + sy * k};
        float l = std::hypot(q.x - cx, q.y - cy);
        dentro = l <= R - 3;
        if (!dentro) { q.x = cx + (q.x - cx) * (R - 3) / l; q.y = cy + (q.y - cy) * (R - 3) / l; }
        return q;
    };
    // rota (vermelha, como GPS)
    for (size_t i = 1; i < rota.size(); i++) {
        bool da, db;
        Vector2 a = tela(rota[i - 1], da), b = tela(rota[i], db);
        if (!da && !db) continue;
        DrawLineEx(a, b, 5.f, Color{120, 20, 15, 255});
        DrawLineEx(a, b, 3.f, Color{215, 45, 35, 255});
    }
    if (temObj) {
        bool dentro;
        Vector2 q = tela(obj, dentro);
        DrawPoly(q, 4, dentro ? 8.f : 6.f, 45, Color{250, 200, 70, 255});
        DrawPolyLines(q, 4, dentro ? 8.f : 6.f, 45, Color{60, 40, 10, 255});
    }
    // jogador: seta apontando para onde ele olha
    float a = yawJ - yawC;
    Vector2 ponta{cx - std::sin(a) * 11.f, cy - std::cos(a) * 11.f}, e1{cx - std::sin(a + 2.5f) * 8.f, cy - std::cos(a + 2.5f) * 8.f},
        e2{cx - std::sin(a - 2.5f) * 8.f, cy - std::cos(a - 2.5f) * 8.f};
    DrawTriangle(ponta, e2, e1, Color{250, 250, 245, 255});
    DrawTriangleLines(ponta, e2, e1, Color{20, 20, 20, 255});
    // norte na borda
    bool dn;
    Vector2 n = tela(jog + Vec3f{0, 0, -1000}, dn);
    DrawCircle(int(n.x), int(n.y), 10, Color{30, 22, 14, 230});
    DrawText("N", int(n.x) - MeasureText("N", 16) / 2, int(n.y) - 8, 16, Color{240, 120, 90, 255});
    DrawCircleLines(int(cx), int(cy), R + 1, Color{200, 175, 130, 255});
}

void MapaTela::desenharCheio(Vec3f jog, float yawJ, const std::vector<Vec3f>& rota, bool temObj, Vec3f obj) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Color{10, 8, 6, 200});
    float lado = float(std::min(W, H) - 90), x0 = (W - lado) / 2, y0 = (H - lado) / 2 + 10;
    DrawRectangle(int(x0) - 8, int(y0) - 8, int(lado) + 16, int(lado) + 16, Color{60, 42, 26, 255});
    DrawTexturePro(tex_, {0, 0, float(tex_.width), float(tex_.height)}, {x0, y0, lado, lado}, {0, 0}, 0, WHITE);
    auto tela = [&](Vec3f p) { return v2(x0 + (p.x + meta_) / (2 * meta_) * lado, y0 + (p.z + meta_) / (2 * meta_) * lado); };
    for (size_t i = 1; i < rota.size(); i++) {
        DrawLineEx(tela(rota[i - 1]), tela(rota[i]), 5.f, Color{120, 20, 15, 255});
        DrawLineEx(tela(rota[i - 1]), tela(rota[i]), 3.f, Color{215, 45, 35, 255});
    }
    for (auto& l : lugares_) {
        Vector2 q = tela({l.x, 0, l.z});
        std::string nome = nomeLegivel(l.nome);
        int tam = l.tipo == TipoLugar::Cidade ? 22 : (l.tipo == TipoLugar::Vilarejo ? 19 : 16);
        DrawCircle(int(q.x), int(q.y), 3, Color{50, 35, 20, 255});
        int tw = MeasureText(nome.c_str(), tam);
        DrawText(nome.c_str(), int(q.x) - tw / 2 + 1, int(q.y) + 7, tam, Color{240, 225, 190, 200});
        DrawText(nome.c_str(), int(q.x) - tw / 2, int(q.y) + 6, tam, Color{45, 30, 18, 255});
    }
    if (temObj) {
        Vector2 q = tela(obj);
        DrawPoly(q, 4, 11.f, 45, Color{250, 200, 70, 255});
        DrawPolyLines(q, 4, 11.f, 45, Color{60, 40, 10, 255});
    }
    Vector2 c = tela(jog);  // jogador: seta (norte = para cima)
    Vector2 ponta{c.x + std::sin(yawJ) * 13.f, c.y + std::cos(yawJ) * 13.f}, e1{c.x + std::sin(yawJ + 2.5f) * 9.f, c.y + std::cos(yawJ + 2.5f) * 9.f},
        e2{c.x + std::sin(yawJ - 2.5f) * 9.f, c.y + std::cos(yawJ - 2.5f) * 9.f};
    DrawTriangle(ponta, e1, e2, Color{250, 250, 245, 255});
    DrawTriangle(ponta, e2, e1, Color{250, 250, 245, 255});
    const char* t = "MAPA   -   M: fechar";
    DrawText(t, W / 2 - MeasureText(t, 22) / 2, int(y0) - 36, 22, Color{240, 225, 190, 255});
}
