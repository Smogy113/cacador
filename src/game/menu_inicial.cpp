#include "game/menu_inicial.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "core/cartaz.h"
#include "render/estilo.h"
#include "rlgl.h"

// O menu é um cartaz de papel pregado sobre a paisagem: papel envelhecido com borda rasgada,
// manchas, prego e carimbo, tudo desenhado por código (sem texturas novas). O conteúdo é
// escrito em "tinta" dentro da folha, em unidades de 0..CARTAZ_LARGURA x 0..CARTAZ_ALTURA,
// e a folha inteira é girada e escalada para a tela (core/cartaz.h).

namespace {
using estilo::Fonte;

// ---------------------------------------------------------------- opções
constexpr const char* PRINCIPAL[] = {"Jogar", "Personagem", "Configurações", "Sair"};
#if defined(PLATFORM_WEB)
constexpr int NUM_PRINCIPAL = 3;  // no navegador não há como sair
#else
constexpr int NUM_PRINCIPAL = 4;
#endif
// Tela Personagem: um item por penteado (core/penteado.h) e "Voltar" no fim.
constexpr int NUM_PERSONAGEM = N_PENTEADOS + 1;
const char* const CABELOS[NUM_PERSONAGEM] = {rotuloPenteado(PENTEADOS[0]), rotuloPenteado(PENTEADOS[1]),
                                             rotuloPenteado(PENTEADOS[2]), "Voltar"};
static_assert(N_PENTEADOS == 3, "Acrescente o novo penteado em CABELOS");
constexpr const char* QUALIDADE[] = {"Baixo", "Médio", "Alto", "Voltar"};
constexpr int NUM_QUALIDADE = 4;

// ---------------------------------------------------------------- folha (unidades do cartaz)
constexpr float W = CARTAZ_LARGURA, H = CARTAZ_ALTURA;
constexpr float Y_ITENS = 352.f, PASSO_ITENS = 70.f, ALTURA_ITEM = 62.f;
constexpr float TAM_ITEM = 44.f;
constexpr float TRACO_DO_CIRCULO = .22f;  // segundos para a pena fechar o círculo na opção em foco

// ---------------------------------------------------------------- cores
constexpr Color PAPEL{226, 209, 172, 255}, PAPEL_CLARO{236, 222, 190, 255}, PAPEL_QUEIMADO{168, 136, 92, 255};
constexpr Color TINTA{44, 31, 22, 255}, TINTA_GASTA{44, 31, 22, 170}, CARIMBO{148, 34, 24, 185};
constexpr Color FERRO{42, 38, 36, 255};

Color comAlfa(Color c, float a) { return estilo::comAlfa(c, a); }

// Ruído determinístico em [0, 1): o mesmo papel em todo quadro.
float ruido(float x) {
    const float s = std::sin(x * 12.9898f + 78.233f) * 43758.5453f;
    return s - std::floor(s);
}
float ruidoSuave(float x) {
    const float i = std::floor(x), f = x - i;
    const float u = f * f * (3.f - 2.f * f);
    return ruido(i) * (1.f - u) + ruido(i + 1.f) * u;
}

// Contorno da folha com a borda rasgada, no sentido anti-horário da tela (o que a raylib preenche):
// desce pela esquerda, atravessa embaixo, sobe pela direita e volta por cima. `recuo` encolhe a
// folha (camadas de papel queimado nas bordas).
std::vector<Vector2> contornoRasgado(float recuo) {
    std::vector<Vector2> p;
    auto rasgo = [&](float t, float forca) { return recuo + forca * (.35f * ruidoSuave(t * .09f) + .65f * ruido(t)); };
    for (float y = 0; y < H; y += 14.f) p.push_back({rasgo(y + 11, 5.f), y});                          // esquerda
    for (float x = 0; x < W; x += 12.f) p.push_back({x, H - rasgo(x + 503, 13.f)});                    // base: mais rasgada
    for (float y = H; y > 0; y -= 14.f) p.push_back({W - rasgo(y + 907, 6.f), y});                     // direita
    for (float x = W; x > 0; x -= 12.f) p.push_back({x, rasgo(x + 1301, 4.f)});                        // topo
    // canto de baixo à direita arrancado
    for (auto& q : p)
        if (q.x + q.y > W + H - 70.f) {
            const float sobra = q.x + q.y - (W + H - 70.f);
            q.x -= sobra * .5f;
            q.y -= sobra * .5f;
        }
    return p;
}

void preencher(const std::vector<Vector2>& contorno, Vector2 deslocamento, Color cor) {
    std::vector<Vector2> leque{{W * .5f + deslocamento.x, H * .5f + deslocamento.y}};
    for (Vector2 q : contorno) leque.push_back({q.x + deslocamento.x, q.y + deslocamento.y});
    leque.push_back(leque[1]);
    DrawTriangleFan(leque.data(), int(leque.size()), cor);
}

void texto(Fonte f, const char* s, float x, float y, float tam, Color cor, int alinha = estilo::CENTRO) {
    estilo::texto(f, s, x, y, tam, cor, alinha, false);
}

// Filete de tipografia antiga: linha grossa, fina por baixo e um losango no meio.
void filete(float y, float meiaLargura) {
    DrawLineEx({W * .5f - meiaLargura, y}, {W * .5f + meiaLargura, y}, 2.5f, TINTA_GASTA);
    DrawLineEx({W * .5f - meiaLargura * .8f, y + 6}, {W * .5f + meiaLargura * .8f, y + 6}, 1.f, TINTA_GASTA);
    DrawPoly({W * .5f, y + 1}, 4, 7.f, 0, PAPEL);
    DrawPoly({W * .5f, y + 1}, 4, 5.f, 0, TINTA);
}

// Círculo à pena em volta da opção: começa em cima à esquerda, passa um pouco do ponto de partida
// (como quem risca rápido) e treme de leve. `progresso` 0..1 desenha o traço aos poucos.
void circuloDeTinta(Vector2 centro, float rx, float ry, float progresso, float semente) {
    constexpr int PONTOS = 64;
    std::vector<Vector2> traco;
    const float voltas = 1.08f, inicio = -2.3f + semente;
    const int n = std::max(2, int(PONTOS * progresso));
    for (int i = 0; i < n; ++i) {
        const float t = float(i) / (PONTOS - 1);
        const float a = inicio + t * voltas * 2.f * 3.14159265f;
        const float tremor = 1.f + .035f * std::sin(a * 3.f + semente * 5.f) + .06f * t;  // espiral leve
        traco.push_back({centro.x + std::cos(a) * rx * tremor, centro.y + std::sin(a) * ry * tremor});
    }
    DrawSplineLinear(traco.data(), int(traco.size()), 4.f, comAlfa(TINTA, .85f));
    for (auto& q : traco) q.y += 1.5f;  // segunda passada da pena, mais fina
    DrawSplineLinear(traco.data(), int(traco.size()), 1.6f, comAlfa(TINTA, .6f));
}

// Tique carimbado na opção que está valendo (penteado e qualidade escolhidos).
void tique(Vector2 c) {
    DrawLineEx({c.x - 13, c.y - 1}, {c.x - 3, c.y + 10}, 5.f, comAlfa(CARIMBO, .95f));
    DrawLineEx({c.x - 3, c.y + 10}, {c.x + 17, c.y - 16}, 5.f, comAlfa(CARIMBO, .95f));
}

void carimbo(Vector2 centro, float angulo) {
    rlPushMatrix();
    rlTranslatef(centro.x, centro.y, 0);
    rlRotatef(angulo, 0, 0, 1);
    const Rectangle fora{-118, -38, 236, 76}, dentro{-110, -30, 220, 60};
    DrawRectangleLinesEx(fora, 3.f, CARIMBO);
    DrawRectangleLinesEx(dentro, 1.5f, CARIMBO);
    texto(Fonte::Forte, "VÁLIDO EM", 0, -27, 23, CARIMBO);
    texto(Fonte::Forte, "TODO O OESTE", 0, -2, 25, CARIMBO);
    // falhas da tinta do carimbo
    for (int i = 0; i < 26; ++i)
        DrawCircleV({-110 + 220 * ruido(i * 3.1f), -32 + 64 * ruido(i * 7.7f)}, 1.f + 2.5f * ruido(i * 1.3f), comAlfa(PAPEL, .8f));
    rlPopMatrix();
}

void prego(Vector2 c) {
    DrawCircleV({c.x + 3, c.y + 4}, 11, comAlfa(BLACK, .35f));  // sombra
    DrawCircleV(c, 10, FERRO);
    DrawCircleV({c.x - 3, c.y - 3}, 4, {110, 104, 98, 255});      // brilho do ferro
    // rugas do papel puxado pelo prego
    for (int i = 0; i < 5; ++i) {
        const float a = .5f + i * 1.2f;
        DrawLineEx({c.x + std::cos(a) * 13, c.y + std::sin(a) * 13}, {c.x + std::cos(a) * 30, c.y + std::sin(a) * 30}, 1.2f,
                   comAlfa(PAPEL_QUEIMADO, .45f));
    }
}

// Papel: sombra, bordas queimadas, miolo claro, manchas de umidade e a dobra do meio.
void folha() {
    preencher(contornoRasgado(0), {14, 18}, comAlfa(BLACK, .28f));
    preencher(contornoRasgado(0), {6, 8}, comAlfa(BLACK, .25f));
    preencher(contornoRasgado(0), {0, 0}, PAPEL_QUEIMADO);
    preencher(contornoRasgado(7), {0, 0}, PAPEL);
    for (int k = 1; k <= 6; ++k)  // o miolo, longe da borda, desbotou menos: clareia em degraus suaves
        DrawEllipse(int(W * .5f), int(H * .47f), W * (.46f - .05f * k), H * (.44f - .05f * k), comAlfa(PAPEL_CLARO, .09f));
    for (int i = 0; i < 7; ++i) {  // manchas de umidade: cada uma é um borrão de várias gotas
        const Vector2 c{60 + (W - 120) * ruido(i * 5.3f + 1), 60 + (H - 120) * ruido(i * 9.1f + 2)};
        const float r = 14 + 34 * ruido(i * 2.7f + 3);
        for (int g = 0; g < 7; ++g) {
            const float a = g * 2.4f + i, d = r * .55f * ruido(i * 13.f + g);
            DrawCircleV({c.x + std::cos(a) * d, c.y + std::sin(a) * d * .7f}, r * (.35f + .4f * ruido(g * 3.3f + i)),
                        comAlfa(PAPEL_QUEIMADO, .05f));
        }
    }
    DrawRing({W * .80f, H * .17f}, 44, 46.5f, 30, 320, 40, comAlfa(PAPEL_QUEIMADO, .30f));  // marca de caneca
    // dobra abaixo das opções: vinco escuro com a luz pegando logo abaixo
    const float dobra = Y_ITENS + 4 * PASSO_ITENS + 18;
    DrawLineEx({8, dobra}, {W - 8, dobra + 3}, 1.5f, comAlfa(PAPEL_QUEIMADO, .55f));
    DrawLineEx({8, dobra + 2}, {W - 8, dobra + 5}, 1.5f, comAlfa(PAPEL_CLARO, .7f));
}

void cabecalho() {
    texto(Fonte::Forte, "A  V  I  S  O", W * .5f, 58, 28, TINTA);
    DrawLineEx({120, 74}, {200, 74}, 2.f, TINTA_GASTA);
    DrawLineEx({W - 200, 74}, {W - 120, 74}, 2.f, TINTA_GASTA);
    float tam = 104.f;
    while (estilo::largura(Fonte::Titulo, "Caçador", tam) > W - 90 && tam > 60) tam -= 4;
    texto(Fonte::Titulo, "Caçador", W * .5f + 2, 102 + 2, tam, comAlfa(TINTA, .25f));  // tinta que borrou
    texto(Fonte::Titulo, "Caçador", W * .5f, 102, tam, TINTA);
    texto(Fonte::Texto, "de recompensas", W * .5f, 218, 36, TINTA);
    filete(268, 210);
}

// Fundo: a foto da entrada, puxada para o sépia, com vinheta e grão de filme.
void fundo(Texture2D foto) {
    const int w = GetScreenWidth(), h = GetScreenHeight();
    ClearBackground({24, 19, 14, 255});
    if (foto.id) {
        const float zoom = std::max(float(w) / foto.width, float(h) / foto.height);
        // Corta o lado esquerdo em janelas estreitas para manter a ponte à vista.
        DrawTexturePro(foto, {foto.width - w / zoom, (foto.height - h / zoom) * .5f, w / zoom, h / zoom},
                       {0, 0, float(w), float(h)}, {0, 0}, 0, {232, 210, 178, 255});
    }
    DrawRectangle(0, 0, w, h, {60, 38, 18, 60});
    const int bw = int(w * .22f), bh = int(h * .22f);
    const Color escuro{14, 10, 6, 190}, nada{14, 10, 6, 0};
    DrawRectangleGradientH(0, 0, bw * 2, h, escuro, nada);  // atrás do cartaz, mais escuro
    DrawRectangleGradientH(w - bw, 0, bw, h, nada, escuro);
    DrawRectangleGradientV(0, 0, w, bh, escuro, nada);
    DrawRectangleGradientV(0, h - bh, w, bh, nada, escuro);
    for (int i = 0; i < 700; ++i) {  // grão
        const int x = GetRandomValue(0, w), y = GetRandomValue(0, h);
        DrawRectangle(x, y, 2, 2, i % 2 ? Color{255, 240, 210, 14} : Color{0, 0, 0, 18});
    }
}
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

void MenuInicial::abrir(const std::string& tela) {
    tela_ = tela == "personagem" ? Tela::Personagem : (tela == "configuracoes" ? Tela::Configuracoes : Tela::Principal);
    focar(0);
}

int MenuInicial::totalDeItens() const {
    return tela_ == Tela::Principal ? NUM_PRINCIPAL : (tela_ == Tela::Personagem ? NUM_PERSONAGEM : NUM_QUALIDADE);
}

Rectangle MenuInicial::item(int indice) const {
    return {110.f, Y_ITENS + indice * PASSO_ITENS, W - 220.f, ALTURA_ITEM};
}

void MenuInicial::focar(int indice) {
    if (indice != foco_) focoDesde_ = GetTime();
    foco_ = indice;
}

MenuInicial::Acao MenuInicial::atualizar(ConfigJogo& config) {
    const int total = totalDeItens();
    auto mudarDeTela = [&](Tela nova, int foco) {
        tela_ = nova;
        foco_ = -1;  // o círculo recomeça na nova folha
        focar(foco);
    };
    auto voltar = [&] { mudarDeTela(Tela::Principal, tela_ == Tela::Personagem ? 1 : 2); };
    if (IsKeyPressed(KEY_ESCAPE) && tela_ != Tela::Principal) {
        voltar();
        return Acao::Nenhuma;
    }
    // O mouse é levado para dentro da folha girada antes de testar as opções.
    const Cartaz cartaz = cartazNaTela(float(GetScreenWidth()), float(GetScreenHeight()), float(GetTime()));
    const Vector2 m = GetMousePosition(), delta = GetMouseDelta();
    const Vec2 naFolha = paraCartaz(cartaz, m.x, m.y);
    const bool clique = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    int acionado = -1;
    if (clique || delta.x != 0 || delta.y != 0) {
        for (int i = 0; i < total; ++i) {
            if (!CheckCollisionPointRec({naFolha.x, naFolha.y}, item(i))) continue;
            focar(i);
            if (clique) acionado = i;
            break;
        }
    }
    if (IsKeyPressed(KEY_UP)) focar((foco_ + total - 1) % total);
    if (IsKeyPressed(KEY_DOWN)) focar((foco_ + 1) % total);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) acionado = foco_;
    if (acionado < 0) return Acao::Nenhuma;

