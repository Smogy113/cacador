#include "core/camrig.h"
#include <algorithm>
#include <cmath>
#include "core/terrain.h"

static constexpr float SENS = 0.0035f;
static constexpr float DIST_MAX_ZOOM = 9.f, PASSO_ZOOM = .65f;

void RigCamera::ajustarZoom(float rodinha, float distanciaPadrao) {
    if (rodinha == 0.f) return;
    float alcance = zoom_.value_or(std::max(0.f, distanciaPadrao - DIST_MIN_TERCEIRA));
    zoom_ = std::clamp(alcance - rodinha * PASSO_ZOOM, 0.f, DIST_MAX_ZOOM - DIST_MIN_TERCEIRA);
}

float RigCamera::distanciaZoom(float distanciaPadrao) const {
    if (!zoom_) return distanciaPadrao;
    return primeiraPessoa() ? 0.f : DIST_MIN_TERCEIRA + *zoom_;
}

void RigCamera::girar(float mdx, float mdy, bool primeiraPessoa) {
    yaw -= mdx * SENS;
    constexpr float LIMITE_OLHOS = 75.f * 0.0174533f;
    pitch = std::clamp(pitch + mdy * SENS, primeiraPessoa ? -LIMITE_OLHOS : PITCH_MIN,
                      primeiraPessoa ? LIMITE_OLHOS : PITCH_MAX);
}

void RigCamera::posicionar(Vec3f pivo, float distancia, float ombro, const Terreno& t, const MundoColisao& col, float dt) {
    Vec3f tras{-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
    if (distancia == 0.f) {
        pos_ = pivo;
        olhar_ = pivo - tras;
        dist_ = distSuave_ = DIST_MIN_TERCEIRA;
        return;
    }
    distSuave_ = dt > 0.f ? lerp(distSuave_, distancia, 1.f - std::exp(-16.f * dt)) : distancia;
    Vec3f direita{-std::cos(yaw), 0, std::sin(yaw)};
    // ombro deslocado para a direita, mas nunca para dentro de uma parede ao lado
    float lado = ombro * smoothstep(DIST_MIN_TERCEIRA, 2.1f, distSuave_);
    if (!col.raioLivre(pivo, pivo + direita * (lado + 0.15f))) {
        float lo = 0.f, hi = lado + 0.15f;
        for (int k = 0; k < 10; k++) {
            float m = (lo + hi) * 0.5f;
            if (col.raioLivre(pivo, pivo + direita * m)) lo = m; else hi = m;
        }
        lado = std::max(0.f, lo - 0.15f);
    }
    Vec3f apoio = pivo + direita * lado;

    // distancia livre: nem caixas nem terreno entre o ombro e a camera
    float livre = distSuave_;
    for (float d = 0.3f; d <= distSuave_; d += 0.25f) {
        Vec3f p = apoio + tras * d;
        if (p.y < t.altura(p.x, p.z) + 0.3f) { livre = std::max(0.3f, d - 0.3f); break; }
    }
    if (!col.raioLivre(apoio, apoio + tras * livre)) {
        float lo = 0.f, hi = livre;
        for (int k = 0; k < 12; k++) {
            float m = (lo + hi) * 0.5f;
            if (col.raioLivre(apoio, apoio + tras * m)) lo = m; else hi = m;
        }
        livre = std::max(0.3f, lo - 0.2f);
    }
    // aproxima rapido, afasta devagar (sem tranco)
    if (dt <= 0.f || livre < dist_) dist_ = livre;
    else dist_ += (livre - dist_) * std::min(1.f, dt * 4.f);

    pos_ = apoio + tras * dist_;
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
    olhar_ = apoio + Vec3f{0, 0.1f, 0};
}
