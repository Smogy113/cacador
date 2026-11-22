#pragma once
#include <vector>
#include "core/mat4.h"

struct Osso {
    int pai;      // -1 = raiz
    Vec3f inicio;  // junta, em pose de repouso (coordenadas do modelo)
    Vec3f fim;
};

struct Esqueleto {
    std::vector<Osso> ossos;  // pai sempre vem antes do filho
    bool poseA = false;        // repouso com bracos abertos; animacoes compensam quando necessario
};

struct Pose {
    std::vector<Vec3f> rot;  // rotacao local por osso (radianos; aplica Z, depois X, depois Y)
    Vec3f raiz{};            // deslocamento da raiz
};

Pose poseRepouso(const Esqueleto& e);
// Matriz de deformacao de cada osso (leva o vertice da pose de repouso para a pose atual).
void calcularMatrizes(const Esqueleto& e, const Pose& p, std::vector<Mat4>& saida);
