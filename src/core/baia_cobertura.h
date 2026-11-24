#pragma once
// Baia de cobertura da arena de teste: uma cobertura baixa na frente de um alvo agachado e duas
// paredes laterais altas, viradas 45° para dentro (um funil aberto para quem atira).
//
// Visto de cima, com o atirador embaixo:
//
//          \   [alvo]   /         <- paredes laterais a 45°, na altura do alvo
//           \ ======== /          <- cobertura baixa (1,3 m): o tiro reto para nela
//                 ^
//              atirador
//
// Um tiro reto na parede lateral, passando ao lado da cobertura, volta de lado na profundidade do
// alvo e o acerta atrás da cobertura. Isso só funciona com a Espiral (voa reto, sem cair) e a Resina
// e Cera (ricocheteia). Lógica pura: as mesmas caixas entram na colisão do jogo e nos testes.
#include "core/collision.h"
#include "core/math.h"

class Terreno;

struct ParametrosBaia {
    float distCobertura = 1.3f;  // m do alvo até o meio da cobertura, para o lado de quem atira
    float meiaLarguraCobertura = 1.4f, meiaEspessuraCobertura = 0.25f, alturaCobertura = 1.3f;
    float afastamentoParede = 3.f;  // m do alvo até o meio de cada parede lateral (de lado)
    float meioComprimentoParede = 1.6f, meiaEspessuraParede = 0.15f, alturaParede = 3.2f;
};

struct BaiaCobertura {
    Vec3f alvo;           // onde fica quem se esconde (pés, no chão)
    float yawFrente = 0;  // direção da baia para o lado de quem atira (0 = +z)
    float afastamentoParede = 3.f;
    CaixaGirada cobertura;
    CaixaGirada paredes[2];  // [0] no lado +lateral, [1] no lado -lateral (ver lateral())
    Vec3f frente() const;    // do alvo para quem atira
    Vec3f lateral() const;   // eixo +x local da baia (perpendicular a frente())
    // Ponto da face interna da parede 'i', na profundidade do alvo e 'altura' acima do chão do alvo:
    // um tiro vindo reto da frente nesse ponto ricocheteia direto no alvo.
    Vec3f miraRicochete(int i, float altura) const;
};

// Monta a baia em volta de 'alvo' (pés no chão), aberta para 'yawFrente'. As caixas ficam assentadas
// no terreno; o jogo as soma a MundoColisao (adicionarGirada) e desenha com primitivas.
BaiaCobertura montarBaia(Vec3f alvo, float yawFrente, const Terreno& t, const ParametrosBaia& p = {});
