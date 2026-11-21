#include <doctest/doctest.h>
#include "core/places.h"

TEST_CASE("lê lugar e estrada") {
    auto m = lerMapa("lugar cidade Valdoro 200 -300 0 90\nlugar vilarejo Pedrabranca -100 150 15 50\nestrada Valdoro Pedrabranca\n");
    REQUIRE(m.lugares.size() == 2);
    CHECK(m.lugares[0].tipo == TipoLugar::Cidade);
    CHECK(m.lugares[0].x == 200);
    CHECK(m.lugares[0].z == -300);
    CHECK(m.lugares[1].rot == 15);
    CHECK(m.lugares[1].raio == 50);
    CHECK(m.estradas.size() == 1);
    CHECK(m.erros.empty());
}
TEST_CASE("ignora comentários e linhas vazias") {
    CHECK(lerMapa("# oi\n\n   \n").erros.empty());
}
TEST_CASE("linha inválida vira erro com número e não derruba o resto") {
    auto m = lerMapa("lugar cidade A 0 0 0 10\nlugar dragao B 1 1 0 5\nlugar cidade C 1\n");
    CHECK(m.lugares.size() == 1);
    REQUIRE(m.erros.size() == 2);
    CHECK(m.erros[0].linha == 2);
    CHECK(m.erros[1].linha == 3);
}
TEST_CASE("estrada para lugar inexistente é erro") {
    auto m = lerMapa("lugar cidade A 0 0 0 10\nestrada A Z\n");
    CHECK(m.estradas.empty());
    CHECK(m.erros.size() == 1);
}
TEST_CASE("fora do mundo é erro") {
    CHECK(lerMapa("lugar cidade A 900 0 0 10\n").erros.size() == 1);
}
TEST_CASE("arquivo ausente") {
    auto m = carregarMapa("nao/existe.txt");
    CHECK(m.lugares.empty());
    CHECK(m.erros.size() == 1);
}
TEST_CASE("data/lugares.txt real é válido") {
    auto m = carregarMapa(DATA_DIR "/lugares.txt");
    CHECK(m.erros.empty());
    CHECK(m.lugares.size() >= 9);
}
