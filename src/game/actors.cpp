#include "game/actors.h"
#include <cmath>
#include "core/meshbuilder.h"
#include "raymath.h"
#include "render/meshconv.h"
#include "rlgl.h"

void ModeloHumano::carregar(const Proporcoes& p, const Visual& v) {
    esq_ = esqueletoHumano(p);
    malha_.carregar(corpoHumano(p, v));
    hd_.carregar(corpoHumanoHD(p, v));
}

void ModeloHumano::descarregar() {
    malha_.descarregar();
    hd_.descarregar();
}

void ModeloHumano::desenhar(Material& mat, Vec3f p, float yaw, const Pose& pose, bool hd) const {
    calcularMatrizes(esq_, pose, ossos_);
    (hd ? hd_ : malha_).desenhar(mat, ossos_, MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z)));
}

void ModeloCavaloAnat::carregar(const VisualCavalo& v) {
    esq_ = esqueletoCavalo();
    malha_.carregar(corpoCavalo(v));
}

void ModeloCavaloAnat::descarregar() { malha_.descarregar(); }

void ModeloCavaloAnat::desenhar(Material& mat, Vec3f p, float yaw, const Pose& pose) const {
    calcularMatrizes(esq_, pose, ossos_);
    malha_.desenhar(mat, ossos_, MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z)));
}

Vec3f ModeloCavaloAnat::assento(Vec3f p, float yaw, const Pose& pose) const {
    std::vector<Mat4> m;
    calcularMatrizes(esq_, pose, m);
    return p + rotY(m[C_CORPO].ponto(assentoCavalo()), yaw);
}

void SombraBolha::carregar() {
    disco_ = LoadModelFromMesh(GenMeshPoly(14, 1.f));
    disco_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {0, 0, 0, 80};
}

void SombraBolha::descarregar() { UnloadModel(disco_); }

void SombraBolha::desenhar(Vec3f p, Vec3f n, float raio) const {
    Vector3 eixo = Vector3CrossProduct({0, 1, 0}, {n.x, n.y, n.z});
    float ang = std::acos(std::fmin(1.f, n.y)) * RAD2DEG;
    if (Vector3Length(eixo) < 1e-4f) eixo = {1, 0, 0};
    rlDisableDepthMask();
    DrawModelEx(disco_, {p.x, p.y + 0.04f, p.z}, eixo, ang, {raio, 1, raio}, WHITE);
    rlEnableDepthMask();
}