    if (tela_ != Tela::Principal) {
        if (acionado == total - 1) voltar();
        else if (tela_ == Tela::Personagem) config.penteado = PENTEADOS[acionado];
        else config.nivel = static_cast<Nivel>(acionado);
    } else if (acionado == 1) {
        mudarDeTela(Tela::Personagem, static_cast<int>(config.penteado));
    } else if (acionado == 2) {
        mudarDeTela(Tela::Configuracoes, static_cast<int>(config.nivel));
    } else {
        return acionado == 0 ? Acao::Jogar : Acao::Sair;
    }
    return Acao::Nenhuma;
}

void MenuInicial::desenhar(const ConfigJogo& config, bool carregando) const {
    fundo(fundo_);
    const Cartaz c = cartazNaTela(float(GetScreenWidth()), float(GetScreenHeight()), float(GetTime()));
    const float s = escalaDoCartaz(c);

    // Daqui até rlPopMatrix tudo é desenhado em unidades da folha, já girada e escalada.
    rlPushMatrix();
    rlTranslatef(c.cx, c.cy, 0);
    rlRotatef(c.angulo, 0, 0, 1);
    rlScalef(s, s, 1);
    rlTranslatef(-W * .5f, -H * .5f, 0);
    folha();
    prego({W * .5f, 26});
    cabecalho();

    const char* const* opcoes = PRINCIPAL;
    int escolhido = -1;
    const char* chamada = "Procura-se gente sem medo de bala.";
    if (tela_ == Tela::Personagem) {
        opcoes = CABELOS;
        escolhido = static_cast<int>(config.penteado);
        chamada = "Como o caçador usa o cabelo";
    } else if (tela_ == Tela::Configuracoes) {
        opcoes = QUALIDADE;
        escolhido = static_cast<int>(config.nivel);
        chamada = "Qualidade da imagem";
    }
    texto(Fonte::Texto, chamada, W * .5f, 296, 27, TINTA_GASTA);

    if (carregando) {
        texto(Fonte::Forte, "Carregando o mundo...", W * .5f, 430, 40, TINTA);
        texto(Fonte::Texto, "a tinta ainda está secando", W * .5f, 482, 26, TINTA_GASTA);
    } else {
        for (int i = 0; i < totalDeItens(); ++i) {
            const Rectangle r = item(i);
            const float cy = r.y + r.height * .5f;
            const bool voltar = tela_ != Tela::Principal && i == totalDeItens() - 1;
            texto(voltar ? Fonte::Texto : Fonte::Forte, opcoes[i], W * .5f, cy - TAM_ITEM * .58f, voltar ? TAM_ITEM * .78f : TAM_ITEM,
                  foco_ == i ? TINTA : TINTA_GASTA);
            if (i == escolhido) tique({r.x + 18, cy});
            if (foco_ == i) {
                const float progresso = std::min(1.f, float(GetTime() - focoDesde_) / TRACO_DO_CIRCULO);
                const float larguraTexto = estilo::largura(Fonte::Forte, opcoes[i], TAM_ITEM);
                circuloDeTinta({W * .5f, cy + 2}, larguraTexto * .5f + 34, 31, 1.f - (1.f - progresso) * (1.f - progresso), i * .9f);
            }
        }
    }
    carimbo({W - 170, H - 150}, -9.f);
    texto(Fonte::Texto, "Recompensa paga em prata na delegacia de Valdoro", W * .5f, H - 58, 20, TINTA_GASTA);
    rlPopMatrix();

    // Dica de teclas fora do cartaz, discreta.
    const float k = GetScreenHeight() / 720.f;
    estilo::texto(Fonte::Texto, "Setas: escolher   ·   Enter: confirmar   ·   Esc: voltar", GetScreenWidth() - 28 * k,
                  GetScreenHeight() - 44 * k, 20 * k, estilo::MARFIM_ESC, estilo::DIR);
}
