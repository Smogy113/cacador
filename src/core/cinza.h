#pragma once
// Corrente da Cinza: o caçador molda fumaça (dos tiros e de fogueiras) e a acende com uma faísca.
//
// Lógica pura, sem raylib e sem a malha do personagem: cada nuvem é uma esfera, o escudo é um
// retângulo vertical e cada detonação é uma esfera de dano. O jogo (game/cinza_jogo.cpp) só lê o
// estado para desenhar com primitivas e aplica as detonações nos alvos.
//
// Ciclo de uma conjuração:
//   1. Fumaça: cada disparo e cada FonteFumaca (fogueira) criam NuvemFumaca, que sobe, cresce e
//      some sozinha depois de ParametrosCinza::vidaNuvem segundos.
//   2. Canalizar (segurar o botão): o tempo do mundo cai para ParametrosCinza::fatorTempo; nuvens
//      dentro de raioAtracao do jogador ou do ponto mirado ficam capturadas e são puxadas para o
//      ponto de moldagem. Custa sangue a cada segundo.
//   3. Faísca (soltar): as capturadas junto ao corpo condensam num EscudoFuligem frontal; as outras
//      detonam, e cada explosão acende as nuvens vizinhas com um pequeno atraso (cadeia).
//   4. Custo de Sangue: não há mana. A conjuração gasta a vida atual e desgasta a vida máxima;
//      o desgaste acumula e só volta descansando na fogueira (restaurarDesgaste).
//   5. Umidade: dentro de uma ZonaUmida (água, chuva) a fumaça se apaga na hora e a conjuração
//      fica bloqueada.
#include <cstdint>
#include <functional>
#include <vector>
#include "core/combat.h"
#include "core/math.h"

// Todos os números de ajuste da Corrente da Cinza. Distâncias em metros, tempos em segundos e
// vida nos mesmos pontos de Vida (100 = vida cheia). Mudar aqui muda o jogo inteiro.
struct ParametrosCinza {
    // ---- fumaça ----
    float vidaNuvem = 10.f;            // até dissipar (tempo do mundo; capturada não envelhece)
    float raioNuvemDisparo = 0.9f;     // raio inicial da nuvem deixada por um tiro
    float distNuvemDisparo = 0.8f;     // a nuvem nasce a esta distância da boca, na trajetória
    float subidaNuvemDisparo = 0.05f;  // m/s para cima
    float raioNuvemFogueira = 1.1f;
    float subidaNuvemFogueira = 0.35f;
    float intervaloFogueira = 1.4f;    // entre duas nuvens da mesma fonte
    float crescimentoNuvem = 0.6f;     // o raio cresce esta fração ao longo da vida
    int maxNuvens = 40;                // teto de memória e de desenho; a nuvem livre mais velha sai

    // ---- canalização ----
    float fatorTempo = 0.4f;     // velocidade do mundo enquanto canaliza (1 = normal)
    float rampaTempo = 0.15f;    // tempo real para entrar e sair da dilatação
    float raioAtracao = 12.f;    // alcance da captura, em volta do jogador e do ponto mirado
    float velAtracao = 6.f;      // m/s máximo ao puxar uma nuvem
    float ganhoAtracao = 2.5f;   // 1/s: quanto mais longe do destino, mais rápido (até velAtracao)
    float espalhamentoMolde = 0.7f;  // as nuvens se arrumam em volta do destino em vez de virar um ponto
    float distMoldeCorpo = 1.2f;     // sem mira: destino a esta distância à frente do peito (vira escudo)
    float alcanceMira = 30.f;        // com mira: até onde o ponto mirado pode ir (cobre o combate do bando, 10-25 m)
    float distMinimaMira = 4.f;      // com mira: nunca mais perto que isto (garante a detonação)

    // ---- Custo de Sangue ----
    float custoVidaPorSegundo = 4.f;   // vida atual gasta por segundo canalizando (tempo real)
    float desgastePorSegundo = 1.5f;   // vida máxima perdida por segundo canalizando
    float custoVidaFaisca = 4.f;       // ao soltar a faísca sobre fumaça capturada
    float desgasteFaisca = 2.f;
    float custoVidaPorNuvem = 1.5f;    // a mais por nuvem condensada ou acesa pela faísca
    float desgastePorNuvem = 1.f;
    float vidaMinimaConjurar = 10.f;   // abaixo disso não canaliza; o custo nunca mata (para em 1)
    float vidaMaximaMinima = 30.f;     // o desgaste nunca derruba a vida máxima abaixo disto
    float recuperacaoDesgaste = 0.f;   // pontos de vida máxima por segundo sem conjurar (0 = só a fogueira)

