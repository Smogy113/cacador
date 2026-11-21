#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/places.h"

enum class Temperamento { Covarde, Teimoso, Fujao };  // se rende / luta ate morrer / foge

struct Cartaz {
    std::string nome, apelido, crime;
    int recompensaVivo = 0, recompensaMorto = 0;  // em moedas de prata
    std::string ultimoVisto;                       // nome de um lugar do mapa (com espacos)
    uint32_t rosto = 0;                            // semente do retrato
    Temperamento temperamento = Temperamento::Teimoso;
    std::string pista;                             // dica no cartaz sobre o temperamento
};

std::vector<Cartaz> gerarCartazes(uint32_t semente, int quantidade, const Mapa& mapa);
// Um cartaz novo para repor no quadro, com nome diferente dos existentes.
Cartaz novoCartaz(uint32_t semente, const Mapa& mapa, const std::vector<Cartaz>& existentes);
// "Ruinas_de_Corvalto" -> "Ruinas de Corvalto"
std::string nomeLegivel(const std::string& nomeArquivo);
