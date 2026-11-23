#include "core/hair_physics.h"
#include <algorithm>
#include <cmath>

namespace {
Vec3f interpolar(Vec3f a, Vec3f b, float t) { return a + (b - a) * t; }

Vec3f limitarDesvio(Vec3f p, Vec3f alvo, float limite) {
    Vec3f delta = p - alvo;
    float d2 = dot(delta, delta);
    return d2 > limite * limite ? alvo + delta * (limite / std::sqrt(d2)) : p;
}

struct Capsula {
    Vec3f a, eixo;
    float invComprimento2, raio2;

    Capsula(Vec3f inicio, Vec3f fim, float raio)
        : a(inicio), eixo(fim - inicio), invComprimento2(1.f / std::max(1e-8f, dot(eixo, eixo))), raio2(raio * raio) {}

    Vec3f radial(Vec3f p) const {
        return p - (a + eixo * clamp01(dot(p - a, eixo) * invComprimento2));
    }

    Vec3f limitar(Vec3f p, Vec3f alvo) const {
        Vec3f delta = radial(p);
        float d2 = dot(delta, delta);
        if (d2 >= raio2) return p;

        // Conserva a folga de repouso mesmo onde a cápsula é mais larga que a pele.
        Vec3f repouso = radial(alvo);
        float limite = std::min(raio2, dot(repouso, repouso));
        if (d2 >= limite) return p;
        Vec3f direcao = d2 > 1e-10f ? delta * (1.f / std::sqrt(d2)) : normalizar(repouso);
        return p - delta + direcao * std::sqrt(limite);
    }
};

Vec3f noEspacoDaCabeca(const Mat4& m, Vec3f v) {
    // A inversa de uma rotação é a transposta. O deslocamento não usa translação.
    return {dot(m.vetor({1, 0, 0}), v), dot(m.vetor({0, 1, 0}), v), dot(m.vetor({0, 0, 1}), v)};
}
}  // namespace

FisicaCabelo::FisicaCabelo(const PerfilCabelo& perfil) : perfil_(perfil) {
    perfil_.quantidade = std::min(perfil_.quantidade, MAX_GUIAS_CABELO);
    for (size_t g = 0; g < perfil_.quantidade; ++g) {
        auto& estado = guias_[g];
        const auto& guia = perfil_.guias[g];
        const auto& p = guia.pontos;
        estado.comprimentos = {len(p[1] - p[0]), len(p[2] - p[1])};
        float comprimento = estado.comprimentos[0] + estado.comprimentos[1];
        float flex = clamp01(guia.flexibilidade);
        float solto = smoothstep(.032f, .20f, comprimento) * lerp(.3f, 1.f, flex);
        estado.rigidez = {lerp(495.f, 200.f, solto), lerp(290.f, 105.f, solto)};
        float desvio = comprimento * lerp(.26f, .60f, flex);
        estado.limites = {std::min(.085f, desvio * .60f), std::min(.16f, desvio)};
    }
}

void FisicaCabelo::atualizar(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f vento) {
    if (dt <= 0.f || perfil_.quantidade == 0) return;
    const auto& c = perfil_.colisores;
    const std::array<Vec3f, 5> colisores{
        cabeca.ponto(c.cranio), cabeca.ponto(c.nuca), pescoco.ponto(c.pescoco),
        peito.ponto(c.ombroE), peito.ponto(c.ombroD),
    };
    std::array<std::array<Vec3f, 3>, MAX_GUIAS_CABELO> alvos;
    for (size_t g = 0; g < perfil_.quantidade; ++g)
        for (size_t i = 0; i < 3; ++i)
            alvos[g][i] = cabeca.ponto(perfil_.guias[g].pontos[i]);

    Vec3f salto = colisores[0] - colisoresAnteriores_[0];
    if (!iniciada_ || dot(salto, salto) > .60f * .60f || dt > .12f) {
        for (size_t g = 0; g < perfil_.quantidade; ++g)
            for (size_t i = 0; i < 2; ++i)
                guias_[g].moveis[i] = {alvos[g][i + 1], {}};
        iniciada_ = true;
        deformacao_ = {};
    } else {
        int passos = std::max(1, int(std::ceil(dt * 120.f)));
        float h = dt / passos, amortecimento = std::exp(-10.f * h);
        // O repouso já inclui o caimento do corte. Uma inclinação muda a direção
        // da gravidade relativa a esse caimento, sem achatar a silhueta parada.
        const Vec3f gravidade{0, -9.81f, 0};
        Vec3f forca = vento + gravidade - cabeca.vetor(gravidade);
        for (int passo = 1; passo <= passos; ++passo) {
            float t = float(passo) / passos;
            std::array<Vec3f, 5> p;
            for (size_t i = 0; i < p.size(); ++i) p[i] = interpolar(colisoresAnteriores_[i], colisores[i], t);
            const Capsula capsulas[] = {
                {p[0], p[1], c.raioCranio}, {p[1], p[2], c.raioPescoco}, {p[3], p[4], c.raioOmbros},
            };
            for (size_t g = 0; g < perfil_.quantidade; ++g) {
                auto& guia = guias_[g];
                Vec3f raiz = interpolar(guia.alvosAnteriores[0], alvos[g][0], t);
                std::array<Vec3f, 2> alvosMoveis, antes;
                for (size_t i = 0; i < guia.moveis.size(); ++i) {
                    auto& no = guia.moveis[i];
                    alvosMoveis[i] = interpolar(guia.alvosAnteriores[i + 1], alvos[g][i + 1], t);
                    antes[i] = no.pos;
                    no.vel = no.vel * amortecimento +
                        ((alvosMoveis[i] - no.pos) * guia.rigidez[i] + forca * (i == 0 ? .50f : 1.f)) * h;
                    no.pos = no.pos + no.vel * h;
                }
                auto& meio = guia.moveis[0].pos;
                auto& ponta = guia.moveis[1].pos;
                for (int it = 0; it < 3; ++it) {
                    meio = raiz + normalizar(meio - raiz) * guia.comprimentos[0];
                    Vec3f delta = ponta - meio;
                    Vec3f excesso = delta - normalizar(delta) * guia.comprimentos[1];
                    meio = meio + excesso * .30f;
                    ponta = ponta - excesso * .70f;

                    for (size_t i = 0; i < guia.moveis.size(); ++i) {
                        auto& pos = guia.moveis[i].pos;
                        pos = limitarDesvio(pos, alvosMoveis[i], guia.limites[i]);
                        for (const auto& capsula : capsulas) pos = capsula.limitar(pos, alvosMoveis[i]);
                    }
                }
                for (size_t i = 0; i < guia.moveis.size(); ++i)
                    guia.moveis[i].vel = (guia.moveis[i].pos - antes[i]) * (1.f / h);
            }
        }
        for (size_t g = 0; g < perfil_.quantidade; ++g) {
            deformacao_.meio[g] = noEspacoDaCabeca(cabeca, guias_[g].moveis[0].pos - alvos[g][1]);
            deformacao_.ponta[g] = noEspacoDaCabeca(cabeca, guias_[g].moveis[1].pos - alvos[g][2]);
        }
    }
    for (size_t g = 0; g < perfil_.quantidade; ++g) guias_[g].alvosAnteriores = alvos[g];
    colisoresAnteriores_ = colisores;
}
