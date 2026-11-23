#include "core/body.h"
#include "core/loft.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace {

using loft::Anel;
using loft::Construtor;
using loft::PI_F;

#include "horse_base_data.inc"

Vec3f desquantizar(const int16_t* coordenadas) {
    return {coordenadas[0] * .0001f, coordenadas[1] * .0001f, coordenadas[2] * .0001f};
}

void ligar(VertPele& v, int a, int b, float peso) {
    v.osso[0] = uint8_t(a);
    v.osso[1] = uint8_t(b);
    v.peso = peso;
}

Anel anel(Vec3f p, float largura, float profundidade, int osso) {
    return {p, largura, profundidade, uint8_t(osso), uint8_t(osso), 1.f};
}

// Pivôs das andaduras e do assento estão no mesmo espaço da escultura.
void prenderPele(VertPele& v, const Esqueleto& e) {
    Vec3f p = v.pos;
    const bool anterior = p.z > 0;
    const int membro = anterior ? (p.x > 0 ? C_BRACO_DE : C_BRACO_DD)
                                : (p.x > 0 ? C_COXA_TE : C_COXA_TD);
    float escapula = smoothstep(.32f, .63f, p.z) * smoothstep(1.28f, 1.68f, p.y);
    float crista = smoothstep(.36f, .55f, p.z) * smoothstep(1.58f, 1.85f, p.y);
    float pescoco = std::max(escapula, crista);
    float cabeca = std::max(smoothstep(1.11f, 1.256f, p.z), smoothstep(2.008f, 2.077f, p.y));
    float lateral = lerp(1.f, smoothstep(.08f, .19f, std::abs(p.x)), smoothstep(.91f, 1.21f, p.y));
    float perna = lateral * smoothstep(.25f, .49f, std::abs(p.z)) *
                  smoothstep(anterior ? 1.47f : 1.56f, .99f, p.y);
    float junta = e.ossos[membro + 1].inicio.y;
    float boleto = e.ossos[membro + 2].inicio.y;
    if (p.y <= boleto + .067f) {
        ligar(v, membro + 2, membro + 1, smoothstep(boleto + .067f, boleto - .055f, p.y));
    } else if (p.y <= junta + .13f) {
        ligar(v, membro + 1, membro, smoothstep(junta + .13f, junta - .115f, p.y));
    } else if (cabeca > 0) {
        ligar(v, C_CABECA, C_PESCOCO, cabeca);
    } else if (pescoco > perna) {
        ligar(v, C_PESCOCO, C_CORPO, pescoco - perna);
    } else {
        ligar(v, membro, C_CORPO, perna - pescoco);
    }
}

