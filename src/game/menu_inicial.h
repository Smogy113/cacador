#pragma once

#include "core/quality.h"
#include "raylib.h"

class MenuInicial {
public:
    enum class Acao { Nenhuma, Jogar, Sair };

    void carregar();
    void descarregar();
    Acao atualizar(Nivel& nivel);
    void desenhar(Nivel nivel, bool carregando = false) const;

private:
    Rectangle item(int indice) const;
    Texture2D fundo_{};
    int foco_ = 0;
    bool configuracoes_ = false;
};
