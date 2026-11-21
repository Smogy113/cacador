#include <doctest/doctest.h>
#include <set>
#include "core/bounty.h"

static Mapa mapaReal() { return carregarMapa(DATA_DIR "/lugares.txt"); }

TEST_CASE("vivo paga mais que morto") {
    for (auto& c : gerarCartazes(1, 20, mapaReal())) {
        CHECK(c.recompensaVivo > c.recompensaMorto);
        CHECK(c.recompensaMorto > 0);
    }
}
TEST_CASE("último visto é um lugar do mapa") {
    Mapa m = mapaReal();
    std::set<std::string> nomes;
    for (auto& l : m.lugares) nomes.insert(nomeLegivel(l.nome));
    for (auto& c : gerarCartazes(2, 20, m)) CHECK(nomes.count(c.ultimoVisto) == 1);
}
TEST_CASE("nomes não repetem") {
    std::set<std::string> vistos;
    for (auto& c : gerarCartazes(3, 30, mapaReal())) CHECK(vistos.insert(c.nome).second);
}
TEST_CASE("determinístico") {
    auto a = gerarCartazes(4, 10, mapaReal()), b = gerarCartazes(4, 10, mapaReal());
    REQUIRE(a.size() == 10);
    for (size_t k = 0; k < a.size(); k++) {
        CHECK(a[k].nome == b[k].nome);
        CHECK(a[k].recompensaVivo == b[k].recompensaVivo);
    }
}
TEST_CASE("mapa sem lugares ainda gera cartazes") {
    auto c = gerarCartazes(5, 3, Mapa{});
    REQUIRE(c.size() == 3);
    CHECK_FALSE(c[0].ultimoVisto.empty());
}
TEST_CASE("nomeLegivel troca _ por espaço") { CHECK(nomeLegivel("Mina_Funda") == "Mina Funda"); }
