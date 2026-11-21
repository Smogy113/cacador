#pragma once
#include <string>
#include "raylib.h"

// Estilo da interface: fontes (Rye para titulos, EB Garamond para texto; licenca OFL em data/fontes),
// cores de papel/couro e pecas de desenho (texto com sombra suave, teclas, faixas que somem nas pontas).
namespace estilo {
void carregar();
void descarregar();

enum class Fonte { Titulo, Texto, Forte };
enum Alinha { ESQ = 0, CENTRO = 1, DIR = 2 };

const Color MARFIM{238, 228, 206, 255}, MARFIM_ESC{200, 186, 158, 255}, OURO{214, 176, 98, 255}, SANGUE{176, 42, 30, 255},
    TINTA{52, 36, 22, 255}, SOMBRA{0, 0, 0, 150};

Color comAlfa(Color c, float a);
float largura(Fonte f, const std::string& s, float tam);
void texto(Fonte f, const std::string& s, float x, float y, float tam, Color c, int alinha = ESQ, bool sombra = true);
// tecla desenhada como uma pecinha (ex.: "E") seguida da acao; retorna a largura total
float tecla(const std::string& t, float x, float y, float tam);
// faixa horizontal escura que some nas pontas (fundo de mensagens)
void faixa(float cx, float y, float largura, float altura, float alfa);
// linha fina dourada que some nas pontas
void linha(float cx, float y, float largura, Color c);
}  // namespace estilo
