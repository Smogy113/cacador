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

// Sugere um nivel pelo nome da placa de video (GL_RENDERER).
Nivel sugerirNivel(const std::string& rendererGL);
// config.txt ao lado do executavel: uma linha "qualidade=baixo|medio|alto"
Nivel lerConfig(const std::string& caminho, Nivel padrao);
bool temConfig(const std::string& caminho);
void salvarConfig(const std::string& caminho, Nivel n);
const char* nomeNivel(Nivel n);
