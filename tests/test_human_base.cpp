#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "core/anim.h"
#include "core/body.h"

namespace {
#include "core/human_base_data.inc"
}

TEST_CASE("base humana compacta: anatomia completa dentro do orçamento") {
    Visual v;
    v.corpoBase=true;
    v.semCabelo=true;
    MalhaPele m=corpoHumano({1.8f,1.f,1.f,1.f},v);
    CHECK(m.triangulos() >= 26000);
    CHECK(m.triangulos() <= 28000);
    CHECK(m.v.size() < 15000);
    CHECK(m.v.size() < 65535);

    float minimo=10.f,maximo=-10.f;
    for (const VertPele& p:m.v) {
        minimo=std::min(minimo,p.pos.y);
        maximo=std::max(maximo,p.pos.y);
        CHECK(p.osso[0] < N_OSSOS_HUMANO);
        CHECK(p.osso[1] < N_OSSOS_HUMANO);
        CHECK(p.peso >= 0.f);
        CHECK(p.peso <= 1.f);
    }
    CHECK(minimo == doctest::Approx(0.f).epsilon(.002));
    CHECK(maximo == doctest::Approx(1.8f).epsilon(.002));
}

TEST_CASE("base humana: A-pose no estúdio e braços baixos na animação") {
    Esqueleto comum=esqueletoHumano({});
    Esqueleto base=esqueletoHumanoBase({});
    CHECK_FALSE(comum.poseA);
    CHECK(base.poseA);

    std::vector<Mat4> matrizes;
    calcularMatrizes(base,poseRepouso(base),matrizes);
    Vec3f aberto=matrizes[MAO_E].ponto(base.ossos[MAO_E].fim);
    CHECK(aberto.x > .55f);

    calcularMatrizes(base,poseParado(base,0.f),matrizes);
    Vec3f baixo=matrizes[MAO_E].ponto(base.ossos[MAO_E].fim);
    CHECK(std::abs(baixo.x) < .32f);
    CHECK(baixo.y < 1.20f);
}

TEST_CASE("base humana: topo do deltoide acompanha o braco ao baixar") {
    const Proporcoes proporcoes{1.82f, 1.07f, 1.f, 1.10f};
    MalhaPele malha = corpoHumanoBase(proporcoes, {});
    Esqueleto esqueleto = esqueletoHumanoBase(proporcoes);
    std::vector<Mat4> matrizes;
    calcularMatrizes(esqueleto, poseParado(esqueleto, 0.f), matrizes);

    for (float lado : {-1.f, 1.f}) {
        float queda = 0.f;
        int vertices = 0;
        for (size_t i = 0; i < HUMANO_VERTICES; ++i) {
            // Seleciona o topo do ombro na malha fonte, antes dos ajustes de proporcao.
            float x = lado * HUMANO_POS[3 * i] * .0001f;
            float y = HUMANO_POS[3 * i + 1] * .0001f;
            if (x < .214f || x > .26f || y < 1.455f) continue;
            const VertPele& v = malha.v[i];
            Vec3f animado = matrizes[v.osso[0]].ponto(v.pos) * v.peso
                          + matrizes[v.osso[1]].ponto(v.pos) * (1.f - v.peso);
            queda += matrizes[PEITO].ponto(v.pos).y - animado.y;
            ++vertices;
        }
        REQUIRE(vertices > 0);
        CHECK(queda / vertices > .015f);
    }
}

TEST_CASE("base humana: ombros nao esticam nas poses de jogo") {
    Visual visual;
    visual.corpoBase = true;
    for (const Proporcoes& proporcoes : {Proporcoes{}, Proporcoes{1.82f, 1.07f, 1.f, 1.10f}}) {
        MalhaPele malha = corpoHumanoBase(proporcoes, visual);
        Esqueleto esqueleto = esqueletoHumanoBase(proporcoes);
        for (const Pose& pose : {poseParado(esqueleto, 0.f), poseAndar(esqueleto, 1.3f, 1.6f), poseMirar(esqueleto, 0.f, 0.f)}) {
            std::vector<Mat4> matrizes;
            calcularMatrizes(esqueleto, pose, matrizes);
            float maiorAlongamento = 0.f;
            for (size_t i = 0; i < malha.idx.size(); i += 3) {
                for (int lado = 0; lado < 3; ++lado) {
                    const VertPele& a = malha.v[malha.idx[i + lado]];
                    const VertPele& b = malha.v[malha.idx[i + (lado + 1) % 3]];
                    Vec3f meio = (a.pos + b.pos) * 0.5f;
                    float escala = proporcoes.altura / 1.8f;
                    if (std::abs(meio.x) > .35f * escala || meio.y < 1.18f * escala || meio.y > 1.52f * escala) continue;
                    auto deformar = [&](const VertPele& v) {
                        return matrizes[v.osso[0]].ponto(v.pos) * v.peso + matrizes[v.osso[1]].ponto(v.pos) * (1.f - v.peso);
                    };
                    maiorAlongamento = std::max(maiorAlongamento, len(deformar(a) - deformar(b)) - len(a.pos - b.pos));
                }
            }
            CHECK(maiorAlongamento < .08f * (proporcoes.altura / 1.8f));
        }
    }
}

