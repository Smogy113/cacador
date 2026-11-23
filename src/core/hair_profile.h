#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "core/math.h"

inline constexpr std::size_t MAX_GUIAS_PRINCIPAIS = 8;
inline constexpr std::size_t MAX_GUIAS_CABELO = 32;
inline constexpr uint8_t SEM_GUIA_CABELO = 255;

struct VinculoCabelo {
    uint8_t a = 0, b = 0;
    float pesoB = 0.f;
};

struct GuiaCabelo {
    std::array<Vec3f, 3> pontos{};  // raiz fixa, dobra e ponta no espaço de repouso do modelo
    float flexibilidade = 1.f;
    VinculoCabelo pais;  // nos guias secundários, índices dos dois guias principais mais próximos
};

// Elipsoide da cabeça para as fitas e cápsulas para os nós simulados.
// As extremidades de nuca e ombros acompanham os respectivos ossos.
struct ColisoresCabelo {
    Vec3f centroCabeca{}, raiosCabeca{};
    Vec3f cranio{}, nuca{}, pescoco{}, ombroE{}, ombroD{};
    float raioCranio = 0, raioPescoco = 0, raioOmbros = 0;
};

struct PerfilCabelo {
    std::array<GuiaCabelo, MAX_GUIAS_CABELO> guias{};
    std::size_t quantidade = 0, principais = 0;  // guias principais no início do vetor
    ColisoresCabelo colisores{};
};
