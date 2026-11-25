#pragma once

#include <string>
#include "core/quality.h"
#include "raylib.h"

// Menu inicial: um cartaz de papel pregado sobre a paisagem da entrada (geometria em core/cartaz.h).
// Telas: Principal (Jogar, Personagem, Configurações, Sair), Personagem (penteados) e Configurações
// (qualidade). Setas, Enter, Esc e mouse; o cartaz é torto, então o clique é testado dentro da folha.
class MenuInicial {
public:
    enum class Acao { Nenhuma, Jogar, Sair };

    void carregar();
    void descarregar();
    Acao atualizar(ConfigJogo& config);
    void desenhar(const ConfigJogo& config, bool carregando = false) const;
    // Abre direto numa tela ("principal", "personagem", "configuracoes"); para prints do menu.
    void abrir(const std::string& tela);

private:
    enum class Tela { Principal, Personagem, Configuracoes };
    // Área da opção `indice` em unidades da folha do cartaz.
    Rectangle item(int indice) const;
    int totalDeItens() const;
    void focar(int indice);
    Texture2D fundo_{};
    int foco_ = 0;
    double focoDesde_ = 0;  // quando o foco mudou (o círculo de tinta se desenha a partir daí)
    Tela tela_ = Tela::Principal;
};
