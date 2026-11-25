#include "game/ui_oeste.h"
#include <algorithm>
#include <cmath>
#include "render/estilo.h"

namespace oeste {
using estilo::Fonte;
using estilo::comAlfa;

namespace {
const Color SOMBRA_PAINEL{0, 0, 0, 110};

Vector2 operator+(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
Vector2 polar(Vector2 c, float r, float ang) { return {c.x + std::cos(ang) * r, c.y + std::sin(ang) * r}; }

// Os 8 cantos do retângulo chanfrado, em sentido anti-horário na tela (o que a raylib desenha).
void octogono(Rectangle r, float c, Vector2 p[8]) {
    c = std::min(c, std::min(r.width, r.height) * 0.5f);
    p[0] = {r.x + c, r.y};
    p[1] = {r.x, r.y + c};
    p[2] = {r.x, r.y + r.height - c};
    p[3] = {r.x + c, r.y + r.height};
    p[4] = {r.x + r.width - c, r.y + r.height};
    p[5] = {r.x + r.width, r.y + r.height - c};
    p[6] = {r.x + r.width, r.y + c};
    p[7] = {r.x + r.width - c, r.y};
}
void preencherChanfrado(Rectangle r, float c, Color cor) {
    Vector2 p[8];
    octogono(r, c, p);
    DrawTriangleFan(p, 8, cor);
}
void contornoChanfrado(Rectangle r, float c, float esp, Color cor) {
    Vector2 p[8];
    octogono(r, c, p);
    for (int i = 0; i < 8; i++) DrawLineEx(p[i], p[(i + 1) % 8], esp, cor);
}
Rectangle encolher(Rectangle r, float d) { return {r.x + d, r.y + d, r.width - 2 * d, r.height - 2 * d}; }
float larguraTecla(const std::string& t, float tam) { return std::max(tam * 1.15f, estilo::largura(Fonte::Forte, t, tam * 0.75f) + tam * 0.6f); }
}  // namespace

// ---------------------------------------------------------------- animação e mouse

float suave(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}
Rectangle escalar(Rectangle r, float s) {
    const float w = r.width * s, h = r.height * s;
    return {r.x + (r.width - w) * 0.5f, r.y + (r.height - h) * 0.5f, w, h};
}
Color misturar(Color a, Color b, float t) {
    auto m = [t](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * std::clamp(t, 0.f, 1.f)); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}
// ---------------------------------------------------------------- tela ampliada

namespace {
Ampliacao ampliacaoAtual;  // a do EspacoAmpliado mais interno; zoom 1 fora de qualquer um
}  // namespace

float Ampliacao::larguraVirtual() const { return float(GetScreenWidth()) / zoom; }
float Ampliacao::alturaVirtual() const { return float(GetScreenHeight()) / zoom; }

Ampliacao ampliacaoParaCaber(float largura, float altura, float ocupacao) {
    const float zoom = std::min(float(GetScreenWidth()) * ocupacao / largura, float(GetScreenHeight()) * ocupacao / altura);
    // Abaixo de 0,5 o texto fica ilegível de qualquer jeito; acima de 4 só em telas que ainda não existem.
    return {std::clamp(zoom, 0.5f, 4.f)};
}

EspacoAmpliado::EspacoAmpliado(Ampliacao a, bool desenhar) : anterior_(ampliacaoAtual), desenhar_(desenhar) {
    ampliacaoAtual = a;
    if (desenhar_) {
        Camera2D camera{};
        camera.zoom = a.zoom;
        BeginMode2D(camera);
    }
}

EspacoAmpliado::~EspacoAmpliado() {
    if (desenhar_) EndMode2D();
    ampliacaoAtual = anterior_;
}

Vector2 mouse() {
    const Vector2 m = GetMousePosition();
    return {m.x / ampliacaoAtual.zoom, m.y / ampliacaoAtual.zoom};
}
bool sobre(Rectangle r) { return CheckCollisionPointRec(mouse(), r); }
bool clicou(Rectangle r) { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && sobre(r); }

// ---------------------------------------------------------------- painéis e controles

void painel(Rectangle r, float alfa, float c) {
    preencherChanfrado({r.x + 3, r.y + 5, r.width, r.height}, c, comAlfa(SOMBRA_PAINEL, alfa));  // sombra
    preencherChanfrado(r, c, comAlfa(FUNDO, alfa));
    contornoChanfrado(r, c, 1.5f, comAlfa(LATAO, alfa));                                  // borda externa
    contornoChanfrado(encolher(r, 4.f), c * 0.7f, 1.f, comAlfa(LATAO, 0.45f * alfa));      // borda interna
}

void card(Rectangle r, bool hover, bool ativo, bool foco, bool bloqueado, float alfa) {
    Color fundo = hover ? misturar(CARD, Color{70, 56, 42, 245}, 0.6f) : CARD;
    if (ativo) fundo = misturar(fundo, Color{80, 60, 34, 245}, 0.5f);
    preencherChanfrado(r, 6.f, comAlfa(fundo, alfa));
    Color borda = bloqueado ? FERRO : (foco && ativo ? BRASA : (ativo ? LATAO : comAlfa(LATAO, hover ? 0.7f : 0.3f)));
    contornoChanfrado(r, 6.f, ativo ? 2.f : 1.f, comAlfa(borda, alfa));
}

float tecla(const std::string& t, float x, float y, float tam, float alfa) {
    const float w = larguraTecla(t, tam), h = tam * 1.15f;
    const Rectangle r{x, y, w, h};
    DrawRectangleRounded(r, 0.25f, 4, comAlfa(Color{58, 44, 28, 255}, alfa));
    DrawRectangleRoundedLinesEx(r, 0.25f, 4, 1.5f, comAlfa(LATAO, alfa));
    DrawLineEx({x + 3, y + h - 2}, {x + w - 3, y + h - 2}, 2.f, comAlfa(Color{20, 14, 10, 255}, 0.7f * alfa));  // borda de baixo
    estilo::texto(Fonte::Forte, t, x + w * 0.5f, y + h * 0.5f - tam * 0.45f, tam * 0.75f, comAlfa(PERGAMINHO, alfa), estilo::CENTRO, false);
    return w;
}

void botao(Rectangle r, const std::string& t, const std::string& rotulo, bool destaque, float alfa) {
    const bool h = sobre(r);
    card(r, h, destaque, false, false, alfa);
    const float tam = std::min(18.f, r.height * 0.55f);
    const float wt = t.empty() ? 0.f : larguraTecla(t, tam) + 8.f;
    const float w = wt + estilo::largura(Fonte::Forte, rotulo, tam);
    float x = r.x + (r.width - w) * 0.5f;
    if (!t.empty()) tecla(t, x, r.y + (r.height - tam * 1.15f) * 0.5f, tam, alfa);
    estilo::texto(Fonte::Forte, rotulo, x + wt, r.y + (r.height - tam) * 0.5f - 2.f, tam, comAlfa(h || destaque ? PERGAMINHO : CINZA, alfa),
                  estilo::ESQ, false);
}

void barra(Rectangle r, float f, Color cor, float alfa) {
    DrawRectangleRec(r, comAlfa(Color{12, 9, 8, 255}, 0.9f * alfa));
    const float w = (r.width - 4.f) * std::clamp(f, 0.f, 1.f);
    DrawRectangleGradientV(int(r.x + 2), int(r.y + 2), int(w), int(r.height - 4), comAlfa(misturar(cor, WHITE, 0.25f), alfa),
                           comAlfa(misturar(cor, BLACK, 0.35f), alfa));
    DrawRectangleLinesEx(r, 1.f, comAlfa(LATAO, alfa));
}

void prompt(const std::string& t, const std::string& acao, float cx, float y, float alfa, float tam) {
    const float wt = t.empty() ? 0.f : larguraTecla(t, tam) + 10.f;
    const float w = wt + estilo::largura(Fonte::Forte, acao, tam) + 2 * PAD, h = tam * 1.15f + 12.f;
    const Rectangle r{cx - w * 0.5f, y, w, h};
    preencherChanfrado({r.x + 2, r.y + 3, r.width, r.height}, 5.f, comAlfa(SOMBRA_PAINEL, alfa));
    preencherChanfrado(r, 5.f, comAlfa(FUNDO, alfa));
    contornoChanfrado(r, 5.f, 1.f, comAlfa(LATAO, 0.8f * alfa));
    if (!t.empty()) tecla(t, r.x + PAD, y + 6.f, tam, alfa);
    estilo::texto(Fonte::Forte, acao, r.x + PAD + wt, y + 6.f + tam * 0.05f - 2.f, tam, comAlfa(PERGAMINHO, alfa), estilo::ESQ, false);
}

void cabecalho(const std::string& num, const std::string& titulo, float x, float y, float w, bool foco) {
    const Color cor = foco ? BRASA : LATAO;
    DrawCircleV({x + 11, y + 11}, 11, comAlfa(cor, 0.25f));
    DrawCircleLinesV({x + 11, y + 11}, 11, cor);
    estilo::texto(Fonte::Forte, num, x + 11, y + 1, 17, PERGAMINHO, estilo::CENTRO, false);
    estilo::texto(Fonte::Forte, maiusculas(titulo), x + 30, y + 1, 17, foco ? PERGAMINHO : CINZA, estilo::ESQ, false);
    DrawRectangleGradientH(int(x), int(y + 26), int(w), 1, comAlfa(cor, 0.9f), comAlfa(cor, 0.f));
}

// ---------------------------------------------------------------- ícones

void iconeEntalhe(Entalhe e, Vector2 c, float r, Color cor) {
    const float esp = std::max(1.5f, r * 0.11f);
    switch (e) {
    case Entalhe::Espiral: {  // espiral de Arquimedes de 2,5 voltas, de dentro para fora
        Vector2 p[48];
        for (int i = 0; i < 48; i++) {
            const float f = float(i) / 47.f, ang = f * 2.5f * 2.f * PI;
            p[i] = polar(c, r * (0.08f + 0.88f * f), ang);
        }
        DrawSplineLinear(p, 48, esp, cor);
        break;
    }
    case Entalhe::Grade: {  // losango com treliça diagonal; as linhas de cima têm uma folga escura (entrelaçado)
        const float m = r * 0.92f;
        DrawPolyLinesEx(c, 4, m, 0, esp, cor);
        for (int k = -1; k <= 1; k++) {
            const float d = float(k) * m * 0.5f;
            // linhas "\" (por baixo)
            DrawLineEx({c.x - m * 0.5f + d * 0.5f, c.y - m * 0.5f - d * 0.5f}, {c.x + m * 0.5f + d * 0.5f, c.y + m * 0.5f - d * 0.5f}, esp * 0.8f, cor);
        }
        for (int k = -1; k <= 1; k++) {
            const float d = float(k) * m * 0.5f;
            const Vector2 a{c.x - m * 0.5f + d * 0.5f, c.y + m * 0.5f + d * 0.5f}, b{c.x + m * 0.5f + d * 0.5f, c.y - m * 0.5f + d * 0.5f};
            DrawLineEx(a, b, esp * 2.2f, Color{20, 15, 12, 200});  // folga do entrelaçado
            DrawLineEx(a, b, esp * 0.8f, cor);                      // linhas "/" (por cima)
        }
        break;
    }
    case Entalhe::Onda: {  // três ondas paralelas
        for (int k = -1; k <= 1; k++) {
            Vector2 p[24];
            for (int i = 0; i < 24; i++) {
                const float f = float(i) / 23.f;
                p[i] = {c.x - r + 2 * r * f, c.y + float(k) * r * 0.55f + std::sin(f * 2.f * PI * 1.5f) * r * 0.2f};
            }
            DrawSplineLinear(p, 24, esp, cor);
        }
        break;
    }
    }
}

void iconeCadeado(Vector2 c, float r, Color cor) {
    const float esp = std::max(1.5f, r * 0.14f);
    DrawRing({c.x, c.y - r * 0.1f}, r * 0.38f - esp * 0.5f, r * 0.38f + esp * 0.5f, 180, 360, 16, cor);  // alça
    DrawLineEx({c.x - r * 0.38f, c.y - r * 0.1f}, {c.x - r * 0.38f, c.y + r * 0.1f}, esp, cor);
    DrawLineEx({c.x + r * 0.38f, c.y - r * 0.1f}, {c.x + r * 0.38f, c.y + r * 0.1f}, esp, cor);
    const Rectangle corpo{c.x - r * 0.6f, c.y + r * 0.05f, r * 1.2f, r * 0.85f};
    DrawRectangleRounded(corpo, 0.25f, 4, cor);
    DrawCircleV({c.x, c.y + r * 0.38f}, r * 0.13f, Color{20, 15, 12, 255});  // fechadura
    DrawLineEx({c.x, c.y + r * 0.4f}, {c.x, c.y + r * 0.66f}, r * 0.1f, Color{20, 15, 12, 255});
}

Color corDaPasta(Pasta p) {
    switch (p) {
    case Pasta::PolvoraFina: return {86, 88, 96, 255};    // chumbo escuro
    case Pasta::ResinaECera: return {214, 140, 46, 255};  // âmbar
    case Pasta::EnxofreESal: return {210, 178, 62, 255};  // amarelo ocre
    case Pasta::PoDePrata: return {150, 190, 214, 255};   // azul-prateado
    }
    return CINZA;
}

void iconeFrasco(Pasta p, Vector2 c, float r, float alfa) {
    const Color vidro = comAlfa(Color{200, 190, 170, 255}, 0.9f * alfa), cor = comAlfa(corDaPasta(p), alfa);
    const Vector2 bojo{c.x, c.y + r * 0.25f};
    DrawCircleV(bojo, r * 0.62f, comAlfa(Color{18, 14, 12, 255}, alfa));
    DrawCircleSector(bojo, r * 0.55f, 0, 180, 16, cor);  // conteúdo (metade de baixo cheia)
    DrawRectangleRec({c.x - r * 0.55f, bojo.y - r * 0.12f, r * 1.1f, r * 0.14f}, cor);
    DrawCircleLinesV(bojo, r * 0.62f, vidro);
    DrawRectangleRec({c.x - r * 0.17f, c.y - r * 0.62f, r * 0.34f, r * 0.3f}, comAlfa(Color{18, 14, 12, 255}, alfa));  // gargalo
    DrawRectangleLinesEx({c.x - r * 0.17f, c.y - r * 0.62f, r * 0.34f, r * 0.3f}, 1.f, vidro);
    DrawRectangleRec({c.x - r * 0.22f, c.y - r * 0.85f, r * 0.44f, r * 0.24f}, comAlfa(Color{120, 84, 52, 255}, alfa));  // rolha
    DrawCircleV({bojo.x - r * 0.25f, bojo.y - r * 0.2f}, r * 0.09f, comAlfa(WHITE, 0.5f * alfa));                     // reflexo
}

void iconeDoador(Doador d, Vector2 c, float r, float alfa) {
    switch (d) {
    case Doador::Pena: {  // haste curva com barbas
        const Color cor = comAlfa(PERGAMINHO, alfa);
        Vector2 haste[12];
        for (int i = 0; i < 12; i++) {
            const float f = float(i) / 11.f;
            haste[i] = {c.x - r * 0.7f + f * r * 1.4f, c.y + r * 0.7f - f * r * 1.4f + std::sin(f * PI) * r * 0.15f};
        }
        DrawSplineLinear(haste, 12, std::max(1.2f, r * 0.07f), cor);
        for (int i = 3; i < 11; i++) {
            const Vector2 b = haste[i];
            const float comp = r * 0.45f * std::sin(float(i) / 11.f * PI);
            DrawLineEx(b, {b.x - comp * 0.75f, b.y - comp * 0.3f}, 1.2f, comAlfa(cor, 0.8f));
            DrawLineEx(b, {b.x + comp * 0.3f, b.y + comp * 0.75f}, 1.2f, comAlfa(cor, 0.8f));
        }
        break;
    }
    case Doador::PedraPesada:  // pedra irregular: dois hexágonos girados
        DrawPoly({c.x, c.y + r * 0.1f}, 6, r * 0.72f, 12, comAlfa(Color{112, 106, 98, 255}, alfa));
        DrawPoly({c.x - r * 0.12f, c.y}, 5, r * 0.55f, 40, comAlfa(Color{140, 132, 120, 255}, alfa));
        DrawPolyLinesEx({c.x, c.y + r * 0.1f}, 6, r * 0.72f, 12, 1.2f, comAlfa(Color{40, 36, 32, 255}, alfa));
        break;
    case Doador::CarvaoEmBrasa:  // carvão escuro com pontos de brasa
        DrawPoly(c, 7, r * 0.7f, 20, comAlfa(Color{34, 28, 26, 255}, alfa));
        DrawPolyLinesEx(c, 7, r * 0.7f, 20, 1.2f, comAlfa(BRASA, 0.8f * alfa));
        DrawCircleV({c.x - r * 0.2f, c.y + r * 0.1f}, r * 0.11f, comAlfa(BRASA, alfa));
        DrawCircleV({c.x + r * 0.25f, c.y - r * 0.15f}, r * 0.08f, comAlfa(Color{250, 170, 70, 255}, alfa));
        DrawCircleV({c.x + r * 0.05f, c.y + r * 0.35f}, r * 0.07f, comAlfa(BRASA, alfa));
        break;
    }
}

Color corDoPavio(EstadoPavio e) {
    switch (e) {
    case EstadoPavio::Seco: return BRASA;
    case EstadoPavio::Umido:
    case EstadoPavio::Molhado: return GELO;
    case EstadoPavio::BloqueadoFerro: return FERRO;
    }
    return CINZA;
}

void iconePavio(EstadoPavio e, float umidade, Vector2 c, float r, float t) {
    switch (e) {
    case EstadoPavio::Seco: {  // chama viva: gota invertida externa (brasa) e interna (amarela), tremulando
        const float bal = std::sin(t * 9.f) * r * 0.06f, alt = 1.f + std::sin(t * 13.f) * 0.06f;
        DrawCircleV({c.x, c.y + r * 0.3f}, r * 0.5f, BRASA);
        DrawTriangle({c.x + bal, c.y - r * 0.85f * alt}, {c.x - r * 0.5f, c.y + r * 0.25f}, {c.x + r * 0.5f, c.y + r * 0.25f}, BRASA);
        DrawCircleV({c.x, c.y + r * 0.42f}, r * 0.26f, Color{250, 196, 90, 255});
        DrawTriangle({c.x + bal * 1.5f, c.y - r * 0.3f * alt}, {c.x - r * 0.26f, c.y + r * 0.4f}, {c.x + r * 0.26f, c.y + r * 0.4f},
                     Color{250, 196, 90, 255});
        break;
    }
    case EstadoPavio::Umido:
    case EstadoPavio::Molhado: {  // gota d'água; úmido = anel da secagem que falta
        const float g = r * 0.62f;
        const Color cor = e == EstadoPavio::Molhado ? comAlfa(GELO, 0.75f + 0.25f * std::sin(t * 5.f)) : GELO;
        DrawCircleV({c.x, c.y + g * 0.25f}, g * 0.55f, cor);
        DrawTriangle({c.x, c.y - g * 0.85f}, {c.x - g * 0.52f, c.y + g * 0.1f}, {c.x + g * 0.52f, c.y + g * 0.1f}, cor);
        DrawCircleV({c.x - g * 0.18f, c.y + g * 0.15f}, g * 0.12f, comAlfa(WHITE, 0.6f));
        DrawRing(c, r * 0.86f, r, 0, 360, 32, comAlfa(GELO, 0.2f));
        if (e == EstadoPavio::Umido) DrawRing(c, r * 0.86f, r, -90.f, -90.f + 360.f * (1.f - umidade), 32, PERGAMINHO);  // já secou
        break;
    }
    case EstadoPavio::BloqueadoFerro: {  // três elos de corrente: deitado, em pé (fino) e deitado
        const float esp = std::max(1.5f, r * 0.14f);
        DrawRectangleRoundedLinesEx({c.x - r, c.y - r * 0.28f, r * 0.85f, r * 0.56f}, 1.f, 8, esp, FERRO);
        DrawRectangleRoundedLinesEx({c.x + r * 0.15f, c.y - r * 0.28f, r * 0.85f, r * 0.56f}, 1.f, 8, esp, FERRO);
        DrawRectangleRoundedLinesEx({c.x - r * 0.16f, c.y - r * 0.42f, r * 0.32f, r * 0.84f}, 1.f, 8, esp, misturar(FERRO, WHITE, 0.3f));
        break;
    }
    }
}

void cartucho(Rectangle r, const SlotMunicao* s, float brilho) {
    const bool cheio = s && s->cheio;
    const float cx = r.x + r.width * 0.5f, ponta = r.height * 0.28f;  // a bala ocupa o topo
    if (brilho > 0.f) DrawCircleGradient(int(cx), int(r.y + r.height * 0.55f), r.height * 0.75f, comAlfa(BRASA, 0.45f * brilho), comAlfa(BRASA, 0.f));
    const Color latao = cheio ? Color{196, 156, 80, 255} : Color{70, 58, 40, 200};
    const Color chumbo = cheio ? Color{120, 118, 122, 255} : Color{50, 46, 44, 200};
    // projétil: semicírculo + corpo
    const float rb = r.width * 0.36f;
    DrawCircleSector({cx, r.y + rb}, rb, 180, 360, 12, chumbo);
    DrawRectangleRec({cx - rb, r.y + rb, rb * 2, ponta - rb}, chumbo);
    // estojo de latão, com gradiente lateral para parecer cilíndrico
    const Rectangle estojo{r.x + r.width * 0.1f, r.y + ponta, r.width * 0.8f, r.height - ponta - r.height * 0.07f};
    DrawRectangleGradientH(int(estojo.x), int(estojo.y), int(estojo.width * 0.5f), int(estojo.height), misturar(latao, BLACK, 0.3f), latao);
    DrawRectangleGradientH(int(estojo.x + estojo.width * 0.5f), int(estojo.y), int(estojo.width * 0.5f + 1), int(estojo.height), latao,
                           misturar(latao, BLACK, 0.45f));
    DrawRectangleRec({r.x, r.y + r.height - r.height * 0.08f, r.width, r.height * 0.08f}, misturar(latao, BLACK, 0.2f));  // aro
    DrawRectangleLinesEx(estojo, 1.f, comAlfa(BLACK, 0.5f));
    if (!cheio) return;
    // faixa da pasta e entalhe gravado no estojo
    DrawRectangleRec({estojo.x, estojo.y + estojo.height * 0.08f, estojo.width, estojo.height * 0.14f}, corDaPasta(s->circuito.pasta));
    iconeEntalhe(s->circuito.entalhe, {cx, estojo.y + estojo.height * 0.58f}, estojo.width * 0.36f, Color{60, 40, 20, 255});
    if (s->imperfeita()) DrawCircleV({estojo.x + estojo.width - 4, estojo.y + estojo.height - 5}, 3.f, BRASA);  // preparo rápido
}

void losangoAlerta(Vector2 c, float r, Color cor) {
    DrawPoly({c.x + 1, c.y + 2}, 4, r, 0, Color{0, 0, 0, 120});
    DrawPoly(c, 4, r, 0, misturar(cor, BLACK, 0.55f));
    DrawPolyLinesEx(c, 4, r, 0, 2.f, cor);
    // olho
    DrawEllipse(int(c.x), int(c.y), r * 0.55f, r * 0.28f, cor);
    DrawCircleV(c, r * 0.17f, Color{20, 14, 10, 255});
}

void badgeGelo(Vector2 c, float r) {
    DrawCircleV(c, r, Color{20, 40, 48, 220});
    DrawCircleLinesV(c, r, GELO);
    for (int k = 0; k < 3; k++) {  // floco: três traços com pontas
        const float a = float(k) * PI / 3.f;
        const Vector2 p = polar(c, r * 0.65f, a), q = polar(c, r * 0.65f, a + PI);
        DrawLineEx(p, q, 1.6f, GELO);
    }
}

void badgeAtordoado(Vector2 c, float r, float t) {
    DrawCircleV(c, r, Color{48, 40, 16, 220});
    DrawCircleLinesV(c, r, ALERTA);
    for (int k = 0; k < 3; k++) {  // três estrelinhas girando
        const Vector2 p = polar(c, r * 0.5f, t * 4.f + float(k) * 2.f * PI / 3.f);
        DrawPoly(p, 4, r * 0.24f, 0, ALERTA);
        DrawPoly(p, 4, r * 0.24f, 45, comAlfa(ALERTA, 0.6f));
    }
}

// ---------------------------------------------------------------- texto

std::string maiusculas(const std::string& s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); i++) {
        unsigned char ch = (unsigned char)r[i];
        if (ch >= 'a' && ch <= 'z') r[i] = char(ch - 32);
        // latim-1 em UTF-8: 0xC3 seguido de 0xA0..0xBE (minúsculas) -> 0x80..0x9E (exceto ÷)
        else if (ch == 0xC3 && i + 1 < r.size()) {
            unsigned char b = (unsigned char)r[i + 1];
            if (b >= 0xA0 && b <= 0xBE && b != 0xB7) r[i + 1] = char(b - 0x20);
            i++;
        }
    }
    return r;
}

