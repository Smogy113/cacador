#pragma once
#include <cstdint>
#include <vector>
#include "core/collision.h"
#include "core/formas.h"
#include "core/math.h"

// Gerador de predios por gramatica de formas: lote -> volume -> andares -> fachadas -> vaos -> pecas.
// Tudo em coordenadas do lote: frente em +z, largura em x, chao de referencia em y = 0.
enum class Estilo { Medieval, Faroeste };
enum class TipoPredio { Casa, Taverna, Xerife, Cadeia, Capela, Celeiro, Barracao };
enum class MatPeca { Reboco, Madeira, MadeiraEsc, Pedra, Telhado, Vidro, Ferro, Escuro, Placa };

struct PecaPredio {
    enum Tipo { Caixa, Prisma, Triangulo } tipo = Caixa;
    Vec3f centro{}, tam{};  // Caixa/Prisma: centro e tamanho (local da peca)
    float rotY = 0;         // giro em torno de y (no lote)
    float inclinacao = 0;   // giro em torno do eixo x local da peca (placas do telhado)
    Vec3f a{}, b{}, c{};    // Triangulo (empenas)
    MatPeca mat = MatPeca::Madeira;
    Rgb cor{};
    bool forma = false;     // registra forma (luz tracada); so caixa sem inclinacao e prisma
    bool soForma = false;   // so a forma (bloqueia luz), sem desenho
    bool detalhe = false;   // fica de fora no Baixo e na casca de longe
};

struct Vao {  // abertura numa fachada: lado 0 frente (+z), 1 fundos, 2 direita (+x), 3 esquerda
    int lado, andar;
    float u0, u1, v0, v1;  // ao longo da fachada (centro = 0) e altura absoluta
    bool porta;
};

struct PortaPredio {
    Vec3f dobradica{};      // eixo vertical da folha (local), no chao do piso
    float largura = 1.1f, altura = 2.15f;
    float rotFechada = 0;   // rotY da folha fechada (ao longo de +x a partir da dobradica)
    float sentido = 1;      // abre girando para dentro (+1 ou -1)
};

struct Planta {
    std::vector<PecaPredio> pecas;
    std::vector<PecaPredio> casca;        // versao simples (longe, Baixo, sombras): volume, telhado, janelas escuras
    std::vector<CaixaGirada> paredes;     // colisao (local); o vao da porta fica livre
    std::vector<CaixaGirada> pisos;       // pisos andaveis (topo = y1)
    std::vector<Vao> vaos;
    std::vector<JanelaLuz> janelas;       // janelas (luz a noite)
    PortaPredio porta;
    Vec3f interiorCentro{}, interiorMeia{};  // volume interior do terreo (sem giro no lote)
    float piso = 0.35f, peDireito = 2.8f;
    Vec3f entrada{};                       // ponto do lado de fora, na frente da porta
};

struct Lote {
    float largura = 7, profundidade = 6;  // pegada das paredes (sem beiral/varanda)
    int andares = 1;
    Estilo estilo = Estilo::Medieval;
    TipoPredio tipo = TipoPredio::Casa;
    uint32_t semente = 1;
    float piso = 0.35f;                    // altura do piso acima do chao de referencia
    Rgb corParede{0.8f, 0.74f, 0.6f}, corTelhado{0.42f, 0.27f, 0.18f};
};

Planta gerarPredio(const Lote& l);
// triangulos que a planta vai gerar (orcamento): caixa 12, prisma 8, triangulo 1
int triangulosPlanta(const Planta& p, bool comDetalhe = true);

// porta e interior ja no mundo (o render guarda; o jogo abre/fecha e escurece o interior)
struct PortaMundo {
    Vec3f dobradica;            // mundo, no nivel do piso
    float largura, altura, rot; // rot: giro da folha fechada (rotY)
    float sentido = 1;          // para que lado abre (para dentro)
    float abertura = 0;         // 0 fechada .. 1 aberta (90 graus)
    bool querAberta = false;
};
struct InteriorMundo {
    Vec3f centro, meia;  // centro no mundo; meia no espaco do predio
    float rot;
    Vec3f porta;         // centro do vao da porta (mundo)
};

// folha da porta no mundo (colisao), conforme o quanto esta aberta
CaixaGirada folhaPorta(const PortaMundo& p);
// a porta pode fechar sem bater em quem esta em "pos" (circulo de raio "raio")?
bool portaPodeFechar(const PortaMundo& p, Vec3f pos, float raio);