void pintarPele(VertPele& v, const VisualCavalo& visual) {
    Vec3f p = v.pos;
    float lado = std::abs(p.x), y = p.y, z = p.z;
    float tronco = smoothstep(.88f, 1.26f, y);
    float dorso = smoothstep(1.40f, 1.75f, y) * smoothstep(.91f, .27f, z);
    float regiaoPerna = smoothstep(.32f, .55f, std::abs(z));
    float canela = smoothstep(.77f, .24f, y) * regiaoPerna;
    float t = -.8f * (y - 2.015f) + .6f * (z - 1.170f);
    float frente = .6f * (y - 2.015f) + .8f * (z - 1.170f);
    float focinho = smoothstep(.365f, .56f, t) * smoothstep(1.24f, 1.43f, z);
    float narina = smoothstep(.032f, .008f, std::hypot(t - .515f, frente + .38f * (t - .515f) - .050f));
    narina *= focinho * smoothstep(.024f, .046f, lado);
    float labio = smoothstep(.018f, .004f, std::abs(t - (.577f - .59f * std::max(-frente, 0.f))));
    labio *= smoothstep(1.29f, 1.44f, z);
    float olho = smoothstep(.075f, .014f, std::hypot((y - 1.928f) * 1.45f, z - 1.299f));
    olho *= smoothstep(.060f, .098f, lado);
    float concha = smoothstep(2.072f, 2.105f, y) * smoothstep(2.228f, 2.198f, y) *
                   smoothstep(.03f, .008f, std::abs(lado - .101f - .30f * (y - 2.135f))) *
                   smoothstep(1.161f, 1.179f, z - .22f * (y - 2.135f));

    // A pelagem conserva o pigmento de cada cavalo e escurece nas pontas.
    Rgb ponta = visual.crina * .55f + visual.pelo * .16f;
    float nuvem = std::sin(y * 13.f + z * 8.f) * std::sin(lado * 24.f - z * 12.f);
    v.cor = visual.pelo * (.95f + .045f * nuvem - .18f * dorso);
    v.cor = lerp(v.cor, v.cor * .94f + Rgb{.030f, .012f, .004f}, tronco * (1.f - dorso));
    v.cor = lerp(v.cor, ponta * .91f, canela * .83f);
    v.cor = lerp(v.cor, ponta * .67f + Rgb{.026f, .025f, .026f}, focinho * .89f);
    v.cor = lerp(v.cor, ponta * .45f, std::max({.70f * labio, .66f * olho, .80f * concha}));
    v.cor = lerp(v.cor, {.014f, .010f, .009f}, .86f * narina);
    if (visual.meias) {
        float altura = .38f + .055f * std::sin(z * 15.f + p.x * 9.f);
        float meia = smoothstep(altura + .045f, altura - .04f, y);
        v.cor = lerp(v.cor, {.69f, .65f, .57f}, meia);
    }
    float axila = smoothstep(.18f, .06f, lado) * smoothstep(1.30f, 1.08f, y) * smoothstep(.38f, .65f, std::abs(z));
    v.ao = 1.f - .12f * tronco * std::max(-v.normal.y, 0.f) - .15f * axila - .18f * narina - .12f * concha;
    Vec3f pelo{0.f, -.85f, -.52f};
    pelo = pelo - v.normal * dot(v.normal, pelo);
    if (dot(pelo, pelo) < .01f) pelo = cross(v.normal, {1, 0, 0});
    v.tang = normalizar(pelo);
    v.material = MAT_PELAGEM_CAVALO;
}

template <size_t Coordenadas, size_t Indices>
void anexarPele(MalhaPele& m, const Esqueleto& e, const VisualCavalo& visual,
                const int16_t (&pos)[Coordenadas], const int8_t (&normais)[Coordenadas],
                const uint16_t (&indices)[Indices]) {
    static_assert(Coordenadas % 3 == 0 && Indices % 3 == 0);
    for (size_t i = 0; i < Coordenadas; i += 3) {
        VertPele v;
        v.pos = desquantizar(pos + i);
        v.normal = normalizar({float(normais[i]), float(normais[i + 1]), float(normais[i + 2])});
        prenderPele(v, e);
        pintarPele(v, visual);
        m.v.push_back(v);
    }
    m.idx.assign(std::begin(indices), std::end(indices));
}

void fazerCascos(Construtor& k, const Esqueleto& e, bool meias, bool perto) {
    k.material = MAT_COURO_CAVALO;
    for (int osso : {C_CASCO_DE, C_CASCO_DD, C_CASCO_TE, C_CASCO_TD}) {
        Vec3f base = e.ossos[osso].fim;
        bool traseiro = osso == C_CASCO_TE || osso == C_CASCO_TD;
        float largura = traseiro ? .058f : .066f;
        const std::vector<Anel> casco = {
            anel(base, largura * .94f, MEIO_COMPRIMENTO_CASCO, osso),
            anel(base + Vec3f{0, .008f, 0}, largura, .085f, osso),
            anel(base + Vec3f{0, .041f, 0}, largura * .94f, .079f, osso),
            anel(base + Vec3f{0, .075f, 0}, largura * .81f, .063f, osso),
            anel(base + Vec3f{0, .103f, 0}, .039f, .044f, osso),
        };
        k.tubo(casco, perto ? 12 : 8, {0, 0, 1}, true, true,
               [&](int i, float ang, Vec3f& pos, Vec3f, Vec3f, float& ao) {
            float frente = ang < 0 ? 0.f : std::sin(ang);
            pos.x = base.x + (pos.x - base.x) * (1.f + .14f * frente);
            pos.z -= pos.y * .26f;  // a pinça avança à medida que o casco se alarga
            Rgb chifre = meias ? Rgb{.235f, .190f, .141f} : Rgb{.116f, .092f, .071f};
            ao = i == 0 ? .78f : 1.f;
            float fibra = .91f + .16f * std::cos(ang * 5.f);
            return chifre * (i == 1 ? .75f : fibra);
        });
    }
}

