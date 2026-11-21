#pragma once
#include "core/collision.h"
#include "core/math.h"

class Terreno;

struct Entrada {
    float frente = 0, lado = 0;  // -1..1 (lado > 0 = direita)
    bool correr = false, interagir = false;
    float mouseDx = 0, mouseDy = 0;
};

struct Corpo {
    Vec3f pos;
    float yaw = 0;  // radianos; 0 = olhando para +z
    float vel = 0;  // m/s atual
};

constexpr float VEL_ANDAR = 1.6f, VEL_CORRER = 5.f, RAIO_PESSOA = 0.4f;

float limitarDt(float dt);
// Direcao "para frente" para um yaw (0 = +z).
Vec3f frenteDe(float yaw);
void moverAPe(Corpo& c, const Entrada& e, float yawCamera, float dt, const Terreno& t, const MundoColisao& col);
