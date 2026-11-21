#pragma once
#include <memory>
#include <string>
#include <vector>
#include "raylib.h"
#include "core/camrig.h"
#include "core/collision.h"
#include "core/daytime.h"
#include "core/motion.h"
#include "game/actors.h"
#include "core/places.h"
#include "core/quality.h"
#include "core/terrain.h"
#include "render/lighting.h"
#include "render/props.h"
#include "render/terrain_mesh.h"

struct OpcoesJogo {
    float hora = 9.f;
    bool temPos = false;
    float x = 0, z = 0, yaw = 0;
    Nivel nivel = Nivel::Baixo;
    bool congelarHora = false;
    bool semEntrada = false;  // modo screenshot
    std::string arquivoMapa = "data/lugares.txt";
};

class Jogo {
public:
    explicit Jogo(const OpcoesJogo& op);
    ~Jogo();
    void atualizar(float dt);
    void desenhar();
    void desenharInterface();
    bool querSair() const { return sair_; }
    size_t triangulos() const { return tris_; }
    int chamadas() const { return chamadas_; }

private:
    OpcoesJogo op_;
    Mapa mapa_;
    std::unique_ptr<Terreno> terreno_;
    std::unique_ptr<TerrenoRender> terrenoR_;
    Cenario cenario_;
    Iluminacao luz_;
    DayClock relogio_;
    Camera3D cam_{};
    MundoColisao col_;
    Corpo jogador_{};
    RigCamera rig_;
    ModeloPessoa modeloJogador_;
    SombraBolha sombra_;
    float passada_ = 0;
    bool pausado_ = false, sair_ = false;
    Entrada lerEntrada();
    void atualizarCamera(float dt, const Entrada& e);
    float tempo_ = 0;
    std::vector<LuzPontual> luzes_;
    size_t tris_ = 0;
    int chamadas_ = 0;
};
