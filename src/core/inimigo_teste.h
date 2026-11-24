#pragma once
// Inimigos de teste (provisórios) da arena dos Circuitos de Queima: manequins que patrulham uma rota,
// percebem as cápsulas marcadas deixadas no chão e reagem aos golpes mágicos.
//
// O corpo é um AlvoTreino (core/fisica_alvo): vida, empurrão, arremesso, derrubada, atordoamento,
// gelo, lentidão e reaparecimento. Aqui fica só a "cabeça": andar a rota, perceber vestígios,
// investigar e voltar. Lógica pura, sem raylib e sem a malha do personagem; o jogo monta, liga aos
// golpes e desenha em game/combate_reativo.cpp.
//
// Estado mostrado na depuração (estado()), do mais forte para o mais fraco:
//   Derrotado > Atordoado (zonzo, caído ou no ar) > Congelado (gelo ou lentidão) > Alerta > Patrulha
//
// Ciclo da cabeça (Conduta):
//   Patrulhando --percebe cápsula / é atacado--> Investigando --chega e olha tempoInspecao--> Patrulhando
// Golpes não mudam a conduta: o corpo impedido (ocupado) só pausa a cabeça até se recuperar.
#include <optional>
#include <string>
#include <vector>
#include "core/fisica_alvo.h"
#include "core/projetil_encantado.h"
#include "core/receitas.h"

class Terreno;

enum class EstadoInimigo : uint8_t { Patrulha, Alerta, Congelado, Atordoado, Derrotado };
const char* nome(EstadoInimigo e);

enum class Conduta : uint8_t { Patrulhando, Investigando };
enum class MotivoAlerta : uint8_t { Nenhum, Vestigio, Atacado };
const char* nome(MotivoAlerta m);

// Números de ajuste dos inimigos de teste (metros, segundos, radianos).
struct ParametrosInimigo {
    float velPatrulha = 1.3f;            // andando a rota
    float velAgachado = 0.5f;            // o sentinela atrás da cobertura
    float velInvestigar = 2.1f;          // indo até o ponto suspeito
    float chegada = 0.35f;               // chegou ao ponto da rota
    float distInspecao = 1.1f;           // para a esta distância do ponto investigado
    float tempoInspecao = 3.5f;          // olhando o ponto antes de voltar à patrulha
    float raioVisaoVestigio = 8.f;       // enxerga cápsulas na frente até aqui...
    float meioConeVisao = 1.05f;         // ...dentro de 60° para cada lado do olhar
    float raioPercepcaoProxima = 2.5f;   // nota cápsulas em volta mesmo sem olhar
    float alturaOlhos = 1.6f, alturaOlhosAgachado = 1.f;
    float alturaEmPe = 1.8f, alturaAgachado = 1.15f;  // altura do alvo no teste de acerto
    float raioCorpo = 0.3f;              // colisão com o cenário
    float giro = 5.f;                    // rad/s virando o corpo
    float tempoReaparecer = 20.f;        // derrotado volta ao começo da rota depois disto
};

struct InimigoTeste {
    std::string nome;
    AlvoTreino corpo;          // vida, física e reaparecimento (corpo.base = começo da rota)
    std::vector<Vec3f> rota;   // pontos no chão, em laço; com 1 ponto fica parado nele
    int proximo = 0;           // ponto da rota para onde anda
    bool agachado = false;     // atrás de cobertura: alvo e olhos mais baixos
    bool posto = false;        // sentinela: não sai do posto para investigar, só vira para olhar
    float yawVigia = 0;        // sentinela: para onde olha enquanto anda de lado no posto
    Conduta conduta = Conduta::Patrulhando;
    MotivoAlerta motivo = MotivoAlerta::Nenhum;
    Vec3f interesse{};         // o ponto investigado
    float inspecao = 0;        // s que faltam olhando o ponto (só conta depois de chegar)
    float travado = 0;         // s sem conseguir chegar ao ponto (parede no caminho): desiste e olha dali
    float vel = 0;             // m/s neste quadro (desenho do passo)
    float passada = 0;         // fase do passo, em ciclos (desenho)
    std::optional<Descoberta> espolio;  // pista que larga ao ser derrotado (uma vez só)

    bool ativo() const { return corpo.ativo(); }
    Vec3f pes() const { return corpo.pos + Vec3f{0, corpo.fisica.voo, 0}; }  // com o voo do arremesso
    // Para lancarTiro: fora do alcance se derrotado.
    Alvo alvo(const ParametrosInimigo& p) const;
};

InimigoTeste criarInimigo(const std::string& nome, std::vector<Vec3f> rota, const ParametrosInimigo& p);
EstadoInimigo estado(const InimigoTeste& in);
// Uma linha para a depuração: "Atordoado (1.8 s)", "Alerta: vestígio", "Congelado (20% da vel.)".
std::string descreverEstado(const InimigoTeste& in);

// O inimigo nota 'ponto' (no chão)? Perto em volta, ou dentro do cone de visão e sem parede no meio.
// Impedido (atordoado, caído, congelado...) ou derrotado não nota nada.
bool percebe(const InimigoTeste& in, Vec3f ponto, const MundoColisao& col, const ParametrosInimigo& p);
// Primeira cápsula ainda não descoberta que o inimigo percebe; -1 se nenhuma.
int vestigioPercebido(const InimigoTeste& in, const std::vector<CapsulaVestigio>& capsulas, const MundoColisao& col,
                      const ParametrosInimigo& p);
// Muda para Investigando o ponto (o sentinela de posto só vira para ele).
void investigar(InimigoTeste& in, Vec3f ponto, MotivoAlerta motivo, const ParametrosInimigo& p);
// Golpe no corpo; true se o derrotou agora.
bool golpear(InimigoTeste& in, const GolpeAlvo& g);
// Um passo: física do corpo (core/fisica_alvo) e, se o corpo estiver livre, a cabeça.
void atualizar(InimigoTeste& in, float dt, const Terreno& t, const MundoColisao& col, const ParametrosInimigo& p);
