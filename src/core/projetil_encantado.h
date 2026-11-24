#pragma once
// Projéteis dos Circuitos de Queima no mundo: voo (com ou sem gravidade), ricochete, perda do
// encantamento no Ferro Frio, impacto, zonas elementais no chão e cápsulas de vestígio.
//
// Diferente do tiro comum (core/combat: raio instantâneo), a bala gravada é um projétil visível
// integrado a cada quadro. O impacto só descreve o que aconteceu; o efeito em cada alvo sai de
// golpeDoImpacto/golpeDaZona e é aplicado pelo jogo (bandidos, bonecos).
#include <array>
#include <vector>
#include "core/circuito.h"
#include "core/collision.h"
#include "core/combat.h"
#include "core/fisica_alvo.h"
#include "core/math.h"

class Terreno;

// Volume de Ferro Frio: circuitos não acendem dentro dele e projéteis que o atravessam viram chumbo.
struct ZonaFerroFrio {
    AABB caixa;
    bool contem(Vec3f p) const;
    // O segmento a->b toca a caixa? 'entrada' recebe o primeiro ponto de contato.
    bool cruza(Vec3f a, Vec3f b, Vec3f* entrada = nullptr) const;
};

struct ProjetilEncantado {
    static constexpr int RASTRO = 6;
    Vec3f pos, vel;
    Circuito circuito;
    EfeitoCircuito efeito;
    bool encantado = true;  // false: virou chumbo (Ferro Frio)
    int ricochetesRestantes = 0;
    float idade = 0;
    std::array<Vec3f, RASTRO> rastro{};  // posições dos últimos quadros (desenho)
    int nRastro = 0;
};

struct Impacto {
    Vec3f ponto, normal{0, 1, 0}, dir{0, 0, 1};
    Circuito circuito;
    EfeitoCircuito efeito;
    bool encantado = true;
    int alvo = -1;              // índice na lista de alvos passada a atualizar(); -1 = superfície
    Parte parte = Parte::Nada;  // onde pegou no alvo
};

// Zona de fogo ou gelo deixada no chão por uma Onda que não é Pólvora Fina.
struct ZonaElemental {
    Vec3f centro;  // no chão
    float raio = 2.5f, restante = 0, duracao = 0, intensidade = 1;
    bool frio = false;
    bool contem(Vec3f pes) const;
};

// Cápsula marcada que cai a cada disparo com bala gravada: o rastro do caçador. Um inimigo em
// patrulha que passa perto e a percebe marca 'descoberta' e vai investigar (core/inimigo_teste).
struct CapsulaVestigio {
    Vec3f pos;
    float yaw = 0, idade = 0;
    Circuito circuito;
    bool descoberta = false;  // algum inimigo já a viu (não chama outro de novo)
};

struct EventosCircuito {
    std::vector<Impacto> impactos;
    std::vector<Vec3f> ricochetes;
    std::vector<Vec3f> desencantos;  // onde um projétil perdeu o efeito no Ferro Frio
    std::vector<Vec3f> sumidos;      // acabou o tempo de voo, sem tocar nada
};

// Efeito mágico de um impacto num alvo com o meio do corpo em 'centroAlvo' (sem o dano do chumbo,
// que o jogo aplica ao alvo atingido em cheio). 'direto' = este é o alvo atingido.
// A Grade (Peso) empurra para fora, derruba e atordoa quem está no raio; o atordoamento passa do
// tempo caído, então o alvo levanta zonzo.
GolpeAlvo golpeDoImpacto(const Impacto& im, Vec3f centroAlvo, bool direto, const ParametrosCircuito& p);
// O que uma zona faz num alvo com os pés em 'pes' durante dt. Fogo queima; a zona congelante deixa
// lento (fatorLentidaoGelo) quem pisa nela, e a lentidão some tempoLentidaoGelo depois de sair.
GolpeAlvo golpeDaZona(const ZonaElemental& z, Vec3f pes, float dt, const ParametrosCircuito& p);

class CircuitosNoMundo {
public:
    ParametrosCircuito p;  // ajuste livre; o Arsenal do jogo nasce destes mesmos números

    void disparar(Vec3f boca, Vec3f dir, const Circuito& c);
    void definirFerro(std::vector<ZonaFerroFrio> zonas) { ferro_ = std::move(zonas); }
    bool noFerro(Vec3f q) const;
    // Avança projéteis e zonas. 'alvos' = corpos que podem ser atingidos (mesmo índice de Impacto::alvo).
    EventosCircuito atualizar(float dt, const Terreno& t, const MundoColisao& col, const std::vector<Alvo>& alvos);

    void soltarCapsula(Vec3f chao, float yaw, const Circuito& c);
    int capsulaPerto(Vec3f q, float raio) const;  // índice da mais próxima, -1 se nenhuma
    void marcarDescoberta(int i);                 // um inimigo percebeu esta cápsula
    int recolher(Vec3f q, float raio);            // apaga as cápsulas no raio; devolve quantas

    const std::vector<ProjetilEncantado>& projeteis() const { return projeteis_; }
    const std::vector<ZonaElemental>& zonas() const { return zonas_; }
    const std::vector<ZonaFerroFrio>& ferro() const { return ferro_; }
    const std::vector<CapsulaVestigio>& capsulas() const { return capsulas_; }

private:
    std::vector<ProjetilEncantado> projeteis_;
    std::vector<ZonaElemental> zonas_;
    std::vector<ZonaFerroFrio> ferro_;
    std::vector<CapsulaVestigio> capsulas_;
};

// Normal da superfície atingida em 'ponto' (vindo na direção 'dir'): do terreno, ou estimada
// sondando as caixas de colisão em volta (serve também para as paredes giradas).
Vec3f normalSuperficie(Vec3f ponto, Vec3f dir, const Terreno& t, const MundoColisao& col);
