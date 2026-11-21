#pragma once
// Gerador de superficies por "loft": aneis elipticos ligados em faixas (uso interno de body*.cpp).
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
#include "core/body.h"

namespace loft {

inline constexpr float PI_F = 3.14159265f;

// Um anel da superficie: centro, raios (lado/frente) e ossos que o movem.
struct Anel {
    Vec3f c;
    float rx, rz;
    uint8_t a, b;
    float peso;  // peso do osso a
};

// Cor e ajustes de forma por vertice: (anel, angulo, direcao "frente" do anel) -> cor; pode mexer no raio.
using Pintor = std::function<Rgb(int anel, float ang, Vec3f& pos, Vec3f lado, Vec3f frente, float& ao)>;

struct Construtor {
    MalhaPele& m;
    float material = 0;       // material padrao do proximo tubo
    float materialVert = -1;  // o pintor pode trocar o material de um vertice (volta a -1 sozinho)

    // Liga uma sequencia de aneis. tampaIni/tampaFim fecham as pontas.
    void tubo(const std::vector<Anel>& an, int n, Vec3f refFrente, bool tampaIni, bool tampaFim, const Pintor& pintar,
              float pontaIni = 0.f, float pontaFim = 0.f) {
        std::vector<float> angs(n);
        for (int q = 0; q < n; q++) angs[q] = 2 * PI_F * q / n;
        tuboA(an, angs, refFrente, tampaIni, tampaFim, pintar, pontaIni, pontaFim);
    }
    // Igual a tubo, mas com os angulos de cada lado dados (permite malha mais densa onde importa).
    void tuboA(const std::vector<Anel>& an, const std::vector<float>& angs, Vec3f refFrente, bool tampaIni, bool tampaFim,
               const Pintor& pintar, float pontaIni = 0.f, float pontaFim = 0.f) {
        const int n = int(angs.size());
        size_t base = m.v.size();
        size_t triIni = m.idx.size();
        std::vector<Vec3f> eixos(an.size());
        for (size_t i = 0; i < an.size(); i++) {
            Vec3f d = an[std::min(i + 1, an.size() - 1)].c - an[i ? i - 1 : 0].c;
            eixos[i] = normalizar(d);
        }
        for (size_t i = 0; i < an.size(); i++) {
            Vec3f ax = eixos[i];
            Vec3f fr = normalizar(refFrente - ax * dot(refFrente, ax));
            Vec3f la = cross(fr, ax);
            for (int k = 0; k < n; k++) {
                float ang = angs[k];
                Vec3f p = an[i].c + la * (std::cos(ang) * an[i].rx) + fr * (std::sin(ang) * an[i].rz);
                VertPele v;
                v.ao = 1.f;
                materialVert = -1;
                v.cor = pintar(int(i), ang, p, la, fr, v.ao);
                v.material = materialVert >= 0 ? materialVert : material;
                v.pos = p;
                v.osso[0] = an[i].a;
                v.osso[1] = an[i].b;
                v.peso = an[i].peso;
                m.v.push_back(v);
            }
        }
        for (size_t i = 0; i + 1 < an.size(); i++)
            for (int k = 0; k < n; k++) {
                uint16_t a = uint16_t(base + i * n + k), b = uint16_t(base + i * n + (k + 1) % n);
                uint16_t c = uint16_t(base + (i + 1) * n + (k + 1) % n), d = uint16_t(base + (i + 1) * n + k);
                m.idx.insert(m.idx.end(), {a, b, c, a, c, d});
            }
        auto tampa = [&](size_t anel, float ponta, bool inicio) {
            Vec3f ax = eixos[anel] * (inicio ? -1.f : 1.f);
            VertPele v;
            v.pos = an[anel].c + ax * ponta;
            float ao = 1.f;
            Vec3f p = v.pos;
            Vec3f fr = normalizar(refFrente - eixos[anel] * dot(refFrente, eixos[anel]));
            materialVert = -1;
            v.cor = pintar(int(anel), -1.f, p, cross(fr, eixos[anel]), fr, ao);
            v.material = materialVert >= 0 ? materialVert : material;
            v.ao = ao;
            v.osso[0] = an[anel].a;
            v.osso[1] = an[anel].b;
            v.peso = an[anel].peso;
            uint16_t ci = uint16_t(m.v.size());
            m.v.push_back(v);
            for (int k = 0; k < n; k++) {
                uint16_t a = uint16_t(base + anel * n + k), b = uint16_t(base + anel * n + (k + 1) % n);
                if (inicio) m.idx.insert(m.idx.end(), {ci, b, a});
                else m.idx.insert(m.idx.end(), {ci, a, b});
            }
        };
        if (tampaIni) tampa(0, pontaIni, true);
        if (tampaFim) tampa(an.size() - 1, pontaFim, false);
        // garante a frente das faces virada para fora (testa a primeira faixa)
        Vec3f p0 = m.v[m.idx[triIni]].pos, p1 = m.v[m.idx[triIni + 1]].pos, p2 = m.v[m.idx[triIni + 2]].pos;
        Vec3f nrm = cross(p1 - p0, p2 - p0);
        if (dot(nrm, p0 - an[0].c) < 0)
            for (size_t t = triIni; t < m.idx.size(); t += 3) std::swap(m.idx[t + 1], m.idx[t + 2]);
    }
};

inline Rgb mix(Rgb a, Rgb b, float t) { return lerp(a, b, clamp01(t)); }

// Ponto e raios interpolados entre aneis (Catmull-Rom), com mistura de ossos correta.
std::vector<Anel> suavizar(const std::vector<Anel>& an, int sub);
// Cabeca com proporcoes de um rosto real (head_hd.cpp)
void cabecaHD(Construtor& k, float s, float ms, const Visual& vis);
// Cascas de pelo para todo vertice com pelos > 0 (chamar depois das normais)
void cascasDePelo(MalhaPele& m, Rgb corPelo);
void normaisSuaves(MalhaPele& m);
}  // namespace loft
