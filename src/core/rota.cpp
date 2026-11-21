#include "core/rota.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include "core/places.h"
#include "core/terrain.h"

Navegador::Navegador(const Terreno& t, const Mapa* mapa) {
    origem_ = -Terreno::META;
    n_ = int(2 * Terreno::META / CELULA);
    custo_.assign(size_t(n_) * n_, 1.f);
    alt_.assign(size_t(n_) * n_, 0.f);
    for (int j = 0; j < n_; j++)
        for (int i = 0; i < n_; i++) {
            Vec3f c = centro(i, j);
            float& k = custo_[size_t(j) * n_ + i];
            alt_[size_t(j) * n_ + i] = t.altura(c.x, c.z);
            if (!t.dentroDoJogavel(c.x, c.z)) { k = -1; continue; }
            float estrada = t.mascaraEstrada(c.x, c.z), rio = t.mascaraRio(c.x, c.z);
            float ny = t.normal(c.x, c.z).y;
            k = 1.f;
            if (ny < 0.85f) k += 6.f * (0.85f - ny) / 0.15f;  // encosta
            if (rio > 0.5f) k += 80.f;                         // agua funda
            k = lerp(k, 0.3f, std::clamp(estrada * 1.6f, 0.f, 1.f));  // estrada e barata
            if (mapa)
                for (auto& l : mapa->lugares)
                    if (l.tipo == TipoLugar::Ponte && std::hypot(c.x - l.x, c.z - l.z) < l.raio + 4.f) k = 0.3f;  // ponte
        }
}

int Navegador::celula(float v) const { return std::clamp(int((v - origem_) / CELULA), 0, n_ - 1); }
Vec3f Navegador::centro(int i, int j) const { return {origem_ + (i + 0.5f) * CELULA, 0.f, origem_ + (j + 0.5f) * CELULA}; }

std::vector<Vec3f> Navegador::rotaDensa(Vec3f de, Vec3f para) const {
    int si = celula(de.x), sj = celula(de.z), gi = celula(para.x), gj = celula(para.z);
    size_t N = size_t(n_) * n_, ini = size_t(sj) * n_ + si, alvo = size_t(gj) * n_ + gi;
    std::vector<float> g(N, 1e30f);
    std::vector<int> veio(N, -1);
    using No = std::pair<float, int>;
    std::priority_queue<No, std::vector<No>, std::greater<No>> fila;
    auto h = [&](int i, int j) { return std::hypot(float(i - gi), float(j - gj)) * 0.3f; };
    g[ini] = 0;
    fila.push({h(si, sj), int(ini)});
    const int di[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dj[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!fila.empty()) {
        auto [f, id] = fila.top();
        fila.pop();
        if (size_t(id) == alvo) break;
        int i = id % n_, j = id / n_;
        if (f - h(i, j) > g[id] + 1e-3f) continue;
        for (int k = 0; k < 8; k++) {
            int a = i + di[k], b = j + dj[k];
            if (a < 0 || b < 0 || a >= n_ || b >= n_) continue;
            size_t nid = size_t(b) * n_ + a;
            float c = custo_[nid];
            if (c < 0 && nid != alvo) continue;
            float passo = (k < 4 ? 1.f : 1.4142f) * 0.5f * (std::max(c, 0.3f) + std::max(custo_[id], 0.3f));
            float ng = g[id] + passo;
            if (ng < g[nid]) {
                g[nid] = ng;
                veio[nid] = id;
                fila.push({ng + h(a, b), int(nid)});
            }
        }
    }
    std::vector<Vec3f> r;
    if (veio[alvo] < 0 && alvo != ini) return {de, para};
    for (int id = int(alvo); id >= 0; id = veio[id]) {
        Vec3f c = centro(id % n_, id / n_);
        c.y = alt_[size_t(id)];
        r.push_back(c);
        if (size_t(id) == ini) break;
    }
    std::reverse(r.begin(), r.end());
    r.front() = {de.x, r.front().y, de.z};
    r.back() = {para.x, r.back().y, para.z};
    return r;
}

std::vector<Vec3f> Navegador::rota(Vec3f de, Vec3f para) const {
    auto d = rotaDensa(de, para);
    if (d.size() < 3) return d;
    std::vector<Vec3f> r{d.front()};
    for (size_t i = 1; i + 1 < d.size(); i++) {
        Vec3f a = d[i] - r.back(), b = d[i + 1] - d[i];
        float cr = a.x * b.z - a.z * b.x;
        if (std::abs(cr) > 1e-3f || std::hypot(d[i].x - r.back().x, d[i].z - r.back().z) > 50.f) r.push_back(d[i]);
    }
    r.push_back(d.back());
    return r;
}
