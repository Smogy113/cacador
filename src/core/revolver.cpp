#include "core/loft.h"

#include <initializer_list>
#include <iterator>

namespace {
using namespace loft;

constexpr float BOCA = .160f;
constexpr Rgb ACO{.23f, .245f, .275f};
constexpr Rgb MADEIRA{.33f, .176f, .091f};

struct Montagem {
    Vec3f origem, eixo, cima, lado;
    float escala;

    Vec3f ponto(Vec3f p) const {
        return origem + (eixo * p.x + cima * p.y + lado * p.z) * escala;
    }

    Vec3f local(Vec3f p) const {
        Vec3f d = (p - origem) * (1.f / escala);
        return {dot(d, eixo), dot(d, cima), dot(d, lado)};
    }

    Anel anel(Vec3f p, float rx, float rz) const {
        return {ponto(p), rx * escala, rz * escala, MAO_D, MAO_D, 1.f};
    }
};

Montagem montagem(const Esqueleto& esq, float escala) {
    const Osso& mao = esq.ossos[MAO_D];
    Vec3f eixo = normalizar(mao.fim - mao.inicio);
    Vec3f lado = normalizar(cross(eixo, {0, 0, 1}));
    Vec3f cima = cross(lado, eixo);
    float altura = esq.poseA ? .047f : .028f;
    float palma = esq.poseA ? .94f : .55f;
    float desvio = esq.poseA ? -.023f : 0.f;
    Vec3f origem = mao.inicio + (mao.fim - mao.inicio) * palma + (cima * altura + lado * desvio) * escala;
    return {origem, eixo, cima, lado, escala};
}

Pintor corSolida(Rgb cor) {
    return [cor](int, float, Vec3f&, Vec3f, Vec3f, float&) { return cor; };
}

uint16_t verticeMetal(MalhaPele& malha, const Montagem& m, Vec3f p, Rgb cor) {
    VertPele v;
    v.pos = m.ponto(p);
    v.cor = cor;
    v.material = MAT_METAL;
    v.osso[0] = v.osso[1] = MAO_D;
    malha.v.push_back(v);
    return uint16_t(malha.v.size() - 1);
}

// O perfil da arma é desenhado no plano do cano (X) e da parte de cima (Y).
// Os contornos convexos em ordem anti-horária formam as faces planas.
void chapa(MalhaPele& malha, const Montagem& m, std::initializer_list<Vec3f> contorno,
           float meiaLargura, Rgb cor, bool bisel) {
    Vec3f centro{};
    for (Vec3f p : contorno) centro = centro + p;
    const int n = int(contorno.size());
    centro = centro * (1.f / n);
    const int faixas = bisel ? 4 : 2;
    const int inicio = int(malha.v.size());
    for (int i = 0; i < faixas; ++i) {
        bool face = i == 0 || i == faixas - 1;
        float z = meiaLargura * (i < faixas / 2 ? -1.f : 1.f) * (face ? 1.f : .82f);
        for (Vec3f p : contorno) {
            Vec3f v = face && bisel ? centro + (p - centro) * .84f : p;
            v.z = z;
            verticeMetal(malha, m, v, face ? cor : cor * 1.20f);
        }
    }
    for (int i = 0; i < faixas - 1; ++i) {
        for (int j = 0; j < n; ++j) {
            uint16_t a = uint16_t(inicio + i * n + j), b = uint16_t(inicio + i * n + (j + 1) % n);
            uint16_t c = b + n, d = a + n;
            malha.idx.insert(malha.idx.end(), {a, b, c, a, c, d});
        }
    }
    uint16_t fundo = uint16_t(inicio), frente = uint16_t(inicio + (faixas - 1) * n);
    for (int j = 1; j + 1 < n; ++j) {
        malha.idx.insert(malha.idx.end(), {fundo, uint16_t(fundo + j + 1), uint16_t(fundo + j),
                                          frente, uint16_t(frente + j), uint16_t(frente + j + 1)});
    }
}

void canoDetalhado(MalhaPele& malha, const Montagem& m) {
    // O perfil volta para dentro na boca para formar uma cavidade escura de pouca profundidade.
    struct Perfil { float x, raio, tom; };
    constexpr Perfil perfil[] = {{.040f, .0113f, .91f}, {.052f, .0105f, 1.f},
        {BOCA - .002f, .009f, 1.03f}, {BOCA, .0078f, 1.37f}, {BOCA, .0057f, .89f},
        {BOCA - .010f, .0053f, .10f}};
    constexpr int lados = 12;
    size_t inicio = malha.v.size();
    for (const auto& anel : perfil) {
        for (int i = 0; i < lados; ++i) {
            float ang = i * 2.f * PI_F / lados;
            verticeMetal(malha, m, {anel.x, anel.raio * std::cos(ang), anel.raio * std::sin(ang)}, ACO * anel.tom);
        }
    }
    constexpr int ultimo = int(std::size(perfil)) - 1;
    uint16_t interior = verticeMetal(malha, m, {perfil[ultimo].x, 0, 0}, ACO * .08f);
    for (int i = 0; i < ultimo; ++i) {
        for (int j = 0; j < lados; ++j) {
            uint16_t a = uint16_t(inicio + i * lados + j), b = uint16_t(inicio + i * lados + (j + 1) % lados);
            uint16_t c = b + lados, d = a + lados;
            malha.idx.insert(malha.idx.end(), {a, b, c, a, c, d});
            if (i == ultimo - 1) malha.idx.insert(malha.idx.end(), {interior, d, c});
        }
    }
}
}  // namespace

