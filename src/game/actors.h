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
    void desenharSimples(Material& mat, Vec3f pes, float yaw) const;       // longe: 1 chamada so

private:
    Mesh corpo_{}, perna_{}, braco_{}, inteiro_{};
    bool ok_ = false;
};

// Cavalo low-poly com patas animadas (passo, trote e galope).
class ModeloCavalo {
public:
    void carregar(Rgb pelo, Rgb crina, bool selado);
    void descarregar();
    // andadura: 0 parado, 1 passo, 2 trote, 3 galope
    void desenhar(Material& mat, Vec3f pos, float yaw, float fase, int andadura) const;
    static Vec3f assento(Vec3f pos, float yaw);  // onde o cavaleiro senta

private:
    Mesh corpo_{}, pata_{};
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
