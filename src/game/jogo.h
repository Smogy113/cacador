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
#include "game/sons.h"
#include "core/places.h"
#include "core/bounty.h"
#include "core/cacada.h"
#include "core/combat.h"
#include "core/quality.h"
#include "core/terrain.h"
#include "render/grama.h"
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
    bool estudio = false;  // so o jogador, fundo escuro, luz de estudio
    bool argila = false;   // material cinza (comparar com esculturas)
    int recompensa = -1;   // aceita o cartaz N do quadro 0 ao iniciar (testes)
    bool tiroteio = false; // comeca perto do esconderijo, com o bando alerta (testes e benchmark)
    bool mirar = false;    // segura a mira (screenshots)
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
    // jogador a ~22 m do esconderijo da recompensa ativa; bandoGrande = aceita o cartaz mais caro (3 capangas)
    void prepararTiroteio(bool bandoGrande = false);
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
    void desenharJogador(Material& matPele, bool hd);
    float tempo_ = 0;
    std::vector<LuzPontual> luzes_;
    size_t tris_ = 0;
    int chamadas_ = 0;
    size_t trisAtores_ = 0;
    bool passePrincipal_ = true;
    int chamadasAtores_ = 0;

    // ---- recompensas e tiroteio (game/combate.cpp) ----
    struct Particula {
        Vec3f pos, vel;
        float vida, tam;
        Rgb cor;
    };
    Cacada cacada_;
    Revolver arma_;
    Vida vida_;
    int dinheiro_ = 0;
    bool mirando_ = false;
    float mira_ = 0, anguloMira_ = 0, tiroT_ = 0;
    float marcaAcerto_ = 0;
    bool marcaMatou_ = false;
    float amarrarT_ = 0, morteT_ = -1;
    float flashT_ = 0;
    Vec3f flashPos_{};
    std::vector<Particula> particulas_;
    std::string mensagem_;
    float mensagemT_ = 0;
    std::vector<Vec3f> xerifes_;
    std::vector<float> yawXerifes_;
    int xerifePerto_ = -1;
    std::vector<ModeloHumano> modelosBando_;  // 0 = chefe; 1..3 capangas
    ModeloHumano modeloXerife_;
    uint32_t rngTiro_ = 12345;
    bool tiroOuvido_ = false;
    Sons sons_;
    Grama grama_;
    void configurarGrama();
    int passoAntes_ = 0, cascoAntes_ = 0;
    EstadoBandido estadoChefeAntes_ = EstadoBandido::Vigiando;
    void iniciarCombate();
    void descarregarCombate();
    void atualizarCombate(float dt, Entrada& e);
    bool interagirCacada(Entrada& e);   // consome o E quando ele serve a cacada
    bool entregarNoXerife();
    void aoMontar();
    void aoDesmontar();
    void aceitarCartaz(int quadro, int indice);
    void avisar(const std::string& texto);
    void desenharCombatentes(Material& matPele, bool sombraBolha, float distMax);
    void desenharEfeitos();
    Pose poseDoJogador(const Esqueleto& e) const;
    Vec3f canoDoJogador() const;
};
