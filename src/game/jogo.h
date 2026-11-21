#pragma once
#include <memory>
#include <string>
#include <vector>
#include "raylib.h"
#include "core/daytime.h"
#include "core/places.h"
#include "core/quality.h"
#include "core/terrain.h"
#include "render/lighting.h"
#include "render/terrain_mesh.h"

struct OpcoesJogo {
    float hora = 9.f;
    bool temPos = false;
    float x = 0, z = 0, yaw = 0;
    Nivel nivel = Nivel::Baixo;
    bool congelarHora = false;
    std::string arquivoMapa = "data/lugares.txt";
};

class Jogo {
public:
    explicit Jogo(const OpcoesJogo& op);
    ~Jogo();
    void atualizar(float dt);
    void desenhar();
    size_t triangulos() const { return tris_; }
    int chamadas() const { return chamadas_; }

private:
    OpcoesJogo op_;
    Mapa mapa_;
    std::unique_ptr<Terreno> terreno_;
    std::unique_ptr<TerrenoRender> terrenoR_;
    Iluminacao luz_;
    DayClock relogio_;
    Camera3D cam_{};
    float tempo_ = 0;
    std::vector<LuzPontual> luzes_;
    size_t tris_ = 0;
    int chamadas_ = 0;
};
