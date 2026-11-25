#pragma once
// Peças comuns aos penteados da base humana (hair.cpp: ondulado; cabelo_longo.cpp: longo).
// Medidas no espaço de 1,80 m da base humana; o penteado aplica a escala do personagem.
#include <vector>
#include "core/loft.h"

namespace loft::cranio {

inline constexpr Vec3f CENTRO{0, 1.698f, .055f};  // centro da cabeça da base humana
inline constexpr float RAD = PI_F / 180.f;

// Direção a partir do CENTRO, em graus. giro: 0 = frente (+z), 90 = lado +x, 180 = nuca.
// altura: 0 = na altura do centro, 90 = topo da cabeça.
Vec3f direcao(float giro, float altura);
// Altura (graus) da linha do cabelo nesse giro: desce na testa, recua nas entradas e cai na nuca.
float linhaDoCabelo(float giro);
// Folga entre o crânio e a camada de cabelo: mais volume no alto e nos lados, menos na risca.
float volumeCoroa(Vec3f d);

// Pele da cabeça (acima do pescoço) consultada por raios saindo do CENTRO.
class Cranio {
public:
    Cranio(const MalhaPele& m, float escala);
    // Ponto da pele na direção d, afastado `folga` metros para fora.
    Vec3f superficie(Vec3f d, float folga) const;

private:
    struct Face {
        Vec3f a, u, v;  // primeiro vértice relativo ao centro e duas arestas
    };
    std::vector<Face> faces_;
};

// Calota de cabelo colada ao crânio, da linha do cabelo ao topo: esconde a pele entre as mechas.
void couroCabeludo(MalhaPele& m, const Cranio& cranio, const CorpoDoCabelo& corpo, float escala, Rgb cor);

}  // namespace loft::cranio
