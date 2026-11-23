#include "core/motion.h"
#include <algorithm>
#include <cmath>
#include "core/terrain.h"

float limitarDt(float dt) { return dt > 0.1f ? 0.1f : dt; }

Vec3f frenteDe(float yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }

static float angDif(float a, float b) {
    float d = std::fmod(b - a + 3.14159265f, 6.2831853f);
    if (d < 0) d += 6.2831853f;
    return d - 3.14159265f;
}

// Leva a velocidade v ate o alvo com aceleracao constante 'taxa' e devolve a distancia percorrida em dt
// (exata mesmo com dt grande: acelera ate o alvo e segue nele o resto do tempo).
static float rampa(float& v, float alvo, float taxa, float dt) {
    float dv = alvo - v;
    float t = std::min(dt, std::abs(dv) / taxa);  // tempo acelerando (ou freando)
    float v1 = v + (dv > 0 ? taxa : -taxa) * t;
    float dist = (v + v1) * 0.5f * t + v1 * (dt - t);
    v = v1;
    return dist;
}

// chao sob o corpo: terreno ou piso (ponte, assoalho)
static float alturaDoChao(const Vec3f& p, const Terreno& t, const MundoColisao& col) {
    return std::max(t.altura(p.x, p.z), col.alturaPiso(p.x, p.z, p.y));
}

void moverAPe(Corpo& c, const Entrada& e, float yawCamera, float dt, const Terreno& t, const MundoColisao& col) {
    Vec3f f = frenteDe(yawCamera);
    Vec3f dir{-std::cos(yawCamera), 0, std::sin(yawCamera)};  // direita da camera
    Vec3f m = f * e.frente + dir * e.lado;
    float l = len(m);
    float alvo = 0.f;  // velocidade que o comando pede
    if (l >= 0.01f) {
        c.dir = normalizar(m);
        alvo = (e.correr ? VEL_CORRER : VEL_ANDAR) * std::min(l, 1.f);
        float chao = std::max(t.altura(c.pos.x, c.pos.z),
                              col.alturaPiso(c.pos.x, c.pos.z, std::max(c.pos.y, t.altura(c.pos.x, c.pos.z))));
        if (chao < t.nivelAgua() - 0.6f) alvo *= 0.45f;  // dentro do rio
        // o corpo vira para onde vai andar; fora da mira, enquanto nao estiver de frente ele segura o passo
        float rumo = std::atan2(c.dir.x, c.dir.z);
        c.yaw += angDif(c.yaw, rumo) * std::min(1.f, dt * 12.f);
        if (!e.mirar) alvo *= 0.5f + 0.5f * std::cos(angDif(c.yaw, rumo));
    }
    float taxa = alvo > c.vel ? (alvo > VEL_ANDAR + 0.1f ? ACEL_CORRER : ACEL_ANDAR) : FREIO_A_PE;
    float total = rampa(c.vel, alvo, taxa, dt);
    // anda em pedacos de ate 25 cm para a colisao nunca deixar atravessar parede
    int passos = std::max(1, int(std::ceil(total / 0.25f)));
    for (int i = 0; i < passos; i++) {
        c.pos = c.pos + c.dir * (total / passos);
        c.pos = col.resolver(c.pos, RAIO_PESSOA);
        c.pos.y = alturaDoChao(c.pos, t, col);
    }
}
