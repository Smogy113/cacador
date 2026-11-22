#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "core/anim.h"
#include "core/body.h"

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
