#include "game/actors.h"
#include <chrono>
#include <cmath>
#include "core/meshbuilder.h"
#include "raymath.h"
#include "render/meshconv.h"
#include "rlgl.h"

void ModeloHumano::carregar(const Proporcoes& p, const Visual& v) {
    baseHumana_ = v.corpoBase;
    esq_ = v.corpoBase ? esqueletoHumanoBase(p) : esqueletoHumano(p);
    malha_.carregar(corpoHumano(p, v));
    if (!baseHumana_) hd_.carregar(corpoHumanoHD(p, v));
}

void ModeloHumano::carregarHeroi(const Proporcoes& p, const Visual& v) {
    carregar(p, v);
    if (v.corpoBase) return;  // a mesma base compacta atende perto e longe; nao duplica na GPU
    if (v.cacadorDeFronteira) return;  // a silhueta vem da malha animada
#if !defined(PLATFORM_WEB)
    Visual sp = v;
    sp.semPeleCabeca = true;
    sp.ladosExtras = 2.f;
    MalhaPele m = corpoHumanoHD(p, sp);
    trisSemPele_ = int(m.triangulos());
    hdSemPele_.carregar(m);
    escY_ = p.altura / 1.8f;
    escXZ_ = escY_ * p.massa;
    volume_ = std::async(std::launch::async, [v] { return gerarVolume(0.0013f, 0.002f, v); });
    volumePendente_ = true;
#endif
}

void ModeloHumano::descarregar() {
    malha_.descarregar();
    hd_.descarregar();
    hdSemPele_.descarregar();
    if (volumePendente_) volume_.wait();
    volumePendente_ = false;
    cabeca_.descarregar();
}

void ModeloHumano::atualizarCabelo(float dt, Vec3f pos, float yaw, const Pose& pose, Vec3f vento, Vec3f camera) {
    calcularMatrizes(esq_, pose, ossos_);
    Mat4 mundo = Mat4::trans(pos) * Mat4::rotY(yaw);
    Mat4 cabeca = mundo * ossos_[CABECA], pescoco = mundo * ossos_[PESCOCO], peito = mundo * ossos_[PEITO];
    Vec3f distancia = camera - cabeca.ponto(esq_.ossos[CABECA].inicio);
    // De 5 a 12 m o movimento próprio das pontas cede lugar ao dos guias principais.
    float detalhe = 1.f - smoothstep(5.f * 5.f, 12.f * 12.f, dot(distancia, distancia));
    for (auto* malha : {&malha_, &hd_, &hdSemPele_})
        malha->atualizarCabelo(dt, cabeca, pescoco, peito, vento, detalhe);
}

void ModeloHumano::reiniciarCabelo() {
    for (auto* malha : {&malha_, &hd_, &hdSemPele_}) malha->reiniciarCabelo();
}

void ModeloHumano::desenhar(Material& mat, Vec3f p, float yaw, const Pose& pose, bool hd, Material* matCabeca) const {
    calcularMatrizes(esq_, pose, ossos_);
    Matrix modelo = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z));
    if (volumePendente_ && volume_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cabeca_.carregar(volume_.get(), escXZ_, escY_);  // envia os atlas a placa (na thread do desenho)
        volumePendente_ = false;
    }
    if (hd && matCabeca && cabeca_.ok()) {
        hdSemPele_.desenhar(mat, ossos_, modelo);
        cabeca_.desenhar(*matCabeca, ossos_, modelo);
        return;
    }
    (hd && !baseHumana_ ? hd_ : malha_).desenhar(mat, ossos_, modelo);
}

void ModeloCavaloAnat::carregar(const VisualCavalo& v) {
    esq_ = esqueletoCavalo();
    detalhada_.carregar(corpoCavalo(v, true));
    leve_.carregar(corpoCavalo(v, false));
}

void ModeloCavaloAnat::descarregar() {
    detalhada_.descarregar();
    leve_.descarregar();
}

void ModeloCavaloAnat::desenhar(Material& mat, Vec3f p, float yaw, const Pose& pose, bool perto) const {
    calcularMatrizes(esq_, pose, ossos_);
    (perto ? detalhada_ : leve_).desenhar(mat, ossos_, MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z)));
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
