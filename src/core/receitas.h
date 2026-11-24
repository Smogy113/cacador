#pragma once
// Engenharia reversa dos Circuitos de Queima: o que o caçador já sabe gravar na fogueira e as pistas
// (equipamentos entalhados) que ensinam entalhes e pastas novos quando inspecionadas.
//
// O caçador começa sabendo só a Espiral e a Pólvora Fina. Cada pista inspecionada (E perto dela)
// ensina um entalhe ou uma pasta, que passa a valer na bancada da fogueira. Os objetos doadores não
// são receita: são estoque do Arsenal.
//
// Lógica pura, sem raylib. O jogo espalha pistas na arena e larga uma de cada inimigo de teste
// derrotado (game/combate_reativo.cpp); o painel da fogueira grava com gravar(), mais abaixo.
#include <cstdint>
#include <string>
#include <vector>
#include "core/circuito.h"
#include "core/math.h"

// O que uma pista ensina: um entalhe ou uma pasta.
struct Descoberta {
    enum class Tipo : uint8_t { Entalhe, Pasta };
    Tipo tipo = Tipo::Entalhe;
    Entalhe entalhe = Entalhe::Espiral;  // vale quando tipo == Entalhe
    Pasta pasta = Pasta::PolvoraFina;    // vale quando tipo == Pasta
    static Descoberta de(Entalhe e);
    static Descoberta de(Pasta p);
};
// "entalhe Grade", "pasta Enxofre e Sal"
std::string nome(const Descoberta& d);

class ReceitasConhecidas {
public:
    bool conhece(Entalhe e) const { return (entalhes_ >> int(e)) & 1u; }
    bool conhece(Pasta p) const { return (pastas_ >> int(p)) & 1u; }
    bool conhece(const Descoberta& d) const;
    // Dá para gravar este circuito? (entalhe e pasta conhecidos; o doador é questão de estoque)
    bool permite(const Circuito& c) const { return conhece(c.entalhe) && conhece(c.pasta); }
    // true se era novidade.
    bool aprender(const Descoberta& d);
    void aprenderTudo();  // testes, capturas e --receitas-todas
    int entalhesConhecidos() const;
    int pastasConhecidas() const;

private:
    // um bit por valor do enum; o padrão é o começo do jogo
    uint8_t entalhes_ = uint8_t(1u << int(Entalhe::Espiral));
    uint8_t pastas_ = uint8_t(1u << int(Pasta::PolvoraFina));
};

// Equipamento entalhado no chão: uma cápsula alheia, um frasco, uma cartucheira... Inspecionar ensina.
struct PistaEntalhada {
    Vec3f pos;  // no chão
    float yaw = 0;
    std::string objeto;  // o que o caçador vê: "Frasco de Enxofre rachado"
    Descoberta ensina;
    bool inspecionada = false;
};

// Índice da pista não inspecionada mais perto de q dentro do raio (plano xz); -1 se nenhuma.
int pistaPerto(const std::vector<PistaEntalhada>& pistas, Vec3f q, float raio);
// Examina a pista: marca como inspecionada e ensina a receita. true se aprendeu algo novo
// (inspecionar de novo, ou uma pista de algo já conhecido, não ensina nada).
bool inspecionar(PistaEntalhada& pista, ReceitasConhecidas& receitas);

// Grava na bancada respeitando o que o caçador conhece. O Arsenal confere o resto (slot, receita
// entalhe+doador e estoque). É a porta de entrada do painel da fogueira.
ResultadoPreparo gravar(Arsenal& arsenal, const ReceitasConhecidas& receitas, int slot, const Circuito& c,
                        Preparo preparo = Preparo::Cuidadoso);
