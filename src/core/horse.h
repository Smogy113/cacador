#pragma once
#include "core/collision.h"
#include "core/motion.h"

class Terreno;

enum class Andadura { Parado, Passo, Trote, Galope };

struct Cavalo {
    Corpo corpo;
    Andadura andadura = Andadura::Parado;
    bool montado = false;
    float passada = 0;  // fase da animacao
};

constexpr float VEL_PASSO = 2.f, VEL_TROTE = 6.f, VEL_GALOPE = 12.f, DIST_MONTAR = 2.5f, RAIO_CAVALO = 0.9f;
constexpr float ACEL_CAVALO = 6.f;       // m/s^2 (parado -> galope em ~2 s)
constexpr float INCLINACAO_MAX = 0.70f;  // tan(35 graus)

bool tentarMontar(Cavalo& c, const Corpo& jogador);
void desmontar(Cavalo& c, Corpo& jogador, const MundoColisao& col);
void moverCavalo(Cavalo& c, const Entrada& e, float dt, const Terreno& t, const MundoColisao& col);
// O cavalo consegue ir de "de" para "para" sem subir um paredao?
bool inclinacaoPermitida(const Terreno& t, Vec3f de, Vec3f para);
