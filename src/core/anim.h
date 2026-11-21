#pragma once
#include "core/body.h"
#include "core/skeleton.h"

// Animacoes procedurais do corpo humano. fase em radianos (um ciclo = dois passos).
Pose poseAndar(const Esqueleto& e, float fase, float velocidade);
Pose poseParado(const Esqueleto& e, float tempo);
Pose poseMontado(const Esqueleto& e, float fase, int andadura);
// Desloca a raiz para o pe mais baixo encostar no chao (y = 0).
void apoiarNoChao(const Esqueleto& e, Pose& p);
float alturaDosPes(const Esqueleto& e, const Pose& p);  // y do ponto mais baixo dos pes
constexpr float FASE_POR_METRO = 4.2f;                  // passada: ~1,5 m por ciclo

// andadura: 0 parado, 1 passo, 2 trote, 3 galope
Pose poseCavalo(const Esqueleto& e, float fase, int andadura, float tempo);
float alturaDosCascos(const Esqueleto& e, const Pose& p);
