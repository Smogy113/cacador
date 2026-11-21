#pragma once
#include <string>

enum class Nivel { Baixo, Medio, Alto };

struct Efeitos {
    bool sombras, bloom, raiosDeLuz;
    int resolucaoSombra;
    float escalaRender;
    float distanciaVisao;
};

Efeitos efeitosDe(Nivel n);