void fazerOlhos(MalhaPele& m, bool perto) {
    constexpr int ANEIS_MODELO = 4, LADOS_MODELO = 16;
    const int aneis = perto ? ANEIS_MODELO : 1;
    const int lados = perto ? LADOS_MODELO : 8;
    for (float sinal : {1.f, -1.f}) {
        auto vertice = [&](int indice, float u, float v) {
            VertPele olho;
            olho.pos = desquantizar(OLHO_POS + 3 * indice);
            olho.pos.x *= sinal;
            float iris = smoothstep(.94f, .32f, std::hypot(u + .09f, v * 1.16f));
            float pupila = smoothstep(.66f, .35f, std::hypot(u + .10f, (v - .10f) * 2.3f));
            olho.cor = lerp({.061f, .040f, .027f}, {.240f, .133f, .062f}, iris * (1.f - pupila));
            olho.ao = lerp(.72f, .96f, 1.f - u * u - v * v);
            olho.material = MAT_OLHO_CAVALO;
            ligar(olho, C_CABECA, C_CABECA, 1.f);
            m.v.push_back(olho);
        };

        uint16_t base = uint16_t(m.v.size());
        vertice(0, 0, 0);
        for (int i = 1; i <= aneis; ++i) {
            float r = float(i) / aneis;
            for (int j = 0; j < lados; ++j) {
                float ang = 2 * PI_F * j / lados;
                int indice = 1 + (i * ANEIS_MODELO / aneis - 1) * LADOS_MODELO + j * LADOS_MODELO / lados;
                vertice(indice, std::cos(ang) * r, std::sin(ang) * r);
            }
        }
        auto id = [&](int i, int j) { return uint16_t(base + 1 + i * lados + j % lados); };
        auto triangulo = [&](uint16_t a, uint16_t b, uint16_t c) {
            if (sinal > 0) std::swap(b, c);
            m.idx.insert(m.idx.end(), {a, b, c});
        };
        for (int j = 0; j < lados; ++j) {
            triangulo(base, id(0, j), id(0, j + 1));
            for (int i = 1; i < aneis; ++i) {
                triangulo(id(i - 1, j), id(i, j), id(i, j + 1));
                triangulo(id(i - 1, j), id(i, j + 1), id(i - 1, j + 1));
            }
        }
    }
}

// As grades assadas seguem a pele de repouso. A versão distante usa os
// mesmos pontos alternados, inclusive nas bordas.
template <int Secoes, int Pontos, typename Pintar>
void fazerSuperficie(MalhaPele& m, const int16_t (&pos)[Secoes * Pontos * 3],
                     bool perto, bool inverter, float espessura, Pintar pintar) {
    static_assert(Secoes % 2 == 1 && Pontos % 2 == 1);
    const int passo = perto ? 1 : 2;
    const int largura = (Pontos - 1) / passo + 1;
    const int altura = (Secoes - 1) / passo + 1;
    const uint16_t base = uint16_t(m.v.size());
    auto ponto = [&](int i, int j) { return desquantizar(pos + 3 * (Pontos * i + j)); };
    auto triangulo = [&](uint16_t a, uint16_t b, uint16_t c) {
        if (inverter) std::swap(b, c);
        m.idx.insert(m.idx.end(), {a, b, c});
    };
    for (int i = 0; i < Secoes; i += passo) {
        for (int j = 0; j < Pontos; j += passo) {
            VertPele v;
            v.pos = ponto(i, j);
            Vec3f fibra = ponto(i, std::min(j + 1, Pontos - 1)) - ponto(i, std::max(j - 1, 0));
            pintar(v, float(i) / (Secoes - 1), float(j) / (Pontos - 1), fibra);
            m.v.push_back(v);
            if (i > 0 && j > 0) {
                uint16_t d = uint16_t(m.v.size() - 1), b = d - 1, c = d - largura, a = c - 1;
                triangulo(a, b, d);
                triangulo(a, d, c);
            }
        }
    }

    // A borda dobra para dentro da pele, escondendo a espessura em ângulos rasantes.
    std::vector<uint16_t> borda;
    const uint16_t baseBorda = uint16_t(m.v.size());
    auto fazerBorda = [&](int linha, int coluna) {
        uint16_t indice = uint16_t(base + linha * largura + coluna);
        borda.push_back(indice);
        int i = linha * passo, j = coluna * passo;
        Vec3f du = ponto(std::min(i + 1, Secoes - 1), j) - ponto(std::max(i - 1, 0), j);
        Vec3f dv = ponto(i, std::min(j + 1, Pontos - 1)) - ponto(i, std::max(j - 1, 0));
        Vec3f fora = normalizar(cross(du, dv)) * (inverter ? -1.f : 1.f);
        VertPele v = m.v[indice];
        v.pos = v.pos - fora * espessura;
        v.cor = v.cor * .85f;
        m.v.push_back(v);
    };
    for (int j = 0; j < largura; ++j) fazerBorda(0, j);
    for (int i = 1; i < altura; ++i) fazerBorda(i, largura - 1);
    for (int j = largura - 2; j >= 0; --j) fazerBorda(altura - 1, j);
    for (int i = altura - 2; i > 0; --i) fazerBorda(i, 0);
    for (size_t i = 0; i < borda.size(); ++i) {
        size_t j = (i + 1) % borda.size();
        uint16_t abaixo = uint16_t(baseBorda + i), seguinte = uint16_t(baseBorda + j);
        triangulo(borda[i], borda[j], seguinte);
        triangulo(borda[i], seguinte, abaixo);
    }
}

