#pragma once
#include <array>
#include "core/mat4.h"

// Guias na ordem esquerda, direita e nuca. Deslocamentos no espaco de repouso
// da cabeca, em metros, para a malha e o mapa de sombras usarem o mesmo movimento.
struct DeformacaoCabelo {
    std::array<Vec3f, 3> meio{}, ponta{};
};

class FisicaCabelo {
public:
    void reiniciar() { iniciada_ = false; deformacao_ = {}; }
    void atualizar(float dt, const Mat4& cabeca, const Mat4& pescoco, float escala, Vec3f vento);
    const DeformacaoCabelo& deformacao() const { return deformacao_; }

private:
    struct No { Vec3f pos, vel; };
    std::array<std::array<No, 2>, 3> guias_{};
    // Guarda as ancoras do quadro anterior para mover os pontos fixos ao longo dos subpassos.
    std::array<std::array<Vec3f, 3>, 3> alvosAnteriores_{};
    Vec3f cranioAnterior_, nucaAnterior_, pescocoAnterior_;
    bool iniciada_ = false;
    DeformacaoCabelo deformacao_{};
};
