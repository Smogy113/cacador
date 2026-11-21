#pragma once
#include <memory>
#include <string>
#include <vector>
#include "raylib.h"
#include "core/camrig.h"
#include "core/collision.h"
#include "core/daytime.h"
#include "core/horse.h"
#include "core/motion.h"
#include "core/npc.h"
#include "game/actors.h"
#include "core/places.h"
#include "core/bounty.h"
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
    bool comecarMontado = false;
    float girarCamera = 0;  // graus (so para screenshots)
    bool abrirQuadro = false;
    float distCamera = 0;  // 0 = padrao (so para screenshots)
    float retrato = 0;     // > 0: camera de frente para o rosto do jogador, a essa distancia (screenshots)
    std::string arquivoMapa = "data/lugares.txt";
    std::string arquivoConfig;  // vazio = nao salvar
};

class Jogo {
public:
    explicit Jogo(const OpcoesJogo& op);
    ~Jogo();
    void atualizar(float dt);
    void desenhar();
    void desenharInterface();
    bool querSair() const { return sair_; }
    void teleportar(float x, float z, float yawGraus, float hora);  // benchmark
    size_t triangulos() const { return tris_; }  // terreno + cenario + personagens
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
    ModeloHumano modeloJogador_;
    Populacao populacao_;
    std::vector<ModeloHumano> roupasNpc_;
    std::vector<Cavalo> cavalos_;
    std::vector<ModeloCavaloAnat> modelosCavalo_;
    int montadoEm_ = -1;
    int cavaloPerto_ = -1;
    std::vector<std::vector<Cartaz>> cartazes_;  // um conjunto por quadro
    int quadroPerto_ = -1, quadroAberto_ = -1, cartazSel_ = 0;
    float tempoTotal_ = 0;
    SombraBolha sombra_;
    float passada_ = 0;
    bool pausado_ = false, sair_ = false;
    Entrada lerEntrada();
    void atualizarCamera(float dt, const Entrada& e);
    void desenharAtores(Material& mat, Material& matPele, bool sombraBolha, float distMax);
    void trocarNivel(Nivel n);
    bool atrasDaCamera(Vec3f p) const;
    float tempo_ = 0;
    std::vector<LuzPontual> luzes_;
    size_t tris_ = 0;
    int chamadas_ = 0;
    size_t trisAtores_ = 0;
    bool passePrincipal_ = true;
    int chamadasAtores_ = 0;
};