TEST_CASE("base humana: transicao das pernas permanece continua ao andar") {
    Visual visual;
    visual.corpoBase = true;
    MalhaPele malha = corpoHumanoBase({}, visual);
    Esqueleto esqueleto = esqueletoHumanoBase({});
    std::vector<Mat4> matrizes;
    calcularMatrizes(esqueleto, poseAndar(esqueleto, 1.3f, 1.6f), matrizes);
    float maiorAlongamento = 0.f;
    for (size_t i = 0; i < malha.idx.size(); i += 3) {
        for (int lado = 0; lado < 3; ++lado) {
            const VertPele& a = malha.v[malha.idx[i + lado]];
            const VertPele& b = malha.v[malha.idx[i + (lado + 1) % 3]];
            Vec3f meio = (a.pos + b.pos) * 0.5f;
            if (std::abs(meio.x) > .08f || meio.y < .70f || meio.y > 1.02f) continue;
            auto deformar = [&](const VertPele& v) {
                return matrizes[v.osso[0]].ponto(v.pos) * v.peso + matrizes[v.osso[1]].ponto(v.pos) * (1.f - v.peso);
            };
            maiorAlongamento = std::max(maiorAlongamento, len(deformar(a) - deformar(b)) - len(a.pos - b.pos));
        }
    }
    CHECK(maiorAlongamento < .045f);
}

TEST_CASE("base humana: proporcoes nao alteram os pesos do rig") {
    Visual visual;
    visual.corpoBase = true;
    MalhaPele normal = corpoHumanoBase({}, visual);
    MalhaPele larga = corpoHumanoBase({1.82f, 1.12f, 1.08f, 1.15f}, visual);
    REQUIRE(normal.v.size() == larga.v.size());
    for (size_t i = 0; i < normal.v.size(); ++i) {
        CHECK(normal.v[i].osso[0] == larga.v[i].osso[0]);
        CHECK(normal.v[i].osso[1] == larga.v[i].osso[1]);
        CHECK(normal.v[i].peso == doctest::Approx(larga.v[i].peso));
    }
}

TEST_CASE("base humana: punhos preservam a geometria nas poses") {
    Visual visual;
    visual.corpoBase = true;
    MalhaPele malha = corpoHumanoBase({}, visual);
    Esqueleto esqueleto = esqueletoHumanoBase({});
    for (const Pose& pose : {poseRepouso(esqueleto), poseParado(esqueleto, 0.f),
                             poseAndar(esqueleto, 1.3f, 1.6f), poseMirar(esqueleto, 0.f, 0.f)}) {
        std::vector<Mat4> matrizes;
        calcularMatrizes(esqueleto, pose, matrizes);
        float maiorAlongamento = 0.f;
        float maiorRazao = 0.f;
        for (size_t i = 0; i < HUMANO_INDICES; i += 3) {
            for (int lado = 0; lado < 3; ++lado) {
                uint16_t ia = HUMANO_IDX[i + lado], ib = HUMANO_IDX[i + (lado + 1) % 3];
                auto original = [](uint16_t indice) {
                    return Vec3f{HUMANO_POS[indice * 3] * .0001f, HUMANO_POS[indice * 3 + 1] * .0001f,
                                 HUMANO_POS[indice * 3 + 2] * .0001f};
                };
                Vec3f a = original(ia), b = original(ib), meio = (a + b) * .5f;
                if (std::abs(meio.x) < .38f || meio.y < .90f || meio.y > 1.22f) continue;
                auto deformar = [&](const VertPele& v) {
                    return matrizes[v.osso[0]].ponto(v.pos) * v.peso + matrizes[v.osso[1]].ponto(v.pos) * (1.f - v.peso);
                };
                float comprimentoOriginal = len(a - b);
                float comprimentoFinal = len(deformar(malha.v[ia]) - deformar(malha.v[ib]));
                maiorAlongamento = std::max(maiorAlongamento, comprimentoFinal - comprimentoOriginal);
                if (comprimentoOriginal > .003f) maiorRazao = std::max(maiorRazao, comprimentoFinal / comprimentoOriginal);
            }
        }
        INFO("alongamento=", maiorAlongamento, ", razao=", maiorRazao);
        CHECK(maiorAlongamento < .025f);
        CHECK(maiorRazao < 3.f);
    }
}

TEST_CASE("base humana: maos acompanham o antebraco") {
    Visual visual;
    visual.corpoBase = true;
    MalhaPele malha = corpoHumanoBase({}, visual);
    float soma = 0.f;
    int n = 0;
    for (const VertPele& v : malha.v) {
        if (v.pos.x > .57f && v.osso[0] == MAO_E) {
            soma += v.pos.z;
            ++n;
        }
    }
    REQUIRE(n > 100);
    CHECK(soma / n < .08f);
}
