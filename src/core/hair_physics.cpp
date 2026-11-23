#include "core/hair_physics.h"
#include <algorithm>

namespace {
using Guia = std::array<Vec3f, 3>;

// Raiz fixa, dobra e ponta. A gravidade e a inercia atuam nas duas ultimas.
constexpr Guia REPOUSO[] = {
    {{{ .071f, 1.748f, .113f}, { .103f, 1.686f, .093f}, { .117f, 1.638f, .061f}}},
    {{{-.071f, 1.748f, .113f}, {-.103f, 1.686f, .093f}, {-.117f, 1.638f, .061f}}},
    {{{ 0.f,   1.735f, -.054f}, { 0.f,   1.654f, -.070f}, { 0.f,   1.572f, -.075f}}},
};

Vec3f interpolar(Vec3f a, Vec3f b, float t) { return a + (b - a) * t; }

Vec3f impedirPenetracao(Vec3f p, Vec3f a, Vec3f b, float raio) {
    Vec3f eixo = b - a;
    float t = clamp01(dot(p - a, eixo) / std::max(1e-8f, dot(eixo, eixo)));
    Vec3f centro = a + eixo * t;
    Vec3f delta = p - centro;
    float d2 = dot(delta, delta);
    return d2 < raio * raio ? centro + normalizar(delta) * raio : p;
}

Vec3f limitarDesvio(Vec3f p, Vec3f alvo, float limite) {
    Vec3f delta = p - alvo;
    float d2 = dot(delta, delta);
    return d2 > limite * limite ? alvo + delta * (limite / std::sqrt(d2)) : p;
}

Vec3f noEspacoDaCabeca(const Mat4& m, Vec3f v) {
    // A inversa de uma rotacao e a transposta. O deslocamento nao usa translacao.
    return {dot(m.vetor({1, 0, 0}), v), dot(m.vetor({0, 1, 0}), v), dot(m.vetor({0, 0, 1}), v)};
}
}

void FisicaCabelo::atualizar(float dt, const Mat4& cabeca, const Mat4& pescoco, float escala, Vec3f vento) {
    if (dt <= 0.f) return;
    std::array<Guia, 3> alvos{};
    for (size_t g = 0; g < alvos.size(); ++g)
        for (size_t i = 0; i < alvos[g].size(); ++i)
            alvos[g][i] = cabeca.ponto(REPOUSO[g][i] * escala);

    Vec3f cranio = cabeca.ponto(Vec3f{0, 1.742f, .052f} * escala);
    Vec3f nuca = cabeca.ponto(Vec3f{0, 1.665f, .047f} * escala);
    Vec3f pesco = pescoco.ponto(Vec3f{0, 1.548f, .029f} * escala);
    Vec3f salto = cranio - cranioAnterior_;
    if (!iniciada_ || dot(salto, salto) > .60f * .60f || dt > .12f) {
        for (size_t g = 0; g < guias_.size(); ++g)
            for (size_t i = 0; i < guias_[g].size(); ++i) guias_[g][i] = {alvos[g][i + 1], {}};
        iniciada_ = true;
        deformacao_ = {};
    } else {
        // Limita o passo a 1/120 s mesmo no preset baixo, sem acumular atraso entre quadros.
        int passos = std::max(1, int(std::ceil(dt * 120.f)));
        float h = dt / passos, amortecimento = std::exp(-10.f * h);
        std::array<float, 3> comprMeio{}, comprPonta{};
        for (size_t g = 0; g < guias_.size(); ++g) {
            comprMeio[g] = len(REPOUSO[g][1] - REPOUSO[g][0]) * escala;
            comprPonta[g] = len(REPOUSO[g][2] - REPOUSO[g][1]) * escala;
        }
        Vec3f forca = vento + Vec3f{0, -1.6f, 0};
        for (int passo = 1; passo <= passos; ++passo) {
            float t = float(passo) / passos;
            Vec3f topo = interpolar(cranioAnterior_, cranio, t);
            Vec3f base = interpolar(nucaAnterior_, nuca, t);
            Vec3f baixo = interpolar(pescocoAnterior_, pesco, t);
            for (size_t g = 0; g < guias_.size(); ++g) {
                No& meio = guias_[g][0];
                No& ponta = guias_[g][1];
                Vec3f meioAntes = meio.pos, pontaAntes = ponta.pos;
                Vec3f raiz = interpolar(alvosAnteriores_[g][0], alvos[g][0], t);
                Vec3f alvoMeio = interpolar(alvosAnteriores_[g][1], alvos[g][1], t);
                Vec3f alvoPonta = interpolar(alvosAnteriores_[g][2], alvos[g][2], t);
                meio.vel = meio.vel * amortecimento + ((alvoMeio - meio.pos) * 305.f + forca * .50f) * h;
                ponta.vel = ponta.vel * amortecimento + ((alvoPonta - ponta.pos) * 165.f + forca) * h;
                meio.pos = meio.pos + meio.vel * h;
                ponta.pos = ponta.pos + ponta.vel * h;

                // A franja cede menos que as camadas longas da nuca.
                float limiteMeio = (g == 2 ? .034f : .025f) * escala;
                float limitePonta = (g == 2 ? .055f : .040f) * escala;
                for (int it = 0; it < 3; ++it) {
                    // Projeta os segmentos para manter o comprimento. A ponta tambem puxa a dobra.
                    meio.pos = raiz + normalizar(meio.pos - raiz) * comprMeio[g];
                    Vec3f delta = ponta.pos - meio.pos;
                    Vec3f excesso = delta - normalizar(delta) * comprPonta[g];
                    meio.pos = meio.pos + excesso * .30f;
                    ponta.pos = ponta.pos - excesso * .70f;

                    meio.pos = limitarDesvio(meio.pos, alvoMeio, limiteMeio);
                    ponta.pos = limitarDesvio(ponta.pos, alvoPonta, limitePonta);
                    meio.pos = impedirPenetracao(meio.pos, topo, base, .081f * escala);
                    ponta.pos = impedirPenetracao(ponta.pos, base, baixo, .066f * escala);
                }
                meio.vel = (meio.pos - meioAntes) * (1.f / h);
                ponta.vel = (ponta.pos - pontaAntes) * (1.f / h);
            }
        }
        for (size_t g = 0; g < guias_.size(); ++g) {
            deformacao_.meio[g] = noEspacoDaCabeca(cabeca, guias_[g][0].pos - alvos[g][1]);
            deformacao_.ponta[g] = noEspacoDaCabeca(cabeca, guias_[g][1].pos - alvos[g][2]);
        }
    }
    alvosAnteriores_ = alvos;
    cranioAnterior_ = cranio;
    nucaAnterior_ = nuca;
    pescocoAnterior_ = pesco;
}
