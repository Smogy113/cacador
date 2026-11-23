#pragma once
#include "core/anim.h"
#include "core/math.h"

// Liga o deslocamento do personagem a pe a animacao: avanca a fase da passada, mistura parado/andando e
// calcula as inclinacoes do corpo que as pessoas fazem sem pensar:
//  - para a frente ao arrancar e para tras ao frear (o tronco "puxa" o corpo);
//  - para dentro da curva (como quem corre em volta de um poste).
// Uso, a cada quadro: depois de mover o corpo, chame atualizar() com o quanto ele andou; para desenhar,
// use poseLocomocao().
struct Locomocao {
    float fase = 0;        // fase de poseAndar (rad); um ciclo = dois passos
    float vel = 0;         // velocidade usada pela animacao (m/s), medida no deslocamento e suavizada
    float peso = 0;        // mistura: 0 = poseParado, 1 = poseAndar
    float inclFrente = 0;  // rad; + = tronco para a frente (acelerando), - = para tras (freando)
    float inclLado = 0;    // rad; + = para a esquerda do personagem (curva para a esquerda)

    // dt: tempo do quadro; deslocamento: quanto o corpo andou (so x e z contam); yaw: para onde o corpo
    // olha depois de andar; inclinarNaCurva: falso quando o giro nao e do corpo (mirando, o yaw segue a
    // camera e o personagem nao deve tombar a cada puxada do mouse).
    void atualizar(float dt, Vec3f deslocamento, float yaw, bool inclinarNaCurva = true);
    // Numero do passo atual: muda a cada toque de calcanhar (usado para tocar o som da pisada).
    int passo() const;

   private:
    float yawAnterior_ = 0, velAnterior_ = 0;
    bool temYaw_ = false;
};

// Pose do personagem a pe: mistura parado/andando pelo peso e aplica as inclinacoes.
Pose poseLocomocao(const Esqueleto& e, const Locomocao& l, float tempo);
// Inclina o tronco (coluna e peito) e compensa a cabeca para o olhar continuar no horizonte.
// frente: + = para a frente; lado: + = para a esquerda do personagem. Em radianos.
void inclinarTronco(Pose& p, float frente, float lado);
