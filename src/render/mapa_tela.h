#pragma once
#include <vector>
#include "core/math.h"
#include "core/places.h"
#include "raylib.h"

class Terreno;
struct EstruturaMapa;

// Mapa na tela: minimapa redondo no canto (gira com a camera) e mapa grande (tecla M),
// pintados a partir do terreno (relevo sombreado, rio, estradas, tom de papel) com a rota.
class MapaTela {
public:
    void carregar(const Terreno& t, const Mapa& m, const std::vector<EstruturaMapa>& estruturas);
    void descarregar();
    static constexpr float RAIO_MINI = 78.f;
    static Vector2 centroMini() { return {RAIO_MINI + 28.f, GetScreenHeight() - RAIO_MINI - 30.f}; }
    void desenharMini(Vec3f jogador, float yawJogador, float yawCamera, const std::vector<Vec3f>& rota, bool temObjetivo, Vec3f objetivo);
    void desenharCheio(Vec3f jogador, float yawJogador, const std::vector<Vec3f>& rota, bool temObjetivo, Vec3f objetivo);

private:
    Texture2D tex_{};
    Shader redondo_{};
    std::vector<Lugar> lugares_;
    float meta_ = 750.f;
};
