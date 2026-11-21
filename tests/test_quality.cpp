#include <doctest/doctest.h>
#include "core/quality.h"

TEST_CASE("Baixo não tem efeitos pesados") {
    auto e = efeitosDe(Nivel::Baixo);
    CHECK_FALSE(e.sombras);
    CHECK_FALSE(e.bloom);
    CHECK_FALSE(e.raiosDeLuz);
    CHECK(e.distanciaVisao == 250);
}
TEST_CASE("Alto tem tudo") {
    auto e = efeitosDe(Nivel::Alto);
    CHECK(e.sombras);
    CHECK(e.bloom);
    CHECK(e.raiosDeLuz);
    CHECK(e.resolucaoSombra == 2048);
    CHECK(e.distanciaVisao == 450);
}
TEST_CASE("Médio tem sombras e mais nada pesado") {
    auto e = efeitosDe(Nivel::Medio);
    CHECK(e.sombras);
    CHECK(e.resolucaoSombra == 1024);
    CHECK_FALSE(e.bloom);
    CHECK(e.distanciaVisao == 350);
}
