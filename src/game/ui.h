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
