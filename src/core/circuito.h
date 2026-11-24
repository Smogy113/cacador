#pragma once
// Circuitos de Queima: munição gravada que transfere a propriedade de um objeto para o tiro.
//
// Um circuito tem três partes, escolhidas na fogueira e gravadas numa bala:
//   - Entalhe geométrico: a propriedade física (Espiral = velocidade, Grade = peso, Onda = calor).
//   - Pasta de preenchimento: como a propriedade é liberada (no ponto, persistente, em área ou
//     invertida pelo pó de prata).
//   - Objeto doador: o sacrifício que alimenta a receita; é preso à bala na fogueira e queimado
//     no disparo. Cada entalhe exige o seu doador (Pena, Pedra Pesada, Carvão em Brasa).
//
// Lógica pura, sem raylib e sem a malha do personagem. O voo e o impacto ficam em
// core/projetil_encantado; o efeito num alvo em core/fisica_alvo; a interface em game/circuitos_jogo.
#include <cstdint>
#include <string>
#include <vector>

enum class Entalhe : uint8_t { Espiral, Grade, Onda };
enum class Pasta : uint8_t { PolvoraFina, ResinaECera, EnxofreESal, PoDePrata };
enum class Doador : uint8_t { Pena, PedraPesada, CarvaoEmBrasa };
constexpr int N_ENTALHES = 3, N_PASTAS = 4, N_DOADORES = 3;

const char* nome(Entalhe e);
const char* nome(Pasta p);
const char* nome(Doador d);
// Doador que a receita de cada entalhe exige.
Doador doadorDaReceita(Entalhe e);

struct Circuito {
    Entalhe entalhe = Entalhe::Espiral;
    Pasta pasta = Pasta::PolvoraFina;
    Doador doador = Doador::Pena;
    bool valido() const { return doador == doadorDaReceita(entalhe); }
};
// "Espiral + Resina e Cera + Pena"
std::string nomeCombinacao(const Circuito& c);

// Propriedade que o tiro carrega depois da pasta (o Pó de Prata inverte a do entalhe).
enum class Propriedade : uint8_t {
    Velocidade,  // Espiral: voa reto, rápido e sem cair
    Arrasto,     // Espiral invertida: voo lento e pesado, em arco curto
    Peso,        // Grade: impacto esmagador que empurra e derruba
    Levante,     // Grade invertida: impulso para cima que arremessa
    Calor,       // Onda: fogo que queima
    Frio         // Onda invertida: gelo que congela
};
// Como a pasta libera a propriedade.
enum class Liberacao : uint8_t {
    Concentrada,  // Pólvora Fina: tudo de uma vez, num raio pequeno
    Persistente,  // Resina e Cera: ricocheteia mantendo o efeito; zonas duram mais
    Area,         // Enxofre e Sal: espalha num raio grande
    Invertida     // Pó de Prata: raio médio, propriedade invertida
};
const char* nome(Propriedade p);

// Todos os números de ajuste dos Circuitos de Queima (metros, segundos, pontos de vida 0..100).
struct ParametrosCircuito {
    // ---- munição ----
    int slotsPreparados = 3;       // balas preparadas guardadas ao mesmo tempo
    int estoqueInicialDoador = 6;  // de cada doador, para o protótipo
    // ---- projétil ----
    float velProjetil = 70.f;            // Grade e Onda (bala visível, com queda)
    float velEspiral = 150.f;            // Espiral: rápida e sem gravidade
    float velArrasto = 26.f;             // Espiral invertida
    float gravidade = 9.8f;
    float gravidadeArrasto = 2.5f;       // multiplica a gravidade na Espiral invertida
    float vidaProjetil = 3.f;            // some no ar depois disso, sem efeito
    int ricochetes = 3;                  // Resina e Cera: quicadas antes do impacto final
    float restituicao = 0.9f;            // velocidade mantida a cada ricochete
    // ---- impacto ----
    float raioConcentrado = 1.2f;        // Pólvora Fina
    float raioPadrao = 2.5f;             // Resina e Cera, Pó de Prata
    float raioEnxofre = 5.f;             // Enxofre e Sal (área)
    float intensidadeConcentrada = 1.6f; // a Pólvora troca raio por força
    float danoBala = 34.f;               // o chumbo no alvo atingido em cheio (como um tiro comum)
    float danoVelocidade = 30.f;         // Espiral: dano extra no impacto
    float danoPeso = 30.f;               // Grade: dano da onda de pressão no centro
    float empurraoPeso = 9.f;            // m/s para fora no centro
    float tempoDerrubado = 1.6f;         // Grade: alvo no chão
    float levante = 9.f;                 // m/s para cima (Grade invertida)
    float danoQueimaInstantanea = 45.f;  // Onda + Pólvora: labareda no ponto
    float danoCalorPorSegundo = 14.f;    // dentro da zona de fogo
    float tempoCongelamento = 3.f;       // Onda invertida + Pólvora: congela na hora (para por completo)
    float duracaoZona = 6.f;             // zonas de fogo e de gelo no chão
    float multPersistente = 2.f;         // Resina e Cera: zonas duram isto vezes mais
    // ---- reação dos alvos (combate reativo: inimigos de teste, bandidos e bonecos) ----
    float tempoAtordoado = 2.5f;         // Grade: zonzo de pé, sem andar nem atirar, no raio do impacto
    float fatorLentidaoGelo = 0.2f;      // zona congelante: quem pisa nela anda a 20% da velocidade
    float tempoLentidaoGelo = 0.4f;      // a lentidão que sobra depois de sair da zona
    // ---- pavio ----
    float tempoSecagem = 5.f;            // depois de sair da água o pavio fica úmido por isto
    float janelaForcar = 1.5f;           // pavio úmido: o 2º clique dentro disto força o disparo
    // ---- preparo e rebote (core/rebote) ----
    float tempoPreparoCuidadoso = 3.f;   // a gravação cuidadosa na fogueira leva isto (o mundo segue)
    float chanceReboteRapido = 0.35f;    // bala do Preparo Rápido: chance de rebote no disparo
    float chanceReboteUmidoMin = 0.3f;   // forçar com o pavio quase seco
    float chanceReboteUmidoMax = 0.85f;  // forçar logo depois de sair da água
    float danoRebote = 12.f;             // o caçador se fere com o rebote na câmara
    float travaRebote = 0.9f;            // s sem poder atirar depois do rebote
    float duracaoTremor = 1.4f;          // s de mira tremida
    float recuoRebote = 0.09f;           // rad: o tranco que joga a mira para cima
    float tremorRebote = 0.035f;         // rad: o balanço da mira (some até o fim do tremor)
    // ---- vestígios ----
    int maxCapsulas = 24;                // cápsulas marcadas no chão (a mais velha some)
    float distRecolher = 1.6f;           // alcance do E para recolher uma cápsula
};

