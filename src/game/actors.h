#pragma once
#include "raylib.h"
#include "core/math.h"

struct Roupa {
    Rgb casaco, calca, camisa, chapeu, pele, lenco;
    bool chapeuAba = true, casacoLongo = true;
};

// Pessoa low-poly articulada (pernas e bracos balancam ao andar).
class ModeloPessoa {
public:
    void carregar(const Roupa& r);
    void descarregar();
    // fase: ciclo da passada (radianos); amplitude: 0 parado .. 1 correndo
    void desenhar(Material& mat, Vec3f pes, float yaw, float fase, float amplitude) const;
    void desenharSentado(Material& mat, Vec3f quadril, float yaw) const;  // montado no cavalo

private:
    Mesh corpo_{}, perna_{}, braco_{};
    bool ok_ = false;
};

// Sombra simples (disco escuro) usada no nivel Baixo.
class SombraBolha {
public:
    void carregar();
    void descarregar();
    void desenhar(Vec3f pos, Vec3f normalChao, float raio) const;

private:
    Model disco_{};
};
