#pragma once
#include <string>
#include "raylib.h"
#include <vector>
#include "core/bounty.h"
#include "core/places.h"

// A fonte padrao da raylib so tem ASCII: troca letras acentuadas.
std::string semAcentos(const std::string& s);
// aceito = indice do cartaz com recompensa ativa neste quadro (-1 = nenhum)
void desenharQuadro(const std::vector<Cartaz>& cartazes, int selecionado, const std::string& lugar, int aceito = -1);
void desenharAvisos(const std::vector<ErroMapa>& erros, float tempoDesdeInicio);
void desenharDica(const char* tecla, const std::string& acao);  // tecla "" = so o texto
// HUD do tiroteio
// vida (barra rústica com o HP exato) ao lado do minimapa (centro/raio), balas e dinheiro no canto
// inferior direito, mira no centro
void desenharHud(float vidaAtual, float vidaMax, int balas, bool recarregando, int dinheiro, bool mirando, float marcaAcerto, bool marcaMatou, Vector2 centroMini, float raioMini);
// posObjetivo = posicaoNaBussola (0..1; fora disso fica preso na borda com seta)
void desenharBussola(float yawCamera, bool temObjetivo, float posObjetivo, float distanciaM);
void desenharMensagem(const std::string& texto, float tempoRestante);  // some nos ultimos 0,5 s
void desenharBordaDano(float vida01);                                  // vermelho nas bordas abaixo de 35%
void desenharTelaMorte(float t);                                       // escurece
void desenharPausa(int nivelGrafico, const char* versao);