// ---------------------------------------------------------------- banner

void Banner::mostrar(const std::string& t, const std::string& d, float agora) {
    titulo = t;
    detalhe = d;
    inicio = agora;
}

void Banner::desenhar(float agora) const {
    const float t = agora - inicio;
    if (t < 0.f || t > DURACAO) return;
    const float entrada = suave(t / 0.45f), alfa = std::min(entrada, std::clamp((DURACAO - t) / 0.9f, 0.f, 1.f));
    const float W = float(GetScreenWidth()), tam = 24.f;
    const float w = std::max(estilo::largura(Fonte::Titulo, titulo, tam), estilo::largura(Fonte::Texto, detalhe, 18)) + 90.f;
    const Rectangle r = escalar({W * 0.5f - w * 0.5f, 62.f, w, 64.f}, 0.92f + 0.08f * entrada);  // entre a bússola e a mensagem central
    painel(r, alfa, 14.f);
    // losangos de latão nas pontas
    DrawPoly({r.x + 22, r.y + r.height * 0.5f}, 4, 7, 0, comAlfa(BRASA, alfa));
    DrawPoly({r.x + r.width - 22, r.y + r.height * 0.5f}, 4, 7, 0, comAlfa(BRASA, alfa));
    estilo::texto(Fonte::Titulo, titulo, W * 0.5f, r.y + 8.f, tam, comAlfa(Color{240, 200, 120, 255}, alfa), estilo::CENTRO);
    estilo::texto(Fonte::Texto, detalhe, W * 0.5f, r.y + 38.f, 18, comAlfa(PERGAMINHO, alfa), estilo::CENTRO);
}

}  // namespace oeste
