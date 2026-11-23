#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "core/math.h"

inline constexpr std::size_t MAX_GUIAS_CABELO = 8;
inline constexpr uint8_t SEM_GUIA_CABELO = 255;

struct GuiaCabelo {
    std::array<Vec3f, 3> pontos{};  // raiz fixa, dobra e ponta no espaço de repouso do modelo
    float flexibilidade = 1.f;
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
    std::size_t quantidade = 0;
    ColisoresCabelo colisores{};
};
