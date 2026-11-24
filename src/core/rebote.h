#pragma once
// Risco de rebote: a falha crítica do circuito dentro da câmara do revólver.
//
// O que acontece ao puxar o gatilho com uma bala gravada na vez (decidirGatilho):
//   - Ferro Frio em volta: o circuito não acende e sai chumbo comum (ApagaNoFerro).
//   - Pavio molhado (dentro da água/chuva): o cão bate e nada sai, nada é gasto (Falha).
//   - Pavio úmido (até tempoSecagem depois de sair da água): o 1º clique falha e avisa; o 2º, dentro
//     de janelaForcar, força o disparo, com chance de rebote maior quanto mais úmido o pavio.
//   - Bala do Preparo Rápido: chanceReboteRapido em todo disparo (soma com a do pavio úmido).
//   - Senão o circuito acende (Acende).
// No rebote (Rebote) a bala e o doador se perdem, o caçador leva danoRebote e a mira dá um tranco
// para cima e treme (TremorMira), sem poder atirar por travaRebote.
//
// Lógica pura: o sorteio vem de fora (o jogo sorteia, os testes fixam). O jogo aplica o resultado em
// game/circuitos_jogo.cpp (Jogo::puxarGatilho e Jogo::sofrerRebote).
#include <cstdint>
#include "core/circuito.h"

enum class Gatilho : uint8_t {
    Comum,        // munição comum (ou slot vazio): o tiro de sempre
    Acende,       // o circuito acende: sai o projétil encantado
    Falha,        // o cão bate e nada sai; nada é gasto
    Rebote,       // falha crítica na câmara: bala e doador perdidos, dano e mira tremida
    ApagaNoFerro  // Ferro Frio: sai chumbo comum (a bala gravada é gasta)
};
const char* nome(Gatilho g);

struct DecisaoGatilho {
    Gatilho gatilho = Gatilho::Comum;
    float chanceRebote = 0;   // a chance que valeu neste disparo (aviso e depuração)
    bool pedeForcar = false;  // Falha por pavio úmido: um 2º clique dentro da janela força
};

// Chance de rebote se a bala for disparada agora (0 se seca e bem preparada). Com o pavio úmido
// conta como se o disparo fosse forçado.
float chanceRebote(const SlotMunicao& bala, const Pavio& pavio, const ParametrosCircuito& p);
// 'bala' = nullptr para a munição comum. 'forcando' = 2º clique dentro da janela do pavio úmido.
// 'sorteio01' em [0, 1): abaixo da chance, rebote.
DecisaoGatilho decidirGatilho(const SlotMunicao* bala, const Pavio& pavio, bool forcando, float sorteio01, const ParametrosCircuito& p);

// Tremor da mira depois de um rebote: um tranco para cima e um balanço que diminui até sumir. O jogo
// soma o deslocamento à câmera a cada quadro (a diferença entre dois quadros), então no fim a mira
// volta para onde estava.
struct TremorMira {
    float duracao = 0, restante = 0;
    float recuo = 0, amplitude = 0;  // rad
    void disparar(float duracao, float recuo, float amplitude);
    void atualizar(float dt);
    bool ativo() const { return restante > 0.f; }
    float forca() const;  // 1 logo depois do tranco, 0 quando passou
    // Deslocamento agora (rad): dPitch > 0 = mira para cima; dYaw > 0 = mira para a direita.
    void deslocamento(float& dYaw, float& dPitch) const;
};
