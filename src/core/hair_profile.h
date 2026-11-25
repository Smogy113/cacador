#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "core/math.h"

inline constexpr std::size_t MAX_GUIAS_PRINCIPAIS = 8;
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB)
// WebGL 1 garante só 128 vetores de uniform no vertex shader (ver render/shaders.cpp).
inline constexpr std::size_t MAX_GUIAS_CABELO = 26;
#else
inline constexpr std::size_t MAX_GUIAS_CABELO = 96;
#endif
inline constexpr uint8_t SEM_GUIA_CABELO = 255;

struct VinculoCabelo {
    uint8_t a = 0, b = 0;
    float pesoB = 0.f;
};

struct GuiaCabelo {
    std::array<Vec3f, 3> pontos{};  // raiz fixa, dobra e ponta no espaço de repouso do modelo
    float flexibilidade = 1.f;
    VinculoCabelo pais;  // nos guias secundários, índices dos dois guias principais mais próximos
    float comprimentoLivre = 0.f;  // arco entre a última região presa e a ponta
};

// Elipsoide da cabeça para as fitas e cápsulas para os nós simulados.
// As extremidades de nuca e ombros acompanham os respectivos ossos.
//
// Orelhas e pescoço são volumes justos ao corpo, que nem os nós da física nem os vértices das fitas
// (na placa de vídeo) atravessam. Existem só nos penteados montados com CorpoDoCabelo (base humana);
// raios zero = sem volume. Volume = cápsula entre a e b com raio por eixo (a == b: elipsoide).
struct ColisoresCabelo {
    Vec3f centroCabeca{}, raiosCabeca{};
    Vec3f cranio{}, nuca{}, pescoco{}, ombroE{}, ombroD{};
    float raioCranio = 0, raioPescoco = 0, raioOmbros = 0;
    Vec3f orelha{}, raiosOrelha{};  // orelha direita (+x), presa à cabeça; a esquerda é o espelho em x
    Vec3f pescocoA{}, pescocoB{}, raiosPescocoJusto{};  // pescoço justo, preso ao osso do pescoço
    Vec3f troncoA{}, troncoB{}, raiosTronco{};          // alto do tronco, preso ao osso do peito
    bool temVolumesJustos() const { return raiosOrelha.x > 0.f; }
};

struct PerfilCabelo {
    std::array<GuiaCabelo, MAX_GUIAS_CABELO> guias{};
    std::size_t quantidade = 0, principais = 0;  // guias principais no início do vetor
    ColisoresCabelo colisores{};
};
