#pragma once
#include <optional>
#include "core/collision.h"
#include "core/math.h"

class Terreno;

// Órbita com colisão em terceira pessoa e visão a partir dos olhos em primeira pessoa.
class RigCamera {
public:
    static constexpr float PITCH_MIN = -20.f * 0.0174533f, PITCH_MAX = 60.f * 0.0174533f;
    static constexpr float DIST_MIN_TERCEIRA = .9f;
    float yaw = 0, pitch = 0.25f;
    void ajustarZoom(float rodinha, float distanciaPadrao);
    float distanciaZoom(float distanciaPadrao) const;
    bool primeiraPessoa() const { return zoom_ && *zoom_ == 0.f; }
    void girar(float mouseDx, float mouseDy, bool primeiraPessoa);
    void posicionar(Vec3f pivo, float distancia, float ombro, const Terreno& t, const MundoColisao& col, float dt);
    Vec3f posicao() const { return pos_; }
    Vec3f olhar() const { return olhar_; }

private:
    Vec3f pos_{}, olhar_{};
    std::optional<float> zoom_;  // zero seleciona primeira pessoa, ausência usa o padrão a pé ou a cavalo
    float dist_ = 4.f, distSuave_ = 4.f;
};
