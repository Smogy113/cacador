#pragma once
#include <string>
#include "core/penteado.h"

enum class Nivel { Baixo, Medio, Alto };

struct ConfigJogo {
    Nivel nivel = Nivel::Baixo;
    Penteado penteado = Penteado::Sem;
};

struct Efeitos {
    bool sombras, bloom, raiosDeLuz;
    int resolucaoSombra;
    float escalaRender;
    float distanciaVisao;
    bool ssao = false;  // oclusao de ambiente em tela
};

Efeitos efeitosDe(Nivel n);

// Sugere um nivel pelo nome da placa de video (GL_RENDERER).
Nivel sugerirNivel(const std::string& rendererGL);
// config.txt ao lado do executavel, com qualidade e cabelo do jogador.
ConfigJogo lerConfig(const std::string& caminho, ConfigJogo padrao);
bool temConfig(const std::string& caminho);
void salvarConfig(const std::string& caminho, ConfigJogo config);
const char* nomeNivel(Nivel n);
