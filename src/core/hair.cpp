#include "core/loft.h"
#include <algorithm>
#include <cmath>

namespace loft {
namespace {
constexpr float FLEX_RAIZ = .12f;

float distanciaGuias(const GuiaCabelo& a, const GuiaCabelo& b) {
    Vec3f raiz = a.pontos[0] - b.pontos[0], meio = a.pontos[1] - b.pontos[1], ponta = a.pontos[2] - b.pontos[2];
    float flex = (a.flexibilidade - b.flexibilidade) * .03f;
    return dot(raiz, raiz) * .65f + dot(meio, meio) * .5f + dot(ponta, ponta) + flex * flex;
}

VinculoCabelo vincular(const GuiaCabelo& guia, const PerfilCabelo& perfil, size_t quantidade) {
    VinculoCabelo v;
    float da = distanciaGuias(guia, perfil.guias[0]), db = 1e6f;
    for (uint8_t i = 1; i < quantidade; ++i) {
        float distancia = distanciaGuias(guia, perfil.guias[i]);
        if (distancia < da) {
            db = da; v.b = v.a; da = distancia; v.a = i;
        } else if (distancia < db) {
            db = distancia; v.b = i;
        }
    }
    if (v.a != v.b) v.pesoB = da / std::max(1e-8f, da + db);
    return v;
}
}  // namespace

ConstrutorCabelo::ConstrutorCabelo(MalhaPele& malha, Vec3f centro, float escala, float massa)
    : m_(malha), centro_(centro), escala_(escala), massa_(massa) {}

Vec3f ConstrutorCabelo::escalar(Vec3f p) const {
    return {p.x * escala_ * massa_, p.y * escala_, p.z * escala_ * massa_};
}

void ConstrutorCabelo::fita(const std::vector<PontoCabelo>& pts, float largura, Rgb cor, float ao) {
    size_t inicio = m_.v.size(), n = pts.size();
    if (n < 2 || inicio + n * 3 >= 64000) return;
    float flexPonta = pts.back().flex;
    float faixaFlexivel = std::max(flexPonta - FLEX_RAIZ, .001f);
    for (size_t i = 0; i < n; ++i) {
        Vec3f p = pts[i].p;
        Vec3f anterior = pts[i ? i - 1 : 0].p, seguinte = pts[std::min(i + 1, n - 1)].p;
        Vec3f tg = normalizar(seguinte - anterior);
        Vec3f nr = normalizar(p - centro_);
        Vec3f lado = normalizar(cross(tg, nr));
        float t = float(i) / (n - 1);
        float w = largura * lerp(.79f, 1.f, smoothstep(0.f, .25f, t)) * (1.f - .69f * smoothstep(.58f, 1.f, t));
        for (int q = -1; q <= 1; ++q) {
            // Faixa abaulada no centro, para que o brilho mostre o volume de cada mecha.
            Vec3f x = p + lado * (float(q) * w) + nr * (q == 0 ? w * .25f : 0.f);
            Vec3f normal = normalizar(nr + lado * (float(q) * .38f));
            VertPele v;
            v.pos = escalar(x);
            v.normal = normalizar({normal.x / massa_, normal.y, normal.z / massa_});
            v.cor = cor;
            v.material = MAT_CARTAO + .5f;
            v.ao = ao;
            v.pelos = (q + 1) * .5f;
            v.tv = t;
            float progresso = clamp01((pts[i].flex - FLEX_RAIZ) / faixaFlexivel);
            v.tang = normalizar(escalar(tg)) * (.05f + progresso);
            v.osso[0] = v.osso[1] = CABECA;
            m_.v.push_back(v);
        }
    }
    for (size_t i = 0; i + 1 < n; ++i) {
        bool livre = pts[i + 1].flex > .14f;
        for (size_t q = 0; q < 2; ++q) {
            uint16_t a = uint16_t(inicio + i * 3 + q), b = a + 1, c = a + 4, d = a + 3;
            m_.idx.insert(m_.idx.end(), {a, b, c, a, c, d});
            if (livre) m_.idx.insert(m_.idx.end(), {a, c, b, a, d, c});
        }
    }
    if (flexPonta <= FLEX_RAIZ) return;
    auto amostrar = [&](float flex) {
        if (pts.front().flex >= flex) return escalar(pts.front().p);
        for (size_t i = 1; i < n; ++i) {
            if (pts[i].flex < flex) continue;
            float t = (flex - pts[i - 1].flex) / (pts[i].flex - pts[i - 1].flex);
            return escalar(pts[i - 1].p + (pts[i].p - pts[i - 1].p) * t);
        }
        return escalar(pts.back().p);
    };
    GuiaCabelo guia{{amostrar(FLEX_RAIZ), amostrar(lerp(FLEX_RAIZ, flexPonta, .5f)), escalar(pts.back().p)},
                    clamp01(flexPonta), {}};
    float a = len(guia.pontos[1] - guia.pontos[0]), b = len(guia.pontos[2] - guia.pontos[1]);
    if (a > .003f && b > .003f && a + b > .018f) fitas_.push_back({guia, a + b, inicio, m_.v.size()});
}

void ConstrutorCabelo::finalizar() {
    if (fitas_.empty()) return;
    auto& perfil = m_.cabelo;
    auto& c = perfil.colisores;
    c.centroCabeca = escalar(centro_);
    c.raiosCabeca = escalar({.077f, .095f, .103f});
    c.cranio = escalar(centro_ + Vec3f{0, .044f, -.003f});
    c.nuca = escalar(centro_ + Vec3f{0, -.033f, -.008f});
    c.pescoco = escalar({0, 1.548f, .029f});
    c.ombroE = escalar({-.17f, 1.43f, 0});
    c.ombroD = escalar({ .17f, 1.43f, 0});
    c.raioCranio = .081f * escala_ * massa_;
    c.raioPescoco = .068f * escala_ * massa_;
    c.raioOmbros = .09f * escala_ * massa_;

    // Amostra as curvas mais afastadas das já escolhidas no espaço de repouso.
    // Isso cobre direções e comprimentos distintos com um orçamento fixo.
    const auto maior = std::max_element(fitas_.begin(), fitas_.end(),
                                        [](const Fita& a, const Fita& b) { return a.comprimento < b.comprimento; });
    perfil.guias[0] = maior->guia;
    perfil.quantidade = 1;
    // A distância ao guia mais próximo é atualizada uma vez por novo guia.
    std::vector<float> distancias;
    distancias.reserve(fitas_.size());
    for (const auto& fita : fitas_) distancias.push_back(distanciaGuias(fita.guia, perfil.guias[0]));
    auto selecionar = [&](size_t limite, float separacao) {
        separacao *= escala_;
        while (perfil.quantidade < limite) {
            auto maisLonge = std::max_element(distancias.begin(), distancias.end());
            if (*maisLonge <= separacao * separacao) break;
            const auto& guia = fitas_[maisLonge - distancias.begin()].guia;
            perfil.guias[perfil.quantidade++] = guia;
            for (size_t i = 0; i < fitas_.size(); ++i) {
                distancias[i] = std::min(distancias[i], distanciaGuias(fitas_[i].guia, guia));
            }
        }
    };
    selecionar(MAX_GUIAS_PRINCIPAIS, .029f);
    perfil.principais = perfil.quantidade;
    selecionar(MAX_GUIAS_CABELO, .011f);
    for (size_t i = perfil.principais; i < perfil.quantidade; ++i)
        perfil.guias[i].pais = vincular(perfil.guias[i], perfil, perfil.principais);

    // Todos os vértices de uma fita compartilham os dois guias e o peso.
    for (const auto& fita : fitas_) {
        VinculoCabelo v = vincular(fita.guia, perfil, perfil.quantidade);
        uint8_t peso = uint8_t(std::lround(255.f * v.pesoB));
        for (size_t i = fita.inicio; i < fita.fim; ++i) {
            m_.v[i].guiaCabelo[0] = v.a;
            m_.v[i].guiaCabelo[1] = v.b;
            m_.v[i].misturaGuias = peso;
        }
    }
}

namespace {
constexpr Vec3f CENTRO{0, 1.698f, .055f};
constexpr float RAD = PI_F / 180.f;

struct FaceCranio {
    Vec3f a, u, v;  // primeiro vertice relativo ao centro e duas arestas
};

Vec3f direcao(float giro, float altura) {
    float g = giro * RAD, h = altura * RAD;
    return {std::cos(h) * std::sin(g), std::sin(h), std::cos(h) * std::cos(g)};
}

float linhaDoCabelo(float giro) {
    float absGiro = std::abs(giro);
    return lerp(37.f, 18.f, smoothstep(4.f, 71.f, absGiro))
         - 55.f * smoothstep(75.f, 166.f, absGiro) + .85f * std::cos(giro * .21f);
}

float volumeCoroa(Vec3f d) {
    return .0035f + .007f * smoothstep(.03f, .30f, std::abs(d.x)) * smoothstep(.1f, .8f, d.y);
}

class Cranio {
public:
    Cranio(const MalhaPele& m, float escala) {
        const float inversa = 1.f / escala;
        for (size_t i = 0; i < m.idx.size(); i += 3) {
            Vec3f a = m.v[m.idx[i]].pos * inversa;
            Vec3f b = m.v[m.idx[i + 1]].pos * inversa;
            Vec3f c = m.v[m.idx[i + 2]].pos * inversa;
            if (std::min({a.y, b.y, c.y}) >= 1.58f) faces_.push_back({a - CENTRO, b - a, c - a});
        }
    }