    // ---- Escudo de Fuligem ----
    float distEscudo = 2.5f;           // nuvem a menos disto do peito vira escudo; mais longe, detona
    float duracaoEscudo = 4.f;
    float larguraEscudo = 2.4f, alturaEscudo = 2.3f;
    float afastamentoEscudo = 1.1f;    // do corpo até o plano do escudo, para a frente
    float absorcaoEscudoMin = 0.5f;    // fração do dano barrada com uma nuvem
    float absorcaoEscudoMax = 0.9f;    // ... e com nuvensEscudoCheio ou mais
    int nuvensEscudoCheio = 3;

    // ---- Detonação ----
    float raioDetonacao = 3.5f;
    float danoDetonacao = 60.f;        // no centro; cai em linha reta até zero na borda
    float empurraoDetonacao = 7.f;     // m/s no centro, também caindo até a borda
    float raioPropagacao = 4.5f;       // entre centros: a explosão acende a nuvem vizinha
    float atrasoPropagacao = 0.12f;    // por elo da cadeia (tempo do mundo)
    float duracaoExplosao = 0.6f;      // só o visual
    float fracaoDanoJogador = 0.f;     // dano da própria detonação no caçador (0 = imune)
};

struct NuvemFumaca {
    Vec3f pos, vel;
    float raio = 1, raioInicial = 1;
    float idade = 0, vida = 10;
    bool capturada = false;
    float controle = 0;     // 0..1: sobe ao capturar (visual "sob controle"), desce ao soltar
    float pavio = -1;       // >= 0: vai detonar quando chegar a zero (propagação em cadeia)
    uint32_t semente = 1;   // variação estável de forma no desenho
    float alfaVida() const;  // 0..1: aparece rápido e some no último terço da vida
};

// Fonte contínua de fumaça (fogueira de teste).
struct FonteFumaca {
    Vec3f pos;
    float acumulado = 0;
    bool ativa = true;
};

// Cilindro vertical de água ou chuva forte: apaga fumaça e bloqueia a conjuração.
struct ZonaUmida {
    Vec3f centro;        // centro da base
    float raio = 4, altura = 12;
    bool contem(Vec3f p) const;
};

// Barreira frontal de fuligem condensada, presa ao corpo do caçador.
struct EscudoFuligem {
    Vec3f centro;        // meio do retângulo (a altura já vem da base no chão)
    float yaw = 0;       // para onde o escudo está virado (mesma convenção do Corpo)
    float restante = 0, duracao = 0;
    float absorcao = 0;  // fração do dano barrada
    int nuvens = 0;
    bool ativo() const { return restante > 0; }
};

// Explosão em andamento (só visual; o dano sai uma vez, em EventosCinza::detonacoes).
struct ExplosaoCinza {
    Vec3f pos;
    float raio = 3, idade = 0, duracao = 0.6f;
};

struct GolpeDetonacao {
    Vec3f centro;
    float raio = 0, dano = 0, empurrao = 0;
    bool primeira = false;  // acesa direto pela faísca (false = veio da cadeia)
};
// Dano e empurrão que uma detonação causa a 'dist' metros do centro (zero fora do raio).
float danoNaDistancia(const GolpeDetonacao& g, float dist);
float empurraoNaDistancia(const GolpeDetonacao& g, float dist);

enum class EstadoConjuracao {
    Pronta,       // pode canalizar
    Canalizando,  // botão segurado
    Bloqueada,    // jogador molhado (água ou chuva)
    SemSangue,    // vida atual abaixo de vidaMinimaConjurar
    Indisponivel  // montado, caído ou com outro menu aberto
};
const char* nomeEstado(EstadoConjuracao e);

