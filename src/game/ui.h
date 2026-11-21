#pragma once
#include <string>
#include <vector>
#include "core/bounty.h"
#include "core/places.h"

// A fonte padrao da raylib so tem ASCII: troca letras acentuadas.
std::string semAcentos(const std::string& s);
// aceito = indice do cartaz com recompensa ativa neste quadro (-1 = nenhum)
void desenharQuadro(const std::vector<Cartaz>& cartazes, int selecionado, const std::string& lugar, int aceito = -1);
void desenharAvisos(const std::vector<ErroMapa>& erros, float tempoDesdeInicio);
void desenharDica(const char* texto);
// HUD do tiroteio
void desenharHud(float vida01, int balas, bool recarregando, int dinheiro, bool mirando, float marcaAcerto, bool marcaMatou);
// posObjetivo = posicaoNaBussola (0..1; fora disso fica preso na borda com seta)
void desenharBussola(float yawCamera, bool temObjetivo, float posObjetivo, float distanciaM);
void desenharMensagem(const std::string& texto, float tempoRestante);  // some nos ultimos 0,5 s
void desenharBordaDano(float vida01);                                  // vermelho nas bordas abaixo de 35%
void desenharTelaMorte(float t);                                       // escurece
