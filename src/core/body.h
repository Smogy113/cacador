#pragma once
#include <cstdint>
#include <utility>
#include <vector>
#include "core/skeleton.h"
#include "core/hair_profile.h"
#include "core/penteado.h"

enum OssoHumano {
    QUADRIL, COLUNA, PEITO, PESCOCO, CABECA,
    BRACO_E, ANTEBRACO_E, MAO_E, BRACO_D, ANTEBRACO_D, MAO_D,
    COXA_E, CANELA_E, PE_E, COXA_D, CANELA_D, PE_D,
    N_OSSOS_HUMANO
};

struct Proporcoes {
    float altura = 1.8f, ombros = 1.f, quadril = 1.f, massa = 1.f;
};

struct Visual {
    Rgb pele{0.78f, 0.58f, 0.45f}, camisa{0.8f, 0.76f, 0.66f}, casaco{0.36f, 0.26f, 0.18f}, calca{0.25f, 0.22f, 0.2f},
        botas{0.18f, 0.12f, 0.08f}, chapeu{0.22f, 0.16f, 0.11f}, lenco{0.55f, 0.12f, 0.1f}, cabelo{0.2f, 0.14f, 0.1f};
    bool sobretudo = true, chapeuAba = true, barba = false;
    bool equipamento = false;  // bandoleira, besta nas costas e laco (so no modelo HD)
    bool revolver = false;     // revolver na mao direita
    bool cacadorDeFronteira = false;  // perfil esculpido e roupa do protagonista
    bool corpoBase = false;           // manequim anatomico sem roupa, cabelo ou equipamento
    bool semCabelo = false;           // mantem a cabeca limpa para receber penteados por cima
    bool cabeloLongo = false;  // mechas compridas ate a nuca, com fisica (o heroi)
    Penteado penteado = Penteado::Sem;  // penteado da base humana (corpoHumanoBase)
    bool heroi = false;        // cabeca esculpida em resolucao maxima (o jogador); os outros usam celulas maiores
    bool semPeleCabeca = false;  // a pele esculpida da cabeca nao entra (desenhada por raymarching); olhos, cilios e cabelo sim
    float ladosExtras = 1.f;     // multiplica os lados dos tubos do heroi (silhueta mais redonda)
};

// Materiais dos personagens (vao para o shader; 0 = sem material especial)
constexpr float MAT_PELE = 10, MAT_TECIDO = 11, MAT_COURO = 12, MAT_METAL = 13, MAT_CABELO = 14, MAT_OLHO = 15, MAT_FELTRO = 16;
constexpr float MAT_COURO_CAVALO = 12.25f, MAT_CRINA_CAVALO = 14.25f;
constexpr float MAT_MADEIRA_ARMA = 12.375f;
constexpr float MAT_PELAGEM_CAVALO = 17, MAT_LABIO = 18, MAT_OLHO_CAVALO = 19;
constexpr float MAT_PELO = 20;  // (antigo) cascas de pelo
constexpr float MAT_CARTAO = 24;  // cartao de pelo: fita com fios (barba, sobrancelha, cabelo, crina)

// Vertice com ate 2 ossos: osso[0] com "peso", osso[1] com 1 - peso.
struct VertPele {
    Vec3f pos, normal;
    Rgb cor;
    float ao = 1.f;
    uint8_t osso[2] = {0, 0};
    uint8_t guiaCabelo[2] = {SEM_GUIA_CABELO, SEM_GUIA_CABELO};
    uint8_t misturaGuias = 0;  // fração do segundo guia em 0..255
    float peso = 1.f;
    float material = 0;  // MAT_*
    float pelos = 0;      // densidade de pelos (barba, sobrancelha, cabelo) 0..1; no cartao: u (0 ou 1, atravessa a fita)
    float compPelo = 0;   // comprimento dos pelos (m)
    Vec3f dirPelo{};      // direcao em que o pelo cresce (repouso); zero = sem pelo
    Vec3f tang{};         // cartao: direcao do fio; pele do rosto: coordenadas do rosto (x, y, 1)
    float tv = 0;         // cartao: v (0 na raiz, 1 na ponta)
    // Fitas de cabelo: plano da pele mais próxima em repouso (normal para fora, distância até ele e o
    // osso que move essa pele). Em movimento, o vértice nunca atravessa esse plano (apoioFolga 0 = livre).
    // Montado em ConstrutorCabelo::fita; aplicado em posicaoDaFita (core) e apoiarNoCorpo (shader).
    Vec3f apoioNormal{};
    float apoioFolga = 0;
    uint8_t apoioOsso = 0;
};

struct MalhaPele {
    std::vector<VertPele> v;
    std::vector<uint16_t> idx;  // triangulos
    PerfilCabelo cabelo;
    size_t triangulos() const { return idx.size() / 3; }
};

Esqueleto esqueletoHumano(const Proporcoes& p);
Esqueleto esqueletoHumanoBase(const Proporcoes& p);
Vec3f bocaDoRevolver(const Esqueleto& esq, float escala);
MalhaPele malhaRevolver(const Esqueleto& esq, float escala);
// Dobra os dedos da base humana e devolve o intervalo de vértices alterados, com fim exclusivo.
std::pair<size_t, size_t> fecharMaoDoRevolver(MalhaPele& malha, const Esqueleto& esq, float escala);
MalhaPele corpoHumano(const Proporcoes& p, const Visual& vis);
// Base humana CC0 quantizada no executavel: anatomia real, A-pose e sem roupa/cabelo.
MalhaPele corpoHumanoBase(const Proporcoes& p, const Visual& vis);
// Versao de alta definicao (perto da camera): ~6-14 mil triangulos, rosto esculpido, dedos, roupa com volume.
MalhaPele corpoHumanoHD(const Proporcoes& p, const Visual& vis);

// ---------------- cavalo ----------------
enum OssoCavalo {
    C_CORPO, C_PESCOCO, C_CABECA, C_CAUDA,
    C_BRACO_DE, C_CANELA_DE, C_CASCO_DE,  // diant. esquerda
    C_BRACO_DD, C_CANELA_DD, C_CASCO_DD,  // diant. direita
    C_COXA_TE, C_JARRETE_TE, C_CASCO_TE,  // tras. esquerda
    C_COXA_TD, C_JARRETE_TD, C_CASCO_TD,  // tras. direita
    N_OSSOS_CAVALO
};

struct VisualCavalo {
    Rgb pelo{.43f, .28f, .20f}, crina{.18f, .13f, .10f};
    bool selado = true, meias = false;
};

Esqueleto esqueletoCavalo();
constexpr float MEIO_COMPRIMENTO_CASCO = .082f;  // da sola, no eixo Z
MalhaPele corpoCavalo(const VisualCavalo& v, bool perto = true);
Vec3f assentoCavalo();  // onde fica o quadril do cavaleiro (espaco do modelo, parado)
