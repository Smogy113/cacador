#include "core/superficie.h"
#include <algorithm>
#include <cmath>

namespace {
// Ponto do triângulo abc mais próximo de p e suas coordenadas baricêntricas (Ericson, Real-Time
// Collision Detection, 5.1.5). Devolve o ponto; u, v, w são os pesos de a, b e c.
Vec3f maisProximoNoTriangulo(Vec3f p, Vec3f a, Vec3f b, Vec3f c, float& u, float& v, float& w) {
    const Vec3f ab = b - a, ac = c - a, ap = p - a;
    const float d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) { u = 1, v = 0, w = 0; return a; }
    const Vec3f bp = p - b;
    const float d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) { u = 0, v = 1, w = 0; return b; }
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        const float t = d1 / (d1 - d3);
        u = 1 - t, v = t, w = 0;
        return a + ab * t;
    }
    const Vec3f cp = p - c;
    const float d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) { u = 0, v = 0, w = 1; return c; }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) {
        const float t = d2 / (d2 - d6);
        u = 1 - t, v = 0, w = t;
        return a + ac * t;
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
        const float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        u = 0, v = 1 - t, w = t;
        return b + (c - b) * t;
    }
    const float denom = 1.f / (va + vb + vc);
    v = vb * denom, w = vc * denom, u = 1 - v - w;
    return a + ab * v + ac * w;
}

// Segmento ab contra o triângulo (Möller-Trumbore, limitado ao comprimento do segmento).
bool segmentoCruza(Vec3f a, Vec3f b, Vec3f t0, Vec3f t1, Vec3f t2) {
    const Vec3f dir = b - a, e1 = t1 - t0, e2 = t2 - t0;
    const Vec3f p = cross(dir, e2);
    const float det = dot(e1, p);
    if (std::abs(det) < 1e-12f) return false;  // paralelo ao plano
    const float inv = 1.f / det;
    const Vec3f s = a - t0;
    const float u = dot(s, p) * inv;
    if (u < 0 || u > 1) return false;
    const Vec3f q = cross(s, e1);
    const float v = dot(dir, q) * inv;
    if (v < 0 || u + v > 1) return false;
    const float t = dot(e2, q) * inv;
    return t >= 0 && t <= 1;
}

// Osso com mais peso somado nos três vértices (cada vértice divide o peso entre dois ossos).
uint8_t ossoDominante(const VertPele& a, const VertPele& b, const VertPele& c) {
    float pesos[256] = {};
    for (const VertPele* v : {&a, &b, &c}) {
        pesos[v->osso[0]] += v->peso;
        pesos[v->osso[1]] += 1.f - v->peso;
    }
    return uint8_t(std::max_element(pesos, pesos + 256) - pesos);
}

