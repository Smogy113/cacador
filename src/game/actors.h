#pragma once
#include "raylib.h"
#include "core/body.h"
#include "core/math.h"
#include "render/skinned.h"


// Pessoa anatomica com esqueleto (malha unica deformada na placa de video).
class ModeloHumano {
public:
    void carregar(const Proporcoes& p, const Visual& v);
    void descarregar();
    void desenhar(Material& mat, Vec3f pes, float yaw, const Pose& pose) const;
    const Esqueleto& esqueleto() const { return esq_; }
    int triangulos() const { return malha_.triangulos(); }

private:
    Esqueleto esq_;
    MalhaEsqueleto malha_;
    mutable std::vector<Mat4> ossos_;
};

// Cavalo anatomico com esqueleto.
class ModeloCavaloAnat {
public:
    void carregar(const VisualCavalo& v);
    void descarregar();
    void desenhar(Material& mat, Vec3f pos, float yaw, const Pose& pose) const;
    // onde fica o quadril do cavaleiro, no mundo (acompanha o balanco do corpo)
    Vec3f assento(Vec3f pos, float yaw, const Pose& pose) const;
    const Esqueleto& esqueleto() const { return esq_; }
    int triangulos() const { return malha_.triangulos(); }

private:
    Esqueleto esq_;
    MalhaEsqueleto malha_;
    mutable std::vector<Mat4> ossos_;
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
