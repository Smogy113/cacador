#pragma once
#include <vector>
#include "raylib.h"
#include "core/terrain.h"

// Terreno desenhado em pedacos de 64 m, com 3 niveis de detalhe (passo 2/4/8 m).
// Malhas sao criadas sob demanda e descartadas quando ficam muito tempo sem uso.
class TerrenoRender {
public:
    static constexpr float TAM = 64.f;
    explicit TerrenoRender(const Terreno& t);
    ~TerrenoRender();
    void atualizar(Vec3f foco, float distMax, float tempo, int maxConstrucoes = 6);
    void desenhar(const Camera3D& cam, float distMax, Material& mat);
    // malha do mapa inteiro em baixo detalhe: montanhas e paisagem distante
    void desenharHorizonte(Material& mat);
    size_t triangulosDesenhados() const { return tris_; }
    int chamadasDesenho() const { return chamadas_; }

private:
    struct Pedaco {
        Mesh lod[3]{};
        bool tem[3]{false, false, false};
        float uso[3]{0, 0, 0};
        Mesh agua{};
        bool temAgua = false, aguaFeita = false;
        float hmin = 0, hmax = 0;
    };
    int lodPara(float dist) const;
    void construir(int ci, int cj, int lod);
    void construirAgua(int ci, int cj);
    Vec3f centro(int ci, int cj) const;
    void construirHorizonte();
    Mesh horizonte_{};
    const Terreno& t_;
    int n_;
    std::vector<Pedaco> p_;
    size_t tris_ = 0;
    int chamadas_ = 0;
    float tempo_ = 0;
};