void fazerCrina(MalhaPele& m, const Esqueleto& e, Rgb cor, bool perto) {
    auto pintarFios = [&](VertPele& v, float u, float t, Vec3f fibra) {
        float faixa = .92f + .10f * std::sin(91.f * u + 1.8f * std::sin(7.f * t));
        v.cor = cor * faixa * lerp(.90f, 1.24f, t);
        v.ao = lerp(.80f, .98f, t);
        v.material = MAT_CRINA_CAVALO;
        v.tang = normalizar(fibra);
        v.pelos = u;
        v.tv = t;
    };
    fazerSuperficie<CRINA_SEC, CRINA_PONTOS>(m, CRINA_POS, perto, true, .027f,
                                           [&](VertPele& v, float u, float t, Vec3f fibra) {
        prenderPele(v, e);
        pintarFios(v, u, t, fibra);
    });
    fazerSuperficie<TOPETE_SEC, TOPETE_PONTOS>(m, TOPETE_POS, perto, true, .019f,
                                             [&](VertPele& v, float u, float t, Vec3f fibra) {
        ligar(v, C_CABECA, C_CABECA, 1.f);
        pintarFios(v, u, t, fibra);
    });
}

// Mechas mais soltas dão movimento à borda da cauda, por cima do volume principal.
void fazerMechaCauda(Construtor& k, Vec3f raiz, Vec3f dobra, Vec3f queda, Vec3f ponta, Rgb cor) {
    auto aneis = loft::suavizar({
        anel(raiz,  .018f, .012f, C_CAUDA),
        anel(dobra, .026f, .019f, C_CAUDA),
        anel(queda, .020f, .013f, C_CAUDA),
        anel(ponta, .002f, .003f, C_CAUDA),
    }, 2);
    k.tubo(aneis, 5, {0, 0, -1}, true, true, [&](int i, float ang, Vec3f&, Vec3f, Vec3f, float& ao) {
        float faixa = ang < 0 ? .65f : .88f + .14f * std::sin(ang);
        ao = lerp(.80f, 1.f, float(i) / float(aneis.size() - 1));
        return cor * faixa;
    });
}

void fazerCauda(Construtor& k, Rgb cor, bool perto) {
    k.material = MAT_CABELO;
    const std::vector<Anel> volume = {
        {{0, 1.532f, -.987f}, .044f, .047f, C_CORPO, C_CAUDA, .83f},
        {{0, 1.450f, -1.071f}, .070f, .055f, C_CAUDA, C_CORPO, .91f},
        anel({.002f, 1.304f, -1.143f}, .092f, .064f, C_CAUDA),
        anel({.010f, 1.089f, -1.180f}, .112f, .061f, C_CAUDA),
        anel({.022f, .857f, -1.187f}, .105f, .052f, C_CAUDA),
        anel({.039f, .608f, -1.211f}, .079f, .047f, C_CAUDA),
        anel({.065f, .419f, -1.258f}, .038f, .032f, C_CAUDA),
        anel({.088f, .355f, -1.290f}, .009f, .012f, C_CAUDA),
    };
    auto aneis = loft::suavizar(volume, perto ? 2 : 1);
    k.tubo(aneis, perto ? 12 : 8, {0, 0, -1}, true, true,
           [&](int i, float ang, Vec3f& p, Vec3f, Vec3f, float& ao) {
        float onda = std::sin(ang * 4.f + p.y * 4.2f);
        ao = i < 3 ? .88f : 1.f;
        return cor * (.77f + .15f * onda + .09f * (1.5f - p.y));
    });

    if (!perto) return;
    for (int i = 0; i < 5; ++i) {
        float t = i / 4.f;
        float x = lerp(-.103f, .112f, t);
        float arco = 1.f - std::abs(2.f * t - 1.f);
        float delta = .027f * std::sin(6.f * PI_F * t + .6f);
        fazerMechaCauda(k, {x * .32f, 1.455f, -1.100f},
                        {x * .77f, 1.246f, -1.163f - arco * .048f},
                        {x + .023f, .850f + delta, -1.188f - arco * .051f},
                        {x * .64f + .068f, .419f + delta + .105f * (1.f - arco), -1.257f - arco * .024f},
                        cor * (.95f + .25f * arco));
    }
}

