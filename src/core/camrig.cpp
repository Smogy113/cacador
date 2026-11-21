#include "core/camrig.h"
#include <algorithm>
#include <cmath>
#include "core/terrain.h"

static constexpr float SENS = 0.0035f;

void RigCamera::atualizar(Vec3f pes, float mdx, float mdy, const Terreno& t, const MundoColisao& col, float dt) {
    yaw -= mdx * SENS;
    pitch = std::clamp(pitch + mdy * SENS, PITCH_MIN, PITCH_MAX);
    Vec3f pivo = pes + Vec3f{0, 1.6f, 0};
    Vec3f direita{-std::cos(yaw), 0, std::sin(yaw)};
    // ombro deslocado para a direita, mas nunca para dentro de uma parede ao lado
    float lado = ombroAlvo;
    if (!col.raioLivre(pivo, pivo + direita * (lado + 0.15f))) {
        float lo = 0.f, hi = lado + 0.15f;
        for (int k = 0; k < 10; k++) {
            float m = (lo + hi) * 0.5f;
            if (col.raioLivre(pivo, pivo + direita * m)) lo = m; else hi = m;
        }
        lado = std::max(0.f, lo - 0.15f);
    }
    Vec3f ombro = pivo + direita * lado;
    Vec3f tras{-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};

    // distancia livre: nem caixas nem terreno entre o ombro e a camera
    float livre = distAlvo;
    for (float d = 0.3f; d <= distAlvo; d += 0.25f) {
        Vec3f p = ombro + tras * d;
        if (p.y < t.altura(p.x, p.z) + 0.3f) { livre = std::max(0.3f, d - 0.3f); break; }
    }
    if (!col.raioLivre(ombro, ombro + tras * livre)) {
        float lo = 0.f, hi = livre;
        for (int k = 0; k < 12; k++) {
            float m = (lo + hi) * 0.5f;
            if (col.raioLivre(ombro, ombro + tras * m)) lo = m; else hi = m;
        }
        livre = std::max(0.3f, lo - 0.2f);
    }
    // aproxima rapido, afasta devagar (sem tranco)
    if (livre < dist_) dist_ = livre;
    else dist_ += (livre - dist_) * std::min(1.f, dt * 4.f);

    pos_ = ombro + tras * dist_;
    pos_.y = std::max(pos_.y, t.altura(pos_.x, pos_.z) + 0.5f);
    // depois de subir acima do chao, confere de novo as caixas: se bater, aproxima do pivo
    if (!col.raioLivre(pivo, pos_)) {
        float lo = 0.f, hi = 1.f;
        for (int k = 0; k < 12; k++) {
            float m = (lo + hi) * 0.5f;
            if (col.raioLivre(pivo, pivo + (pos_ - pivo) * m)) lo = m; else hi = m;
        }
        pos_ = pivo + (pos_ - pivo) * std::max(0.f, lo - 0.05f);
        pos_.y = std::max(pos_.y, t.altura(pos_.x, pos_.z) + 0.5f);
    }
    olhar_ = ombro + Vec3f{0, 0.1f, 0};
}
