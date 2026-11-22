#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/collision.h"
#include "core/formas.h"
#include "core/predio.h"
#include "core/places.h"
#include "core/terrain.h"
#include "raylib.h"
#include "render/lighting.h"

struct Quadro {
    Vec3f pos;
    float yaw;
    std::string lugar;
};

struct ArvoreInst {
    Vec3f pos;
    float escala, rot;
    uint8_t tipo;  // 0 pinheiro, 1 carvalho, 2 arbusto, 3 pedra
    float tom;     // variacao de cor 0..1
};

// Tudo que fica parado no mundo: construcoes, arvores, tochas.
// Agrupado em pedacos de 64 m (uma malha por pedaco) para gastar poucas chamadas de desenho.
class Cenario {
public:
    static constexpr float TAM = 64.f;
    void construir(const Mapa& mapa, const Terreno& t, MundoColisao& col, uint32_t semente);
    void atualizar(Vec3f foco, float tempo, int maxConstrucoes = 4);
    // matConstr: construcoes; partes: 1 construcoes, 2 arvores, 3 ambas
    // simples: so as cascas dos predios (Baixo)
    // distDetalhe: ate onde os predios sao desenhados detalhados (alem: casca; 0 = sempre casca)
    void desenhar(const Camera3D& cam, float distMax, Material& mat, Material* matConstr = nullptr, int partes = 3, float distDetalhe = 70.f);
    void desenharArea(Vec3f centro, float raio, Material& mat);  // passe de sombra
    void descarregar();

    std::vector<LuzPontual> luzes;
    std::vector<Quadro> quadros;
    std::vector<std::vector<Vec3f>> rotasNPC;  // pontos de passeio, um grupo por lugar
    std::vector<Vec3f> cavalos;                // onde os cavalos comecam (estabulos)
    size_t triangulos() const { return tris_; }
    int chamadas() const { return chamadas_; }
    size_t totalArvores() const;
    // construcoes como formas exatas e janelas (luz tracada nas paredes)
    const std::vector<FormaCena>& formas() const { return formas_; }
    const std::vector<JanelaLuz>& janelas() const { return janelas_; }
    std::vector<PortaMundo>& portas() { return portas_; }
    const std::vector<InteriorMundo>& interiores() const { return interiores_; }

private:
    struct Pedaco {
        Vec3f centro{};
        float raio = 0;
        Mesh construcoes{}, arvPerto{}, arvLonge{};
        struct Quadrante {  // predios de 32 x 32 m: detalhados perto, casca simples longe (e no Baixo/sombras)
            Vec3f centro{};
            Mesh det{}, casca{};
            bool tem = false;
        };
        Quadrante q[4];
        bool temConstr = false, temPerto = false, temLonge = false, vazioPerto = false;
        float usoPerto = 0;
        std::vector<ArvoreInst> arvores;
    };
    void construirPerto(Pedaco& p);
    std::vector<Pedaco> p_;
    std::vector<FormaCena> formas_;
    std::vector<JanelaLuz> janelas_;
    std::vector<PortaMundo> portas_;
    std::vector<InteriorMundo> interiores_;
    const Terreno* ter_ = nullptr;
    const MundoColisao* col_ = nullptr;
    int n_ = 0;
    float tempo_ = 0;
    size_t tris_ = 0;
    int chamadas_ = 0;
};
