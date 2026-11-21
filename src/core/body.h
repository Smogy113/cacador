#pragma once
#include <cstdint>
#include <vector>
#include "core/skeleton.h"

enum OssoHumano {
    QUADRIL, COLUNA, PEITO, PESCOCO, CABECA,
    BRACO_E, ANTEBRACO_E, MAO_E, BRACO_D, ANTEBRACO_D, MAO_D,
    COXA_E, CANELA_E, PE_E, COXA_D, CANELA_D, PE_D,
    N_OSSOS_HUMANO
};

struct Proporcoes {
    float altura = 1.8f, ombros = 1.f, quadril = 1.f, massa = 1.f;
};

struct Visual {
    Rgb pele{0.78f, 0.58f, 0.45f}, camisa{0.8f, 0.76f, 0.66f}, casaco{0.36f, 0.26f, 0.18f}, calca{0.25f, 0.22f, 0.2f},
        botas{0.18f, 0.12f, 0.08f}, chapeu{0.22f, 0.16f, 0.11f}, lenco{0.55f, 0.12f, 0.1f}, cabelo{0.2f, 0.14f, 0.1f};
    bool sobretudo = true, chapeuAba = true, barba = false;
};

// Vertice com ate 2 ossos: osso[0] com "peso", osso[1] com 1 - peso.
struct VertPele {
    Vec3f pos, normal;
    Rgb cor;
    float ao = 1.f;
    uint8_t osso[2] = {0, 0};
    float peso = 1.f;
};

struct MalhaPele {
    std::vector<VertPele> v;
    std::vector<uint16_t> idx;  // triangulos
    size_t triangulos() const { return idx.size() / 3; }
};

Esqueleto esqueletoHumano(const Proporcoes& p);
MalhaPele corpoHumano(const Proporcoes& p, const Visual& vis);
