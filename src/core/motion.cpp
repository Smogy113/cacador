#include "core/motion.h"
#include <cmath>
#include "core/terrain.h"

float limitarDt(float dt) { return dt > 0.1f ? 0.1f : dt; }

Vec3f frenteDe(float yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }

static float angDif(float a, float b) {
    float d = std::fmod(b - a + 3.14159265f, 6.2831853f);
    if (d < 0) d += 6.2831853f;
    return d - 3.14159265f;
}

void moverAPe(Corpo& c, const Entrada& e, float yawCamera, float dt, const Terreno& t, const MundoColisao& col) {
    Vec3f f = frenteDe(yawCamera);
    Vec3f dir{-std::cos(yawCamera), 0, std::sin(yawCamera)};  // direita da camera
    Vec3f m = f * e.frente + dir * e.lado;
    float l = len(m);
    if (l < 0.01f) {
        c.vel = 0;
        c.pos = col.resolver(c.pos, RAIO_PESSOA);
        c.pos.y = t.altura(c.pos.x, c.pos.z);
        return;
    }
    m = m * (1.f / std::max(l, 1.f));
    float vel = e.correr ? VEL_CORRER : VEL_ANDAR;
    if (t.altura(c.pos.x, c.pos.z) < t.nivelAgua() - 0.6f) vel *= 0.45f;  // dentro do rio
    float total = vel * len(m) * dt;
    int passos = std::max(1, int(std::ceil(total / 0.25f)));
    Vec3f mn = normalizar(m);
    for (int i = 0; i < passos; i++) {
        c.pos = c.pos + mn * (total / passos);
        c.pos = col.resolver(c.pos, RAIO_PESSOA);
        c.pos.y = t.altura(c.pos.x, c.pos.z);
    }
    c.vel = vel * len(m);
    float alvo = std::atan2(mn.x, mn.z);
    c.yaw += angDif(c.yaw, alvo) * std::min(1.f, dt * 12.f);
}
