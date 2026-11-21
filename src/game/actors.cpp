#include "game/actors.h"
#include <cmath>
#include "core/meshbuilder.h"
#include "raymath.h"
#include "render/meshconv.h"
#include "rlgl.h"

void ModeloHumano::carregar(const Proporcoes& p, const Visual& v) {
    esq_ = esqueletoHumano(p);
    malha_.carregar(corpoHumano(p, v));
}

void ModeloHumano::descarregar() { malha_.descarregar(); }

void ModeloHumano::desenhar(Material& mat, Vec3f p, float yaw, const Pose& pose) const {
    calcularMatrizes(esq_, pose, ossos_);
    malha_.desenhar(mat, ossos_, MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z)));
}

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
    MeshBuilder tudo = c;
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
    // versao inteira, parada, para quem esta longe
    for (int lado = -1; lado <= 1; lado += 2) {
        tudo.caixa({0.11f * lado, 0.92f - 0.42f, 0}, {0.17f, 0.82f, 0.2f}, 0, r.calca);
        tudo.caixa({0.11f * lado, 0.92f - 0.83f, 0.04f}, {0.19f, 0.14f, 0.28f}, 0, Rgb{0.18f, 0.12f, 0.08f});
        tudo.caixa({0.3f * lado, 1.5f - 0.28f, 0}, {0.13f, 0.56f, 0.15f}, 0, r.casaco * 0.95f);
    }
    inteiro_ = paraMesh(tudo);
    ok_ = true;
}

void ModeloPessoa::desenharSimples(Material& mat, Vec3f p, float yaw) const {
    DrawMesh(inteiro_, mat, MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z)));
}

void ModeloPessoa::descarregar() {
    if (!ok_) return;
    UnloadMesh(corpo_);
    UnloadMesh(perna_);
    UnloadMesh(braco_);
    UnloadMesh(inteiro_);
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
        Matrix perna = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixRotateX(-1.1f), MatrixRotateZ(0.3f * lado)), MatrixTranslate(0.38f * lado, 0.92f, 0)), base);
        DrawMesh(perna_, mat, perna);
        Matrix braco = MatrixMultiply(MatrixMultiply(MatrixRotateX(-0.7f), MatrixTranslate(0.3f * lado, 1.5f, 0)), base);
        DrawMesh(braco_, mat, braco);
    }
}

void ModeloCavalo::carregar(Rgb pelo, Rgb crina, bool selado) {
    MeshBuilder c;
    c.caixa({0, 1.38f, 0}, {0.72f, 0.72f, 1.85f}, 0, pelo);
    c.caixa({0, 1.42f, 0.88f}, {0.66f, 0.62f, 0.3f}, 0, pelo * 1.04f);  // peito
    c.caixa({0, 1.45f, -0.85f}, {0.68f, 0.6f, 0.3f}, 0, pelo * 0.97f);  // anca
    c.caixa({0, 1.82f, 1.0f}, {0.4f, 0.62f, 0.5f}, 0, pelo);            // pescoco
    c.caixa({0, 2.15f, 1.22f}, {0.36f, 0.5f, 0.42f}, 0, pelo * 1.03f);
    c.caixa({0, 2.22f, 1.55f}, {0.3f, 0.32f, 0.62f}, 0, pelo);           // cabeca
    c.caixa({0, 2.14f, 1.86f}, {0.26f, 0.24f, 0.2f}, 0, pelo * 0.75f);  // focinho
    for (float s : {-1.f, 1.f}) {
        c.caixa({s * 0.1f, 2.46f, 1.36f}, {0.07f, 0.18f, 0.07f}, 0, pelo * 0.9f);  // orelhas
        c.caixa({s * 0.155f, 2.28f, 1.62f}, {0.02f, 0.06f, 0.06f}, 0, Rgb{0.05f, 0.04f, 0.03f});  // olhos
    }
    c.caixa({0, 2.18f, 1.05f}, {0.12f, 0.62f, 0.62f}, 0, crina);         // crina
    c.caixa({0, 2.42f, 1.32f}, {0.1f, 0.12f, 0.3f}, 0, crina);
    c.caixa({0, 1.3f, -1.1f}, {0.16f, 0.8f, 0.2f}, 0, crina);            // cauda
    if (selado) {
        Rgb couro{0.32f, 0.2f, 0.12f};
        c.caixa({0, 1.77f, -0.12f}, {0.8f, 0.06f, 0.85f}, 0, Rgb{0.55f, 0.15f, 0.12f});  // manta
        c.caixa({0, 1.85f, -0.12f}, {0.6f, 0.12f, 0.62f}, 0, couro);
        c.caixa({0, 1.98f, 0.14f}, {0.22f, 0.18f, 0.12f}, 0, couro);         // cabeça da sela
        for (float s : {-1.f, 1.f}) c.caixa({s * 0.4f, 1.35f, -0.1f}, {0.04f, 0.6f, 0.06f}, 0, couro);  // estribos
        c.caixa({0, 2.2f, 1.7f}, {0.34f, 0.06f, 0.06f}, 0, couro);           // redea
    }
    corpo_ = paraMesh(c);
    MeshBuilder p;
    p.caixa({0, -0.5f, 0}, {0.16f, 1.0f, 0.2f}, 0, pelo * 0.95f);
    p.caixa({0, -1.0f, 0.02f}, {0.17f, 0.12f, 0.22f}, 0, Rgb{0.12f, 0.1f, 0.08f});  // casco
    pata_ = paraMesh(p);
    ok_ = true;
}

void ModeloCavalo::descarregar() {
    if (!ok_) return;
    UnloadMesh(corpo_);
    UnloadMesh(pata_);
    ok_ = false;
}

Vec3f ModeloCavalo::assento(Vec3f pos, float yaw) { return pos + rotY({0, 1.95f, -0.15f}, yaw); }

void ModeloCavalo::desenhar(Material& mat, Vec3f p, float yaw, float fase, int and_) const {
    Matrix base = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z));
    float amp = and_ == 0 ? 0.f : (and_ == 1 ? 0.35f : (and_ == 2 ? 0.55f : 0.85f));
    float sobe = and_ >= 2 ? std::abs(std::sin(fase)) * 0.08f * amp : 0.f;
    Matrix corpo = MatrixMultiply(MatrixTranslate(0, sobe, 0), base);
    DrawMesh(corpo_, mat, corpo);
    // fases por pata: diant. esq, diant. dir, tras. esq, tras. dir
    float ph[4];
    if (and_ == 3) { ph[0] = 0; ph[1] = 0.4f; ph[2] = 3.14f; ph[3] = 3.54f; }   // galope
    else if (and_ == 2) { ph[0] = 0; ph[1] = 3.14f; ph[2] = 3.14f; ph[3] = 0; } // trote (diagonais)
    else { ph[0] = 0; ph[1] = 3.14f; ph[2] = 1.57f; ph[3] = 4.71f; }            // passo
    const float px[4] = {0.22f, -0.22f, 0.22f, -0.22f}, pz[4] = {0.75f, 0.75f, -0.75f, -0.75f};
    for (int k = 0; k < 4; k++) {
        float giro = std::sin(fase + ph[k]) * amp;
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixRotateX(giro), MatrixTranslate(px[k], 1.08f, pz[k])), corpo);
        DrawMesh(pata_, mat, m);
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
