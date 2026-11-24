#pragma once
#include "core/collision.h"
#include "core/math.h"

class Terreno;

struct Entrada {
    float frente = 0, lado = 0;  // -1..1 (lado > 0 = direita)
    bool correr = false, interagir = false;
    bool mirar = false, atirar = false, recarregar = false, segurarInteragir = false, aceitar = false;
    float mouseDx = 0, mouseDy = 0, rodinha = 0;
};

struct Corpo {
    Vec3f pos;
    float yaw = 0;          // radianos; 0 = olhando para +z
    float vel = 0;          // m/s atual
    Vec3f dir{0, 0, 1};     // direcao do movimento (a pe): continua valendo enquanto o corpo freia
};

constexpr float VEL_ANDAR = 1.6f, VEL_CORRER = 5.f, RAIO_PESSOA = medidas::RAIO_ADULTO;
// Aceleracao a pe (m/s^2). Uma pessoa chega ao passo normal em cerca de um passo e para em meio passo;
// correndo leva mais tempo para embalar e para frear (de 5 m/s ate parar: ~0,6 s e ~1,5 m).
constexpr float ACEL_ANDAR = 4.5f, ACEL_CORRER = 6.f, FREIO_A_PE = 8.f;

float limitarDt(float dt);
// Direcao "para frente" para um yaw (0 = +z).
Vec3f frenteDe(float yaw);
// Move o corpo pelo comando (relativo a camera): acelera e freia em vez de saltar de velocidade, vira o
// corpo para onde anda e, fora da mira, desacelera ao mudar muito de direcao (ninguem da meia-volta em
// velocidade cheia). Mirando, anda de lado e para tras sem girar o corpo.
void moverAPe(Corpo& c, const Entrada& e, float yawCamera, float dt, const Terreno& t, const MundoColisao& col,
              bool orientarPelaCamera = false);
