#include "raylib.h"
#include <cstdlib>
#include <cstring>
#include <string>

struct Opcoes {
    std::string screenshot;
    int frames = 60;
};

static Opcoes lerArgs(int argc, char** argv) {
    Opcoes o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto prox = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--screenshot") o.screenshot = prox();
        else if (a == "--frames") o.frames = std::atoi(prox());
    }
    return o;
}

int main(int argc, char** argv) {
    Opcoes op = lerArgs(argc, argv);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Caçador");
    SetTargetFPS(60);
    Camera3D cam{};
    cam.position = {4, 3, 6};
    cam.target = {0, 0.5f, 0};
    cam.up = {0, 1, 0};
    cam.fovy = 55;
    cam.projection = CAMERA_PERSPECTIVE;
    int frame = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground({120, 170, 220, 255});
        BeginMode3D(cam);
        DrawCube({0, 0.5f, 0}, 1, 1, 1, {200, 120, 60, 255});
        DrawPlane({0, 0, 0}, {20, 20}, {90, 140, 70, 255});
        EndMode3D();
        EndDrawing();
        frame++;
        if (!op.screenshot.empty() && frame >= op.frames) {
            Image img = LoadImageFromScreen();
            ExportImage(img, op.screenshot.c_str());
            UnloadImage(img);
            break;
        }
    }
    CloseWindow();
    return 0;
}
