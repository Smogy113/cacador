#pragma once
// Planta da arena de teste dos Circuitos de Queima, perto do ponto de partida do jogo (entrada de
// Pedrabranca, olhando para a ponte). Tudo em metros a partir da largada: 'frente' no olhar inicial
// e 'direita' ao lado. Jogo::pontoDaArena() converte para o mundo, já no chão.
//
//   frente
//     ^         [Sentinela atrás da baia]              Dupla A/B (vai e volta)
//  30 |   corredor de      (31, 15)                       (9..24, 26/27,6)
//     |   ricochete
//  20 |  (4..34, -6)   Ferro Frio (22, 2)      Patrulheiro (laço 2,5..12 × 7..21)
//     |                  bonecos (14, 7)
//  10 |                pista Grade (8,5, 4,5)
//     | chuva (3, -15)   fogueira (2, 3)
//   0 +-----------------largada-------------------------------------> direita
//
// circuitos_jogo.cpp monta a parte dos Circuitos (fogueira, chuva, corredor, bonecos, Ferro Frio);
// combate_reativo.cpp monta o pátio de patrulha (inimigos de teste, baia e pistas).
namespace arena {
constexpr float INICIO_X = -40.f, INICIO_Z = 10.f, INICIO_YAW_GRAUS = 160.f;

struct Ponto {
    float frente, direita;
};
}  // namespace arena
