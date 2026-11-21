#include <cstdio>
#include <cstdlib>
#include <string>
#include "game/jogo.h"
#include "raylib.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

struct Opcoes {
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
        else if (a == "--yaw") o.jogo.yaw = (float)std::atof(prox());
        else if (a == "--qualidade") {
            std::string q = prox();
            o.jogo.nivel = q == "alto" ? Nivel::Alto : (q == "medio" ? Nivel::Medio : Nivel::Baixo);
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
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);  // Esc pausa; sair pelo menu de pausa
    g_jogo = new Jogo(g_op.jogo);
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
    CloseWindow();
    return 0;
}
