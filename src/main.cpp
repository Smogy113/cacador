#include <algorithm>
#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall MessageBoxA(void* janela, const char* texto, const char* titulo, unsigned int tipo);
#endif
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <string>
#include <vector>
#include "game/jogo.h"
#include "game/menu_inicial.h"
#include "raylib.h"
#include "render/estilo.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

extern "C" const unsigned char* glGetString(unsigned int nome);

struct Opcoes {
    bool qualidadeDada = false;
    bool cabeloDado = false;
    bool benchmark = false;
    bool direto = false;
    std::string screenshot;
    int frames = 0;
    int congelarNoQuadro = 0;  // > 0: a lógica para nesse quadro e a tela continua (capturas no navegador)
    int larguraJanela = 1280, alturaJanela = 720;  // --resolucao L A (prints na resolução de quem joga)
    OpcoesJogo jogo;
};

static Opcoes lerArgs(int argc, char** argv) {
    Opcoes o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto prox = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : "0"; };
        if (a == "--screenshot") { o.screenshot = prox(); o.jogo.congelarHora = true; o.jogo.semEntrada = true; }
        else if (a == "--frames") o.frames = std::max(1, std::atoi(prox()));
        else if (a == "--resolucao") {
            o.larguraJanela = std::max(320, std::atoi(prox()));
            o.alturaJanela = std::max(240, std::atoi(prox()));
        }
        else if (a == "--hora") o.jogo.hora = (float)std::atof(prox());
        else if (a == "--pos") { o.jogo.temPos = true; o.jogo.x = (float)std::atof(prox()); o.jogo.z = (float)std::atof(prox()); }
        else if (a == "--girar-camera") o.jogo.girarCamera = (float)std::atof(prox());
        else if (a == "--benchmark") { o.benchmark = true; o.jogo.semEntrada = true; o.jogo.congelarHora = true; }
        else if (a == "--direto") o.direto = true;
        else if (a == "--estudio") o.jogo.estudio = true;
        else if (a == "--argila") { o.jogo.estudio = true; o.jogo.argila = true; }
        else if (a == "--retrato") o.jogo.retrato = (float)std::atof(prox());
        else if (a == "--dist-camera") o.jogo.distCamera = (float)std::atof(prox());
        else if (a == "--abrir-quadro") o.jogo.abrirQuadro = true;
        else if (a == "--recompensa") o.jogo.recompensa = std::atoi(prox());
        else if (a == "--tiroteio") o.jogo.tiroteio = true;
        else if (a == "--mirar") o.jogo.mirar = true;
        else if (a == "--andar") o.jogo.andar = 1;
        else if (a == "--correr") o.jogo.andar = 2;
        else if (a == "--abrir-mapa") o.jogo.abrirMapa = true;
        else if (a == "--depuracao") o.jogo.abrirDepuracao = true;
        else if (a == "--pitch") o.jogo.pitch = (float)std::atof(prox());
        else if (a == "--mapa") o.jogo.arquivoMapa = prox();
        else if (a == "--montado") o.jogo.comecarMontado = true;
        else if (a == "--sem-raio") o.jogo.semCabecaRaio = true;
        else if (a == "--sem-luz-personagem") o.jogo.semLuzPersonagem = true;
        else if (a == "--sem-foco") o.jogo.semFoco = true;
        else if (a == "--sem-paredes") o.jogo.semParedes = true;
        else if (a == "--portas-abertas") o.jogo.portasAbertas = true;
        else if (a == "--yaw") o.jogo.yaw = (float)std::atof(prox());
        else if (a == "--abrir-fogueira") o.jogo.abrirFogueira = true;
        else if (a == "--roteiro-circuito") o.jogo.roteiroCircuito = std::atoi(prox());  // 1..10, ver OpcoesJogo
        else if (a == "--receitas-todas") o.jogo.receitasTodas = true;
        // entrada simulada sem tirar print: roteiros e capturas na versão web (o mouse e o teclado ficam de fora)
        else if (a == "--sem-entrada") { o.jogo.semEntrada = true; o.jogo.congelarHora = true; }
        else if (a == "--congelar-no-quadro") o.congelarNoQuadro = std::max(1, std::atoi(prox()));
        else if (a == "--cabelo") {
            if (lerPenteado(prox(), o.jogo.penteado)) o.cabeloDado = true;  // sem | ondulado | longo
        } else if (a == "--qualidade") {
            std::string q = prox();
            o.jogo.nivel = q == "alto" ? Nivel::Alto : (q == "medio" ? Nivel::Medio : Nivel::Baixo);
            o.qualidadeDada = true;
        }
    }
    return o;
}