void fazerSela(Construtor& k, bool perto) {
    fazerSuperficie<MANTA_SEC, MANTA_PONTOS>(k.m, MANTA_POS, perto, false, .024f,
                                            [](VertPele& v, float u, float t, Vec3f) {
        float borda = std::min({u, 1.f - u, t, 1.f - t});
        v.cor = lerp({.271f, .177f, .113f}, {.291f, .130f, .089f}, smoothstep(0.f, .078f, borda));
        v.ao = .92f;
        v.material = MAT_TECIDO;
    });
    const Rgb couro{.216f, .123f, .070f}, ferragem{.25f, .239f, .214f};
    auto corUniforme = [](Rgb cor) {
        return [cor](int, float, Vec3f&, Vec3f, Vec3f, float&) { return cor; };
    };
    k.material = MAT_COURO_CAVALO;
    const std::vector<Anel> assento = {
        anel({0, 1.681f, -.429f}, .106f, .033f, C_CORPO),
        anel({0, 1.712f, -.351f}, .174f, .063f, C_CORPO),
        anel({0, 1.693f, -.251f}, .181f, .064f, C_CORPO),
        anel({0, 1.677f, -.123f}, .155f, .063f, C_CORPO),
        anel({0, 1.702f, .043f}, .145f, .051f, C_CORPO),
        anel({0, 1.739f, .167f}, .113f, .059f, C_CORPO),
        anel({0, 1.714f, .247f}, .079f, .036f, C_CORPO),
    };
    k.tubo(assento, perto ? 12 : 8, {0, 1, 0}, true, true,
           [&](int i, float ang, Vec3f&, Vec3f, Vec3f, float& ao) {
        float topo = std::max(0.f, std::sin(ang));
        ao = .81f + .19f * topo;
        return couro * (lerp(.70f, 1.0f, topo) + (i == 1 || i == 5 ? .10f : 0.f));
    });
    const std::vector<Anel> cilha = {
        anel({0, 1.290f, .155f}, .352f, .420f, C_CORPO),
        anel({0, 1.290f, .224f}, .340f, .416f, C_CORPO),
    };
    // O arco fica ligeiramente acima da pele, sem tampas no interior do tronco.
    k.tubo(cilha, perto ? 22 : 14, {0, 1, 0}, false, false, corUniforme(couro * .63f));
    for (float s : {1.f, -1.f}) {
        k.material = MAT_COURO_CAVALO;
        const std::vector<Anel> aba = {
            anel({s * .122f, 1.719f, .026f}, .053f, .133f, C_CORPO),
            anel({s * .229f, 1.601f, .036f}, .041f, .150f, C_CORPO),
            anel({s * .305f, 1.476f, .058f}, .024f, .139f, C_CORPO),
            anel({s * .342f, 1.381f, .037f}, .016f, .080f, C_CORPO),
        };
        k.tubo(aba, perto ? 12 : 7, {0, 0, 1}, true, true, corUniforme(couro * .84f));
        // O apoio fica sob a parte dianteira do pé na pose montada.
        const Vec3f apoio{s * .549f, 1.174f, .310f};
        const std::vector<Anel> loro = {
            anel({s * .225f, 1.621f, .044f}, .016f, .026f, C_CORPO),
            anel({s * .376f, 1.427f, .180f}, .013f, .022f, C_CORPO),
            anel(apoio + Vec3f{0, .108f, 0}, .010f, .019f, C_CORPO),
        };
        k.tubo(loro, 6, {0, 0, 1}, true, true, corUniforme(couro * .57f));

        k.material = MAT_METAL;
        // Estribo aberto, com apoio mais largo que o arco.
        std::vector<Anel> estribo;
        for (Vec3f p : {Vec3f{.004f, .108f, 0}, {.038f, .078f, 0}, {.063f, .028f, 0}, {.047f, 0, 0},
                        {-.047f, 0, 0}, {-.063f, .028f, 0}, {-.038f, .078f, 0}, {-.004f, .108f, 0}}) {
            Vec3f local{s * p.x, p.y, -.61f * p.x};
            estribo.push_back(anel(apoio + local, .008f, p.y < .01f ? .020f : .009f, C_CORPO));
        }
        k.tubo(estribo, perto ? 6 : 5, {s * .52f, 0, .85f}, true, true, corUniforme(ferragem));
    }
}

}  // namespace

