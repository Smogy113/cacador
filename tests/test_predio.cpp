#include <doctest/doctest.h>
#include <cmath>
#include <set>
#include <string>
#include "core/predio.h"

static std::vector<Lote> lotes() {
    std::vector<Lote> v;
    const TipoPredio tiposM[] = {TipoPredio::Casa, TipoPredio::Taverna, TipoPredio::Xerife, TipoPredio::Cadeia, TipoPredio::Capela};
    const TipoPredio tiposF[] = {TipoPredio::Casa, TipoPredio::Celeiro, TipoPredio::Barracao, TipoPredio::Taverna};
    for (uint32_t s = 1; s <= 60; s++) {
        for (auto t : tiposM) {
            Lote l;
            l.semente = s * 7919u + uint32_t(t);
            l.estilo = Estilo::Medieval;
            l.tipo = t;
            l.largura = 5.5f + float(s % 7) * 0.9f;
            l.profundidade = 4.8f + float(s % 5) * 0.8f;
            l.andares = (t == TipoPredio::Taverna || s % 3 == 0) && t != TipoPredio::Capela && t != TipoPredio::Cadeia ? 2 : 1;
            v.push_back(l);
        }
        for (auto t : tiposF) {
            Lote l;
            l.semente = s * 104729u + uint32_t(t);
            l.estilo = Estilo::Faroeste;
            l.tipo = t;
            l.largura = t == TipoPredio::Barracao ? 4.2f : 6.f + float(s % 6);
            l.profundidade = t == TipoPredio::Barracao ? 3.6f : 5.f + float(s % 4);
            l.andares = t == TipoPredio::Taverna ? 2 : 1;
            v.push_back(l);
        }
    }
    return v;
}
// algum retangulo de colisao (local) toca o ponto/altura?
static bool colide(const Planta& p, Vec3f q, float raio) {
    MundoColisao m;
    for (auto& c : p.paredes) m.adicionarGirada(c);
    Vec3f r = m.resolver(q, raio);
    return len(r - q) > 1e-3f;
}

TEST_CASE("predio: porta no terreo e para fora") {
    for (auto& l : lotes()) {
        Planta p = gerarPredio(l);
        int portas = 0;
        for (auto& v : p.vaos)
            if (v.porta) {
                portas++;
                CHECK(v.andar == 0);
                CHECK(v.lado == 0);
                CHECK(v.v0 == doctest::Approx(p.piso));
            }
        CHECK(portas == 1);
        CHECK(p.entrada.z > l.profundidade * 0.5f);        // entrada do lado de fora, na frente
        CHECK(std::abs(p.entrada.x - p.porta.dobradica.x) < p.porta.largura + 0.2f);
    }
}
TEST_CASE("predio: vao da porta livre de colisao (>= 1,0 x 2,1)") {
    for (auto& l : lotes()) {
        Planta p = gerarPredio(l);
        CHECK(p.porta.largura >= 1.0f);
        CHECK(p.porta.altura >= 2.1f);
        for (auto& v : p.vaos) {
            if (!v.porta) continue;
            CHECK(v.u1 - v.u0 >= 1.0f);
            CHECK(v.v1 - v.v0 >= 2.1f);
            // o meio do vao, na linha da parede, nao colide (circulo pequeno)
            Vec3f q{(v.u0 + v.u1) * 0.5f, p.piso, l.profundidade * 0.5f};
            CHECK_FALSE(colide(p, q, 0.3f));
        }
    }
}
TEST_CASE("predio: janelas sem sobreposicao entre si e com a porta") {
    for (auto& l : lotes()) {
        Planta p = gerarPredio(l);
        for (size_t i = 0; i < p.vaos.size(); i++)
            for (size_t j = i + 1; j < p.vaos.size(); j++) {
                const Vao &a = p.vaos[i], &b = p.vaos[j];
                if (a.lado != b.lado) continue;
                bool sep = a.u1 + 0.25f <= b.u0 || b.u1 + 0.25f <= a.u0 || a.v1 + 0.3f <= b.v0 || b.v1 + 0.3f <= a.v0;
                CHECK(sep);
            }
    }
}
TEST_CASE("predio: interior livre >= 2,6 de altura e 2,5 de lado") {
    for (auto& l : lotes()) {
        Planta p = gerarPredio(l);
        CHECK(p.peDireito >= 2.6f);
        CHECK(p.interiorMeia.x * 2.f >= 2.5f);
        CHECK(p.interiorMeia.z * 2.f >= 2.5f);
        CHECK(p.interiorMeia.y * 2.f >= 2.6f);
        // o centro do interior nao colide com nada
        CHECK_FALSE(colide(p, {p.interiorCentro.x, p.piso, p.interiorCentro.z}, 0.4f));
    }
}
TEST_CASE("predio: caminho da entrada ao centro sem colisao") {
    for (auto& l : lotes()) {
        Planta p = gerarPredio(l);
        MundoColisao m;
        for (auto& c : p.paredes) m.adicionarGirada(c);
        Vec3f porta{0, p.piso, l.profundidade * 0.5f};
        for (auto& v : p.vaos)
            if (v.porta) porta.x = (v.u0 + v.u1) * 0.5f;
        Vec3f alvo{p.interiorCentro.x, p.piso, p.interiorCentro.z};
        Vec3f dentro{porta.x, p.piso, l.profundidade * 0.5f - 1.0f};  // atravessa reto e depois segue
        // entrada -> porta -> dentro -> centro, em passos curtos, circulo do cacador (0,35)
        int presos = 0;
        for (auto [a, b] : {std::pair<Vec3f, Vec3f>{Vec3f{porta.x, p.piso, p.entrada.z}, porta}, std::pair<Vec3f, Vec3f>{porta, dentro},
                            std::pair<Vec3f, Vec3f>{dentro, alvo}})
            for (float t = 0; t <= 1.f; t += 0.02f) {
                Vec3f q = a + (b - a) * t;
                if (len(m.resolver(q, 0.35f) - q) > 1e-3f) presos++;
            }
        CHECK(presos == 0);
    }
}
TEST_CASE("predio: pecas dentro do lote (+ beiral e varanda)") {
    for (auto& l : lotes()) {
        Planta p = gerarPredio(l);
        float mx = l.largura * 0.5f + 1.6f, mz = l.profundidade * 0.5f + 3.2f;
        for (auto& pc : p.pecas) {
            if (pc.tipo == PecaPredio::Triangulo) continue;
            CHECK(std::abs(pc.centro.x) <= mx);
            CHECK(std::abs(pc.centro.z) <= mz);
        }
    }
}
TEST_CASE("predio: variedade") {
    std::set<std::string> assinaturas;
    for (uint32_t s = 1; s <= 20; s++) {
        Lote l;
        l.semente = s * 2654435761u;
        Planta p = gerarPredio(l);
        std::string a;
        for (auto& v : p.vaos) a += std::to_string(v.lado) + ":" + std::to_string(int(std::round(v.u0 * 10))) + (v.porta ? "P" : "J") + ",";
        for (auto& pc : p.pecas)
            if (pc.mat == MatPeca::Telhado && pc.inclinacao != 0) { a += std::to_string(int(pc.inclinacao * 100)); break; }
        assinaturas.insert(a);
    }
    CHECK(assinaturas.size() >= 12);
}
TEST_CASE("predio: orcamento") {
    for (auto& l : lotes()) {
        Planta p = gerarPredio(l);
        CHECK(p.pecas.size() <= 600);
        CHECK(triangulosPlanta(p) <= 6000);
        CHECK(triangulosPlanta(p, false) < triangulosPlanta(p));
    }
}
