#pragma once
#include "core/body.h"
#include "core/skeleton.h"

// Animacoes procedurais do corpo humano. fase em radianos (um ciclo = dois passos).
// Andar/correr a partir de dados de marcha humana (ver anim.cpp): pe de apoio travado no chao, bacia
// subindo e descendo como numa pessoa e mistura suave de andar (< 2,2 m/s) para correr (> 4,6 m/s).
// fase = pi/2: o calcanhar esquerdo toca o chao; o direito toca meio ciclo depois.
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
// Quanto a fase avanca por metro andado, casando com o tamanho do passo (o pe de apoio nao escorrega):
// a 1,6 m/s ~0,8 m por passo (~120 passos/min); a 5 m/s ~1,4 m por passo (~215 passos/min). Abaixo de
// 1 m/s os passos encurtam e a cadencia se mantem.
float fasePorMetro(float velocidade);

// andadura: 0 parado, 1 passo, 2 trote, 3 galope
Pose poseCavalo(const Esqueleto& e, float fase, int andadura, float tempo);
float alturaDosCascos(const Esqueleto& e, const Pose& p);
