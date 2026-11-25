#include "core/cabelo_cranio.h"
#include <algorithm>
#include <cmath>

namespace loft::cranio {

Vec3f direcao(float giro, float altura) {
    float g = giro * RAD, h = altura * RAD;
    return {std::cos(h) * std::sin(g), std::sin(h), std::cos(h) * std::cos(g)};
}

float linhaDoCabelo(float giro) {
    float absGiro = std::abs(giro);
    // A linha desce na testa e recua um pouco nas entradas antes das costeletas.
    float frente = 22.f + 3.f * smoothstep(8.f, 45.f, absGiro);
    return lerp(frente, 18.f, smoothstep(44.f, 71.f, absGiro))
         - 55.f * smoothstep(75.f, 166.f, absGiro) + 1.1f * std::cos(giro * .21f + .55f);
}

float volumeCoroa(Vec3f d) {
    float volume = .007f * smoothstep(.03f, .30f, std::abs(d.x)) * smoothstep(.1f, .8f, d.y);
    float pertoDaRisca = 1.f - smoothstep(.03f, .25f, std::abs(d.x));
    float folgaNaRisca = .002f * smoothstep(.25f, .7f, d.z) + .0015f * smoothstep(.70f, .98f, d.y);
    return .0035f + volume + pertoDaRisca * folgaNaRisca;
}

Cranio::Cranio(const MalhaPele& m, float escala) {
    const float inversa = 1.f / escala;
    for (size_t i = 0; i < m.idx.size(); i += 3) {
        Vec3f a = m.v[m.idx[i]].pos * inversa;
        Vec3f b = m.v[m.idx[i + 1]].pos * inversa;
        Vec3f c = m.v[m.idx[i + 2]].pos * inversa;
        if (std::min({a.y, b.y, c.y}) >= 1.58f) faces_.push_back({a - CENTRO, b - a, c - a});
    }
}

Vec3f Cranio::superficie(Vec3f d, float folga) const {
    float alcance = 0.f;
    for (const auto& f : faces_) {
        Vec3f p = cross(d, f.v);
        float det = dot(f.u, p);
        if (std::abs(det) < 1e-10f) continue;
        Vec3f origem = f.a * -1.f;
        float u = dot(origem, p) / det;
        if (u < 0.f || u > 1.f) continue;
        Vec3f q = cross(origem, f.u);
        float v = dot(d, q) / det;
        if (v < 0.f || u + v > 1.f) continue;
        alcance = std::max(alcance, dot(f.v, q) / det);
    }
    return CENTRO + d * ((alcance > 0.f ? alcance : .088f) + folga);
}

void couroCabeludo(MalhaPele& m, const Cranio& cranio, const CorpoDoCabelo& corpo, float escala, Rgb cor) {
    constexpr int VOLTA = 40, ANEIS = 9;
    uint16_t base = uint16_t(m.v.size());
    auto adicionar = [&](Vec3f d, float risca) {
        VertPele v;
        // A borda da calota passa por cima das orelhas: nenhum vértice fica dentro delas.
        v.pos = corpo.paraFora(cranio.superficie(d, volumeCoroa(d)) * escala, .002f);
        v.normal = d;
        v.osso[0] = v.osso[1] = CABECA;
        v.material = MAT_CABELO;
        v.cor = lerp(cor * .88f, cor * 1.52f, risca);
        v.ao = .82f;
        m.v.push_back(v);
    };
    for (int i = 0; i < ANEIS; ++i) {
        for (int j = 0; j < VOLTA; ++j) {
            float giro = j * 360.f / VOLTA - 180.f;
            Vec3f d = direcao(giro, lerp(linhaDoCabelo(giro), 90.f, float(i) / ANEIS));
            float risca = (1.f - smoothstep(.004f, .053f, std::abs(d.x))) * smoothstep(-.17f, .65f, d.z) * .7f;
            adicionar(d, risca);
        }
    }
    uint16_t topo = uint16_t(m.v.size());
    adicionar({0, 1, 0}, .2f);
    for (int i = 0; i < ANEIS; ++i) {
        for (int j = 0; j < VOLTA; ++j) {
            uint16_t a = base + i * VOLTA + j, b = base + i * VOLTA + (j + 1) % VOLTA;
            if (i == ANEIS - 1) m.idx.insert(m.idx.end(), {a, b, topo});
            else {
                uint16_t c = b + VOLTA, d = a + VOLTA;
                m.idx.insert(m.idx.end(), {a, b, c, a, c, d});
            }
        }
    }
}

}  // namespace loft::cranio