Vec3f bocaDoRevolver(const Esqueleto& esq, float escala) {
    return montagem(esq, escala).ponto({BOCA, 0, 0});
}

void loft::revolverNaMao(Construtor& k, const Esqueleto& esq, float escala, bool detalhado) {
    const Montagem m = montagem(esq, escala);
    auto anel = [&](float x, float y, float z, float rx, float rz) { return m.anel({x, y, z}, rx, rz); };
    const float materialAnterior = k.material;
    k.material = MAT_METAL;

    if (detalhado) {
        canoDetalhado(k.m, m);
        k.tubo({anel(.040f, -.013f, .009f, .006f, .006f), anel(.146f, -.012f, .009f, .005f, .005f)},
                8, m.cima, true, true, corSolida(ACO * .86f));
        k.tubo({anel(-.016f, -.012f, 0, .017f, .017f), anel(-.012f, -.012f, 0, .021f, .021f),
                anel(.034f, -.012f, 0, .021f, .021f), anel(.039f, -.012f, 0, .0175f, .0175f)},
                18, m.cima, true, true, [&](int faixa, float ang, Vec3f& p, Vec3f lado, Vec3f cima, float& ao) {
            if (faixa == 0 || faixa == 3 || ang < 0.f) return ACO * 1.09f;
            float canal = std::max(0.f, std::cos(6.f * ang));
            p = p - (lado * std::cos(ang) + cima * std::sin(ang)) * (.0035f * escala * canal);
            ao = 1.f - .16f * canal;
            return ACO * (1.13f - .24f * canal);
        });
        chapa(k.m, m, {{.132f, .008f}, {.151f, .008f}, {.149f, .018f}, {.140f, .016f}}, .003f, ACO, true);
        chapa(k.m, m, {{-.037f, -.008f}, {-.018f, -.001f}, {-.028f, .020f}, {-.044f, .023f}, {-.049f, .016f}},
              .0055f, ACO, true);
    } else {
        k.tubo({anel(.033f, 0, 0, .011f, .011f), anel(BOCA, 0, 0, .008f, .008f)},
                8, m.cima, true, true, corSolida(ACO));
        k.tubo({anel(-.014f, -.012f, 0, .020f, .020f), anel(.038f, -.012f, 0, .020f, .020f)},
                8, m.cima, true, true, corSolida(ACO * 1.12f));
    }

    chapa(k.m, m, {{-.040f, -.042f}, {.037f, -.037f}, {.055f, -.023f}, {.038f, .006f}, {-.036f, .006f}},
          .0118f, ACO * .88f, detalhado);

    // O guarda-mato é um arco separado da armação, com espaço visível por dentro.
    std::vector<Anel> guarda{anel(-.026f, -.038f, 0, .0057f, .0030f), anel(-.008f, -.063f, 0, .0055f, .0026f),
                            anel(.028f, -.061f, 0, .0053f, .0026f), anel(.039f, -.036f, 0, .0059f, .0030f)};
    k.tubo(detalhado ? suavizar(guarda, 2) : guarda, detalhado ? 5 : 3, m.lado, false, false, corSolida(ACO * .94f));
    if (detalhado) {
        k.tubo({anel(.005f, -.034f, 0, .0032f, .0026f), anel(.008f, -.046f, 0, .0032f, .0025f),
                anel(.002f, -.054f, 0, .0029f, .0017f)}, 4, m.lado, false, true, corSolida(ACO * 1.15f));
    }

    const Anel topo = anel(-.030f, -.027f, 0, .013f, .0105f);
    const Anel base = anel(-.058f, -.090f, 0, .019f, .010f);
    std::vector<Anel> cabo{topo};
    if (detalhado) {
        cabo.push_back(anel(-.036f, -.045f, 0, .013f, .012f));
        cabo.push_back(anel(-.052f, -.080f, 0, .021f, .014f));
    }
    cabo.push_back(base);
    k.material = MAT_MADEIRA_ARMA;
    size_t inicioCabo = k.m.v.size();
    k.tubo(cabo, detalhado ? 8 : 6, m.eixo, true, true, [&](int faixa, float ang, Vec3f&, Vec3f, Vec3f, float&) {
        return MADEIRA * (.87f + .12f * std::abs(std::sin(ang)) + .045f * float(faixa));
    });
    // Coordenadas locais para o veio da madeira. A estampa acompanha a mão em todas as poses.
    for (size_t i = inicioCabo; i < k.m.v.size(); ++i) {
        auto& v = k.m.v[i];
        v.tang = m.local(v.pos);
    }
    k.material = materialAnterior;
}

