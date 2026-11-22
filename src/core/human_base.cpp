#include "core/body.h"

#include <algorithm>
#include <cmath>

#include "core/loft.h"

namespace {
#include "core/human_base_data.inc"

float projecao(Vec3f p, Vec3f a, Vec3f b) {
    Vec3f ab = b - a;
    return clamp01(dot(p - a, ab) / std::max(dot(ab, ab), 1e-8f));
}

void influencias(Vec3f p, VertPele& v) {
    const float ax = std::abs(p.x), y = p.y;
    auto pesos = [&](int a, int b, float wa) {
        v.osso[0] = uint8_t(a);
        v.osso[1] = uint8_t(b);
        v.peso = clamp01(wa);
    };

    float limiteBraco = .205f + .55f * std::max(0.f, 1.38f - y);
    float transicaoBraco = smoothstep(limiteBraco - .05f, limiteBraco + .05f, ax);
    // O topo do deltoide acompanha o braco. A transicao se inclina para fora
    // na axila, onde o peito e as costas precisam continuar presos ao tronco.
    float faixaOmbro = smoothstep(1.35f, 1.44f, y);
    float lateralOmbro = ax - .85f * std::max(0.f, 1.435f - y);
    float pesoDeltoide = faixaOmbro * smoothstep(.125f, .254f, lateralOmbro);
    if ((transicaoBraco > 0.f || pesoDeltoide > 0.f) && y > .65f) {
        const bool esquerda = p.x > 0;
        const int braco = esquerda ? BRACO_E : BRACO_D;
        const int antebraco = esquerda ? ANTEBRACO_E : ANTEBRACO_D;
        const int mao = esquerda ? MAO_E : MAO_D;
        Vec3f q{ax, y, p.z};
        const Vec3f ombro{.194f, 1.441f, .020f}, cotovelo{.366f, 1.244f, .020f};
        const Vec3f pulso{.535f, 1.075f, .050f}, dedos{.660f, .950f, .050f};
        float t1 = projecao(q, ombro, cotovelo);
        float t2 = projecao(q, cotovelo, pulso);
        float t3 = projecao(q, pulso, dedos);
        if (transicaoBraco < 1.f || (t1 < .86f && ax < .43f))
            pesos(braco, PEITO, lerp(transicaoBraco * smoothstep(0.f, .22f, t1), 1.f, pesoDeltoide));
        else if (t2 < .82f && ax < .51f)
            pesos(antebraco, braco, smoothstep(.02f, .32f, t2));
        else
            pesos(mao, antebraco, smoothstep(.02f, .42f, t3));
        return;
    }

    if (y < 1.02f) {
        const bool esquerda = p.x > 0;
        const int coxa = esquerda ? COXA_E : COXA_D;
        const int canela = esquerda ? CANELA_E : CANELA_D;
        const int pe = esquerda ? PE_E : PE_D;
        if (y < .14f) pesos(pe, canela, smoothstep(.13f, .055f, y));
        else if (y < .54f) pesos(canela, coxa, smoothstep(.51f, .25f, y));
        else pesos(coxa, QUADRIL, smoothstep(1.01f, .86f, y) * smoothstep(0.f, .06f, ax));
        return;
    }

    if (y < 1.07f) pesos(QUADRIL, COLUNA, 1.f);
    else if (y < 1.28f) pesos(COLUNA, QUADRIL, smoothstep(1.09f, 1.22f, y));
    else if (y < 1.51f) pesos(PEITO, COLUNA, smoothstep(1.28f, 1.41f, y));
    else if (y < 1.60f) pesos(PESCOCO, PEITO, smoothstep(1.50f, 1.58f, y));
    else pesos(CABECA, PESCOCO, smoothstep(1.56f, 1.64f, y));
}
}  // namespace

MalhaPele corpoHumanoBase(const Proporcoes& pr, const Visual& vis) {
    MalhaPele m;
    m.v.resize(HUMANO_VERTICES);
    m.idx.assign(HUMANO_IDX, HUMANO_IDX + HUMANO_INDICES);
    const float s = pr.altura / 1.8f;

    for (uint32_t i = 0; i < HUMANO_VERTICES; ++i) {
        Vec3f p{HUMANO_POS[i*3] * .0001f, HUMANO_POS[i*3+1] * .0001f,
                HUMANO_POS[i*3+2] * .0001f};
        float ax0 = std::abs(p.x);
        float limiteBraco = .205f + .55f * std::max(0.f, 1.38f - p.y);
        float mascaraBraco = smoothstep(limiteBraco - .05f, limiteBraco + .05f, ax0);
        float lado = p.x > 0.f ? 1.f : -1.f;
        Vec3f eixo{lado * .70710678f, -.70710678f, 0.f};
        Vec3f cotovelo{lado * .366f, 1.244f, .020f};
        float aoLongo = dot(p - cotovelo, eixo), profundidade = p.z - cotovelo.z;
        float angulo = .70f * mascaraBraco * smoothstep(-.01f, .12f, aoLongo);
        float seno = std::sin(angulo), cosseno = std::cos(angulo);
        p = p + eixo * (aoLongo * cosseno + profundidade * seno - aoLongo);
        p.z += profundidade * cosseno - aoLongo * seno - profundidade;
        if (p.y < .96f && ax0 > .03f && mascaraBraco < 1.f) {
            float fechar = .09f * (1.f - smoothstep(.08f, .96f, p.y)) * smoothstep(.03f, .08f, ax0);
            p.x -= std::copysign(fechar * (1.f - mascaraBraco), p.x);
        }
        VertPele& v = m.v[i];
        influencias(p, v);
        // Uma queda suave entre o trapezio e a lateral do ombro na malha de repouso.
        float deltoide = smoothstep(.10f, .235f, ax0) * smoothstep(1.32f, 1.455f, p.y)
                         * (1.f - smoothstep(1.48f, 1.59f, p.y));
        p.y -= .016f * deltoide;
        p.x -= std::copysign(.007f * deltoide, p.x);
        float cabeca = smoothstep(1.50f, 1.64f, p.y);
        float alto = smoothstep(1.16f, 1.45f, p.y) * (1.f - cabeca);
        float pelve = 1.f - smoothstep(.91f, 1.22f, p.y);
        float massaCorpo = lerp(pr.massa, 1.f, cabeca);
        float largura = massaCorpo * (1.f + (pr.ombros-1.f)*alto + (pr.quadril-1.f)*pelve);
        p.x *= largura;
        p.z *= massaCorpo;

        v.pos = p * s;
        v.cor = vis.pele;
        v.ao = 1.f;
        v.material = MAT_PELE;
    }
    // Globos encaixados atras das palpebras do hm08. A normal da superficie
    // orienta a iris no shader.
    loft::Construtor construtor{m};
    construtor.material = MAT_OLHO;
    for (float sx : {1.f, -1.f}) {
        Vec3f c{sx * .034f * s, 1.687f * s, .136f * s};
        std::vector<loft::Anel> aneis;
        constexpr int linhas = 8;
        const float r = .013f * s;
        for (int i = 0; i <= linhas; ++i) {
            float ang = loft::PI_F * (1.f - i / float(linhas));
            aneis.push_back({c + Vec3f{0, 0, std::cos(ang) * r}, std::sin(ang) * r, std::sin(ang) * r,
                             CABECA, CABECA, 1.f});
        }
        construtor.tubo(aneis, 16, {0, 1, 0}, true, true,
                        [](int, float, Vec3f&, Vec3f, Vec3f, float&) { return Rgb{.34f, .22f, .11f}; });
    }
    loft::normaisSuaves(m);
    return m;
}