Esqueleto esqueletoCavalo() {
    Esqueleto e;
    e.ossos.resize(N_OSSOS_CAVALO);
    e.ossos[C_CORPO] = {-1, {0, 1.35f, -.55f}, {0, 1.38f, .55f}};
    e.ossos[C_PESCOCO] = {C_CORPO, {0, 1.55f, .70f}, {0, 2.015f, 1.17f}};
    e.ossos[C_CABECA] = {C_PESCOCO, {0, 2.015f, 1.17f}, {0, 1.56f, 1.53f}};
    e.ossos[C_CAUDA] = {C_CORPO, {0, 1.532f, -.987f}, {0, .85f, -1.19f}};
    for (int lado = 0; lado < 2; ++lado) {
        float s = lado == 0 ? 1.f : -1.f;
        const int frente = lado == 0 ? C_BRACO_DE : C_BRACO_DD;
        const int tras = lado == 0 ? C_COXA_TE : C_COXA_TD;
        const Vec3f carpo{s * .215f, .617f, .633f}, boletoFrente{s * .213f, .167f, .618f};
        const Vec3f jarrete{s * .215f, .582f, -.853f}, boletoTras{s * .220f, .172f, -.741f};
        e.ossos[frente] = {C_CORPO, {s * .211f, 1.22f, .571f}, carpo};
        e.ossos[frente + 1] = {frente, carpo, boletoFrente};
        e.ossos[frente + 2] = {frente + 1, boletoFrente, {s * .213f, 0.f, .671f}};
        e.ossos[tras] = {C_CORPO, {s * .205f, 1.28f, -.712f}, jarrete};
        e.ossos[tras + 1] = {tras, jarrete, boletoTras};
        e.ossos[tras + 2] = {tras + 1, boletoTras, {s * .220f, 0.f, -.684f}};
    }
    return e;
}

Vec3f assentoCavalo() { return {0, 1.79f, -.08f}; }

MalhaPele corpoCavalo(const VisualCavalo& visual, bool perto) {
    MalhaPele m;
    m.v.reserve(perto ? 4800 : 1600);
    m.idx.reserve(3 * (perto ? 9000 : 2850));
    Esqueleto e = esqueletoCavalo();
    if (perto) anexarPele(m, e, visual, PERTO_POS, PERTO_NORMAL, PERTO_IDX);
    else anexarPele(m, e, visual, LONGE_POS, LONGE_NORMAL, LONGE_IDX);

    const size_t pele = m.v.size();
    Construtor k{m};
    fazerCascos(k, e, visual.meias, perto);
    fazerOlhos(m, perto);
    fazerCrina(m, e, visual.crina, perto);
    fazerCauda(k, visual.crina, perto);
    if (visual.selado) fazerSela(k, perto);

    // Preserva as normais da escultura e suaviza as peças adicionadas.
    for (size_t i = 0; i < m.idx.size(); i += 3) {
        auto a = m.idx[i], b = m.idx[i + 1], c = m.idx[i + 2];
        if (a < pele) continue;
        Vec3f n = cross(m.v[b].pos - m.v[a].pos, m.v[c].pos - m.v[a].pos);
        for (uint16_t id : {a, b, c}) m.v[id].normal = m.v[id].normal + n;
    }
    for (size_t i = pele; i < m.v.size(); ++i) m.v[i].normal = normalizar(m.v[i].normal);
    return m;
}
