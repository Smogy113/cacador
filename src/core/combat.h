#pragma once
#include <vector>
#include "core/collision.h"
#include "core/math.h"

class Terreno;

// Revolver de 6 tiros: cadencia minima e recarga demorada.
struct Revolver {
    static constexpr int CAPACIDADE = 6;
    static constexpr float CADENCIA = 0.35f, TEMPO_RECARGA = 2.f;
    int balas = CAPACIDADE;
    float recarga = 0, espera = 0;  // recarga > 0: recarregando; espera: falta para o proximo tiro
    bool podeAtirar() const { return balas > 0 && recarga <= 0 && espera <= 0; }
    void atirar();
    void recarregar();
    void atualizar(float dt);
    bool recarregando() const { return recarga > 0; }
};

// Vida que se recupera sozinha depois de um tempo sem levar dano.
struct Vida {
    static constexpr float ATRASO_REGEN = 5.f, REGEN = 12.f;
    float atual = 100, max = 100, semDano = 1e9f;
    void dano(float v);
    bool morto() const { return atual <= 0; }
    float fracao() const { return atual / max; }
    void atualizar(float dt, bool regenera);
    void curar() { atual = max; semDano = 1e9f; }
};

constexpr float DANO_CORPO = 34.f, DANO_BANDIDO = 18.f, LIMIAR_RENDER = 0.35f;
constexpr float RAIO_CORPO = 0.28f, RAIO_CABECA = 0.13f;

enum class Parte { Nada, Corpo, Cabeca };
// Pessoa para o teste de acerto: cilindro do corpo ate 82% da altura e esfera da cabeca a 92%.
struct Alvo {
    Vec3f pes;
    float altura = 1.8f;
};
struct Acerto {
    int indice = -1;
    Parte parte = Parte::Nada;
    float dist = 0;
    Vec3f ponto{};
};
// Raio de 'origem' na direcao 'dir' (normalizada) ate 'alcance'. Para no terreno e nas caixas de colisao;
// devolve o alvo mais proximo atingido antes disso (indice -1 se nenhum).
Acerto lancarTiro(Vec3f origem, Vec3f dir, float alcance, const std::vector<Alvo>& alvos, const Terreno& t,
                  const MundoColisao& col);
// Linha de tiro livre entre a e b: nem caixas nem relevo no caminho.
bool linhaLivre(Vec3f a, Vec3f b, const Terreno& t, const MundoColisao& col);
// Desvio maximo da mira em radianos: menor mirando, maior andando/correndo.
float espalhamento(bool mirando, float velAtirador);
