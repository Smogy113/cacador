#pragma once
#include <vector>
#include "raylib.h"
#include "core/daytime.h"
#include "core/formas.h"
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
    Material& materialConstrucoes() { return matConstr_; }  // construcoes: com o bloco das paredes
    const Efeitos& efeitos() const { return ef_; }
    Nivel nivel() const { return nivel_; }
    void definirRaioGrama(float r);
    float posSolTela_[2] = {-1.f, -1.f};
    void definirCabelo(Vec3f deslocamentoModelo);
    void definirHeroi(bool sim);
    void definirFoco(float distHeroi);
    // construcoes perto da camera (formas exatas) e janelas, para a luz tracada nas paredes
    void definirFormas(const std::vector<FormaCena>& fs, const std::vector<int>& idx, const std::vector<JanelaLuz>& janelas, Vec3f foco);
    // luz das paredes guardada por face: prepara o atlas uma vez (depois de gerar o cenario) e, a cada
    // quadro, recalcula algumas faces perto da camera (rodizio; faces novas primeiro)
    void prepararLuzParedes(const std::vector<FormaCena>& fs);
    void atualizarLuzParedes(Vec3f camera, int porQuadro = 48);  // camera de cinema: o heroi fica em foco  // o personagem desenhado a seguir e o cacador (luz de personagem)  // fisica do cabelo do personagem desenhado a seguir  // grama densa: some depois desse raio

    // Sombras: desenhe o mundo entre comecarSombra() e terminarSombra() com materialSombra().
    // painel de depuracao (F1): desliga efeitos do nivel atual e ajusta valores na hora
    struct Depuracao {
        bool semSombras = false, semSSAO = false, semBloom = false, semRaios = false, semNuvens = false, semNeblina = false;
        bool semCabecaRaio = false;
        bool semLuzPersonagem = false;  // contraluz/preenchimento e olhos de cinema no heroi
        bool semFoco = false, semGrao = false;
        bool semParedes = false;  // relevo e luz tracada nas paredes das casas  // profundidade de campo; granulacao e aberracao (Alto)  // cabeca do heroi de triangulos (comparar com o raymarching)
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
    // cabeca do heroi por raymarching (Alto e Medio no PC)
    bool usaCabecaRaio() const;
    Material& materialCabeca() { return matCabeca_; }

    // Pos-processamento (brilho e raios de luz): envolve o desenho da cena.
    bool usaPos() const { return (ef_.bloom || ef_.raiosDeLuz || ef_.ssao) && posOk_; }
    const Efeitos& efeitosNivel() const { return ef_; }
    void comecarPos();
    void terminarPos(const Camera3D& cam);

private:
    void uniformesComuns(Shader s, const Paleta& p, Vec3f sol, float noite);
    void liberarEfeitos();
    void assarMateriais();
    RenderTexture2D texMat_[3]{};  // material das paredes: alturas, fatores de cor, pesos
    // luz guardada das paredes
    static constexpr int TAM_ATLAS = 2048;
    static constexpr float METRO_TEXEL = 0.25f;
    const std::vector<FormaCena>* formasLuz_ = nullptr;
    std::vector<std::array<RetAtlas, 4>> atlas_;
    std::vector<std::vector<int>> pertoFace_;  // formas perto de cada forma (calculado uma vez)
    std::vector<uint8_t> feita_;               // por face (forma*4+k): ja calculada desde que entrou no raio
    std::vector<int> fila_;                    // faces no raio, em rodizio
    size_t cursor_ = 0;
    RenderTexture2D luz1_{}, luz2_{};
    Texture2D texFaces_{};
    Shader luzParede_{};
    bool luzParedeOk_ = false;
    void liberarLuzParedes();
    std::vector<Shader> personagens() const;  // pele_ e, se houver, cabeca_
    std::vector<Shader> sombreados() const;   // lit_, pele_ e cabeca_
    Shader lit_{}, ceu_{}, prof_{}, brilho_{}, blur_{}, raios_{}, junta_{}, ssao_{}, blur4_{}, longe_{};
    Shader pele_{}, profPele_{}, cabeca_{}, litParede_{};
    bool paredeOk_ = false;
    bool cabecaOk_ = false;
    Material mat_{}, matProf_{}, matPele_{}, matProfPele_{}, matCabeca_{}, matConstr_{};
    Model esfera_{};
    Texture2D ruidoNuvem_{};  // ruido 3D barato (1 leitura) para as nuvens
    RenderTexture2D sombraPertoRT_{};
    Matrix luzVPPerto_{};
    bool sombraPertoOk_ = false;
    Vec3f dirSombra_{0, 1, 0};
    RenderTexture2D sombraRT_{}, cena_{}, meiaA_{}, meiaB_{}, raiosRT_{}, aoA_{}, aoB_{}, focoA_{}, focoB_{};
    float distFoco_ = 4.f;
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
