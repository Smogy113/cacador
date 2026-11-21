#pragma once
#include "core/mat4.h"

// "Pele inversa" do pescoco: leva um ponto da cabeca ja deformada (pose atual, espaco do modelo)
// de volta a pose de repouso, onde a escultura (sdf / volume) esta. A malha mistura os ossos
// cabeca/pescoco pela altura de repouso; aqui a mistura e desfeita por iteracao.
Mat4 inversaRigida(const Mat4& m);  // so rotacao + translacao
// peso do osso da cabeca pela altura de repouso (escultura): o mesmo de head_hd.cpp
float pesoCabeca(float yRepouso);
// escalaY: altura do modelo / altura da escultura (personagem com altura != 1,80 m)
Vec3f paraRepouso(Vec3f p, const Mat4& invCabeca, const Mat4& invPescoco, float escalaY = 1.f);