// O que um circuito faz quando acende: lido pelo projétil (voo) e pelo impacto (efeito).
struct EfeitoCircuito {
    Propriedade propriedade = Propriedade::Velocidade;
    Liberacao liberacao = Liberacao::Concentrada;
    float velocidade = 70.f;
    float gravidadeMult = 1.f;  // 0 = sem gravidade
    int ricochetes = 0;
    float raio = 2.5f;
    float intensidade = 1.f;    // multiplica dano, empurrão, levante e tempo de gelo
    float duracaoZona = 0.f;    // > 0: deixa zona de fogo ou gelo no chão
};
EfeitoCircuito resolverEfeito(const Circuito& c, const ParametrosCircuito& p);
// Bala de chumbo comum (circuito que não acendeu ou perdeu o efeito no Ferro Frio).
EfeitoCircuito efeitoChumbo(const ParametrosCircuito& p);
// Uma linha para a interface: "Voa reto sem cair; ricocheteia 3 vezes"
std::string descreverEfeito(const Circuito& c, const ParametrosCircuito& p);

// ---------------------------------------------------------------- pavio

// Seco: acende. Úmido: acabou de sair da água; falha, mas dá para forçar (risco de rebote).
// Molhado: dentro da água/chuva; falha sempre. Ferro Frio: nenhum circuito acende.
enum class EstadoPavio { Seco, Umido, Molhado, BloqueadoFerro };
const char* nome(EstadoPavio e);

// O pavio da munição gravada: molha na água/chuva e só seca tempoSecagem depois de sair.
struct Pavio {
    float umidade = 0;        // segundos até secar
    bool encharcado = false;  // dentro da água/chuva agora
    bool ferro = false;       // dentro do Ferro Frio agora
    void atualizar(float dt, bool naAgua, bool noFerro, float tempoSecagem);
    EstadoPavio estado() const;
    // 1 = acabou de sair da água, 0 = seco
    float fracaoUmidade(float tempoSecagem) const { return tempoSecagem > 0.f ? umidade / tempoSecagem : 0.f; }
};

// ---------------------------------------------------------------- arsenal

// Como a bala foi gravada na fogueira. O Rápido grava na hora, mas a bala sai imperfeita e pode
// dar rebote na câmara (core/rebote); o Cuidadoso leva tempoPreparoCuidadoso.
enum class Preparo : uint8_t { Cuidadoso, Rapido };
const char* nome(Preparo p);

struct SlotMunicao {
    bool cheio = false;
    Circuito circuito;
    Preparo preparo = Preparo::Cuidadoso;
    bool imperfeita() const { return cheio && preparo == Preparo::Rapido; }
};

// EntalheDesconhecido/PastaDesconhecida: a receita ainda não foi aprendida (core/receitas).
enum class ResultadoPreparo { Ok, SlotInvalido, ReceitaInvalida, SemDoador, EntalheDesconhecido, PastaDesconhecida };
const char* nome(ResultadoPreparo r);

// Balas preparadas, estoque de doadores e qual munição está na vez.
class Arsenal {
public:
    explicit Arsenal(const ParametrosCircuito& p = {});
    int slots() const { return int(slots_.size()); }
    const SlotMunicao& slot(int i) const { return slots_[size_t(i)]; }
    int estoque(Doador d) const { return estoque_[int(d)]; }
    void reporEstoque();  // protótipo: volta ao estoque inicial

    // Grava o circuito no slot: prende o doador à bala (sai do estoque). Regravar devolve o anterior.
    // Não confere se o caçador conhece a receita: para isso use gravar() de core/receitas.
    ResultadoPreparo preparar(int slot, const Circuito& c, Preparo preparo = Preparo::Cuidadoso);
    void esvaziar(int slot);  // devolve o doador

    // -1 = munição comum; 0..slots-1 = bala preparada (pode estar vazia: aí atira comum)
    int selecionado() const { return sel_; }
    void selecionar(int i);
    void proximo();  // comum -> próximos slots cheios -> comum
    bool preparadaNaVez() const { return sel_ >= 0 && slots_[size_t(sel_)].cheio; }
    const Circuito* circuitoNaVez() const { return preparadaNaVez() ? &slots_[size_t(sel_)].circuito : nullptr; }
    const SlotMunicao* slotNaVez() const { return preparadaNaVez() ? &slots_[size_t(sel_)] : nullptr; }
    // Tira a bala da vez do slot (o doador queima no disparo) e volta para a munição comum.
    Circuito consumirNaVez();

private:
    std::vector<SlotMunicao> slots_;
    int estoque_[N_DOADORES]{};
    int estoqueInicial_ = 0;
    int sel_ = -1;
};
