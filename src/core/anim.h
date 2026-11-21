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
// ---- tiroteio e captura ----
// Braco direito estendido a frente; 'angulo' (rad, > 0 = para cima) levanta o braco; recuo 0..1 levanta o antebraco.
Pose poseMirar(const Esqueleto& e, float angulo, float recuo);
Pose poseRecarregar(const Esqueleto& e, float t);  // t 0..1
void aplicarTranco(Pose& p, float t);               // levou tiro: tronco joga para tras (t 1 -> 0)
Pose poseCair(const Esqueleto& e, float t);         // 0 = em pe, 1 = deitado de costas
Pose poseRendido(const Esqueleto& e, float tempo);  // ajoelhado, maos acima da cabeca
Pose poseAmarrado(const Esqueleto& e, float tempo); // sentado no chao, maos para tras
Pose poseGarupa(const Esqueleto& e);                // dobrado na cintura (atravessado no cavalo); raiz sem ajuste
constexpr float FASE_POR_METRO = 4.2f;                  // passada: ~1,5 m por ciclo

// andadura: 0 parado, 1 passo, 2 trote, 3 galope
Pose poseCavalo(const Esqueleto& e, float fase, int andadura, float tempo);
float alturaDosCascos(const Esqueleto& e, const Pose& p);
