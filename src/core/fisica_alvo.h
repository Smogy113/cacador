#pragma once
// O que um golpe mágico faz no corpo de um alvo, igual para bandidos e bonecos de treino:
// empurrão pelo chão, arremesso para cima (com queda), derrubada e congelamento.
// Lógica pura: só mexe na posição; quem desenha lê o estado (voo, derrubado, congelado...).
#include "core/collision.h"
#include "core/combat.h"
#include "core/math.h"

class Terreno;

// Um golpe num alvo, já medido pela distância ao impacto (ver golpeDoImpacto/golpeDaZona).
struct GolpeAlvo {
    float dano = 0;
    Vec3f empurrao{};    // m/s horizontal
    float levante = 0;   // m/s para cima
    float derrubar = 0;  // s caído no chão
    float congelar = 0;  // s congelado
    float queimar = 0;   // s pegando fogo (visual; o dano já vem em 'dano')
    bool vazio() const {
        return dano <= 0 && empurrao.x == 0 && empurrao.z == 0 && levante <= 0 && derrubar <= 0 && congelar <= 0 && queimar <= 0;
    }
};

struct FisicaAlvo {
    static constexpr float GRAVIDADE = 9.8f;
    static constexpr float QUEDA_SEM_DANO = 7.f;  // m/s de impacto no chão que não machucam
    static constexpr float DANO_POR_MS = 4.f;     // vida por m/s acima disso
    Vec3f empurrao{};
    float voo = 0, vy = 0;  // altura acima do chão e velocidade vertical
    float congelado = 0, derrubado = 0, queimando = 0;

    bool noAr() const { return voo > 0.f || vy > 0.f; }
    // Sem controle próprio (a IA espera): no ar, congelado ou caído.
    bool ocupado() const { return noAr() || congelado > 0.f || derrubado > 0.f; }
    void aplicar(const GolpeAlvo& g);
    // Integra um passo. 'pos' é a posição dos pés no chão (y = terreno); o voo fica à parte.
    // Devolve o dano da queda ao aterrissar.
    float atualizar(float dt, Vec3f& pos, const Terreno& t, const MundoColisao& col, float raio);
};

// Boneco de treino da arena de teste: aguenta golpes, cai, congela e reaparece.
struct AlvoTreino {
    Vec3f base, pos;
    float yaw = 0;
    Vida vida;
    FisicaAlvo fisica;
    float reaparecer = 0;        // > 0: destruído, volta à base quando zerar
    float tempoReaparecer = 4.f; // quanto fica destruído antes de voltar
    float tranco = 0;      // 0..1 sacudida visual do último golpe
    bool ativo() const { return !vida.morto(); }
};
// Devolve true se o golpe destruiu o boneco.
bool golpear(AlvoTreino& a, const GolpeAlvo& g);
void atualizar(AlvoTreino& a, float dt, const Terreno& t, const MundoColisao& col);
