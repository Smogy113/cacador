#include <cmath>
#include "core/bussola.h"
#include "doctest/doctest.h"

static const float CAMPO = 1.6f;  // ~92 graus visiveis na faixa
TEST_CASE("bussola: objetivo a frente fica no meio") {
    CHECK(posicaoNaBussola(0, {0, 0, 0}, {0, 0, 10}, CAMPO) == doctest::Approx(0.5f));
}
TEST_CASE("bussola: objetivo a direita fica a direita") {
    // yaw 0 olha para +z; a direita do jogador e -x
    float p = posicaoNaBussola(0, {0, 0, 0}, {-5, 0, 10}, CAMPO);
    CHECK(p > 0.55f);
    CHECK(p < 1.f);
    CHECK(posicaoNaBussola(0, {0, 0, 0}, {5, 0, 10}, CAMPO) < 0.45f);
}
TEST_CASE("bussola: objetivo atras fica fora") {
    float p = posicaoNaBussola(0, {0, 0, 0}, {0, 0, -10}, CAMPO);
    CHECK((p < 0.f || p > 1.f));
}
TEST_CASE("bussola: norte e leste") {
    // norte = z negativo; leste = +x (a direita de quem olha para o norte)
    float pi = 3.14159265f;
    CHECK(posicaoNaBussola(pi, {0, 0, 0}, {0, 0, -10}, CAMPO) == doctest::Approx(0.5f));
    CHECK(posicaoNaBussola(pi, {0, 0, 0}, {10, 0, -10}, CAMPO) > 0.5f);
    CHECK(rumoDoNorte(pi) == doctest::Approx(0.f).epsilon(1e-4));
    CHECK(std::abs(rumoDoNorte(0)) == doctest::Approx(pi));
}
