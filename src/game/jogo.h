#pragma once
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "raylib.h"
#include "core/camrig.h"
#include "core/collision.h"
#include "core/daytime.h"
#include "core/horse.h"
#include "core/locomocao.h"
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
#include "core/rota.h"
#include "render/grama.h"
#include "render/mapa_tela.h"
#include "render/lighting.h"
#include "render/perfil.h"
#include "render/props.h"
#include "render/terrain_mesh.h"

struct OpcoesJogo {
    float hora = 9.f;
    bool temPos = false;
    float x = 0, z = 0, yaw = 0;
    Nivel nivel = Nivel::Baixo;
    bool cabeloOndulado = false;
    bool congelarHora = false;
    bool semEntrada = false;  // modo screenshot
    bool comecarMontado = false;
    float girarCamera = 0;  // graus (so para screenshots)
    bool abrirQuadro = false;
    float distCamera = 0;  // 0 = padrao (so para screenshots)
    float retrato = 0;     // > 0: camera de frente para o rosto do jogador, a essa distancia (screenshots)
    bool estudio = false;  // so o jogador, fundo escuro, luz de estudio
    bool argila = false;   // material cinza (comparar com esculturas)
    bool semCabecaRaio = false;  // cabeca do heroi de triangulos (comparar com o raymarching)
    bool semLuzPersonagem = false;
    bool semFoco = false;
    bool semParedes = false;
    bool portasAbertas = false;  // capturas: todas as portas abertas
    int recompensa = -1;   // aceita o cartaz N do quadro 0 ao iniciar (testes)
    bool tiroteio = false; // comeca perto do esconderijo, com o bando alerta (testes e benchmark)
    bool mirar = false;    // segura a mira (screenshots)
    int andar = 0;         // anda sozinho para a frente: 1 = andando, 2 = correndo (screenshots da marcha)
    bool abrirMapa = false;
    bool abrirDepuracao = false;  // comeca com o painel F1 aberto (screenshots)  // comeca com o mapa grande aberto (screenshots)
    float pitch = -999;    // inclinacao da camera em graus (screenshots; < 0 = olhando para cima)
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
    bool luzParedesPronta_ = false;
    // portas dos predios: a mais perto (E abre/fecha), malha da folha e animacao
    int portaPerto_ = -1;
    Mesh malhaPorta_{};
    void atualizarPortas(float dt, bool interagir);
    void desenharPortas(Material& mat, float raio);  // atlas da luz das paredes (feito no primeiro quadro)
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
    Locomocao loco_;  // passada, mistura parado/andando e inclinacoes do caçador a pé
    bool pausado_ = false, sair_ = false;
    Entrada lerEntrada();
    float distanciaPadraoCamera() const;
    bool emPrimeiraPessoa() const;
    void atualizarCamera(float dt);
    void desenharAtores(Material& mat, Material& matPele, bool sombraBolha, float distMax);
    void trocarNivel(Nivel n);
    bool atrasDaCamera(Vec3f p) const;
    Pose poseDaMontaria(size_t indice) const;
    void desenharJogador(Material& matPele, bool hd, bool primeiraPessoa = false);
    bool revolverEmMaos() const;
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
        float vida, duracao, tam, expansao;
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
    static constexpr float DURACAO_FLASH = 0.035f;
    struct Clarao {
        Vec3f pos, dir;
        float vida;
    };
    Texture2D texturaFumaca_{};
    std::vector<Clarao> claroes_;
    void efeitoTiro(Vec3f pos, Vec3f dir);

    std::vector<Particula> particulas_;
    std::string mensagem_;
    float mensagemT_ = 0;
    std::vector<Vec3f> xerifes_;
    std::vector<float> yawXerifes_;
    int xerifePerto_ = -1;
    std::vector<ModeloHumano> modelosBando_;  // 0 = chefe; 1..3 capangas
    ModeloHumano modeloXerife_;
    uint32_t rngTiro_ = 12345, rngEfeito_ = 18431;
    bool tiroOuvido_ = false;
    Sons sons_;
    PisoSonoro pisoSonoro(Vec3f p) const;
    float panSonoro(Vec3f fonte, float abertura) const;
    void atualizarAmbienteSonoro(float dt, bool pausado = false);
    Grama grama_;
    // mapa na tela e rota ate o objetivo
    std::unique_ptr<Navegador> nav_;
    MapaTela mapaTela_;
    bool mapaAberto_ = false;
    std::vector<Vec3f> rota_;
    Vec3f objetivoRota_{};
    float tempoRota_ = 0;
    void atualizarRota(float dt);
    // painel de depuracao (F1)
    struct Dbg {
        bool aberto = false, pausado = false, invencivel = false, municaoInf = false;
        float velTempo = 1.f, velJogador = 1.f, fov = 60.f, raioGrama = -1.f, densGrama = -1.f;
        float msQuadro = 0;
        float msAtualizar = 0, msDesenharCPU = 0;  // processador: logica e preparo do desenho
    } dbg_;
    PerfilGPU perfil_;
    void desenharDepuracao();
public:
    void imprimirPerfil() const;  // tempos por parte (capturas com --depuracao)
private:
    Vec3f direitaJogador() const { return {-std::cos(jogador_.yaw), 0, std::sin(jogador_.yaw)}; }
    void atualizarCabelo(float dt);
    void configurarGrama();
    int passoAntes_ = 0;
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
