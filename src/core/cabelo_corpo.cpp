#include "core/cabelo_corpo.h"
#include <algorithm>
#include <cmath>

namespace {
Vec3f dividir(Vec3f a, Vec3f b) { return {a.x / b.x, a.y / b.y, a.z / b.z}; }
Vec3f multiplicar(Vec3f a, Vec3f b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
// O cabelo só alcança a pele do alto das costas para cima (espaço de 1,80 m).
constexpr float Y_PELE_DO_CABELO = 1.25f;
}  // namespace

float Elipsoide::distancia(Vec3f p) const {
    const Vec3f q = dividir(p - centro, raios);
    const float s = len(q);
    // Em unidades do menor raio: subestima a distância nos lados mais compridos (seguro para colisão).
    return (s - 1.f) * std::min({raios.x, raios.y, raios.z});
}

Vec3f Elipsoide::paraFora(Vec3f p, float folga) const {
    const Vec3f r = raios + Vec3f{folga, folga, folga};
    const Vec3f q = dividir(p - centro, r);
    const float s = len(q);
    if (s >= 1.f) return p;
    const Vec3f direcao = s > 1e-6f ? q * (1.f / s) : Vec3f{1, 0, 0};
    return centro + multiplicar(direcao, r);
}

bool Elipsoide::atravessa(Vec3f a, Vec3f b) const {
    // Na esfera unitária: o ponto do segmento mais perto do centro está dentro?
    const Vec3f qa = dividir(a - centro, raios), qb = dividir(b - centro, raios);
    const Vec3f d = qb - qa;
    const float t = std::clamp(-dot(qa, d) / std::max(dot(d, d), 1e-12f), 0.f, 1.f);
    const Vec3f q = qa + d * t;
    return dot(q, q) < 1.f;
}

CorpoDoCabelo::CorpoDoCabelo(const MalhaPele& malha, float yMinimo, Elipsoide orelhaDireita)
    : pele_(peleParaCabelo(malha, yMinimo)), orelhas_{orelhaDireita, orelhaDireita.espelhadoEmX()} {}

Vec3f CorpoDoCabelo::paraFora(Vec3f p, float folga) const {
    // Sair da orelha pode encostar no crânio atrás dela e vice-versa: algumas voltas resolvem.
    for (int volta = 0; volta < 3; ++volta) {
        for (const Elipsoide& e : orelhas_) p = e.paraFora(p, folga);
        p = pele_.paraFora(p, folga);
        if (std::all_of(orelhas_.begin(), orelhas_.end(), [&](const Elipsoide& e) { return e.distancia(p) >= 0.f; })) break;
    }
    return p;
}

float CorpoDoCabelo::distancia(Vec3f p) const {
    float d = pele_.maisProximo(p).distancia;
    for (const Elipsoide& e : orelhas_) d = std::min(d, e.distancia(p));
    return d;
}

bool CorpoDoCabelo::atravessa(Vec3f a, Vec3f b) const {
    return pele_.atravessa(a, b) || orelhas_[0].atravessa(a, b) || orelhas_[1].atravessa(a, b);
}

CorpoDoCabelo corpoDaBaseHumana(const MalhaPele& malha, float escala) {
    return CorpoDoCabelo(malha, Y_PELE_DO_CABELO * escala, ORELHA_DIREITA_BASE.escalado(escala));
}
