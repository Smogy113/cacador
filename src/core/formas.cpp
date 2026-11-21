#include "core/formas.h"
#include <algorithm>
#include <cmath>

namespace {
Vec3f paraLocal(const FormaCena& f, Vec3f p) {  // inverso de girarY(., rotY)
    Vec3f d = p - f.centro;
    float c = std::cos(f.rotY), s = std::sin(f.rotY);
    return {d.x * c - d.z * s, d.y, d.x * s + d.z * c};
}
float caixa(Vec3f q, Vec3f b) {
    Vec3f d{std::abs(q.x) - b.x, std::abs(q.y) - b.y, std::abs(q.z) - b.z};
    Vec3f m{std::max(d.x, 0.f), std::max(d.y, 0.f), std::max(d.z, 0.f)};
    return len(m) + std::min(std::max(d.x, std::max(d.y, d.z)), 0.f);
}
}  // namespace

float distanciaForma(const FormaCena& f, Vec3f p) {
    Vec3f q = paraLocal(f, p);
    const Vec3f& h = f.meia;
    switch (f.tipo) {
        case TipoForma::Caixa: return caixa(q, h);
        case TipoForma::Cilindro: {
            float dr = std::sqrt(q.x * q.x + q.z * q.z) - h.x, dy = std::abs(q.y) - h.y;
            return std::min(std::max(dr, dy), 0.f) + std::sqrt(std::max(dr, 0.f) * std::max(dr, 0.f) + std::max(dy, 0.f) * std::max(dy, 0.f));
        }
        case TipoForma::Prisma: {
            // aguas: retas de (z = +-h.z, y = -h.y) ate (z = 0, y = +h.y); interseccao dos semiespacos
            float gz = 1.f / h.z, gy = 1.f / (2.f * h.y), L = std::sqrt(gz * gz + gy * gy);
            float agua = (std::abs(q.z) * gz + (q.y + h.y) * gy - 1.f) / L;
            float d = std::max(agua, std::max(-h.y - q.y, std::abs(q.x) - h.x));
            // perto das arestas a interseccao subestima; fora da caixa envolvente, usa-a como piso
            return std::max(d, caixa(q, h));
        }
    }
    return 1e9f;
}

float distanciaCena(const std::vector<FormaCena>& fs, Vec3f p) {
    float d = 1e9f;
    for (auto& f : fs) d = std::min(d, distanciaForma(f, p));
    return d;
}

std::vector<int> formasPerto(const std::vector<FormaCena>& fs, Vec3f foco, float raio, int max) {
    std::vector<std::pair<float, int>> v;
    for (int i = 0; i < int(fs.size()); i++) {
        float d = distanciaForma(fs[i], foco);
        if (d < raio) v.push_back({d, i});
    }
    std::sort(v.begin(), v.end());
    std::vector<int> r;
    for (int i = 0; i < int(v.size()) && i < max; i++) r.push_back(v[i].second);
    return r;
}
