#include "core/meshbuilder.h"
#include <algorithm>
#include <cmath>
#include "core/terrain.h"

namespace {
constexpr float PI = 3.14159265f;
constexpr float ALCANCE_AO = 2.f;

Vec3f girarY(Vec3f p, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
}
}  // namespace

void MeshBuilder::triangulo(Vec3f a, Vec3f b, Vec3f c, Rgb cor) {
    Vec3f n = normalizar(cross(b - a, c - a));
    v_.push_back({a, n, cor, 1.f, tipoAtual_});
    v_.push_back({b, n, cor, 1.f, tipoAtual_});
    v_.push_back({c, n, cor, 1.f, tipoAtual_});
    dono_.insert(dono_.end(), 3, donoAtual_);
}

// Quad a-b-c-d (em sequencia). Garante a frente virada para "dir". Subdivide faces grandes (para a AO ficar boa).
void MeshBuilder::quadOrientado(Vec3f a, Vec3f b, Vec3f c, Vec3f d, Vec3f dir, Rgb cor, float tamMax) {
    if (dot(cross(b - a, c - a), dir) < 0) std::swap(b, d);
    int nu = std::max(1, int(std::ceil(len(b - a) / tamMax)));
    int nw = std::max(1, int(std::ceil(len(d - a) / tamMax)));
    auto P = [&](int i, int j) {
        float u = i / float(nu), w = j / float(nw);
        Vec3f ab = a + (b - a) * u, dc = d + (c - d) * u;
        return ab + (dc - ab) * w;
    };
    for (int j = 0; j < nw; j++)
        for (int i = 0; i < nu; i++) {
            Vec3f p00 = P(i, j), p10 = P(i + 1, j), p11 = P(i + 1, j + 1), p01 = P(i, j + 1);
            triangulo(p00, p10, p11, cor);
            triangulo(p00, p11, p01, cor);
        }
}

void MeshBuilder::triOrientado(Vec3f a, Vec3f b, Vec3f c, Vec3f dir, Rgb cor) {
    if (dot(cross(b - a, c - a), dir) < 0) std::swap(b, c);
    triangulo(a, b, c, cor);
}

void MeshBuilder::addOclusor(Vec3f mn, Vec3f mx) {
    oclusores_.push_back({mn, mx});
    donoAtual_ = int(oclusores_.size()) - 1;
}