    Vec3f superficie(Vec3f d, float folga) const {
        float alcance = 0.f;
        for (const auto& f : faces_) {
            Vec3f p = cross(d, f.v);
            float det = dot(f.u, p);
            if (std::abs(det) < 1e-10f) continue;
            Vec3f origem = f.a * -1.f;
            float u = dot(origem, p) / det;
            if (u < 0.f || u > 1.f) continue;
            Vec3f q = cross(origem, f.u);
            float v = dot(d, q) / det;
            if (v < 0.f || u + v > 1.f) continue;
            alcance = std::max(alcance, dot(f.v, q) / det);
        }
        return CENTRO + d * ((alcance > 0.f ? alcance : .088f) + folga);
    }

private:
    std::vector<FaceCranio> faces_;
};

void couroCabeludo(MalhaPele& m, const Cranio& cranio, float escala, Rgb cor) {
    constexpr int VOLTA = 40, ANEIS = 9;
    uint16_t base = uint16_t(m.v.size());
    auto adicionar = [&](Vec3f d, float risca) {
        VertPele v;
        v.pos = cranio.superficie(d, volumeCoroa(d)) * escala;
        v.normal = d;
        v.osso[0] = v.osso[1] = CABECA;
        v.material = MAT_CABELO;
        v.cor = lerp(cor * .88f, cor * 1.52f, risca);
        v.ao = .82f;
        m.v.push_back(v);
    };
    for (int i = 0; i < ANEIS; ++i) {
        for (int j = 0; j < VOLTA; ++j) {
            float giro = j * 360.f / VOLTA - 180.f;
            Vec3f d = direcao(giro, lerp(linhaDoCabelo(giro), 90.f, float(i) / ANEIS));
            float risca = (1.f - smoothstep(.004f, .053f, std::abs(d.x))) * smoothstep(-.17f, .65f, d.z) * .7f;
            adicionar(d, risca);
        }
    }
    uint16_t topo = uint16_t(m.v.size());
    adicionar({0, 1, 0}, .2f);
    for (int i = 0; i < ANEIS; ++i) {
        for (int j = 0; j < VOLTA; ++j) {
            uint16_t a = base + i * VOLTA + j, b = base + i * VOLTA + (j + 1) % VOLTA;
            if (i == ANEIS - 1) m.idx.insert(m.idx.end(), {a, b, topo});
            else {
                uint16_t c = b + VOLTA, d = a + VOLTA;
                m.idx.insert(m.idx.end(), {a, b, c, a, c, d});
            }
        }
    }
}
}  // namespace

void cabeloOndulado(MalhaPele& m, float escala, Rgb cor) {
    Cranio cranio(m, escala);
    couroCabeludo(m, cranio, escala, cor);
    ConstrutorCabelo cabelo(m, CENTRO, escala, 1.f);

    // As camadas mais curtas começam junto da risca. As mechas inferiores saem da
    // lateral do couro e terminam em outra altura, com curvaturas alternadas.
    for (float lado : {-1.f, 1.f}) {
        for (int camada = 0; camada < 2; ++camada) {
            bool porCima = camada == 1;
            for (int k = 0; k < 12; ++k) {
                float u = (k + .5f) / 12.f;
                float fase = 2.37f * k + lado * .73f + camada * 1.81f;
                float giro = lerp(49.f, 178.f, u) + std::sin(fase) * 3.f;
                float fim = lerp(-13.f, -35.f, u);
                float raiz = lerp(49.f, 121.f, u);
                Vec3f d0 = normalizar({lado * (porCima ? .064f : .51f), std::sin(raiz * RAD), std::cos(raiz * RAD)});
                Vec3f d1 = direcao(lado * giro, fim);
                std::vector<PontoCabelo> pontos;
                const int amostrasArco = porCima ? 10 : 7;
                for (int i = 0; i <= amostrasArco; ++i) {
                    float t = float(i) / amostrasArco;
                    Vec3f d = normalizar(d0 + (d1 - d0) * t + Vec3f{0, .42f * std::sin(PI_F * t), 0});
                    float volume = volumeCoroa(d) + .002f + (porCima ? .005f : .001f) * std::sin(PI_F * t);
                    pontos.push_back({cranio.superficie(d, volume), .10f * t});
                }
                Vec3f inicio = pontos.back().p;
                Vec3f fora = normalizar(Vec3f{d1.x, 0, d1.z});
                Vec3f ladoDaMecha = cross(fora, {0, 1, 0});
                float comprimento = lerp(.060f, .095f, u) + .009f * std::cos(fase) - (porCima ? .023f : 0.f);
                const int amostrasPonta = porCima ? 7 : 5;
                for (int i = 1; i <= amostrasPonta; ++i) {
                    float t = float(i) / amostrasPonta;
                    float onda = .009f * std::sin(t * 5.5f + fase) * t;
                    float raio = (.018f + .003f * std::cos(fase)) * std::sin(t * PI_F) + .020f * t * t * t;
                    Vec3f p = inicio + Vec3f{0, -comprimento * t, 0} + fora * raio
                            + ladoDaMecha * (.014f * t * t + onda);
                    pontos.push_back({p, lerp(.10f, .69f, t * t)});
                }
                float tom = (porCima ? .92f : .80f) + .095f * std::cos(fase);
                cabelo.fita(pontos, porCima ? .0135f : .0145f, cor * tom, porCima ? .95f : .8f);
            }
        }

        // Franja cortina: sobe perto da risca, acompanha as temporas e vira para fora nas pontas.
        for (int k = 0; k < 8; ++k) {
            float u = k / 7.f;
            float fase = 2.2f * k + lado * .91f;
            float raiz = lerp(40.f, 68.f, u);
            auto alto = [&](Vec3f d, float folga) { return cranio.superficie(d, volumeCoroa(d) + folga); };
            Vec3f inicio = alto(normalizar({lado * lerp(.03f, .13f, u),
                                            std::sin(raiz * RAD), std::cos(raiz * RAD)}), .001f);
            Vec3f arco[] = {
                inicio,
                alto(direcao(lado * lerp(15.f, 33.f, u), lerp(43.f, 66.f, u)), .0045f),
                alto(direcao(lado * lerp(37.f, 65.f, u), lerp(31.f, 45.f, u)), .005f),
                alto(direcao(lado * lerp(43.f, 71.f, u), 15.f), .010f) + Vec3f{lado * .004f, 0, 0},
                {lado * lerp(.075f, .098f, u), lerp(1.684f, 1.666f, u), lerp(.129f, .080f, u)},
                {lado * lerp(.087f, .121f, u), lerp(1.657f, 1.631f, u) + .008f * std::sin(fase), lerp(.108f, .061f, u)},
                {lado * lerp(.110f, .130f, u), lerp(1.669f, 1.641f, u) + .010f * std::sin(fase), lerp(.077f, .039f, u)},
            };
            std::vector<Anel> aneis;
            for (Vec3f p : arco) aneis.push_back({p, 1.f, 1.f, CABECA, CABECA, 1.f});
            std::vector<PontoCabelo> pontos;
            auto curva = suavizar(aneis, 3);
            for (size_t i = 0; i < curva.size(); ++i) {
                float t = float(i) / (curva.size() - 1);
                Vec3f p = curva[i].c, d = normalizar(p - CENTRO);
                if (t < .83f) {
                    Vec3f limite = cranio.superficie(d, .006f);
                    if (dot(limite - p, d) > 0.f) p = limite;
                }
                pontos.push_back({p, .58f * smoothstep(.3f, 1.f, t)});
            }
            cabelo.fita(pontos, lerp(.0075f, .012f, u),
                         cor * (.96f + .10f * std::sin(fase)), .97f);
        }
    }
    cabelo.finalizar();
}
}  // namespace loft
