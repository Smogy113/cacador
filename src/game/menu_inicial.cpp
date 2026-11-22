#include "game/menu_inicial.h"
#include <algorithm>
#include "render/estilo.h"

namespace {
using estilo::Fonte;
constexpr Color FUNDO{18, 17, 15, 255};
constexpr const char* PRINCIPAL[] = {"Jogar", "Configurações", "Sair"};
constexpr const char* QUALIDADE[] = {"Baixo", "Médio", "Alto", "Voltar"};
#if defined(PLATFORM_WEB)
constexpr int NUM_PRINCIPAL = 2;
#else
constexpr int NUM_PRINCIPAL = 3;
#endif
float escala() { return std::min(GetScreenWidth() / 850.f, GetScreenHeight() / 720.f); }
float margem() { return GetScreenWidth() * 0.065f; }
float topo() { return GetScreenHeight() * 0.25f; }
}  // namespace

void MenuInicial::carregar() {
    estilo::carregar();
    fundo_ = LoadTexture(DATA_DIR "/menu/entrada.jpg");
    if (fundo_.id) SetTextureFilter(fundo_, TEXTURE_FILTER_BILINEAR);
}

void MenuInicial::descarregar() {
    if (fundo_.id) UnloadTexture(fundo_);
    fundo_ = {};
}

Rectangle MenuInicial::item(int indice) const {
    float s = escala();
    return {margem(), topo() + (166.f + indice * 49.f) * s, 292.f * s, 46.f * s};
}

MenuInicial::Acao MenuInicial::atualizar(Nivel& nivel) {
    int total = configuracoes_ ? 4 : NUM_PRINCIPAL;
    if (IsKeyPressed(KEY_ESCAPE) && configuracoes_) {
        configuracoes_ = false;
        foco_ = 1;
        return Acao::Nenhuma;
    }
    Vector2 mouse = GetMousePosition();
    Vector2 delta = GetMouseDelta();
    bool clique = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    int acionado = -1;
    if (clique || delta.x != 0 || delta.y != 0) {
        for (int i = 0; i < total; ++i) {
            if (!CheckCollisionPointRec(mouse, item(i))) continue;
            foco_ = i;
            if (clique) acionado = i;
            break;
        }
    }
    if (IsKeyPressed(KEY_UP)) foco_ = (foco_ + total - 1) % total;
    if (IsKeyPressed(KEY_DOWN)) foco_ = (foco_ + 1) % total;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) acionado = foco_;
    if (acionado < 0) return Acao::Nenhuma;

    if (configuracoes_) {
        if (acionado == 3) {
            configuracoes_ = false;
            foco_ = 1;
        } else {
            nivel = static_cast<Nivel>(acionado);
        }
    } else if (acionado == 1) {
        configuracoes_ = true;
        foco_ = static_cast<int>(nivel);
    } else {
        return acionado == 0 ? Acao::Jogar : Acao::Sair;
    }
    return Acao::Nenhuma;
}

void MenuInicial::desenhar(Nivel nivel, bool carregando) const {
    int w = GetScreenWidth(), h = GetScreenHeight();
    float s = escala(), x = margem(), y = topo();
    ClearBackground(FUNDO);
    if (fundo_.id) {
        float zoom = std::max(float(w) / fundo_.width, float(h) / fundo_.height);
        // Corta o lado esquerdo em janelas estreitas para manter a paisagem visivel.
        DrawTexturePro(fundo_, {fundo_.width - w / zoom, (fundo_.height - h / zoom) * .5f, w / zoom, h / zoom},
                       {0, 0, float(w), float(h)}, {0, 0}, 0, WHITE);
    }
    int escuro = int(w * .23f), transicao = int(w * .54f);
    Color borda{15, 17, 16, 242};
    DrawRectangle(0, 0, escuro, h, borda);
    DrawRectangleGradientH(escuro, 0, transicao, h, borda, BLANK);
    DrawRectangleGradientV(0, h * 4 / 5, w, h - h * 4 / 5, BLANK, {13, 15, 14, 198});

    estilo::texto(Fonte::Titulo, "Caçador", x, y, 84.f * s, estilo::MARFIM);
    DrawRectangleRec({x + 2 * s, y + 102.f * s, 62.f * s, std::max(1.f, s)}, estilo::OURO);
    if (carregando) {
        estilo::texto(Fonte::Texto, "Carregando o mundo...", x + 2 * s, y + 171.f * s, 28.f * s, estilo::MARFIM);
        return;
    }
    if (configuracoes_)
        estilo::texto(Fonte::Forte, "Qualidade gráfica", x + 2 * s, y + 121.f * s, 24.f * s, estilo::MARFIM_ESC);
    int total = configuracoes_ ? 4 : NUM_PRINCIPAL;
    for (int i = 0; i < total; ++i) {
        Rectangle r = item(i);
        if (foco_ == i) {
            DrawRectangleGradientH(int(r.x), int(r.y), int(r.width), int(r.height), {179, 140, 79, 57}, BLANK);
            DrawRectangleRec({r.x, r.y + 7 * s, 2 * s, r.height - 14 * s}, estilo::OURO);
        }
        const char* rotulo = configuracoes_ ? QUALIDADE[i] : PRINCIPAL[i];
        estilo::texto(Fonte::Forte, rotulo, r.x + 15 * s, r.y + 5 * s, 31.f * s,
                       foco_ == i ? estilo::MARFIM : estilo::MARFIM_ESC);
        if (configuracoes_ && i == static_cast<int>(nivel))
            DrawCircleV({r.x + 195 * s, r.y + 22 * s}, 4 * s, estilo::OURO);
    }
    estilo::texto(Fonte::Texto, "Setas para navegar   ·   Enter para selecionar", x, h - 53.f * s,
                   20.f * s, estilo::MARFIM_ESC);
}