Vec3f minimo(Vec3f a, Vec3f b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
Vec3f maximo(Vec3f a, Vec3f b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
}  // namespace

Superficie::Superficie(const MalhaPele& malha, const Filtro& incluir, float celula) : celula_(celula) {
    for (size_t i = 0; i + 2 < malha.idx.size(); i += 3) {
        const VertPele& a = malha.v[malha.idx[i]];
        const VertPele& b = malha.v[malha.idx[i + 1]];
        const VertPele& c = malha.v[malha.idx[i + 2]];
        if (incluir(a, b, c)) tris_.push_back({a.pos, b.pos, c.pos, a.normal, b.normal, c.normal, ossoDominante(a, b, c)});
    }
    if (tris_.empty()) return;

    Vec3f lo = tris_[0].a, hi = tris_[0].a;
    for (const Tri& t : tris_) {
        lo = minimo(lo, minimo(t.a, minimo(t.b, t.c)));
        hi = maximo(hi, maximo(t.a, maximo(t.b, t.c)));
    }
    origem_ = lo;
    const Vec3f tamanho = hi - lo;
    const float medidas[3] = {tamanho.x, tamanho.y, tamanho.z};
    for (int k = 0; k < 3; ++k) n_[k] = std::max(1, int(std::ceil(medidas[k] / celula_)) + 1);

    // Duas passadas: conta quantos triângulos tocam cada célula e depois preenche a lista.
    const size_t celulas = size_t(n_[0]) * n_[1] * n_[2];
    std::vector<uint32_t> contagem(celulas + 1, 0);
    auto paraCadaCelula = [&](const Tri& t, auto&& fazer) {
        int c0[3], c1[3];
        celulaDe(minimo(t.a, minimo(t.b, t.c)), c0);
        celulaDe(maximo(t.a, maximo(t.b, t.c)), c1);
        for (int z = c0[2]; z <= c1[2]; ++z)
            for (int y = c0[1]; y <= c1[1]; ++y)
                for (int x = c0[0]; x <= c1[0]; ++x) fazer(indice(x, y, z));
    };
    for (const Tri& t : tris_) paraCadaCelula(t, [&](int k) { ++contagem[k + 1]; });
    for (size_t k = 1; k <= celulas; ++k) contagem[k] += contagem[k - 1];
    inicio_ = contagem;
    lista_.resize(inicio_.back());
    for (uint32_t i = 0; i < tris_.size(); ++i) paraCadaCelula(tris_[i], [&](int k) { lista_[contagem[k]++] = i; });
}

void Superficie::celulaDe(Vec3f p, int c[3]) const {
    const Vec3f q = (p - origem_) * (1.f / celula_);
    const float v[3] = {q.x, q.y, q.z};
    for (int k = 0; k < 3; ++k) c[k] = std::clamp(int(std::floor(v[k])), 0, n_[k] - 1);
}

Superficie::Contato Superficie::maisProximo(Vec3f p, float alcance) const {
    Contato melhor{p, {0, 1, 0}, alcance};
    if (tris_.empty()) return melhor;
    float melhor2 = alcance * alcance;
    int centro[3];
    celulaDe(p, centro);
    // Fora da grade, a célula de partida é a da borda; o quanto p está longe dela entra na conta abaixo.
    const Vec3f cantoCelula = origem_ + Vec3f{float(centro[0]), float(centro[1]), float(centro[2])} * celula_;
    const Vec3f dentro = maximo(cantoCelula, minimo(p, cantoCelula + Vec3f{celula_, celula_, celula_}));
    const float foraDaCelula = len(p - dentro);
    // Busca em cascas de células cada vez mais largas. Depois da casca k, um triângulo ainda não
    // visto está a pelo menos k células da célula de partida: para quando o melhor já é menor.
    const int maxCascas = int(std::ceil((alcance + foraDaCelula) / celula_)) + 1;
    const int maiorLado = std::max({n_[0], n_[1], n_[2]});
    for (int k = 0; k <= std::min(maxCascas, maiorLado); ++k) {
        for (int z = std::max(0, centro[2] - k); z <= std::min(n_[2] - 1, centro[2] + k); ++z) {
            for (int y = std::max(0, centro[1] - k); y <= std::min(n_[1] - 1, centro[1] + k); ++y) {
                for (int x = std::max(0, centro[0] - k); x <= std::min(n_[0] - 1, centro[0] + k); ++x) {
                    const bool borda = std::abs(x - centro[0]) == k || std::abs(y - centro[1]) == k || std::abs(z - centro[2]) == k;
                    if (!borda) continue;  // o miolo já foi visto nas cascas anteriores
                    const int c = indice(x, y, z);
                    for (uint32_t j = inicio_[c]; j < inicio_[c + 1]; ++j) {
                        const Tri& t = tris_[lista_[j]];
                        float u, v, w;
                        const Vec3f q = maisProximoNoTriangulo(p, t.a, t.b, t.c, u, v, w);
                        const Vec3f d = p - q;
                        const float d2 = dot(d, d);
                        if (d2 >= melhor2) continue;
                        melhor2 = d2;
                        melhor.ponto = q;
                        melhor.normal = normalizar(t.na * u + t.nb * v + t.nc * w);
                        melhor.osso = t.osso;
                    }
                }
            }
        }
        if (k == 0 && foraDaCelula > alcance) break;  // a grade inteira está longe demais
        if (melhor2 < alcance * alcance && std::sqrt(melhor2) <= float(k) * celula_ - foraDaCelula) break;
    }
    if (melhor2 < alcance * alcance) {
        const float d = std::sqrt(melhor2);
        melhor.distancia = dot(p - melhor.ponto, melhor.normal) < 0 ? -d : d;
    }
    return melhor;
}

Vec3f Superficie::paraFora(Vec3f p, float folga) const {
    // Algumas voltas: sair de uma região (a orelha) pode encostar em outra (a cabeça atrás dela).
    for (int volta = 0; volta < 4; ++volta) {
        const Contato c = maisProximo(p, std::max(.08f, folga * 2.f));
        if (c.distancia >= folga - 1e-5f) break;
        p = c.ponto + c.normal * folga;
    }
    return p;
}

bool Superficie::atravessa(Vec3f a, Vec3f b) const {
    if (tris_.empty()) return false;
    int c0[3], c1[3];
    celulaDe(minimo(a, b), c0);
    celulaDe(maximo(a, b), c1);
    for (int z = c0[2]; z <= c1[2]; ++z)
        for (int y = c0[1]; y <= c1[1]; ++y)
            for (int x = c0[0]; x <= c1[0]; ++x) {
                const int c = indice(x, y, z);
                for (uint32_t j = inicio_[c]; j < inicio_[c + 1]; ++j) {
                    const Tri& t = tris_[lista_[j]];
                    if (segmentoCruza(a, b, t.a, t.b, t.c)) return true;
                }
            }
    return false;
}

Superficie peleParaCabelo(const MalhaPele& malha, float yMinimo) {
    return Superficie(malha, [yMinimo](const VertPele& a, const VertPele& b, const VertPele& c) {
        const bool pele = a.material == MAT_PELE && b.material == MAT_PELE && c.material == MAT_PELE;
        return pele && std::max({a.pos.y, b.pos.y, c.pos.y}) >= yMinimo;
    });
}
