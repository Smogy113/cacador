#pragma once
#include <vector>
#include "core/math.h"

class Terreno;
struct Mapa;

// Caminho tipo GPS no mapa: A* numa grade sobre o terreno. Estrada e barata, rio (fora da
// estrada/ponte) e encosta ingreme sao caros, montanha fora da area jogavel e bloqueada.
class Navegador {
public:
    static constexpr float CELULA = 5.f;
    // com o mapa, as pontes contam como caminho (o rio so e atravessado nelas)
    explicit Navegador(const Terreno& t, const Mapa* mapa = nullptr);
    // pontos de 'de' ate 'para' (simplificados: so onde o caminho muda de direcao)
    std::vector<Vec3f> rota(Vec3f de, Vec3f para) const;
    // um ponto por celula (para testes e desenho detalhado)
    std::vector<Vec3f> rotaDensa(Vec3f de, Vec3f para) const;

private:
    int n_ = 0;
    float origem_ = 0;
    std::vector<float> custo_;  // por celula; < 0 = bloqueada
    std::vector<float> alt_;
    int celula(float v) const;
    Vec3f centro(int i, int j) const;
};
