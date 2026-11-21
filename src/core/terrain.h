#pragma once
#include <cstdint>
#include <vector>
#include "core/math.h"
#include "core/places.h"

class Terreno {
public:
    static constexpr float META = 750.f;  // mundo de -META..META
    static constexpr int RES = 1501;      // 1 amostra por metro
    Terreno(const Mapa& mapa, uint32_t semente);
    float altura(float x, float z) const;
    Vec3f normal(float x, float z) const;
    float mascaraEstrada(float x, float z) const;  // 0..1
    float mascaraRio(float x, float z) const;      // 0..1 (1 = agua)
    float nivelAgua() const;
    bool dentroDoJogavel(float x, float z) const;  // |x|,|z| <= 700
    uint32_t semente() const { return semente_; }

private:
    float amostra(const std::vector<float>& g, float x, float z) const;
    std::vector<float> alt_, estrada_, rio_;
    uint32_t semente_;
};