MalhaPele malhaRevolver(const Esqueleto& esq, float escala) {
    MalhaPele malha;
    loft::Construtor k{malha};
    loft::revolverNaMao(k, esq, escala, true);
    loft::normaisSuaves(malha);
    return malha;
}

std::pair<size_t, size_t> fecharMaoDoRevolver(MalhaPele& malha, const Esqueleto& esq, float escala) {
    Montagem mao = montagem(esq, escala);
    mao.origem = esq.ossos[MAO_D].inicio;
    size_t inicio = malha.v.size();
    size_t fim = inicio;
    std::vector<bool> movidos(malha.v.size());
    for (size_t i = 0; i < malha.v.size(); ++i) {
        auto& v = malha.v[i];
        if (v.osso[0] != MAO_D) continue;
        Vec3f p = mao.local(v.pos);
        float polegar = smoothstep(.043f, .095f, p.y) * (1.f - smoothstep(.121f, .146f, p.x));
        float palma = smoothstep(.082f, .151f, p.x);
        if (palma == 0.f && polegar == 0.f) continue;

        // As juntas externas começam antes. O indicador fecha com um raio maior,
        // enquanto os outros dedos dão a volta no cabo e preservam a espessura.
        float junta = lerp(.125f, .151f, smoothstep(-.048f, .014f, p.y));
        float livre = std::max(0.f, p.x - junta);
        float indicador = smoothstep(.035f, .055f, p.y);
        float raio = lerp(.024f, .049f, indicador);
        float giro = std::min(3.15f, .84f * livre / raio);
        float espessura = p.z + .32f * livre;
        p.x += (raio + espessura) * std::sin(giro) - livre + .024f * polegar;
        p.z += (raio + espessura) * (std::cos(giro) - 1.f) + .32f * livre + .010f * polegar;
        p.y = lerp(p.y, .64f * p.y - .010f, palma) - .060f * polegar;
        v.pos = mao.ponto(p);
        inicio = std::min(inicio, i);
        fim = i + 1;
        movidos[i] = true;
    }
    // As normais da vizinhança também mudam. Limita a atualização a essa região
    // para preservar as normais já preparadas do cabelo e dos olhos.
    std::vector<bool> recalcular = movidos;
    for (size_t i = 0; i < malha.idx.size(); i += 3) {
        uint16_t a = malha.idx[i], b = malha.idx[i + 1], c = malha.idx[i + 2];
        if (movidos[a] || movidos[b] || movidos[c]) {
            inicio = std::min(inicio, size_t(std::min({a, b, c})));
            fim = std::max(fim, size_t(std::max({a, b, c})) + 1);
            recalcular[a] = recalcular[b] = recalcular[c] = true;
        }
    }
    std::vector<Vec3f> normais(fim - inicio);
    for (size_t i = 0; i < malha.idx.size(); i += 3) {
        uint16_t a = malha.idx[i], b = malha.idx[i + 1], c = malha.idx[i + 2];
        if (!recalcular[a] && !recalcular[b] && !recalcular[c]) continue;
        Vec3f origem = malha.v[a].pos;
        Vec3f n = cross(malha.v[b].pos - origem, malha.v[c].pos - origem);
        for (uint16_t v : {a, b, c})
            if (recalcular[v]) normais[v - inicio] = normais[v - inicio] + n;
    }
    for (size_t i = inicio; i < fim; ++i)
        if (recalcular[i]) malha.v[i].normal = normalizar(normais[i - inicio]);
    return {inicio, fim};
}
