#pragma once
#include <cstdint>
#include <vector>
#include "core/bounty.h"
#include "core/combat.h"
#include "core/fisica_alvo.h"
#include "core/motion.h"

class Terreno;

enum class EstadoBandido { Vigiando, Alerta, Combate, Fugindo, Rendido, Amarrado, Morto, Sumiu };

struct Bandido {
    Corpo corpo;
    Vida vida;
    Revolver arma;
    EstadoBandido estado = EstadoBandido::Vigiando;
    bool chefe = false;
    Temperamento temperamento = Temperamento::Teimoso;
    float reacao = 0.6f, tranco = 0, tempoMorto = 0, tempoEstado = 0;
    Vec3f cobertura{};
    bool temCobertura = false;
    float tempoCobertura = 0, proxTiro = 0, lado = 1, tempoLado = 0, semLinha = 0;
    int rajada = 0;  // tiros que faltam na rajada atual (> 0 = atirando)
    uint32_t semente = 1;
    FisicaAlvo fisica;  // golpes dos Circuitos de Queima: empurrado, no ar, caido ou congelado
};

struct EventosBando {
    int tirosNoJogador = 0;
    std::vector<Vec3f> claroes;  // de onde saiu cada tiro (clarao do cano)
    bool chefeRendeu = false, chefeMorreu = false;
};

constexpr float VISAO_BANDIDO = 35.f, AUDICAO_BANDIDO = 80.f, REACAO_BANDIDO = 0.6f;
constexpr float DIST_COMBATE_MIN = 10.f, DIST_COMBATE_MAX = 25.f, DIST_RENDE_FUJAO = 4.f, DIST_SOME = 120.f;
constexpr float VEL_FUJAO = 3.5f, VEL_CAPANGA_FUGA = 4.5f, VEL_COMBATE = 2.5f;
constexpr int MAX_ATIRANDO = 2;

// Chance de um tiro de bandido acertar o jogador: cai com a distancia e com a velocidade do jogador.
float chanceAcerto(float dist, float velJogador);

// O procurado (chefe) e seus capangas num esconderijo.
class Bando {
public:
    Bando() = default;
    Bando(Vec3f esconderijo, Temperamento t, int capangas, uint32_t semente, const Terreno& ter, const MundoColisao& col);
    EventosBando atualizar(float dt, Vec3f jogador, float velJogador, bool jogadorMontado, bool tiroOuvido,
                           const Terreno& ter, const MundoColisao& col);
    // O jogador acertou o bandido 'indice'. Retorna true se matou.
    bool levarTiro(int indice, Parte parte);
    // Golpe de um circuito (dano, empurrao, arremesso, derrubada, gelo). Retorna true se matou.
    // Quem esta no ar, caido ou congelado nao anda nem atira ate se recuperar.
    bool aplicarGolpe(int indice, const GolpeAlvo& g);
    // Um alvo por bandido, no mesmo indice; quem nao pode levar tiro fica fora do alcance.
    std::vector<Alvo> alvos() const;
    std::vector<Bandido>& bandidos() { return b_; }
    const std::vector<Bandido>& bandidos() const { return b_; }
    Bandido* chefe();
    const Bandido* chefe() const;
    void curarTodos();  // morte do jogador: vivos com vida cheia e vigiando; mortos, rendidos e amarrados ficam
    bool vazio() const { return b_.empty(); }
    int atirando() const;

private:
    bool levarDano(int indice, float dano);  // reage como a um tiro no corpo; true se matou
    void alertarTodos();
    std::vector<Bandido> b_;
    bool pendRendeu_ = false, pendMorreu_ = false;
};
