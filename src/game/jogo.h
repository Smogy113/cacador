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
#include "game/arena_teste.h"
#include "game/sons.h"
#include "core/places.h"
#include "core/bounty.h"
#include "core/cacada.h"
#include "core/baia_cobertura.h"
#include "core/circuito.h"
#include "core/combat.h"
#include "core/inimigo_teste.h"
#include "core/projetil_encantado.h"
#include "core/quality.h"
#include "core/rebote.h"
#include "core/receitas.h"
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
    // screenshots dos Circuitos de Queima: 1 Espiral+Resina (ricochete), 2 Grade+Enxofre (onda de
    // pressao), 3 Onda+Prata (zona de gelo), 4 pavio molhado (falha), 5 atravessando o Ferro Frio.
    // Combate reativo: 6 Grade+Enxofre atordoa a Dupla, 7 Onda+Prata deixa o Patrulheiro lento,
    // 8 Espiral+Resina pega o Sentinela atras da cobertura, 9 rebote do preparo rapido,
    // 10 o Patrulheiro percebe a capsula no chao e vai investigar
    int roteiroCircuito = 0;
    bool abrirFogueira = false;  // comeca com o painel da fogueira aberto (screenshots)
    bool receitasTodas = false;  // comeca sabendo todos os entalhes e pastas (testes)
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

    // ---- Circuitos de Queima (regra em core/circuito, core/projetil_encantado e core/fisica_alvo;
    //      jogo em game/circuitos_jogo.cpp). Tudo desenhado com primitivas e sprites provisorios. ----
    // Arena de teste perto do inicio: fogueira (gravar balas), corredor de paredes (ricochete),
    // bonecos de treino, zona de chuva (pavio molhado) e uma barreira de Ferro Frio.
    struct ArenaCircuitos {
        bool pronta = false;
        Vec3f fogueira{}, chuva{};
        float raioChuva = 4.f, alturaChuva = 14.f;
        std::vector<CaixaGirada> paredes;  // corredor de ricochete (tambem na colisao)
        AABB ferro{};                      // barreira de Ferro Frio (volume atravessavel)
    };
    // Painel da fogueira: monta [Entalhe] + [Pasta] + [Doador] e grava num dos slots.
    // O Preparo Cuidadoso leva tempoPreparoCuidadoso: a barra enche com o painel aberto (E/Esc cancela).
    struct PainelFogueira {
        static constexpr int LINHAS = 5;
        bool aberto = false;
        int linha = 0, slot = 0;  // linha: 0 slot, 1 entalhe, 2 pasta, 3 doador, 4 preparo
        Circuito circuito;
        Preparo preparo = Preparo::Cuidadoso;
        float gravando = 0;  // > 0: gravacao cuidadosa em andamento (s que faltam)
        std::string retorno;
        float retornoT = 0;
    };
    struct Faisca {  // particula aditiva ou de fumaca dos efeitos provisorios
        Vec3f pos, vel;
        float vida, duracao, tam, gravidade;
        Rgb cor;
        bool aditiva;
    };
    struct OndaVisual {  // anel de pressao (Grade) ou coluna de levante (Grade invertida)
        Vec3f centro;
        float raio, idade, duracao;
        Rgb cor;
        bool sobe;
    };
    // O que o desenho provisorio junta para o fim do passe: billboards (fumaca e brilho aditivo) e
    // volumes translucidos (gelo, Ferro Frio), desenhados de tras para a frente sem gravar profundidade.
    struct SpriteEfeito {
        Vec3f pos;
        float tam;
        Color cor;
    };
    struct CaixaVidro {
        Vec3f centro, tam;
        Color cor;
    };
    struct Transparencias {
        std::vector<SpriteEfeito> fumaca, brilho;
        std::vector<CaixaVidro> vidros;
    };
    // Marcas de estado sobre qualquer alvo (bandido, boneco ou inimigo de teste): estrelas do
    // atordoamento, geada da lentidao, bloco de gelo e fogo. 'altura' = altura do corpo de pe.
    void desenharEstadoDoAlvo(Vec3f pes, float altura, const FisicaAlvo& f, Transparencias& tr) const;
    CircuitosNoMundo circuitos_;
    Arsenal arsenal_{circuitos_.p};  // depois de circuitos_: nasce dos mesmos parametros
    Pavio pavio_;
    std::vector<AlvoTreino> bonecos_;
    ArenaCircuitos arena_;
    PainelFogueira painel_;
    std::vector<Faisca> faiscas_;
    std::vector<OndaVisual> ondas_;
    bool pertoFogueira_ = false;
    int capsulaPerto_ = -1, quadroRoteiro_ = 0;
    void iniciarCircuitos();
    Vec3f pontoChao(Vec3f p) const;  // mesmo x,z na altura do terreno
    bool naAgua(Vec3f p) const;      // rio do terreno ou zona de chuva da arena
    bool interagirCircuitos(Entrada& e);  // E: abre a fogueira ou recolhe a capsula perto
    void atualizarPainelFogueira(float dt);
    void roteiroCircuitos(Entrada& e);
    // Gatilho (regra em core/rebote): decide o que a bala na vez faz e ja resolve Falha (o cao bate,
    // nada sai) e Rebote (sofrerRebote). Comum/Acende/ApagaNoFerro seguem para o disparo.
    DecisaoGatilho puxarGatilho(Vec3f boca);
    void sofrerRebote(Vec3f boca);
    void soltarCapsulaMarcada(const Circuito& c);  // o vestigio de toda bala gravada gasta
    void aplicarTremorMira(float dt);  // soma o tremor do rebote a camera (a mira volta no fim)
    // true = saiu um projetil encantado (o dano vem no impacto); false = tiro comum (raio instantaneo).
    bool dispararCircuito(Vec3f boca, Vec3f dir, Gatilho gatilho);
    // bandidos primeiro (mesmo indice do bando), depois os bonecos, depois os inimigos de teste
    std::vector<Alvo> alvosCircuito() const;
    // Golpe num alvo da arena pelo indice em alvosCircuito() menos os bandidos: bonecos e depois os
    // inimigos de teste. true se derrotou.
    bool golpearAlvoDaArena(int indice, const GolpeAlvo& g);
    Vec3f pontoDaArena(arena::Ponto q) const;  // largada + frente/direita, no chao
    void aplicarImpacto(const Impacto& im);
    void atualizarCircuitos(float dt, const Entrada& e);
    void soltarFaiscas(Vec3f p, int n, Rgb cor, float vel, float vida, float tam, bool aditiva, float gravidade, Vec3f base = {});
    void desenharCircuitos();      // 3D, no passe de efeitos
    void desenharHudCircuitos();   // 2D: slots, pavio e combinacao ativa
    void desenharPainelFogueira();

    // ---- Combate reativo (regra em core/inimigo_teste, core/receitas, core/rebote e
    //      core/baia_cobertura; jogo em game/combate_reativo.cpp). Primitivas provisorias, sem o
    //      modelo final do personagem. ----
    // Patio de patrulha ao lado da arena: inimigos de teste que patrulham, percebem as capsulas
    // deixadas no chao e reagem aos golpes; pistas que ensinam entalhes e pastas; uma baia com
    // cobertura baixa e paredes laterais para o ricochete.
    ParametrosInimigo paramInimigo_;
    std::vector<InimigoTeste> inimigos_;
    std::vector<PistaEntalhada> pistas_;
    ReceitasConhecidas receitas_;  // o que a fogueira deixa gravar (comeca com Espiral + Polvora)
    BaiaCobertura baia_;
    bool baiaPronta_ = false;
    int pistaPerto_ = -1;
    bool depuracaoCombate_ = true;  // F3: rotulos sobre os inimigos e quadro de depuracao
    // rebote (core/rebote)
    TremorMira tremor_;
    float janelaForcar_ = 0;    // > 0: o proximo clique forca o pavio umido
    float sorteioRebote_ = -1;  // >= 0: sorteio fixo do rebote (capturas); < 0 = aleatorio
    uint32_t rngRebote_ = 90731;
    void iniciarCombateReativo();  // chamado por iniciarCircuitos(): inimigos, baia e pistas
    void atualizarCombateReativo(float dt);
    bool interagirPista(Entrada& e);  // E perto de uma pista: inspeciona e ensina a receita
    // Golpe no inimigo de teste k: aplica, alerta (se for ataque) e larga o espolio ao derrotar.
    bool golpearInimigo(int k, const GolpeAlvo& g);
    void soltarEspolio(InimigoTeste& in);
    // capturas --roteiro-circuito 6..10: onde o cacador comeca e para onde a mira vai
    void prepararRoteiroReativo(int modo);
    Vec3f alvoRoteiroReativo(int modo) const;
    void desenharCombateReativo(Transparencias& tr);  // 3D: inimigos, baia, pistas e cones de visao
    void desenharRotulosCombate();     // 2D: estado sobre cada inimigo, pistas e vestigios descobertos
    void desenharDepuracaoCombate();   // 2D: quadro com inimigos, receitas, vestigios e rebote
};
