#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <functional>
#include <string>

#include "core/body.h"
#include "core/cabelo_corpo.h"
#include "core/hair_physics.h"
#include "core/superficie.h"

// O cabelo não pode atravessar o corpo: nem as orelhas, nem o rosto, o pescoço ou os ombros.
// Estes testes montam o personagem de verdade (base humana + penteado) e conferem a geometria:
//   - em repouso, nenhuma fita entra na pele nem é furada por ela (a orelha era o caso clássico);
//   - em movimento, rodando a física e a mesma conta do vertex shader (posicaoDaFita).
// Se um teste de movimento falhar depois de mexer na física, no shader ou num penteado, rode
// `testes --test-case="cabelo: em movimento*" -s` para ver qual movimento e quantos vértices.

namespace {
// Caixa fechada de lado 2 centrada na origem, com normais para fora.
MalhaPele caixa() {
    MalhaPele m;
    const Vec3f normais[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (Vec3f n : normais) {
        // Dois eixos do plano da face, com u x v = n (triângulos no sentido anti-horário vistos de fora).
        Vec3f u = std::abs(n.x) > .5f ? Vec3f{0, 1, 0} : Vec3f{1, 0, 0};
        Vec3f v = cross(n, u);
        u = cross(v, n);
        const uint16_t base = uint16_t(m.v.size());
        for (Vec3f canto : {n - u - v, n + u - v, n + u + v, n - u + v}) {
            VertPele p;
            p.pos = canto;
            p.normal = n;
            p.material = MAT_PELE;
            m.v.push_back(p);
        }
        m.idx.insert(m.idx.end(), {base, uint16_t(base + 1), uint16_t(base + 2), base, uint16_t(base + 2), uint16_t(base + 3)});
    }
    return m;
}

struct Penetracao {
    int verticesDentro = 0;   // vértices do cabelo mais de `tolerancia` para dentro da pele
    float pior = 0;           // o mais fundo deles (m)
    int arestasDoCabelo = 0;  // arestas das fitas que cruzam a pele
    int arestasDaPele = 0;    // arestas da pele que cruzam as fitas (a orelha furando uma fita)
};

// O personagem com cabelo comparado com a mesma base sem cabelo: os vértices e triângulos que vêm
// depois dos da base são o penteado. As arestas só são contadas quando `arestas` é verdadeiro (é a
// parte cara; em movimento, encostar de raspão numa quina conta como cruzar e não aparece na tela).
Penetracao medirPenetracao(const MalhaPele& comCabelo, const MalhaPele& base, float tolerancia, bool arestas) {
    const Superficie pele = peleParaCabelo(comCabelo, 1.25f);
    Penetracao r;
    for (size_t i = base.v.size(); i < comCabelo.v.size(); ++i) {
        const float d = pele.maisProximo(comCabelo.v[i].pos).distancia;
        if (d >= -tolerancia) continue;
        ++r.verticesDentro;
        r.pior = std::max(r.pior, -d);
    }
    if (!arestas) return r;

    const size_t primeiro = base.idx.size();
    const VertPele* primeiroVerticeDoCabelo = comCabelo.v.data() + base.v.size();
    const Superficie cabelo(comCabelo, [&](const VertPele& a, const VertPele&, const VertPele&) { return &a >= primeiroVerticeDoCabelo; });
    for (size_t i = primeiro; i + 2 < comCabelo.idx.size(); i += 3)
        for (int k = 0; k < 3; ++k)
            if (pele.atravessa(comCabelo.v[comCabelo.idx[i + k]].pos, comCabelo.v[comCabelo.idx[i + (k + 1) % 3]].pos)) ++r.arestasDoCabelo;
    for (size_t i = 0; i + 2 < primeiro; i += 3) {
        const VertPele* v[3] = {&comCabelo.v[comCabelo.idx[i]], &comCabelo.v[comCabelo.idx[i + 1]], &comCabelo.v[comCabelo.idx[i + 2]]};
        if (v[0]->material != MAT_PELE || v[0]->pos.y < 1.25f) continue;
        for (int k = 0; k < 3; ++k)
            if (cabelo.atravessa(v[k]->pos, v[(k + 1) % 3]->pos)) ++r.arestasDaPele;
    }
    return r;
}

struct Personagem {
    MalhaPele base, comCabelo;
};

Personagem montar(Penteado penteado) {
    Visual visual;
    Personagem p;
    p.base = corpoHumanoBase({}, visual);
    visual.penteado = penteado;
    p.comCabelo = corpoHumanoBase({}, visual);
    return p;
}
}  // namespace

TEST_CASE("superficie: distancia com sinal, empurrao para fora e travessia") {
    const MalhaPele m = caixa();
    const Superficie s(m, [](const VertPele&, const VertPele&, const VertPele&) { return true; });
    CHECK(s.triangulos() == 12);
    CHECK(s.maisProximo({0, 0, 1.5f}, 1.f).distancia == doctest::Approx(.5f));
    CHECK(s.maisProximo({.2f, .1f, .8f}, 1.f).distancia == doctest::Approx(-.2f));
    CHECK(s.maisProximo({.2f, .1f, .95f}).distancia == doctest::Approx(-.05f));
    CHECK(s.maisProximo({0, 0, 5.f}, .5f).distancia == doctest::Approx(.5f));  // fora do alcance

    const Vec3f fora = s.paraFora({.3f, .2f, .96f}, .05f);
    CHECK(fora.z == doctest::Approx(1.05f));
    CHECK(fora.x == doctest::Approx(.3f));
    CHECK(s.paraFora({0, 0, 3.f}, .05f).z == doctest::Approx(3.f));  // já fora: não mexe

    CHECK(s.atravessa({0, 0, 0}, {0, 0, 2}));
    CHECK_FALSE(s.atravessa({0, 0, 1.2f}, {0, 0, 2}));
}

TEST_CASE("cabelo: o volume da orelha contem a orelha inteira da base humana") {
    // Pele ao lado do crânio, na altura da orelha: tudo o que passa de x = 7,4 cm é a aba da orelha.
    const MalhaPele m = montar(Penteado::Sem).base;
    int naOrelha = 0;
    for (const VertPele& v : m.v) {
        const Vec3f p = v.pos;
        if (v.material != MAT_PELE || std::abs(p.x) < .074f || p.y < 1.625f || p.y > 1.73f || p.z < .005f || p.z > .085f) continue;
        const Elipsoide orelha = p.x > 0 ? ORELHA_DIREITA_BASE : ORELHA_DIREITA_BASE.espelhadoEmX();
        CHECK(orelha.distancia(p) <= 0.f);
        ++naOrelha;
    }
    CHECK(naOrelha > 100);
}

TEST_CASE("cabelo: em repouso, nenhuma fita entra na pele nem e furada por ela") {
    for (Penteado penteado : {Penteado::Ondulado, Penteado::Longo}) {
        CAPTURE(nomePenteado(penteado));
        const Personagem p = montar(penteado);
        const Penetracao r = medirPenetracao(p.comCabelo, p.base, .0005f, true);
        CHECK(r.verticesDentro == 0);
        CHECK(r.arestasDoCabelo == 0);
        CHECK(r.arestasDaPele == 0);
    }
}

// ---------------------------------------------------------------- cabelo em movimento

namespace {
using Ossos = std::array<Mat4, N_OSSOS_HUMANO>;

Mat4 girarEm(Vec3f pivo, const Mat4& rotacao) { return Mat4::trans(pivo) * rotacao * Mat4::trans(pivo * -1.f); }

// Personagem posado: pele por skinning (dois ossos por vértice) e fitas pela mesma conta da placa
// de vídeo (posicaoDaFita: presas à cabeça, curvadas pela física e apoiadas no corpo).
MalhaPele posar(const MalhaPele& repouso, const Ossos& ossos, const Mat4& mundo, const FisicaCabelo& fisica) {
    MalhaPele m = repouso;
    for (size_t i = 0; i < m.v.size(); ++i) {
        const VertPele& v = repouso.v[i];
        if (v.guiaCabelo[0] != SEM_GUIA_CABELO) {
            m.v[i].pos = mundo.ponto(posicaoDaFita(repouso.cabelo, fisica.deformacao(), v, ossos.data()));
            continue;
        }
        const Mat4 a = mundo * ossos[v.osso[0]], b = mundo * ossos[v.osso[1]];
        m.v[i].pos = a.ponto(v.pos) * v.peso + b.ponto(v.pos) * (1.f - v.peso);
        m.v[i].normal = normalizar(a.vetor(v.normal) * v.peso + b.vetor(v.normal) * (1.f - v.peso));
    }
    return m;
}

struct Quadro {
    Ossos ossos;
    Mat4 mundo = Mat4::id();
    Quadro() { ossos.fill(Mat4::id()); }
};

// Roda a física quadro a quadro (60 Hz) e mede a penetração a cada 6 quadros. Devolve o pior.
Penetracao simular(const Personagem& p, const std::function<Quadro(float)>& movimento, float duracao) {
    constexpr float dt = 1.f / 60.f;
    FisicaCabelo fisica(p.comCabelo.cabelo);
    Penetracao pior;
    for (int q = 0; q <= int(duracao / dt); ++q) {
        const float t = q * dt;
        const Quadro f = movimento(t);
        fisica.atualizar(dt, f.mundo * f.ossos[CABECA], f.mundo * f.ossos[PESCOCO], f.mundo * f.ossos[PEITO],
                         {1.2f * std::sin(t * 1.9f), 0, .8f * std::cos(t * 1.1f)});
        if (q < 3 || q % 6 != 0) continue;
        const Penetracao r = medirPenetracao(posar(p.comCabelo, f.ossos, f.mundo, fisica), p.base, .002f, false);
        pior.verticesDentro = std::max(pior.verticesDentro, r.verticesDentro);
        pior.pior = std::max(pior.pior, r.pior);
    }
    return pior;
}

// Movimentos que jogam o cabelo contra o corpo, nas velocidades do jogo: corrida com parada brusca,
// meia-volta, olhar para os lados, deitar a cabeça no ombro, olhar para baixo e para cima.
// `intensidade` multiplica amplitude e velocidade (2 = teste de estresse, fora da suíte).
std::vector<std::pair<std::string, std::function<Quadro(float)>>> movimentos(float intensidade) {
    const Esqueleto esq = esqueletoHumanoBase({});
    const Vec3f pescoco = esq.ossos[PESCOCO].inicio, cabeca = esq.ossos[CABECA].inicio;
    const float k = intensidade;
    // Cabeça girada (yaw), inclinada para o lado (roll) e para a frente (pitch); o pescoço leva 40%.
    auto cabecaGirada = [=](float yaw, float roll, float pitch) {
        Quadro f;
        f.ossos[PESCOCO] = girarEm(pescoco, Mat4::rotY(yaw * .4f) * Mat4::rotZ(roll * .4f) * Mat4::rotX(pitch * .4f));
        f.ossos[CABECA] = f.ossos[PESCOCO] * girarEm(cabeca, Mat4::rotY(yaw * .6f) * Mat4::rotZ(roll * .6f) * Mat4::rotX(pitch * .6f));
        return f;
    };
    return {
        {"corrida e parada", [=](float t) {
             // Acelera até 3 m/s x intensidade em 0,6 s, corre e para em 0,25 s / intensidade.
             const float v = 3.f * k, frear = 1.2f, parada = .25f / k;
             float z = t < .6f ? .5f * v / .6f * t * t : v * (t - .3f);
             if (t > frear) {
                 const float u = std::min(t - frear, parada);
                 z = v * (frear - .3f) + v * u - .5f * v / parada * u * u;
             }
             Quadro f;
             f.mundo = Mat4::trans({0, .02f * std::sin(t * 18.f), z});
             return f;
         }},
        {"meia-volta", [=](float t) {
             Quadro f;
             f.mundo = Mat4::rotY(1.4f * std::sin(t * 2.5f * k));  // pico de 3,5 rad/s x intensidade
             return f;
         }},
        {"cabeca vira de um lado para o outro", [=](float t) { return cabecaGirada(.55f * k * std::sin(t * 3.f * k), 0, 0); }},
        {"cabeca deita no ombro", [=](float t) { return cabecaGirada(0, .25f * k * std::sin(t * 3.f * k), 0); }},
        {"cabeca olha para baixo e para cima", [=](float t) { return cabecaGirada(0, 0, .3f * k * std::sin(t * 3.f * k)); }},
    };
}

void conferirMovimentos(Penteado penteado, float intensidade, int maximoDeVertices, float maximaProfundidade) {
    const Personagem p = montar(penteado);
    for (const auto& [nome, movimento] : movimentos(intensidade)) {
        const Penetracao r = simular(p, movimento, 1.6f);  // a parada da corrida acaba em 1,45 s
        MESSAGE(std::string(nomePenteado(penteado)), " x", intensidade, " / ", nome, ": ", r.verticesDentro, " vertices dentro, o pior a ",
                r.pior * 1000.f, " mm");
        CAPTURE(nome);
        CHECK(r.verticesDentro <= maximoDeVertices);
        CHECK(r.pior <= maximaProfundidade);
    }
}
}  // namespace

// Cada penteado tem ~10 mil vértices de fitas. O que sobra é raso e passageiro: um ou outro vértice
// de ponta roçando a pele num quadro (a física move guias; os vértices entre eles interpolam).
TEST_CASE("cabelo: em movimento, o ondulado nao entra no corpo") { conferirMovimentos(Penteado::Ondulado, 1.f, 8, .015f); }
TEST_CASE("cabelo: em movimento, o longo nao entra no corpo") { conferirMovimentos(Penteado::Longo, 1.f, 8, .015f); }

// Estresse (o dobro da velocidade e da amplitude): fora da suíte padrão, para comparar mudanças.
// testes --test-case="cabelo: estresse*" --no-skip -s
TEST_CASE("cabelo: estresse em movimento" * doctest::skip()) {
    for (Penteado penteado : {Penteado::Ondulado, Penteado::Longo}) conferirMovimentos(penteado, 2.f, 1000000, 1.f);
}
