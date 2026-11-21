#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/places.h"

struct Cartaz {
    std::string nome, apelido, crime;
    int recompensaVivo = 0, recompensaMorto = 0;  // em moedas de prata
    std::string ultimoVisto;                       // nome de um lugar do mapa (com espacos)
    uint32_t rosto = 0;                            // semente do retrato
};

std::vector<Cartaz> gerarCartazes(uint32_t semente, int quantidade, const Mapa& mapa);
// "Ruinas_de_Corvalto" -> "Ruinas de Corvalto"
std::string nomeLegivel(const std::string& nomeArquivo);
