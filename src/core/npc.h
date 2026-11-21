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

constexpr float RAIO_EXCLUSAO = 80.f;  // moradores somem perto do esconderijo de um bando
constexpr float VEL_NPC = 1.4f, DIST_ATUALIZA_SEMPRE = 120.f, DIST_FORA_DE_VISTA = 60.f;

// Moradores andando entre pontos de cada lugar. Um grupo de pontos por lugar.
class Populacao {
public:
    Populacao() = default;
    Populacao(const std::vector<std::vector<Vec3f>>& grupos, uint32_t semente, int quantidade);
    void atualizar(float dt, const DayClock& relogio, const Terreno& t, const MundoColisao& col, Vec3f jogador);
    // Moradores cuja rota passa a menos de 'raio' de 'centro' saem de cena (fugiram do bando); raio 0 desliga.
    void definirExclusao(Vec3f centro, float raio) { centroEx_ = centro; raioEx_ = raio; }
    const std::vector<Npc>& npcs() const { return npcs_; }
    int ativos() const;

private:
    void passo(Npc& n, float dt, bool deveFicar, const Terreno& t, const MundoColisao& col, Vec3f jogador);
    bool excluido(const Npc& n) const;
    std::vector<Npc> npcs_;
    Vec3f centroEx_{};
    float raioEx_ = 0;
};
