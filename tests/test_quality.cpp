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
#include <cstdio>
#include <fstream>

TEST_CASE("Intel sugere Baixo, NVIDIA sugere Alto") {
    CHECK(sugerirNivel("Intel(R) HD Graphics 4000") == Nivel::Baixo);
    CHECK(sugerirNivel("llvmpipe (LLVM 17.0.6, 256 bits)") == Nivel::Baixo);
    CHECK(sugerirNivel("NVIDIA GeForce GTX 1060/PCIe/SSE2") == Nivel::Alto);
    CHECK(sugerirNivel("AMD Radeon RX 580") == Nivel::Alto);
    CHECK(sugerirNivel("Placa Misteriosa 3000") == Nivel::Medio);
}
TEST_CASE("Intel Arc é placa dedicada") { CHECK(sugerirNivel("Intel(R) Arc(TM) A770 Graphics") == Nivel::Alto); }
TEST_CASE("config inválida usa padrão") {
    CHECK(lerConfig("nao/existe.txt", {Nivel::Medio}).nivel == Nivel::Medio);
    CHECK_FALSE(temConfig("nao/existe.txt"));
    const char* p = "teste_config_lixo.txt";
    { std::ofstream f(p); f << "qualidade=ultra\nbobagem\n"; }
    CHECK(lerConfig(p, {}).nivel == Nivel::Baixo);
    CHECK(lerConfig(p, {}).penteado == Penteado::Sem);
    std::remove(p);
}
TEST_CASE("salvar e ler config") {
    const char* p = "teste_config.txt";
    { std::ofstream f(p); f << "qualidade=medio\n"; }
    CHECK(lerConfig(p, {}).nivel == Nivel::Medio);
    CHECK(lerConfig(p, {}).penteado == Penteado::Sem);
    salvarConfig(p, {Nivel::Alto, Penteado::Ondulado});
    CHECK(temConfig(p));
    CHECK(lerConfig(p, {}).nivel == Nivel::Alto);
    CHECK(lerConfig(p, {}).penteado == Penteado::Ondulado);
    salvarConfig(p, {Nivel::Medio, Penteado::Longo});
    CHECK(lerConfig(p, {}).penteado == Penteado::Longo);
    CHECK(lerConfig(p, {}).nivel == Nivel::Medio);
    salvarConfig(p, {Nivel::Medio, Penteado::Sem});
    CHECK(lerConfig(p, {Nivel::Baixo, Penteado::Ondulado}).penteado == Penteado::Sem);
    std::remove(p);
}
TEST_CASE("penteado: nomes do config.txt e do --cabelo") {
    for (Penteado pt : PENTEADOS) {
        Penteado lido = Penteado::Sem;
        CHECK(lerPenteado(nomePenteado(pt), lido));
        CHECK(lido == pt);
    }
    Penteado p = Penteado::Longo;
    CHECK(lerPenteado("ONDULADO", p));
    CHECK(p == Penteado::Ondulado);
    CHECK_FALSE(lerPenteado("moicano", p));
    CHECK(p == Penteado::Ondulado);  // nome desconhecido não mexe
}
TEST_CASE("AMD integrada não começa no Alto") {
    CHECK(sugerirNivel("AMD Radeon(TM) Graphics") != Nivel::Alto);
    CHECK(sugerirNivel("AMD Radeon(TM) Vega 8 Graphics") != Nivel::Alto);
    CHECK(sugerirNivel("AMD Radeon R5 Graphics") != Nivel::Alto);
    CHECK(sugerirNivel("AMD Radeon RX 580") == Nivel::Alto);
}
TEST_CASE("SSAO só no Alto") {
    CHECK(efeitosDe(Nivel::Alto).ssao);
    CHECK_FALSE(efeitosDe(Nivel::Medio).ssao);
    CHECK_FALSE(efeitosDe(Nivel::Baixo).ssao);
}
