#pragma once
#include "core/hair_profile.h"
#include "core/mat4.h"

// Deslocamentos no espaço de repouso da cabeça, em metros.
struct DeformacaoCabelo {
    std::array<Vec3f, MAX_GUIAS_CABELO> meio{}, ponta{};
};

class FisicaCabelo {
public:
    explicit FisicaCabelo(const PerfilCabelo& perfil);
    void reiniciar() { iniciada_ = false; deformacao_ = {}; }
    void atualizar(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f vento);
    bool iniciada() const { return iniciada_; }
    const PerfilCabelo& perfil() const { return perfil_; }
    const DeformacaoCabelo& deformacao() const { return deformacao_; }

private:
    struct No { Vec3f pos, vel; };
    struct EstadoGuia {
        std::array<No, 2> moveis{};
        std::array<Vec3f, 3> alvosAnteriores{};
        std::array<float, 2> comprimentos{}, rigidez{}, limites{};
    };
    PerfilCabelo perfil_;
    std::array<EstadoGuia, MAX_GUIAS_CABELO> guias_{};
    std::array<Vec3f, 5> colisoresAnteriores_{};
    bool iniciada_ = false;
    DeformacaoCabelo deformacao_{};
};
