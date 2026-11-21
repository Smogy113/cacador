#pragma once
#include "core/collision.h"
#include "core/math.h"

class Terreno;

// Camera em 3a pessoa atras do ombro (matematica pura; a parte da raylib fica em game/camera).
class RigCamera {
public:
    static constexpr float PITCH_MIN = -20.f * 0.0174533f, PITCH_MAX = 60.f * 0.0174533f;
    float yaw = 0, pitch = 0.25f;
    float distAlvo = 4.f;
    float ombroAlvo = 0.45f;  // deslocamento do ombro para a direita (mirando fica maior)
    void atualizar(Vec3f alvoPes, float mouseDx, float mouseDy, const Terreno& t, const MundoColisao& col, float dt);
    Vec3f posicao() const { return pos_; }
    Vec3f olhar() const { return olhar_; }

private:
    Vec3f pos_{}, olhar_{};
    float dist_ = 4.f;
};
