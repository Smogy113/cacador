#include "game/actors.h"
#include <cmath>
#include "core/meshbuilder.h"
#include "raymath.h"
#include "render/meshconv.h"
#include "rlgl.h"

void ModeloPessoa::carregar(const Roupa& r) {
    MeshBuilder c;
    // tronco e casaco
    c.caixa({0, 1.22f, 0}, {0.46f, 0.62f, 0.28f}, 0, r.casaco);
    c.caixa({0, 1.24f, 0.142f}, {0.16f, 0.5f, 0.02f}, 0, r.camisa);  // camisa aparecendo
    if (r.casacoLongo) c.caixa({0, 0.78f, -0.02f}, {0.5f, 0.42f, 0.3f}, 0, r.casaco * 0.92f);  // abas do sobretudo
    c.caixa({0, 0.93f, 0}, {0.48f, 0.08f, 0.3f}, 0, Rgb{0.22f, 0.15f, 0.1f});                  // cinto
    c.caixa({0.12f, 0.9f, 0.16f}, {0.08f, 0.07f, 0.03f}, 0, Rgb{0.75f, 0.62f, 0.3f});         // fivela
    // pescoco, lenco, cabeca
    c.caixa({0, 1.58f, 0}, {0.3f, 0.1f, 0.22f}, 0, r.lenco);
    c.caixa({0, 1.75f, 0.01f}, {0.25f, 0.27f, 0.25f}, 0, r.pele);
    c.caixa({0, 1.68f, 0.13f}, {0.2f, 0.1f, 0.02f}, 0, r.pele * 0.85f);  // barba por fazer
    if (r.chapeuAba) {
        c.cilindro({0, 1.86f, 0}, 0.34f, 0.025f, 10, r.chapeu);
        c.cilindro({0, 1.88f, 0}, 0.16f, 0.15f, 8, r.chapeu * 1.08f);
        c.cilindro({0, 1.9f, 0}, 0.165f, 0.035f, 8, Rgb{0.2f, 0.12f, 0.08f});  // faixa
    } else {
        c.caixa({0, 1.9f, -0.01f}, {0.27f, 0.07f, 0.27f}, 0, r.chapeu);  // cabelo/gorro
    }
    corpo_ = paraMesh(c);
    // perna (pivo no quadril, y=0.92)
    MeshBuilder p;
    p.caixa({0, -0.42f, 0}, {0.17f, 0.82f, 0.2f}, 0, r.calca);
    p.caixa({0, -0.83f, 0.04f}, {0.19f, 0.14f, 0.28f}, 0, Rgb{0.18f, 0.12f, 0.08f});  // bota
    perna_ = paraMesh(p);
    // braco (pivo no ombro)
    MeshBuilder b;
    b.caixa({0, -0.28f, 0}, {0.13f, 0.56f, 0.15f}, 0, r.casaco * 0.95f);
    b.caixa({0, -0.62f, 0}, {0.11f, 0.12f, 0.12f}, 0, r.pele);
    braco_ = paraMesh(b);
    ok_ = true;
}

void ModeloPessoa::descarregar() {
    if (!ok_) return;
    UnloadMesh(corpo_);
    UnloadMesh(perna_);
    UnloadMesh(braco_);
    ok_ = false;
}

void ModeloPessoa::desenhar(Material& mat, Vec3f p, float yaw, float fase, float amp) const {
    Matrix base = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z));
    // leve balanco vertical da passada
    float sobe = std::abs(std::sin(fase)) * 0.04f * amp;
    Matrix corpo = MatrixMultiply(MatrixTranslate(0, sobe, 0), base);
    DrawMesh(corpo_, mat, corpo);
    float giro = std::sin(fase) * 0.7f * amp;
    for (int lado = -1; lado <= 1; lado += 2) {
        Matrix perna = MatrixMultiply(MatrixMultiply(MatrixRotateX(giro * lado), MatrixTranslate(0.11f * lado, 0.92f, 0)), corpo);
        DrawMesh(perna_, mat, perna);
        Matrix braco = MatrixMultiply(MatrixMultiply(MatrixRotateX(-giro * lado * 0.8f), MatrixTranslate(0.3f * lado, 1.5f, 0)), corpo);
        DrawMesh(braco_, mat, braco);
    }
}

void ModeloPessoa::desenharSentado(Material& mat, Vec3f q, float yaw) const {
    Matrix base = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(q.x, q.y - 0.92f, q.z));
    DrawMesh(corpo_, mat, base);
    for (int lado = -1; lado <= 1; lado += 2) {
        Matrix perna = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixRotateX(-1.1f), MatrixRotateZ(0.35f * lado)), MatrixTranslate(0.14f * lado, 0.92f, 0)), base);
        DrawMesh(perna_, mat, perna);
        Matrix braco = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.7f), MatrixTranslate(0.3f * lado, 1.5f, 0)), base);
        DrawMesh(braco_, mat, braco);
    }
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
