#include <algorithm>
#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall MessageBoxA(void* janela, const char* texto, const char* titulo, unsigned int tipo);
#endif
#include <cstdio>
#include <cstdlib>
#include <string>
#include "game/jogo.h"
#include "raylib.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

extern "C" const unsigned char* glGetString(unsigned int nome);

struct Opcoes {
    bool qualidadeDada = false;
    bool benchmark = false;
    std::string screenshot;
    int frames = 30;
    OpcoesJogo jogo;
};

static Opcoes lerArgs(int argc, char** argv) {
    Opcoes o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto prox = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : "0"; };
        if (a == "--screenshot") { o.screenshot = prox(); o.jogo.congelarHora = true; o.jogo.semEntrada = true; }
        else if (a == "--frames") o.frames = std::atoi(prox());
        else if (a == "--hora") o.jogo.hora = (float)std::atof(prox());
        else if (a == "--pos") { o.jogo.temPos = true; o.jogo.x = (float)std::atof(prox()); o.jogo.z = (float)std::atof(prox()); }
        else if (a == "--girar-camera") o.jogo.girarCamera = (float)std::atof(prox());
        else if (a == "--benchmark") { o.benchmark = true; o.jogo.semEntrada = true; o.jogo.congelarHora = true; }
        else if (a == "--estudio") o.jogo.estudio = true;
        else if (a == "--argila") { o.jogo.estudio = true; o.jogo.argila = true; }
        else if (a == "--retrato") o.jogo.retrato = (float)std::atof(prox());
        else if (a == "--dist-camera") o.jogo.distCamera = (float)std::atof(prox());
        else if (a == "--abrir-quadro") o.jogo.abrirQuadro = true;
        else if (a == "--recompensa") o.jogo.recompensa = std::atoi(prox());
        else if (a == "--tiroteio") o.jogo.tiroteio = true;
        else if (a == "--mirar") o.jogo.mirar = true;
        else if (a == "--abrir-mapa") o.jogo.abrirMapa = true;
        else if (a == "--depuracao") o.jogo.abrirDepuracao = true;
        else if (a == "--pitch") o.jogo.pitch = (float)std::atof(prox());
        else if (a == "--mapa") o.jogo.arquivoMapa = prox();
        else if (a == "--montado") o.jogo.comecarMontado = true;
        else if (a == "--yaw") o.jogo.yaw = (float)std::atof(prox());
        else if (a == "--qualidade") {
            std::string q = prox();
            o.jogo.nivel = q == "alto" ? Nivel::Alto : (q == "medio" ? Nivel::Medio : Nivel::Baixo);
            o.qualidadeDada = true;
        }
    }
    return o;
}

static Opcoes g_op;
static Jogo* g_jogo = nullptr;
static int g_frame = 0;

static void quadro() {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.1f;
    g_jogo->atualizar(dt);
    BeginDrawing();
    g_jogo->desenhar();
    g_jogo->desenharInterface();
    if (g_op.screenshot.empty()) DrawFPS(10, 10);
    EndDrawing();
    g_frame++;
}

int main(int argc, char** argv) {
    g_op = lerArgs(argc, argv);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Caçador");
    if (g_op.screenshot.empty() && !g_op.benchmark) InitAudioDevice();  // sons gerados por codigo
    if (!IsWindowReady()) {
#if defined(_WIN32)
        MessageBoxA(nullptr, "Nao foi possivel abrir a janela do jogo.\n\nO Cacador precisa de uma placa de video com OpenGL 3.3.\nAtualize o driver de video e tente de novo.", "Cacador", 0x10);
#endif
        std::fprintf(stderr, "Nao foi possivel abrir a janela (OpenGL 3.3 necessario)\n");
        return 1;
    }
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
#if !defined(PLATFORM_WEB)
    // data/ e config.txt ficam ao lado do executavel
    if (!FileExists(g_op.jogo.arquivoMapa.c_str())) ChangeDirectory(GetApplicationDirectory());
#endif  // Esc pausa; sair pelo menu de pausa
    BeginDrawing();
    ClearBackground({18, 16, 14, 255});
    DrawText("Carregando o mundo...", 40, GetScreenHeight() - 70, 28, {220, 200, 160, 255});
    EndDrawing();
    if (!g_op.qualidadeDada) {
        const char* cfg = "config.txt";
        const unsigned char* r = glGetString(0x1F01);  // GL_RENDERER
        std::string placa = r ? (const char*)r : "";
        g_op.jogo.nivel = temConfig(cfg) ? lerConfig(cfg, Nivel::Baixo) : sugerirNivel(placa);
        g_op.jogo.arquivoConfig = cfg;
        if (!temConfig(cfg)) salvarConfig(cfg, g_op.jogo.nivel);
        std::printf("placa=%s nivel=%s\n", placa.c_str(), nomeNivel(g_op.jogo.nivel));
    }
    double t0 = GetTime();
    g_jogo = new Jogo(g_op.jogo);
    std::printf("carregamento=%.2fs\n", GetTime() - t0);
    if (g_op.benchmark) {
        // trajeto fixo: cidade, vilarejo, ponte, floresta, castelo, acampamento, mina; 4 horas do dia
        const float pontos[][3] = {{320, 345, 0}, {320, 345, 90}, {316, 352, 200}, {230, 260, 45}, {-60, 60, 30}, {60, -100, 200},
                                   {-420, -230, 0}, {-420, -230, 120}, {-150, -440, 200}, {330, -400, 230}, {-470, 545, 220}, {0, 0, 300}};
        const float horas[] = {8, 12, 18.5f, 22};
        size_t maxT = 0, somaT = 0;
        int maxC = 0, somaC = 0, n = 0;
        for (float h : horas)
            for (auto& p : pontos) {
                g_jogo->teleportar(p[0], p[1], p[2], h);
                for (int f = 0; f < 3; f++) quadro();
                size_t t = g_jogo->triangulos();
                int c = g_jogo->chamadas();
                maxT = std::max(maxT, t); maxC = std::max(maxC, c);
                somaT += t; somaC += c; n++;
            }
        // tiroteio: o cartaz mais caro (alvo + 3 capangas atirando, claroes), ~3 s de jogo
        g_jogo->prepararTiroteio(true);
        size_t maxTT = 0;
        int maxCT = 0;
        for (int f = 0; f < 60; f++) {
            quadro();
            maxTT = std::max(maxTT, g_jogo->triangulos());
            maxCT = std::max(maxCT, g_jogo->chamadas());
        }
        maxT = std::max(maxT, maxTT);
        maxC = std::max(maxC, maxCT);
        std::printf("tiroteio: triangulos max=%zu | chamadas max=%d\n", maxTT, maxCT);
        std::printf("benchmark: triangulos max=%zu media=%zu | chamadas max=%d media=%d | quadros=%d\n", maxT, somaT / n, maxC, somaC / n, n);
        delete g_jogo;
        CloseWindow();
        return 0;
    }
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(quadro, 0, 1);
#else
    while (!WindowShouldClose() && !g_jogo->querSair()) {
        quadro();
        if (!g_op.screenshot.empty() && g_frame >= g_op.frames) {
            Image img = LoadImageFromScreen();
            ExportImage(img, g_op.screenshot.c_str());
            UnloadImage(img);
            std::printf("triangulos=%zu chamadas=%d\n", g_jogo->triangulos(), g_jogo->chamadas());
            break;
        }
    }
#endif
    delete g_jogo;
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
    return 0;
}