static Opcoes g_op;
static Jogo* g_jogo = nullptr;
static MenuInicial g_menu;
static bool g_carregando = false, g_sair = false;
static int g_frame = 0;

static void quadro() {
    // Capturas e benchmark simulam o mesmo tempo de jogo em qualquer maquina.
    float dt = g_op.jogo.semEntrada ? 1.f / 60.f : GetFrameTime();
    if (dt > 0.1f) dt = 0.1f;
    // --congelar-no-quadro N: depois do quadro N só desenha (o roteiro fica parado na tela para a captura)
    if (g_op.congelarNoQuadro <= 0 || g_frame < g_op.congelarNoQuadro) g_jogo->atualizar(dt);
    BeginDrawing();
    g_jogo->desenhar();
    g_jogo->desenharInterface();
    if (g_op.screenshot.empty()) DrawFPS(10, 10);
    EndDrawing();
    g_frame++;
}

static void iniciarJogo() {
    double t0 = GetTime();
    g_jogo = new Jogo(g_op.jogo);
    std::printf("carregamento=%.2fs\n", GetTime() - t0);
}

static void quadroAplicacao() {
    if (g_jogo) {
        quadro();
        return;
    }
    if (g_carregando) {
        g_menu.descarregar();
        iniciarJogo();
        return;
    }
    ConfigJogo anterior{g_op.jogo.nivel, g_op.jogo.penteado};
    ConfigJogo config = anterior;
    auto acao = g_menu.atualizar(config);
    g_op.jogo.nivel = config.nivel;
    g_op.jogo.penteado = config.penteado;
    if ((anterior.nivel != config.nivel || anterior.penteado != config.penteado)
        && !g_op.jogo.arquivoConfig.empty())
        salvarConfig(g_op.jogo.arquivoConfig, config);
    if (acao == MenuInicial::Acao::Sair) {
        g_sair = true;
        return;
    }
    if (acao == MenuInicial::Acao::Jogar) g_carregando = true;
    BeginDrawing();
    g_menu.desenhar(config, g_carregando);
    EndDrawing();
}

struct Medicao {
    std::vector<double> tempos;
    size_t maxTriangulos = 0, somaTriangulos = 0;
    int maxChamadas = 0;
    size_t somaChamadas = 0;

    void medirQuadro() {
        double inicio = GetTime();
        quadro();
        tempos.push_back((GetTime() - inicio) * 1000.0);
        size_t t = g_jogo->triangulos();
        int c = g_jogo->chamadas();
        maxTriangulos = std::max(maxTriangulos, t);
        maxChamadas = std::max(maxChamadas, c);
        somaTriangulos += t;
        somaChamadas += c;
    }

    void imprimir(const char* cena) {
        std::sort(tempos.begin(), tempos.end());
        size_t n = tempos.size();
        auto percentil = [&](size_t p) { return tempos[(p * n + 99) / 100 - 1]; };
        double media = std::accumulate(tempos.begin(), tempos.end(), 0.0) / n;
        std::printf("%s: quadro ms media=%.2f p50=%.2f p95=%.2f max=%.2f | FPS medio=%.1f\n",
                    cena, media, percentil(50), percentil(95), tempos.back(), 1000.0 / media);
        std::printf("%s: triangulos max=%zu media=%zu | chamadas max=%d media=%zu | quadros=%zu\n",
                    cena, maxTriangulos, somaTriangulos / n, maxChamadas, somaChamadas / n, n);
    }
};

