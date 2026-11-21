#pragma once
#include <cstdint>
#include <vector>
#include "core/collision.h"
#include "core/daytime.h"
#include "core/motion.h"

class Terreno;

struct Npc {
    Corpo corpo;
    std::vector<Vec3f> rota;
    int alvo = 0;
    bool ativo = true;
    bool noturno = false;  // continua na rua a noite
    uint32_t semente = 0;
    float espera = 0, acumulado = 0, parado = 0;
    float passada = 0;
};

constexpr float VEL_NPC = 1.4f, DIST_ATUALIZA_SEMPRE = 120.f, DIST_FORA_DE_VISTA = 60.f;

// Moradores andando entre pontos de cada lugar. Um grupo de pontos por lugar.
class Populacao {
public:
    Populacao() = default;
    Populacao(const std::vector<std::vector<Vec3f>>& grupos, uint32_t semente, int quantidade);
    void atualizar(float dt, const DayClock& relogio, const Terreno& t, const MundoColisao& col, Vec3f jogador);
    const std::vector<Npc>& npcs() const { return npcs_; }
    int ativos() const;

private:
    void passo(Npc& n, float dt, bool deveFicar, const Terreno& t, const MundoColisao& col, Vec3f jogador);
    std::vector<Npc> npcs_;
};