void MeshBuilder::caixa(Vec3f centro, Vec3f t, float rotY, Rgb cor) {
    Vec3f h = t * 0.5f;
    Vec3f mn{1e9f, 1e9f, 1e9f}, mx{-1e9f, -1e9f, -1e9f};
    for (int k = 0; k < 8; k++) {
        Vec3f p = centro + girarY({(k & 1) ? h.x : -h.x, (k & 2) ? h.y : -h.y, (k & 4) ? h.z : -h.z}, rotY);
        mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
        mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
    }
    addOclusor(mn, mx);
    const Vec3f eixos[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (auto n : eixos) {
        Vec3f w = std::abs(n.y) > 0.5f ? Vec3f{0, 0, 1} : Vec3f{0, 1, 0};
        Vec3f u = cross(w, n);
        Vec3f cf{n.x * h.x, n.y * h.y, n.z * h.z};
        Vec3f us{u.x * h.x, u.y * h.y, u.z * h.z}, ws{w.x * h.x, w.y * h.y, w.z * h.z};
        auto W = [&](Vec3f p) { return centro + girarY(p, rotY); };
        quadOrientado(W(cf - us - ws), W(cf + us - ws), W(cf + us + ws), W(cf - us + ws), girarY(n, rotY), cor, 2.f);
    }
    donoAtual_ = -1;
}

void MeshBuilder::cilindro(Vec3f base, float r, float alt, int lados, Rgb cor) {
    addOclusor({base.x - r * .7f, base.y, base.z - r * .7f}, {base.x + r * .7f, base.y + alt, base.z + r * .7f});
    Vec3f topo = base + Vec3f{0, alt, 0};
    for (int i = 0; i < lados; i++) {
        float a0 = 2 * PI * i / lados, a1 = 2 * PI * (i + 1) / lados, am = (a0 + a1) / 2;
        Vec3f o0{std::cos(a0) * r, 0, std::sin(a0) * r}, o1{std::cos(a1) * r, 0, std::sin(a1) * r};
        Vec3f fora{std::cos(am), 0, std::sin(am)};
        quadOrientado(base + o0, base + o1, topo + o1, topo + o0, fora, cor, 1e9f);
        triOrientado(topo, topo + o0, topo + o1, {0, 1, 0}, cor);
    }
    donoAtual_ = -1;
}

void MeshBuilder::cone(Vec3f base, float r, float alt, int lados, Rgb cor) {
    addOclusor({base.x - r * .5f, base.y, base.z - r * .5f}, {base.x + r * .5f, base.y + alt * .6f, base.z + r * .5f});
    Vec3f topo = base + Vec3f{0, alt, 0};
    for (int i = 0; i < lados; i++) {
        float a0 = 2 * PI * i / lados, a1 = 2 * PI * (i + 1) / lados, am = (a0 + a1) / 2;
        Vec3f o0{std::cos(a0) * r, 0, std::sin(a0) * r}, o1{std::cos(a1) * r, 0, std::sin(a1) * r};
        triOrientado(base + o0, base + o1, topo, {std::cos(am), r / alt, std::sin(am)}, cor);
        triOrientado(base, base + o1, base + o0, {0, -1, 0}, cor);
    }
    donoAtual_ = -1;
}

void MeshBuilder::prisma(Vec3f centro, Vec3f t, float rotY, Rgb cor) {
    Vec3f h = t * 0.5f;
    auto W = [&](Vec3f p) { return centro + girarY(p, rotY); };
    addOclusor(centro - Vec3f{std::max(h.x, h.z), h.y, std::max(h.x, h.z)} * 0.8f, centro + Vec3f{std::max(h.x, h.z) * .8f, 0, std::max(h.x, h.z) * .8f});
    Vec3f r0{-h.x, h.y, 0}, r1{h.x, h.y, 0};
    Vec3f f0{-h.x, -h.y, h.z}, f1{h.x, -h.y, h.z}, b0{-h.x, -h.y, -h.z}, b1{h.x, -h.y, -h.z};
    quadOrientado(W(f0), W(f1), W(r1), W(r0), girarY({0, h.z, h.y}, rotY), cor, 2.f);
    quadOrientado(W(b1), W(b0), W(r0), W(r1), girarY({0, h.z, -h.y}, rotY), cor, 2.f);
    triOrientado(W(f0), W(r0), W(b0), girarY({-1, 0, 0}, rotY), cor);
    triOrientado(W(f1), W(b1), W(r1), girarY({1, 0, 0}, rotY), cor);
    quadOrientado(W(f0), W(b0), W(b1), W(f1), {0, -1, 0}, cor, 2.f);
    donoAtual_ = -1;
}

void MeshBuilder::calcularAO(const Terreno* t, int amostras) {
    // direcoes fixas no hemisferio (espiral de fibonacci, ponderada por cosseno)
    std::vector<Vec3f> dirs;
    for (int k = 0; k < amostras; k++) {
        float u = (k + 0.5f) / amostras;
        float r = std::sqrt(u), phi = k * 2.39996323f;
        dirs.push_back({r * std::cos(phi), std::sqrt(1 - u), r * std::sin(phi)});
    }
    for (size_t vi = 0; vi < v_.size(); vi++) {
        Vert& v = v_[vi];
        Vec3f n = v.normal;
        Vec3f w = std::abs(n.y) > 0.9f ? Vec3f{1, 0, 0} : Vec3f{0, 1, 0};
        Vec3f tx = normalizar(cross(w, n)), tz = cross(n, tx);
        Vec3f o = v.pos + n * 0.03f;
        // oclusores proximos
        std::vector<const Caixa*> perto;
        for (size_t c = 0; c < oclusores_.size(); c++) {
            if (int(c) == dono_[vi]) continue;
            const Caixa& b = oclusores_[c];
            if (o.x < b.min.x - ALCANCE_AO || o.x > b.max.x + ALCANCE_AO || o.y < b.min.y - ALCANCE_AO ||
                o.y > b.max.y + ALCANCE_AO || o.z < b.min.z - ALCANCE_AO || o.z > b.max.z + ALCANCE_AO)
                continue;
            perto.push_back(&b);
        }
        int bloqueados = 0;
        for (auto& d0 : dirs) {
            Vec3f d = tx * d0.x + n * d0.y + tz * d0.z;
            bool hit = false;
            for (auto* b : perto) {
                float tmin = 0, tmax = ALCANCE_AO;
                const float* oo = &o.x; const float* dd = &d.x;
                const float* mn = &b->min.x; const float* mx = &b->max.x;
                bool ok = true;
                for (int e = 0; e < 3 && ok; e++) {
                    if (std::abs(dd[e]) < 1e-6f) {
                        if (oo[e] < mn[e] || oo[e] > mx[e]) ok = false;
                    } else {
                        float t1 = (mn[e] - oo[e]) / dd[e], t2 = (mx[e] - oo[e]) / dd[e];
                        if (t1 > t2) std::swap(t1, t2);
                        tmin = std::max(tmin, t1);
                        tmax = std::min(tmax, t2);
                        if (tmin > tmax) ok = false;
                    }
                }
                if (ok) { hit = true; break; }
            }
            if (!hit && t) {
                for (float s : {0.3f, 0.8f, 1.4f, 2.f}) {
                    Vec3f p = o + d * s;
                    if (p.y < t->altura(p.x, p.z)) { hit = true; break; }
                }
            }
            bloqueados += hit;
        }
        float livre = 1.f - bloqueados / float(amostras);
        v.ao = 0.25f + 0.75f * livre;
    }
}

void MeshBuilder::aoVertical(float y0, float y1) {
    for (auto& v : v_) v.ao = 0.45f + 0.55f * smoothstep(y0, y1, v.pos.y);
}

void MeshBuilder::anexar(const MeshBuilder& o) {
    v_.insert(v_.end(), o.v_.begin(), o.v_.end());
    dono_.insert(dono_.end(), o.v_.size(), -1);
}