static void rodarBenchmark() {
    const auto* placa = glGetString(0x1F01);  // GL_RENDERER
    std::printf("benchmark: placa=%s | %dx%d | nivel=%s\n", placa ? (const char*)placa : "?", GetRenderWidth(),
                GetRenderHeight(), nomeNivel(g_op.jogo.nivel));
    int porCena = g_op.frames ? g_op.frames : 12;
    auto medirCena = [&](Medicao& medicao) {
        for (int i = 0; i < 3; i++) quadro();  // aquece shaders e o atlas de luz
        for (int i = 0; i < porCena; i++) medicao.medirQuadro();
    };
    Medicao trajeto;
    if (g_op.jogo.temPos || g_op.jogo.tiroteio || g_op.jogo.estudio) {
        medirCena(trajeto);
        trajeto.imprimir("benchmark");
        return;
    }
    // Cidade, vilarejo, ponte, floresta, castelo, acampamento e mina, em quatro horas do dia.
    const float pontos[][3] = {{320, 345, 0}, {320, 345, 90}, {316, 352, 200}, {230, 260, 45}, {-60, 60, 30}, {60, -100, 200},
                               {-420, -230, 0}, {-420, -230, 120}, {-150, -440, 200}, {330, -400, 230}, {-470, 545, 220}, {0, 0, 300}};
    for (float hora : {8.f, 12.f, 18.5f, 22.f})
        for (const auto& ponto : pontos) {
            g_jogo->teleportar(ponto[0], ponto[1], ponto[2], hora);
            medirCena(trajeto);
        }
    trajeto.imprimir("benchmark");

    g_jogo->prepararTiroteio(true);  // alvo e tres capangas
    Medicao tiroteio;
    for (int i = 0; i < 60; i++) tiroteio.medirQuadro();
    tiroteio.imprimir("tiroteio");
}

int main(int argc, char** argv) {
    g_op = lerArgs(argc, argv);
    unsigned int flags = FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE;
    if (!g_op.benchmark) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);
    InitWindow(g_op.larguraJanela, g_op.alturaJanela, "Caçador");
    if (!IsWindowReady()) {
#if defined(_WIN32)
        MessageBoxA(nullptr, "Nao foi possivel abrir a janela do jogo.\n\nO Cacador precisa de uma placa de video com OpenGL 3.3.\nAtualize o driver de video e tente de novo.", "Cacador", 0x10);
#endif
        std::fprintf(stderr, "Nao foi possivel abrir a janela (OpenGL 3.3 necessario)\n");
        return 1;
    }
    if (!g_op.benchmark) SetTargetFPS(60);
    SetExitKey(KEY_NULL);
#if !defined(PLATFORM_WEB)
    // data/ e config.txt ficam ao lado do executavel
    if (!FileExists(g_op.jogo.arquivoMapa.c_str())) ChangeDirectory(GetApplicationDirectory());
#endif  // Esc pausa; sair pelo menu de pausa
    const char* cfg = "config.txt";
    bool temPreferencias = temConfig(cfg);
    ConfigJogo config = lerConfig(cfg, {});
    if (!g_op.cabeloDado) g_op.jogo.penteado = config.penteado;
    if (!g_op.qualidadeDada) {
        const unsigned char* r = glGetString(0x1F01);  // GL_RENDERER
        std::string placa = r ? (const char*)r : "";
        g_op.jogo.nivel = temPreferencias ? config.nivel : sugerirNivel(placa);
        std::printf("placa=%s nivel=%s\n", placa.c_str(), nomeNivel(g_op.jogo.nivel));
    }
    if (!g_op.qualidadeDada && !g_op.cabeloDado) {
        g_op.jogo.arquivoConfig = cfg;
        if (!temPreferencias) salvarConfig(cfg, {g_op.jogo.nivel, g_op.jogo.penteado});
    }
    // O backend de audio, inclusive WebAudio, e inicializado antes do loop.
    if (g_op.screenshot.empty() && !g_op.benchmark) InitAudioDevice();
    if (g_op.direto || g_op.jogo.semEntrada || g_op.jogo.estudio) {
        BeginDrawing();
        ClearBackground({18, 16, 14, 255});
        DrawText("Carregando o mundo...", 40, GetScreenHeight() - 70, 28, {220, 200, 160, 255});
        EndDrawing();
        iniciarJogo();
    } else {
        g_menu.carregar();
    }
    if (g_op.benchmark) {
        rodarBenchmark();
        delete g_jogo;
        CloseWindow();
        return 0;
    }
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(quadroAplicacao, 0, 1);
#else
    while (!WindowShouldClose() && !g_sair && (!g_jogo || !g_jogo->querSair())) {
        quadroAplicacao();
        if (!g_op.screenshot.empty() && g_frame >= (g_op.frames ? g_op.frames : 30)) {
            Image img = LoadImageFromScreen();
            ExportImage(img, g_op.screenshot.c_str());
            g_jogo->imprimirPerfil();
            UnloadImage(img);
            std::printf("triangulos=%zu chamadas=%d\n", g_jogo->triangulos(), g_jogo->chamadas());
            break;
        }
    }
#endif
    delete g_jogo;
    g_menu.descarregar();
    estilo::descarregar();
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
    return 0;
}
