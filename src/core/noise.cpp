#include "core/noise.h"
#include <cmath>

uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float aleatorio01(uint32_t& estado) {
    estado = hash32(estado + 0x9e3779b9U);
    return (estado & 0xffffff) / float(0x1000000);
}

static float gradiente(int ix, int iz, uint32_t s, float dx, float dz) {
    uint32_t h = hash32(uint32_t(ix) * 73856093U ^ uint32_t(iz) * 19349663U ^ hash32(s));
    float ang = (h & 0xffff) / 65536.f * 6.2831853f;
    return std::cos(ang) * dx + std::sin(ang) * dz;
}

static float suave(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }

float ruido2D(float x, float z, uint32_t semente) {
    int x0 = (int)std::floor(x), z0 = (int)std::floor(z);
    float fx = x - x0, fz = z - z0;
    float a = gradiente(x0, z0, semente, fx, fz);
    float b = gradiente(x0 + 1, z0, semente, fx - 1, fz);
    float c = gradiente(x0, z0 + 1, semente, fx, fz - 1);
    float d = gradiente(x0 + 1, z0 + 1, semente, fx - 1, fz - 1);
    float u = suave(fx), v = suave(fz);
    float r = (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v;
    r *= 1.41421356f;  // gradiente 2D fica em ~[-0.707, 0.707]
    return r < -1 ? -1 : (r > 1 ? 1 : r);
}

float fbm2D(float x, float z, uint32_t semente, int oitavas) {
    float soma = 0, amp = 1, norm = 0, f = 1;
    for (int i = 0; i < oitavas; i++) {
        soma += ruido2D(x * f, z * f, semente + i * 101) * amp;
        norm += amp;
        amp *= 0.5f;
        f *= 2.f;
    }
    return soma / norm;
}
