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
    float invComprimento2, raio2, yMin, yMax;

    Capsula(Vec3f inicio, Vec3f fim, float raio)
        : a(inicio), eixo(fim - inicio), invComprimento2(1.f / std::max(1e-8f, dot(eixo, eixo))),
          raio2(raio * raio), yMin(std::min(inicio.y, fim.y) - raio), yMax(std::max(inicio.y, fim.y) + raio) {}

    Vec3f radial(Vec3f p) const {
        return p - (a + eixo * clamp01(dot(p - a, eixo) * invComprimento2));
    }

    Vec3f limitar(Vec3f p, Vec3f alvo) const {
        // Descarta mechas acima dos ombros ou abaixo do crânio antes da projeção.
        if (p.y >= yMax || p.y <= yMin) return p;
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

std::array<Capsula, 3> capsulasDaPose(const ColisoresCabelo& c, const std::array<Vec3f, 5>& p) {
    return {{ {p[0], p[1], c.raioCranio}, {p[1], p[2], c.raioPescoco}, {p[3], p[4], c.raioOmbros} }};
}
}  // namespace

FisicaCabelo::FisicaCabelo(const PerfilCabelo& perfil) : perfil_(perfil) {
    perfil_.quantidade = std::min(perfil_.quantidade, MAX_GUIAS_CABELO);
    perfil_.principais = std::min({perfil_.principais, perfil_.quantidade, MAX_GUIAS_PRINCIPAIS});
    for (size_t g = 0; g < perfil_.quantidade; ++g) {
        const auto& guia = perfil_.guias[g];
        const auto& p = guia.pontos;
        float l0 = len(p[1] - p[0]), l1 = len(p[2] - p[1]);
        float comprimento = l0 + l1;
        float flex = clamp01(guia.flexibilidade);
        float arco = std::max(comprimento, guia.comprimentoLivre);
        float solto = smoothstep(.032f, .20f, arco) * lerp(.3f, 1.f, flex);
        float rigidezPonta = lerp(290.f, 105.f, solto);
        float desvio = comprimento * lerp(.26f, .60f, flex);
        float limitePonta = std::min(.16f, desvio);
        if (g < perfil_.principais) {
            auto& estado = guias_[g];
            estado.comprimentos = {l0, l1};
            estado.rigidez = {lerp(495.f, 200.f, solto), rigidezPonta};
            estado.limites = {std::min(.085f, desvio * .60f), limitePonta};
        } else {
            const auto& v = guia.pais;
            const auto& a = guias_[v.a].comprimentos;
            const auto& b = guias_[v.b].comprimentos;
            auto escala = [](float proprio, float herdado) {
                return std::clamp(proprio / std::max(.003f, herdado), .45f, 1.65f);
            };
            auto& estado = secundarios_[g];
            estado.comprimento = l1;
            estado.rigidez = rigidezPonta;
            estado.limite = limitePonta;
            estado.escalaMeio = escala(l0, lerp(a[0], b[0], v.pesoB));
            estado.escalaPonta = escala(comprimento, lerp(a[0] + a[1], b[0] + b[1], v.pesoB));
        }
    }
}

void FisicaCabelo::atualizar(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f vento, float detalhe) {
    if (dt <= 0.f || perfil_.principais == 0) return;
    const auto& c = perfil_.colisores;
    const std::array<Vec3f, 5> colisores{
        cabeca.ponto(c.cranio), cabeca.ponto(c.nuca), pescoco.ponto(c.pescoco),
        peito.ponto(c.ombroE), peito.ponto(c.ombroD),
    };
    std::array<std::array<Vec3f, 3>, MAX_GUIAS_PRINCIPAIS> alvos;
    for (size_t g = 0; g < perfil_.principais; ++g)
        for (size_t i = 0; i < 3; ++i)
            alvos[g][i] = cabeca.ponto(perfil_.guias[g].pontos[i]);

    Vec3f salto = colisores[0] - colisoresAnteriores_[0];
    if (!iniciada_ || dot(salto, salto) > .60f * .60f || dt > .12f) {
        for (size_t g = 0; g < perfil_.principais; ++g)
            for (size_t i = 0; i < 2; ++i)
                guias_[g].moveis[i] = {alvos[g][i + 1], {}};
        iniciada_ = true;
        detalhesAtivos_ = false;
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
            const auto capsulas = capsulasDaPose(c, p);
            for (size_t g = 0; g < perfil_.principais; ++g) {
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
        for (size_t g = 0; g < perfil_.principais; ++g) {
            deformacao_.meio[g] = noEspacoDaCabeca(cabeca, guias_[g].moveis[0].pos - alvos[g][1]);
            deformacao_.ponta[g] = noEspacoDaCabeca(cabeca, guias_[g].moveis[1].pos - alvos[g][2]);
        }
        if (perfil_.quantidade > perfil_.principais)
            atualizarSecundarios(dt, cabeca, forca, clamp01(detalhe), colisores);
    }
    for (size_t g = 0; g < perfil_.principais; ++g) guias_[g].alvosAnteriores = alvos[g];
    colisoresAnteriores_ = colisores;
}

void FisicaCabelo::atualizarSecundarios(float dt, const Mat4& cabeca, Vec3f forca, float detalhe,
                                      const std::array<Vec3f, 5>& colisores) {
    const bool simular = detalhe > 0.f;
    const bool inicializar = simular && !detalhesAtivos_;
    const auto capsulas = capsulasDaPose(perfil_.colisores, colisores);
    for (size_t g = perfil_.principais; g < perfil_.quantidade; ++g) {
        const auto& guia = perfil_.guias[g];
        const auto& v = guia.pais;
        auto& estado = secundarios_[g];
        Vec3f meio = interpolar(deformacao_.meio[v.a], deformacao_.meio[v.b], v.pesoB) * estado.escalaMeio;
        Vec3f herdada = interpolar(deformacao_.ponta[v.a], deformacao_.ponta[v.b], v.pesoB) * estado.escalaPonta;
        deformacao_.meio[g] = meio;
        deformacao_.ponta[g] = herdada;
        if (!simular) continue;

        Vec3f repouso = cabeca.ponto(guia.pontos[2]);
        auto& no = estado.ponta;
        if (inicializar) {
            no = {repouso + cabeca.vetor(herdada), {}};
            continue;
        }

        // A dobra acompanha o movimento dos guias principais. Uma única ponta
        // em espaço de mundo mantém a inércia própria da mecha quando a cabeça gira.
        Vec3f dobra = cabeca.ponto(guia.pontos[1] + meio);
        Vec3f alvo = repouso + cabeca.vetor(meio) * .8f;
        Vec3f anterior = no.pos;
        // Euler implícito para a mola linear, em um passo por quadro.
        float amortecimento = 1.f / (1.f + 10.f * dt + estado.rigidez * dt * dt);
        no.vel = (no.vel + ((alvo - no.pos) * estado.rigidez + forca) * dt) * amortecimento;
        no.pos = dobra + normalizar(no.pos + no.vel * dt - dobra) * estado.comprimento;
        no.pos = limitarDesvio(no.pos, repouso, estado.limite);
        for (const auto& capsula : capsulas) no.pos = capsula.limitar(no.pos, repouso);
        no.vel = (no.pos - anterior) * (1.f / dt);
        Vec3f propria = noEspacoDaCabeca(cabeca, no.pos - repouso);
        deformacao_.ponta[g] = interpolar(herdada, propria, detalhe);
    }
    detalhesAtivos_ = simular;
}
