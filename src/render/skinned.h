#pragma once
#include <memory>
#include <vector>
#include "core/body.h"
#include "core/hair_physics.h"
#include "raylib.h"

Matrix paraRaylib(const Mat4& m);

// Malha com pesos por osso; a deformacao e feita no vertex shader (uniform ossos[17]).
class MalhaEsqueleto {
public:
    void carregar(const MalhaPele& m);
    void descarregar();
    void desenhar(Material& mat, const std::vector<Mat4>& ossos, Matrix modelo) const;
    int triangulos() const { return mesh_.triangleCount; }
    bool temCabelo() const { return bool(cabelo_); }
    void atualizarCabelo(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f vento);
    void reiniciarCabelo();

private:
    Mesh mesh_{};
    std::unique_ptr<FisicaCabelo> cabelo_;
    bool ok_ = false;
};
