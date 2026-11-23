#pragma once

#include "core/quality.h"
#include "raylib.h"

class MenuInicial {
public:
    enum class Acao { Nenhuma, Jogar, Sair };

    void carregar();
    void descarregar();
    Acao atualizar(ConfigJogo& config);
    void desenhar(const ConfigJogo& config, bool carregando = false) const;

private:
    Rectangle item(int indice) const;
    enum class Tela { Principal, Personagem, Configuracoes };
    Texture2D fundo_{};
    int foco_ = 0;
    Tela tela_ = Tela::Principal;
};