// Vida máxima com o desgaste da Corrente da Cinza.
struct CustoSangue {
    float vidaBase = 100.f;
    float desgaste = 0.f;  // quanto a vida máxima está abaixo da base
    float vidaMaxima() const { return vidaBase - desgaste; }
    // Gasta 'vida' da atual (sem descer de 1) e 'desgaste' da máxima (sem descer de minima).
    void pagar(Vida& v, float vida, float desgasteExtra, float minima);
    void recuperar(Vida& v, float pontos);  // devolve vida máxima (não cura a atual)
    void restaurar(Vida& v) { recuperar(v, desgaste); }
};

struct EntradaCinza {
    bool segurar = false;       // botão de conjuração
    bool podeConjurar = true;   // false: montado, morto, menu...
    bool jogadorUmido = false;  // pisando na água ou debaixo de chuva (o jogo decide)
    Vec3f pes;                  // posição do caçador no chão
    Vec3f peito;                // centro do corpo, ~1,35 m acima dos pés
    Vec3f frente{0, 0, 1};      // horizontal, para onde o caçador olha
    Vec3f pontoMolde;           // para onde puxar a fumaça capturada
    bool moldeMirado = false;   // true: pontoMolde veio da mira (também captura em volta dele)
};

struct EventosCinza {
    std::vector<GolpeDetonacao> detonacoes;  // aplicar nos alvos neste quadro
    bool comecouCanalizar = false;
    bool faiscaSolta = false;     // soltou o botão com fumaça capturada
    bool faiscaVazia = false;     // soltou sem nada capturado (nada acontece, nada custa)
    bool escudoErguido = false;
    bool interrompida = false;    // molhou ou ficou sem sangue no meio da canalização
    int apagadasPelaAgua = 0;
};

class CorrenteCinza {
public:
    ParametrosCinza p;  // ajuste livre; vale a partir do próximo atualizar()

    // Uma nuvem por disparo, um pouco à frente da boca, na direção do tiro.
    void emitirDisparo(Vec3f boca, Vec3f dir);
    void emitir(Vec3f pos, float raio, Vec3f vel);
    int adicionarFonte(Vec3f pos);
    std::vector<FonteFumaca>& fontes() { return fontes_; }
    void definirZonasUmidas(std::vector<ZonaUmida> zonas) { zonas_ = std::move(zonas); }
    // Teste extra de água (rio do terreno, por exemplo). Vazio = só as zonas.
    void definirTesteAgua(std::function<bool(Vec3f)> teste) { testeAgua_ = std::move(teste); }
    bool umido(Vec3f p) const;

    // dtReal: tempo de relógio (canalização, puxão e custo); dtMundo: tempo já dilatado (fumaça,
    // escudo, cadeia). A vida do jogador paga o custo aqui dentro.
    EventosCinza atualizar(float dtReal, float dtMundo, const EntradaCinza& e, Vida& vida);

    // Multiplicador do dano de um tiro que vai de 'origem' até 'alvo': (1 - absorcao) se a linha
    // atravessa o escudo, 1 se não.
    float fatorDano(Vec3f origem, Vec3f alvo) const;
    // Descanso na fogueira: a vida máxima volta à base.
    void restaurarDesgaste(Vida& v) { sangue_.restaurar(v); }

    // Leitura (desenho e HUD)
    float fatorTempo() const { return fatorAtual_; }
    EstadoConjuracao estado() const { return estado_; }
    const std::vector<NuvemFumaca>& nuvens() const { return nuvens_; }
    const EscudoFuligem& escudo() const { return escudo_; }
    const std::vector<ExplosaoCinza>& explosoes() const { return explosoes_; }
    const CustoSangue& sangue() const { return sangue_; }
    int capturadas() const;
    float tempoCanalizando() const { return tempoCanalizando_; }

private:
    void soltarFaisca(const EntradaCinza& e, Vida& vida, EventosCinza& ev);
    void soltarCaptura();
    void detonar(size_t i, bool primeira, EventosCinza& ev);
    void limitarNuvens();

    std::vector<NuvemFumaca> nuvens_;
    std::vector<FonteFumaca> fontes_;
    std::vector<ZonaUmida> zonas_;
    std::function<bool(Vec3f)> testeAgua_;
    std::vector<ExplosaoCinza> explosoes_;
    EscudoFuligem escudo_;
    CustoSangue sangue_;
    EstadoConjuracao estado_ = EstadoConjuracao::Pronta;
    bool canalizando_ = false;
    float tempoCanalizando_ = 0;
    float fatorAtual_ = 1.f;
    uint32_t rng_ = 0xC1A2u;
};
