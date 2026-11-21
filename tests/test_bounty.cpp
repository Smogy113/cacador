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

static bool lugarDeEsconderijo(TipoLugar t) {
    return t == TipoLugar::Castelo || t == TipoLugar::Acampamento || t == TipoLugar::Mina || t == TipoLugar::Floresta ||
           t == TipoLugar::Cemiterio || t == TipoLugar::Ponte;
}
TEST_CASE("cartaz: temperamento ~40/35/25") {
    Mapa m = mapaReal();
    int n[3] = {0, 0, 0}, total = 0;
    for (uint32_t s = 0; s < 400; s++)
        for (auto& c : gerarCartazes(1000 + s, 6, m)) {
            n[int(c.temperamento)]++;
            total++;
        }
    CHECK(n[int(Temperamento::Covarde)] / float(total) == doctest::Approx(0.40).epsilon(0.125));
    CHECK(n[int(Temperamento::Teimoso)] / float(total) == doctest::Approx(0.35).epsilon(0.143));
    CHECK(n[int(Temperamento::Fujao)] / float(total) == doctest::Approx(0.25).epsilon(0.2));
}
TEST_CASE("cartaz: esconderijo fora de cidade, vilarejo e fazenda") {
    Mapa m = mapaReal();
    for (uint32_t s = 0; s < 50; s++)
        for (auto& c : gerarCartazes(s, 6, m)) {
            const Lugar* l = nullptr;
            for (auto& x : m.lugares)
                if (nomeLegivel(x.nome) == c.ultimoVisto) l = &x;
            REQUIRE(l);
            CHECK(lugarDeEsconderijo(l->tipo));
        }
}
TEST_CASE("cartaz: pista combina com temperamento") {
    for (auto& c : gerarCartazes(9, 20, mapaReal())) {
        if (c.temperamento == Temperamento::Covarde) CHECK(c.pista == "Costuma se entregar");
        if (c.temperamento == Temperamento::Teimoso) CHECK(c.pista == "Perigoso: nao se entrega");
        if (c.temperamento == Temperamento::Fujao) CHECK(c.pista == "Costuma fugir");
    }
}
TEST_CASE("novoCartaz: nome inedito") {
    Mapa m = mapaReal();
    auto cs = gerarCartazes(5, 6, m);
    for (uint32_t s = 0; s < 30; s++) {
        Cartaz n = novoCartaz(s, m, cs);
        for (auto& c : cs) CHECK(c.nome != n.nome);
        CHECK(n.recompensaVivo > n.recompensaMorto);
    }
}
