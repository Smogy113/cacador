#pragma once
#include <array>
#include "core/collision.h"
#include "core/motion.h"

class Terreno;

enum class Andadura { Parado, Passo, Trote, Galope };

struct BatidaCasco {
    float intensidade, lado;  // lado -1 à esquerda, +1 à direita
};
struct ContatosCavalo {
    std::array<BatidaCasco, 4> batidas{};
    int total = 0;
};

struct Cavalo {
    Corpo corpo;
    Andadura andadura = Andadura::Parado;
    bool montado = false;
    float passada = 0;  // fase da animacao
};

constexpr float VEL_PASSO = 2.f, VEL_TROTE = 5.f, VEL_GALOPE = 9.f, DIST_MONTAR = 2.5f;
// Na ordem de toque: posterior direita, posterior esquerda, dianteira direita,
// dianteira esquerda. Som e animação usam as mesmas fases, em radianos.
inline constexpr std::array<float, 4> TOQUES_GALOPE{.80f, 1.48f, 2.50f, 3.78f};
constexpr float FASE_GALOPE_POR_METRO = 1.8f;
constexpr float INICIO_GALOPE = (VEL_TROTE + VEL_GALOPE) * .5f;
inline float pesoGalopeCavalo(float velocidade) {
    return smoothstep(INICIO_GALOPE - 1.f, INICIO_GALOPE + 1.f, velocidade);
}

constexpr float ACEL_CAVALO = 4.5f;       // m/s^2 (parado -> galope em ~2 s)
constexpr float INCLINACAO_MAX = 0.70f;  // tan(35 graus)

bool tentarMontar(Cavalo& c, const Corpo& jogador);
void desmontar(Cavalo& c, Corpo& jogador, const MundoColisao& col);
Vec3f resolverCavalo(Vec3f pes, float yaw, const MundoColisao& col, bool montado = false);
void moverCavalo(Cavalo& c, const Entrada& e, float dt, const Terreno& t, const MundoColisao& col);
// O cavalo consegue ir de "de" para "para" sem subir um paredao?
bool inclinacaoPermitida(const Terreno& t, Vec3f de, Vec3f para);

// Batidas cruzadas pela fase da passada entre dois quadros, em ordem de contato.
ContatosCavalo contatosCavalo(Andadura andadura, float faseAnterior, float faseAtual);
