#pragma once
#include <vector>
#include "raylib.h"
#include "core/daytime.h"
#include "core/quality.h"

struct LuzPontual {
    Vec3f pos;
    Rgb cor;
    float raio;
    float tremor;  // 0 = luz firme, 1 = fogo tremulando
};

class Iluminacao {
public:
    static constexpr int MAX_LUZES = 8;
    static constexpr float AREA_SOMBRA = 80.f;  // metros cobertos pelo mapa de sombras
    void carregar(Nivel nivel);                 // pode ser chamado de novo para trocar o nivel
    void descarregar();
    // Prepara as luzes do frame: escolhe as 8 mais proximas de "foco".
    void comecarCena(const Camera3D& cam, const DayClock& relogio, const std::vector<LuzPontual>& todas, Vec3f foco, float tempo);
    void desenharCeu(const Camera3D& cam);
    void modoHorizonte(float descartarPerto);  // 0 = normal
    // Estudio (comparar a forma com esculturas): luz fixa, fundo escuro, sem neblina; argila = material cinza
    void modoEstudio(Vec3f dirLuz, bool argila);
    Shader shaderLit() const { return lit_; }
    Material& material() { return mat_; }
    const Efeitos& efeitos() const { return ef_; }
    Nivel nivel() const { return nivel_; }
    void definirRaioGrama(float r);
    float posSolTela_[2] = {-1.f, -1.f};
    void definirCabelo(Vec3f deslocamentoModelo);  // fisica do cabelo do personagem desenhado a seguir  // grama densa: some depois desse raio

    // Sombras: desenhe o mundo entre comecarSombra() e terminarSombra() com materialSombra().
    // painel de depuracao (F1): desliga efeitos do nivel atual e ajusta valores na hora
    struct Depuracao {
        bool semSombras = false, semSSAO = false, semBloom = false, semRaios = false, semNuvens = false, semNeblina = false;
        float passosNuvem = -1;   // -1 = do nivel
        float exposicao = 1.f;    // multiplica a exposicao da paleta
    } dbg;
    bool usaSombras() const { return ef_.sombras && sombraOk_ && !dbg.semSombras; }
    void comecarSombra(Vec3f centro);
    void terminarSombra();
    // Sombra de perto (3 m, alta resolucao) so para o personagem: chapeu no rosto, braco no corpo.
    bool usaSombraPerto() const { return sombraPertoOk_; }
    void comecarSombraPerto(Vec3f centro);
    void terminarSombraPerto();
    Material& materialSombra() { return matProf_; }
    // personagens com esqueleto
    Material& materialPele() { return matPele_; }
    Material& materialSombraPele() { return matProfPele_; }

    // Pos-processamento (brilho e raios de luz): envolve o desenho da cena.
    bool usaPos() const { return (ef_.bloom || ef_.raiosDeLuz || ef_.ssao) && posOk_; }
    const Efeitos& efeitosNivel() const { return ef_; }
    void comecarPos();
    void terminarPos(const Camera3D& cam);

private:
    void uniformesComuns(Shader s, const Paleta& p, Vec3f sol, float noite);
    void liberarEfeitos();
    Shader lit_{}, ceu_{}, prof_{}, brilho_{}, blur_{}, raios_{}, junta_{}, ssao_{};
    Shader pele_{}, profPele_{};
    Material mat_{}, matProf_{}, matPele_{}, matProfPele_{};
    Model esfera_{};
    Texture2D ruidoNuvem_{};  // ruido 3D barato (1 leitura) para as nuvens
    RenderTexture2D sombraPertoRT_{};
    Matrix luzVPPerto_{};
    bool sombraPertoOk_ = false;
    Vec3f dirSombra_{0, 1, 0};
    RenderTexture2D sombraRT_{}, cena_{}, meiaA_{}, meiaB_{}, raiosRT_{}, aoA_{}, aoB_{};
    bool cenaComProfundidade_ = false;
    Matrix luzVP_{};
    bool sombraOk_ = false, posOk_ = false;
    int largPos_ = 0, altPos_ = 0;
    Efeitos ef_{};
    Nivel nivel_ = Nivel::Baixo;
    Paleta paleta_{};
    Vec3f sol_{}, lua_{};
    float noite_ = 0;
    bool carregado_ = false;
};
