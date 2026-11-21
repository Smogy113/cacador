#pragma once
#include "core/math.h"

// Bussola no topo da tela: faixa de largura 'campo' (radianos) centrada para onde a camera olha.
// Angulo de um rumo em relacao a quem olha em 'yaw' (radianos; > 0 = a direita), em -pi..pi.
float anguloRelativo(float yawCamera, float rumo);
// Posicao 0..1 do ponto 'para' na faixa (0,5 = a frente); < 0 ou > 1 = fora da faixa.
float posicaoNaBussola(float yawCamera, Vec3f de, Vec3f para, float campo);
// Angulo relativo do norte (z negativo), para desenhar N/L/S/O.
float rumoDoNorte(float yawCamera);
