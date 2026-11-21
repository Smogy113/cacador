#include "core/collision.h"
#include <algorithm>
#include <cmath>

void MundoColisao::adicionar(AABB c) {
    int id = int(caixas_.size());
    caixas_.push_back(c);
    int i0 = int(std::floor(c.min.x / CEL)), i1 = int(std::floor(c.max.x / CEL));
    int j0 = int(std::floor(c.min.z / CEL)), j1 = int(std::floor(c.max.z / CEL));
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) grade_[chave(i, j)].push_back(id);
}

Vec3f MundoColisao::resolver(Vec3f p, float raio) const {
    for (int iter = 0; iter < 3; iter++) {
        int ci = int(std::floor(p.x / CEL)), cj = int(std::floor(p.z / CEL));
        bool mexeu = false;
        for (int j = cj - 1; j <= cj + 1; j++)
            for (int i = ci - 1; i <= ci + 1; i++) {
                auto it = grade_.find(chave(i, j));
                if (it == grade_.end()) continue;
                for (int id : it->second) {
                    const AABB& b = caixas_[id];
                    if (p.y >= b.max.y - 0.3f || p.y + 1.8f < b.min.y) continue;
                    float qx = std::clamp(p.x, b.min.x, b.max.x), qz = std::clamp(p.z, b.min.z, b.max.z);
                    float dx = p.x - qx, dz = p.z - qz, d = std::sqrt(dx * dx + dz * dz);
                    if (d >= raio) continue;
                    if (d > 1e-5f) {
                        p.x = qx + dx / d * raio;
                        p.z = qz + dz / d * raio;
                    } else {  // centro dentro da caixa: sai pelo lado mais proximo
                        float e[4] = {p.x - b.min.x, b.max.x - p.x, p.z - b.min.z, b.max.z - p.z};
                        int k = int(std::min_element(e, e + 4) - e);
                        if (k == 0) p.x = b.min.x - raio;
                        else if (k == 1) p.x = b.max.x + raio;
                        else if (k == 2) p.z = b.min.z - raio;
                        else p.z = b.max.z + raio;
                    }
                    mexeu = true;
                }
            }
        if (!mexeu) break;
    }
    p.x = std::clamp(p.x, -LIMITE + 0.01f, LIMITE - 0.01f);
    p.z = std::clamp(p.z, -LIMITE + 0.01f, LIMITE - 0.01f);
    return p;
}

bool MundoColisao::raioLivre(Vec3f a, Vec3f b) const {
    Vec3f d = b - a;
    int i0 = int(std::floor(std::min(a.x, b.x) / CEL)), i1 = int(std::floor(std::max(a.x, b.x) / CEL));
    int j0 = int(std::floor(std::min(a.z, b.z) / CEL)), j1 = int(std::floor(std::max(a.z, b.z) / CEL));
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) {
            auto it = grade_.find(chave(i, j));
            if (it == grade_.end()) continue;
            for (int id : it->second) {
                const AABB& bx = caixas_[id];
                float tmin = 0, tmax = 1;
                const float* o = &a.x; const float* dd = &d.x;
                const float* mn = &bx.min.x; const float* mx = &bx.max.x;
                bool acerta = true;
                for (int e = 0; e < 3 && acerta; e++) {
                    if (std::abs(dd[e]) < 1e-8f) {
                        if (o[e] < mn[e] || o[e] > mx[e]) acerta = false;
                    } else {
                        float t1 = (mn[e] - o[e]) / dd[e], t2 = (mx[e] - o[e]) / dd[e];
                        if (t1 > t2) std::swap(t1, t2);
                        tmin = std::max(tmin, t1);
                        tmax = std::min(tmax, t2);
                        if (tmin > tmax) acerta = false;
                    }
                }
                if (acerta) return false;
            }
        }
    return true;
}

void MundoColisao::adicionarPiso(AABB p) { pisos_.push_back(p); }

float MundoColisao::alturaPiso(float x, float z, float y) const {
    float melhor = -1e9f;
    for (auto& p : pisos_)
        if (x >= p.min.x && x <= p.max.x && z >= p.min.z && z <= p.max.z && p.max.y <= y + 0.7f) melhor = std::max(melhor, p.max.y);
    return melhor;
}

std::vector<AABB> MundoColisao::caixasPerto(Vec3f p, float r) const {
    std::vector<AABB> res;
    std::vector<int> vistos;
    int i0 = int(std::floor((p.x - r) / CEL)), i1 = int(std::floor((p.x + r) / CEL));
    int j0 = int(std::floor((p.z - r) / CEL)), j1 = int(std::floor((p.z + r) / CEL));
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) {
            auto it = grade_.find(chave(i, j));
            if (it == grade_.end()) continue;
            for (int id : it->second) {
                const AABB& b = caixas_[id];
                if (b.max.x < p.x - r || b.min.x > p.x + r || b.max.z < p.z - r || b.min.z > p.z + r) continue;
                if (std::find(vistos.begin(), vistos.end(), id) != vistos.end()) continue;
                vistos.push_back(id);
                res.push_back(b);
            }
        }
    return res;
}
