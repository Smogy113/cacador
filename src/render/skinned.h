#pragma once
#include <vector>
#include "core/body.h"
#include "raylib.h"

Matrix paraRaylib(const Mat4& m);

// Malha com pesos por osso; a deformacao e feita no vertex shader (uniform ossos[17]).
class MalhaEsqueleto {
public:
    void carregar(const MalhaPele& m);
    void descarregar();
    void desenhar(Material& mat, const std::vector<Mat4>& ossos, Matrix modelo) const;
    int triangulos() const { return mesh_.triangleCount; }

private:
    Mesh mesh_{};
    bool ok_ = false;
};
