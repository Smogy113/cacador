#include "core/locomocao.h"
#include <algorithm>
#include <cmath>

namespace {
// Suavizacao da velocidade medida (1/s): tira os trancos da colisao sem atrasar a animacao.
constexpr float SUAVIZA_VEL = 10.f;
// Acima disso o deslocamento de um quadro e teletransporte (montar, desmontar, reaparecer), nao passo.
constexpr float VEL_MAX_A_PE = 12.f;
// Mistura parado/andando: abaixo de VEL_PARADO so a pose parada; a partir de VEL_ANDANDO so a de andar.
constexpr float VEL_PARADO = 0.05f, VEL_ANDANDO = 0.5f;
// Inclinacao pela aceleracao (rad por m/s^2) e limites: ~5 graus ao arrancar, ~6 ao frear correndo.
constexpr float GANHO_ACEL = 0.022f, INCL_FRENTE_MAX = 0.14f, INCL_TRAS_MAX = 0.10f;
// Inclinacao na curva: fracao do angulo que a fisica pede (atan(v * giro / g)) e limite (~9 graus).
constexpr float GANHO_CURVA = 0.55f, INCL_LADO_MAX = 0.16f, GRAVIDADE = 9.8f;
// Giro maior que isso num quadro e o yaw sendo posto no lugar (nao o corpo virando).
constexpr float GIRO_MAX_QUADRO = 1.2f;
// Suavizacao das inclinacoes (1/s): o tronco responde em ~0,12 s, como o de uma pessoa.
constexpr float SUAVIZA_INCL = 8.f;
const float PI = 3.14159265f;

float aproximar(float atual, float alvo, float taxa, float dt) { return atual + (alvo - atual) * (1.f - std::exp(-taxa * dt)); }
float diferencaAngular(float de, float para) {
    float d = std::fmod(para - de + PI, 2.f * PI);
    if (d < 0) d += 2.f * PI;
    return d - PI;
}
}  // namespace

void Locomocao::atualizar(float dt, Vec3f deslocamento, float yaw, bool inclinarNaCurva) {
    if (dt <= 0.f) return;
    float dist = std::hypot(deslocamento.x, deslocamento.z);
    float medida = dist / dt;
    if (medida > VEL_MAX_A_PE) {  // teletransporte: nao vira passo nem inclinacao
        dist = 0.f;
        medida = vel;
        temYaw_ = false;
    }
    vel = aproximar(vel, medida, SUAVIZA_VEL, dt);
    // andando para tras (mirando e recuando) as pernas fazem a passada ao contrario
    float sentido = 1.f;
    if (dist > 1e-5f) {
        float frente = (deslocamento.x * std::sin(yaw) + deslocamento.z * std::cos(yaw)) / dist;
        if (frente < -0.3f) sentido = -1.f;
    }
    fase += sentido * dist * fasePorMetro(std::max(vel, 0.05f));
    peso = smoothstep(VEL_PARADO, VEL_ANDANDO, vel);

    // arrancando o tronco vai a frente; freando, para tras
    float acel = (vel - velAnterior_) / dt;
    velAnterior_ = vel;
    float alvoFrente = std::clamp(acel * GANHO_ACEL, -INCL_TRAS_MAX, INCL_FRENTE_MAX);
    inclFrente = aproximar(inclFrente, alvoFrente, SUAVIZA_INCL, dt);

    // na curva o corpo tomba para dentro
    float dif = temYaw_ ? diferencaAngular(yawAnterior_, yaw) : 0.f;
    if (std::abs(dif) > GIRO_MAX_QUADRO) dif = 0.f;
    yawAnterior_ = yaw;
    temYaw_ = true;
    float alvoLado = 0.f;
    if (inclinarNaCurva) {
        float giro = dif / dt;  // rad/s; + = virando para a esquerda
        alvoLado = std::clamp(std::atan(vel * giro / GRAVIDADE) * GANHO_CURVA, -INCL_LADO_MAX, INCL_LADO_MAX);
    }
    inclLado = aproximar(inclLado, alvoLado, SUAVIZA_INCL, dt);
}

int Locomocao::passo() const {
    // poseAndar: o calcanhar esquerdo pisa em fase = pi/2 e o direito meio ciclo depois
    return int(std::floor((fase + PI * 0.5f) / PI));
}

Pose poseLocomocao(const Esqueleto& e, const Locomocao& l, float tempo) {
    Pose p;
    if (l.peso <= 0.001f) {
        p = poseParado(e, tempo);
    } else {
        p = poseAndar(e, l.fase, l.vel);
        if (l.peso < 0.999f) {
            Pose parado = poseParado(e, tempo);
            for (size_t i = 0; i < p.rot.size(); ++i) p.rot[i] = parado.rot[i] + (p.rot[i] - parado.rot[i]) * l.peso;
            p.raiz = parado.raiz + (p.raiz - parado.raiz) * l.peso;
            apoiarNoChao(e, p);
        }
    }
    inclinarTronco(p, l.inclFrente, l.inclLado);
    return p;
}

void inclinarTronco(Pose& p, float frente, float lado) {
    // a coluna leva a maior parte e o peito o resto; a cabeca devolve quase tudo (olhar no horizonte)
    p.rot[COLUNA].x += frente * 0.6f;
    p.rot[PEITO].x += frente * 0.4f;
    p.rot[CABECA].x -= frente * 0.7f;
    // rot.z > 0 tomba para a direita do personagem (-x); lado > 0 e para a esquerda, dai o sinal
    p.rot[COLUNA].z -= lado * 0.6f;
    p.rot[PEITO].z -= lado * 0.4f;
    p.rot[CABECA].z += lado * 0.7f;
}
