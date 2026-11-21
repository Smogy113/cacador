#pragma once
#include <cstdint>

float ruido2D(float x, float z, uint32_t semente);  // -1..1, suave
float fbm2D(float x, float z, uint32_t semente, int oitavas);
uint32_t hash32(uint32_t x);
float aleatorio01(uint32_t& estado);  // gerador simples e deterministico
