// Cabeca em alta definicao com proporcoes de um rosto real (antropometria de homem adulto):
// distancia entre pupilas 63 mm, nariz 54 mm (raiz ao subnasal) e 20 mm de projecao, boca 53 mm,
// labio superior 22 mm, queixo 45 mm abaixo da boca. Perfil lateral (testa, glabela, raiz do nariz,
// dorso, ponta, columela, filtro, labios, sulco do queixo, queixo) tirado dessas medidas.
// A malha e mais densa na frente (olhos, nariz, boca) e mais rala atras.
#include <algorithm>
#include <cmath>
#include "core/body.h"
#include "core/head_cor.h"
#include "core/loft.h"
#include "core/noise.h"
#include "core/sdf_head.h"

namespace loft {
namespace {

float g(float x, float c, float s) {
    float t = (x - c) / s;
    return std::exp(-t * t);
}

// interpolacao suave numa tabela (y crescente)
struct Pt { float y, v; };
template <size_t N>
float tabela(const Pt (&t)[N], float y) {
    if (y <= t[0].y) return t[0].v;
    for (size_t i = 0; i + 1 < N; i++)
        if (y <= t[i + 1].y) {
            float u = (y - t[i].y) / (t[i + 1].y - t[i].y);
            u = u * u * (3 - 2 * u);
            return lerp(t[i].v, t[i + 1].v, u);
        }
    return t[N - 1].v;
}

// ---- perfil do meio do rosto (z, metros) com e sem o nariz ----
const Pt PERFIL[] = {{1.555f, .030f}, {1.565f, .036f}, {1.575f, .053f}, {1.585f, .082f}, {1.597f, .101f}, {1.610f, .094f},
                     {1.620f, .099f}, {1.627f, .1005f}, {1.632f, .097f}, {1.637f, .1012f}, {1.645f, .1005f}, {1.654f, .099f},
                     {1.660f, .112f}, {1.667f, .121f}, {1.676f, .1145f}, {1.688f, .108f}, {1.700f, .099f}, {1.709f, .093f},
                     {1.720f, .099f}, {1.735f, .095f}, {1.755f, .082f}, {1.775f, .060f}, {1.795f, .025f}};
const Pt BASE[] = {{1.555f, .030f}, {1.565f, .036f}, {1.575f, .053f}, {1.585f, .082f}, {1.597f, .101f}, {1.610f, .094f},
                   {1.620f, .099f}, {1.627f, .1005f}, {1.632f, .097f}, {1.637f, .1012f}, {1.645f, .1005f}, {1.654f, .099f},
                   {1.660f, .100f}, {1.667f, .100f}, {1.676f, .099f}, {1.688f, .097f}, {1.700f, .095f}, {1.709f, .094f},
                   {1.720f, .099f}, {1.735f, .095f}, {1.755f, .082f}, {1.775f, .060f}, {1.795f, .025f}};
// quanto o rosto curva para tras dos lados (z -= a * x^2)
const Pt CURVA[] = {{1.555f, 18}, {1.585f, 13}, {1.612f, 9}, {1.632f, 8.5f}, {1.66f, 8}, {1.69f, 7}, {1.72f, 6.5f}, {1.76f, 7}, {1.795f, 9}};
// largura do nariz
const Pt NARIZ_L[] = {{1.650f, .012f}, {1.656f, .0145f}, {1.662f, .016f}, {1.668f, .013f}, {1.678f, .011f}, {1.690f, .0085f}, {1.709f, .0065f}};
// cranio: raios lateral e frontal e centro em z
const Pt CR_X[] = {{1.53f, .076f}, {1.555f, .072f}, {1.575f, .069f}, {1.59f, .068f}, {1.61f, .070f}, {1.63f, .071f}, {1.66f, .072f}, {1.69f, .074f},
                   {1.72f, .076f}, {1.745f, .075f}, {1.77f, .068f}, {1.79f, .054f}, {1.80f, .03f}};
const Pt CR_Z[] = {{1.53f, .066f}, {1.555f, .066f}, {1.575f, .068f}, {1.59f, .075f}, {1.61f, .082f}, {1.63f, .088f}, {1.66f, .094f}, {1.69f, .098f},
                   {1.72f, .100f}, {1.745f, .098f}, {1.77f, .088f}, {1.79f, .068f}, {1.80f, .04f}};
const Pt CR_C[] = {{1.53f, .006f}, {1.555f, .005f}, {1.575f, .0f}, {1.59f, -.005f}, {1.61f, -.01f}, {1.63f, -.012f}, {1.72f, -.012f}, {1.745f, -.014f}, {1.80f, -.016f}};

constexpr float OLHO_X = 0.0315f, OLHO_Y = 1.701f, OLHO_R = 0.0122f;
constexpr float ALM_W = 0.0145f, ALM_H = 0.0049f;  // meia abertura do olho (abertura real ~10 mm)

float distSeg2D(float x, float y, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay;
    float t = std::clamp(((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy), 0.f, 1.f);
    float dx = ax + vx * t - x, dy = ay + vy * t - y;
    return std::sqrt(dx * dx + dy * dy);
}

// superficie do rosto (z) num ponto (x, y) da frente, sem o corte dos olhos
float rostoSemCorte(float x, float y) {
    float ax = std::abs(x);
    float base = tabela(BASE, y) - tabela(CURVA, y) * x * x;
    float nariz = (tabela(PERFIL, y) - tabela(BASE, y)) * g(x, 0, tabela(NARIZ_L, y));
    float z = base + nariz;
    z += 0.0045f * g(ax, 0.0165f, 0.0045f) * g(y, 1.659f, 0.0045f);                   // asas do nariz
    z -= 0.0075f * g(ax, OLHO_X, 0.02f) * g(y, OLHO_Y, 0.014f);                         // orbita funda (olhos encovados)
    z += 0.007f * g(y, 1.72f, 0.0065f) * g(ax, 0.028f, 0.024f);                         // arco da sobrancelha forte
    z += 0.0095f * g(y, 1.686f, 0.011f) * g(ax, 0.058f, 0.014f);                        // macas do rosto altas
    z -= 0.0040f * g(y, 1.655f, 0.012f) * g(ax, 0.05f, 0.012f);                         // bochecha cavada abaixo
    z -= 0.0022f * g(y, 1.688f, 0.005f) * g(ax, 0.03f, 0.012f);                         // olheira
    float dn = distSeg2D(ax, y, 0.020f, 1.660f, 0.034f, 1.632f);                        // sulco nariz-boca
    z -= 0.0028f * std::exp(-(dn / 0.0035f) * (dn / 0.0035f));
    z -= 0.0015f * g(x, 0, 0.0038f) * g(y, 1.646f, 0.005f);                             // filtro
    z += 0.0007f * g(ax, 0.0055f, 0.0018f) * g(y, 1.646f, 0.005f);
    z -= 0.0045f * g(ax, 0.0275f, 0.004f) * g(y, 1.632f, 0.004f);                       // cantos da boca
    z -= 0.0035f * std::max(0.f, ax - 0.026f) * 60.f * g(y, 1.6315f, 0.007f);          // labios terminam
    z += 0.0045f * g(y, 1.598f, 0.009f) * g(ax, 0, 0.026f);                             // queixo largo
    z -= 0.0009f * g(x, 0, 0.0028f) * g(y, 1.599f, 0.007f);                             // covinha do queixo
    z += 0.0035f * g(y, 1.603f, 0.012f) * g(ax, 0.052f, 0.012f);                        // angulo da mandibula
    return z;
}

float corteOlho(float x, float y) {
    float dx = std::abs(x) - OLHO_X, dy = y - OLHO_Y;
    float q = (dx / (ALM_W + 0.0027f)) * (dx / (ALM_W + 0.0027f)) + (dy / (ALM_H + 0.0027f)) * (dy / (ALM_H + 0.0027f));
    return 0.016f * (1.f - smoothstep(0.7f, 1.0f, q));
}

float rosto(float x, float y) { return rostoSemCorte(x, y) - corteOlho(x, y); }

// mascara de barba (barba curta "fechada"), em coordenadas do rosto
float barba(float ax, float y, float f) {
    // so bigode, cavanhaque no queixo e uma faixa rente na borda de baixo da mandibula (bochechas sem pelo)
    if (f < -0.35f) return 0.f;
    float bigode = smoothstep(1.6525f, 1.649f, y) * smoothstep(1.638f, 1.6405f, y) * smoothstep(0.028f, 0.022f, ax);
    float queixo = smoothstep(1.6235f, 1.6205f, y) * smoothstep(0.032f, 0.024f, ax);              // abaixo do labio
    float bordaMand = smoothstep(1.613f, 1.606f, y) * smoothstep(0.066f, 0.058f, ax);             // embaixo do queixo e da mandibula
    float cantos = smoothstep(1.6405f, 1.636f, y) * smoothstep(1.62f, 1.623f, y) * smoothstep(0.03f, 0.026f, ax) * smoothstep(0.022f, 0.026f, ax);  // liga bigode ao cavanhaque
    float b = std::max(std::max(bigode, queixo), std::max(bordaMand, cantos));
    float boca = (ax / 0.027f) * (ax / 0.027f) + ((y - 1.632f) / 0.0078f) * ((y - 1.632f) / 0.0078f);
    b *= smoothstep(0.9f, 1.1f, boca);                                                             // labios sem pelos
    b *= smoothstep(-0.35f, -0.1f, f);                                                             // nao vai para a nuca
    if (y < 1.59f) b *= smoothstep(1.548f, 1.562f, y);                                            // termina no pescoco
    return std::clamp(b, 0.f, 1.f);
}

float sobrancelha(float ax, float y, float f) {
    if (f < 0.2f) return 0.f;
    float t = (ax - 0.033f) / 0.024f;
    float yc = 1.7165f + 0.0035f * (1.f - t * t) - 0.0015f * std::max(0.f, t);
    float espessura = 0.0057f * (1.f - 0.3f * std::max(0.f, t));  // sobrancelha grossa (~8 mm no meio)
    float b = smoothstep(espessura, espessura * 0.4f, std::abs(y - yc)) * smoothstep(1.15f, 0.85f, std::abs(t));
    return b * 0.95f;
}

float cabelo(float y, float f, float x, bool comprido) {
    float linha = 1.768f;
    if (comprido) {
        float tempora = smoothstep(0.024f, 0.071f, std::abs(x));
        linha = 1.749f + 0.009f * tempora + 0.002f * std::sin(x * 84.f);
    }
    float frente = comprido ? smoothstep(linha, linha + 0.006f, y + 0.01f * (1.f - f))
                            : smoothstep(1.768f, 1.776f, y + 0.01f * (1.f - f));
    float lados = (y > 1.715f ? 1.f : smoothstep(1.66f, 1.69f, y)) * smoothstep(-0.05f, -0.4f, f);
    float tempora = smoothstep(1.70f, 1.715f, y) * smoothstep(0.45f, 0.2f, f);
    return std::clamp(std::max(frente, std::max(lados, tempora)), 0.f, 1.f);
}

// pontos (u = -1 canto interno .. +1 canto externo) da borda das palpebras
Vec3f bordaPalpebra(float sx, float u, bool cima, float cz) {
    float x = sx * (OLHO_X + u * ALM_W);
    float y = cima ? OLHO_Y + ALM_H * std::pow(std::max(0.f, 1 - u * u), 0.55f) + 0.0014f * u
                   : OLHO_Y - ALM_H * 0.78f * std::pow(std::max(0.f, 1 - u * u), 0.7f) + 0.0014f * u;
    float dx = x - sx * OLHO_X, dy = y - OLHO_Y;
    float z = cz + std::sqrt(std::max(OLHO_R * OLHO_R - dx * dx - dy * dy, 0.f)) + 0.0007f;
    return {x, y, z};
}

}  // namespace

}  // namespace loft

CorPele corDaPele(Vec3f p, Vec3f n, const Visual& vis) {
    using namespace loft;
    CorPele r;
    float x = p.x, y = p.y, z = p.z, ax = std::abs(x);
    float f = (z + 0.014f) / std::max(1e-4f, std::sqrt(x * x + (z + 0.014f) * (z + 0.014f)));  // 1 = frente
    Rgb labio{vis.pele.r * 0.84f, vis.pele.g * 0.69f, vis.pele.b * 0.67f};
    Rgb c = vis.pele;
    c = lerp(c, Rgb{c.r * 1.03f, c.g * 0.86f, c.b * 0.84f}, 0.6f * g(y, 1.666f, 0.008f) * g(x, 0, 0.012f) * (f > 0));   // ponta do nariz
    c = lerp(c, Rgb{c.r * 1.02f, c.g * 0.88f, c.b * 0.86f}, 0.4f * g(y, 1.678f, 0.014f) * g(ax, 0.052f, 0.016f) * (f > 0));  // bochechas
    c = lerp(c, Rgb{c.r * 1.02f, c.g * 0.86f, c.b * 0.84f}, 0.35f * g(ax, 0.083f, 0.008f) * g(y, 1.688f, 0.03f));            // orelhas
    if (y > 1.648f && y < 1.66f && ax < 0.014f && z > 0.092f) {  // fundo das narinas: escuro (a malha e rasa)
        float dn = sdf::narina(p);
        float k = smoothstep(0.0007f, -0.0004f, dn) * smoothstep(0.1f, -0.4f, n.y);
        c = lerp(c, Rgb{c.r * 0.16f, c.g * 0.1f, c.b * 0.09f}, k);
        r.aoMax = 1.f - 0.75f * k;
    }
    if (std::abs(y - 1.632f) < 0.014f && ax < 0.032f && z + 13.f * x * x > 0.087f) {
        // vermelhao: no meio, a borda exata da escultura; nos cantos (onde o labio some sob a pele),
        // uma faixa que afina ate a comissura
        float mL = smoothstep(0.0009f, -0.0006f, sdf::labios(p));
        float mC = 0.f;  // cantos: o labio continua ate a comissura, com a cor sumindo aos poucos
        float eL = (ax / 0.0245f) * (ax / 0.0245f);
        if (eL < 1.f) {
            float sL = 1.f - eL;
            float topo = 1.6322f + 0.0066f * sL, fundo = 1.6322f - 0.0074f * sL;
            mC = smoothstep(topo + 0.0006f, topo - 0.0006f, y) * smoothstep(fundo - 0.0006f, fundo + 0.0006f, y) *
                 smoothstep(0.012f, 0.02f, ax);
        }
        if (std::max(mL, mC) > 0.2f) {
            r.labio = true;
            c = lerp(c, labio, std::max(mL, mC * lerp(0.45f, 0.1f, smoothstep(0.017f, 0.0245f, ax))));
        }
    }
    float pb = vis.barba ? barba(ax, y, f) : 0.f;
    float ps = sobrancelha(ax, y, f) * smoothstep(0.06f, 0.08f, z);
    float pc = cabelo(y, f, p.x, vis.cabeloLongo) * (1.f - smoothstep(0.075f, 0.082f, ax) * g(y, 1.69f, 0.03f));  // nao nas orelhas
    if (vis.chapeuAba && y > 1.768f) pc = 0.f;
    float pel = std::max(pb, std::max(ps, pc));
    if (pel > 0.02f) {
        r.pelos = pel;
        float sx = x >= 0 ? 1.f : -1.f;
        if (pb >= std::max(ps, pc)) {
            r.compPelo = (y > 1.642f && ax < 0.032f) ? 0.0034f : 0.003f;
            r.dirPelo = (y > 1.642f && ax < 0.032f) ? normalizar({sx * 0.7f, -1.f, 0.15f})
                      : (y < 1.592f ? normalizar({0, -0.35f, -1.f}) : normalizar({sx * 0.08f, -1.f, 0.2f}));
        } else if (ps >= pc) {
            r.compPelo = 0.0035f;  // sobrancelha desenhada na pele (shader), tracos para os lados
            r.dirPelo = ax < 0.019f ? normalizar({sx * 0.3f, 1.f, 0.15f}) : normalizar({sx * 1.f, 0.22f, 0.1f});
        } else {
            r.compPelo = 0.0035f;
            r.dirPelo = normalizar({sx * 0.15f, -0.5f, -1.f});
        }
        float baseCor = pc >= std::max(pb, ps) ? 0.8f : (ps >= pb ? 0.55f : 0.38f);
        c = lerp(c, vis.cabelo * 0.85f, baseCor * smoothstep(0.25f, 0.85f, pel));
    }
    r.cor = c;
    return r;
}

namespace loft {
void cabecaHD(Construtor& k, float s, float ms, const Visual& vis) {
    MalhaPele& m = k.m;
    // malha da escultura (volume anatomico), gerada uma vez e reaproveitada por todos (so muda a escala)
    static const auto alta = []() -> const sdf::MalhaSup& { static const auto m = sdf::malhar(0.0026f); return m; };  // ~37 mil vertices
    static const sdf::MalhaSup escNpc = sdf::malhar(0.0038f);    // ~17 mil vertices
    const sdf::MalhaSup& esc = vis.heroi && !vis.cacadorDeFronteira ? alta() : escNpc;
    auto W = [&](Vec3f p) {
        if (vis.cacadorDeFronteira) {
            float frente = smoothstep(0.025f, 0.075f, p.z);
            float maxilar = g(p.y, 1.623f, 0.020f);
            p.x *= 1.f + 0.18f * maxilar;
            p.z += frente * (0.009f * maxilar + 0.008f * g(p.y, 1.683f, 0.019f) * g(p.x, 0.f, 0.014f));
        }
        return Vec3f{p.x * s * ms, p.y * s, p.z * s * ms}; };
    auto Wn = [&](Vec3f n) { return normalizar({n.x / (s * ms), n.y / s, n.z / (s * ms)}); };
    auto ossos = [&](float y, VertPele& v) {
        if (y < 1.585f) {
            v.osso[0] = CABECA;
            v.osso[1] = PESCOCO;
            v.peso = smoothstep(1.53f, 1.585f, y);
        } else {
            v.osso[0] = v.osso[1] = CABECA;
            v.peso = 1;
        }
    };
    size_t base = m.v.size();
    for (const auto& e : (vis.semPeleCabeca ? std::vector<sdf::VertSup>{} : esc.v)) {
        VertPele v;
        v.pos = W(e.pos);
        v.normal = Wn(e.normal);
        v.ao = e.ao;
        v.material = MAT_PELE;
        v.tang = {e.pos.x, e.pos.y, 1.f};  // coordenadas do rosto (rugas e pelos por pixel)
        ossos(e.pos.y, v);
        CorPele cp = corDaPele(e.pos, e.normal, vis);
        v.cor = cp.cor;
        v.ao = std::min(v.ao, cp.aoMax);
        if (cp.labio) v.material = MAT_LABIO;
        v.pelos = cp.pelos;
        v.compPelo = cp.compPelo;
        v.dirPelo = cp.dirPelo;
        m.v.push_back(v);
    }
    for (size_t t = 0; !vis.semPeleCabeca && t < esc.idx.size(); t += 3)
        m.idx.insert(m.idx.end(), {uint16_t(base + esc.idx[t]), uint16_t(base + esc.idx[t + 1]), uint16_t(base + esc.idx[t + 2])});

    // ---- globos oculares: a iris e desenhada no shader a partir da normal ----
    Rgb iris = vis.cabelo.r > 0.4f ? Rgb{0.28f, 0.42f, 0.52f} : Rgb{0.36f, 0.23f, 0.12f};
    for (float sx : {1.f, -1.f}) {
        Vec3f c = W({sx * sdf::OLHO_X, sdf::OLHO_Y, sdf::OLHO_Z - sdf::OLHO_RECUO});
        float r = sdf::OLHO_R * s;
        std::vector<Anel> olho;
        const int NR = 10;
        for (int i = 0; i <= NR; i++) {
            float th = PI_F * (1.f - i / float(NR)) * 0.97f + 0.015f;
            float bojo = i >= NR - 2 ? 0.0006f * s : 0.f;  // cornea
            olho.push_back(Anel{c + Vec3f{0, 0, std::cos(th) * r + bojo}, std::sin(th) * r, std::sin(th) * r, CABECA, CABECA, 1});
        }
        k.material = MAT_OLHO;
        k.tubo(olho, 18, {0, 1, 0}, true, true, [&](int, float, Vec3f&, Vec3f, Vec3f, float&) -> Rgb { return iris; }, 0.f, 0.0009f * s);
    }

    // ---- cilios na borda das palpebras ----
    auto faixa = [&](const std::vector<std::vector<Vec3f>>& linhas, Rgb cor, bool cima) {
        size_t b0 = m.v.size();
        int cols = int(linhas[0].size());
        for (size_t r = 0; r < linhas.size(); r++)
            for (int q = 0; q < cols; q++) {
                VertPele v;
                v.pos = W(linhas[r][q]);
                v.cor = cor;
                v.material = MAT_CARTAO;
                v.ao = 0.8f;
                v.pelos = (q % 2) ? 1.f : 0.f;  // u: muitos fios ao longo da palpebra
                v.tv = float(r);
                v.tang = {0, cima ? 1.f : -1.f, 0};
                v.compPelo = 0.006f;
                v.normal = {0, 0, 1};
                ossos(1.7f, v);
                m.v.push_back(v);
            }
        for (size_t r = 0; r + 1 < linhas.size(); r++)
            for (int q = 0; q + 1 < cols; q++) {
                uint16_t a = uint16_t(b0 + r * cols + q), b2 = uint16_t(b0 + r * cols + q + 1);
                uint16_t c = uint16_t(b0 + (r + 1) * cols + q + 1), d = uint16_t(b0 + (r + 1) * cols + q);
                Vec3f n = cross(m.v[b2].pos - m.v[a].pos, m.v[c].pos - m.v[a].pos);
                if (n.z >= 0) m.idx.insert(m.idx.end(), {a, b2, c, a, c, d});
                else m.idx.insert(m.idx.end(), {a, c, b2, a, d, c});
            }
    };
    const int M = 16;
    for (float sx : {1.f, -1.f}) {
        std::vector<std::vector<Vec3f>> cil(2), cilB(2);
        for (int i = 0; i <= M; i++) {
            float u = -1.f + 2.f * i / M;
            float dx = u * sdf::ABERT_W;
            float yc = sdf::ABERT_HC * std::sqrt(std::max(0.f, 1 - u * u)) + 0.0013f * u;
            float yb = -sdf::ABERT_HB * std::sqrt(std::max(0.f, 1 - u * u)) + 0.0013f * u;
            auto naPalpebra = [&](float ddx, float ddy) {
                float zz = sdf::OLHO_Z + std::sqrt(std::max(0.f, sdf::PALP_R * sdf::PALP_R - ddx * ddx - ddy * ddy));
                return Vec3f{sx * (sdf::OLHO_X + ddx), sdf::OLHO_Y + ddy, zz};
            };
            Vec3f bc = naPalpebra(dx, yc), bb = naPalpebra(dx * 0.97f, yb);
            float h = std::pow(std::max(0.f, 1 - u * u), 0.35f);
            cil[0].push_back(bc + Vec3f{0, 0.0002f, 0.0006f});
            cil[1].push_back(bc + Vec3f{sx * 0.0012f * u, 0.0042f * h + 0.0004f, 0.0055f * h + 0.0008f});
            cilB[0].push_back(bb + Vec3f{0, -0.0002f, 0.0005f});
            cilB[1].push_back(bb + Vec3f{0, -0.002f * h, 0.0026f * h + 0.0006f});
        }
        faixa(cil, vis.cabelo * 0.45f, true);
        faixa(cilB, vis.cabelo * 0.45f, false);
    }

    if (vis.cabeloLongo) cabeloComprido(m, s, ms, vis.cabelo, false);
}

void cabeloComprido(MalhaPele& m, float s, float ms, Rgb cor, bool simples) {
    const Vec3f centro{0, 1.7155f, -0.014f};
    ConstrutorCabelo cabelo(m, centro, s, ms);
    constexpr float rad = 3.14159265f / 180.f;
    uint32_t rng = 9137u;
    auto rnd = [&]() { rng = rng * 1664525u + 1013904223u; return float((rng >> 8) & 0xFFFF) / 65535.f; };
    auto direcao = [](float yaw, float altura) {
        float t = yaw * rad, f = altura * rad;
        return Vec3f{std::cos(f) * std::sin(t), std::sin(f), std::cos(f) * std::cos(t)};
    };
    auto naCabeca = [&](Vec3f d, float folga) {
        Vec3f p = centro + Vec3f{d.x * 0.0765f, d.y * 0.0865f, d.z * 0.099f} * 0.85f;
        for (int it = 0; it < 80 && sdf::cabeca(p) < folga; ++it) p = p + d * 0.0015f;
        return p;
    };
    // Base rente ao cranio: cobre os espacos entre as mechas, inclusive na coroa.
    constexpr int volta = 24, aneis = 12;
    if (m.v.size() + volta * aneis + 1 < 64000) {
        uint16_t base = uint16_t(m.v.size());
        auto couro = [&](Vec3f d) {
            Vec3f p = naCabeca(d, 0.0024f);
            VertPele v;
            v.pos = {p.x * s * ms, p.y * s, p.z * s * ms};
            v.normal = normalizar({d.x / ms, d.y, d.z / ms});
            v.cor = cor * 0.74f;
            v.material = MAT_CABELO;
            v.ao = 0.8f;
            v.osso[0] = v.osso[1] = CABECA;
            m.v.push_back(v);
        };
        for (int i = 0; i < aneis; ++i) {
            for (int j = 0; j < volta; ++j) {
                float giro = j * 360.f / volta, frente = std::cos(giro * rad);
                float linha = frente > 0.f ? lerp(11.f, 24.f, frente) : lerp(11.f, -38.f, -frente);
                couro(direcao(giro, lerp(linha, 90.f, float(i) / aneis)));
            }
        }
        uint16_t topo = uint16_t(m.v.size());
        couro({0, 1, 0});
        for (int i = 0; i < aneis; ++i) {
            for (int j = 0; j < volta; ++j) {
                uint16_t a = base + i * volta + j, b = base + i * volta + (j + 1) % volta;
                if (i + 1 == aneis) {
                    m.idx.insert(m.idx.end(), {a, b, topo});
                } else {
                    uint16_t c = b + volta, dd = a + volta;
                    m.idx.insert(m.idx.end(), {a, b, c, a, c, dd});
                }
            }
        }
    }
    // Risca fora do centro. A camada interna cobre o couro cabeludo e as mechas de fora
    // atravessam o topo em diagonal, formando um topete baixo com volume.
    for (int camada = 0; camada < (simples ? 1 : 2); ++camada) {
        bool base = camada == 0;
        int quantidade = simples ? 18 : base ? 22 : 24;
        for (int k = 0; k < quantidade; ++k) {
            float u = (k + 0.5f + 0.42f * (rnd() - 0.5f)) / quantidade * 2.f - 1.f;
            float lado = u < -0.04f ? -1.f : 1.f;
            float th = u * 102.f;
            float altura = lerp(22.f, 10.f, smoothstep(0.25f, 1.f, std::abs(u)));
            float alvo = lado * (165.f - 74.f * std::abs(u) + 5.f * rnd());
            float onda = 0.003f + 0.005f * rnd();
            float fase = rnd() * 6.2831853f;
            int arco = simples ? 8 : 10, cauda = simples ? 4 : 6;
            std::vector<PontoCabelo> pts;
            for (int i = 0; i <= arco; ++i) {
                float t = float(i) / arco;
                float giro = lerp(th, alvo, t);
                float topo = std::sin(3.14159265f * t);
                float elevacao = lerp(altura, -16.f, t) + topo * lerp(86.f, 33.f, std::pow(std::abs(u), 1.4f));
                if (std::abs(giro) > 72.f && std::abs(giro) < 104.f) elevacao = std::max(elevacao, 20.f);
                float volume = lerp(0.001f, base ? 0.003f : 0.008f, smoothstep(0.f, 0.22f, t))
                             + topo * (base ? 0.009f : 0.021f);
                pts.push_back({naCabeca(direcao(giro, elevacao), volume), 0.14f * smoothstep(0.35f, 1.f, t)});
            }
            Vec3f inicio = pts.back().p, fora = normalizar({inicio.x, 0, inicio.z - centro.z});
            Vec3f transversal = cross(fora, Vec3f{0, 1, 0});
            float nuca = lerp(1.587f, 1.615f, rnd());
            for (int i = 1; i <= cauda; ++i) {
                float t = float(i) / cauda;
                Vec3f p = inicio + Vec3f{0, (nuca - inicio.y) * t, 0}
                        + fora * (t * (0.009f + t * t * 0.027f))
                        + transversal * (t * onda * std::sin(t * 8.f + fase));
                pts.push_back({p, lerp(0.14f, 1.f, t * t)});
            }
            float tom = (base ? 0.77f : 1.02f) * lerp(0.91f, 1.18f, rnd());
            cabelo.fita(pts, (simples ? 0.020f : 0.016f) * lerp(0.78f, 1.28f, rnd()),
                         cor * tom, base ? 0.73f : 0.94f);
        }
    }

    // Mechas curtas na frente, saindo da risca e caindo diagonalmente sobre a tempora.
    for (int k = 0; k < (simples ? 7 : 12); ++k) {
        float u = rnd();
        float lado = k % 2 ? 1.f : -1.f;
        float th0 = lado * lerp(2.f, 15.f, u);
        float th1 = lado * lerp(48.f, 94.f, u);
        std::vector<PontoCabelo> pts;
        for (int i = 0; i <= 10; ++i) {
            float t = i / 10.f;
            float ang = lerp(th0, th1, t);
            float altura = lerp(47.f + 12.f * u, -10.f + 16.f * u, t) + 14.f * std::sin(3.14159265f * t);
            float volume = lerp(0.009f, 0.007f, t) + 0.017f * std::sin(3.14159265f * t);
            pts.push_back({naCabeca(direcao(ang, altura), volume), 0.49f * smoothstep(0.2f, 1.f, t)});
        }
        float largura = lerp(0.006f, 0.010f, rnd());
        cabelo.fita(pts, largura, cor * lerp(0.90f, 1.18f, rnd()), 0.94f);
    }
    cabelo.finalizar();
}

void cartoesDePelo(MalhaPele& m, Rgb corPelo) {
    // Raizes espalhadas por AREA (nao por vertice): em cada triangulo com pelo sorteia pontos com
    // densidade suficiente para cobrir a pele ~3x. Cada raiz vira uma fita com ~9 fios (desenhados no shader),
    // inclinada no sentido do crescimento, quase deitada sobre a pele.
    size_t nTri = m.idx.size();
    uint32_t h = 12345u;
    auto r01 = [&]() { h = hash32(h + 0x9e3779b9u); return (h & 0xffffff) / 16777215.f; };
    for (size_t t = 0; t < nTri; t += 3) {
        // copias (m.v cresce dentro do laco e realoca: ponteiros ficariam invalidos)
        const VertPele tv0 = m.v[m.idx[t]], tv1 = m.v[m.idx[t + 1]], tv2 = m.v[m.idx[t + 2]];
        const VertPele* tv[3] = {&tv0, &tv1, &tv2};
        bool ok = true;
        float pel = 0;
        for (auto* q : tv) {
            if (q->material >= MAT_PELO || len(q->dirPelo) < 0.5f) ok = false;
            pel += q->pelos / 3.f;
        }
        if (!ok || pel < 0.05f) continue;
        if ((tv0.compPelo + tv1.compPelo + tv2.compPelo) / 3.f < 0.004f) continue;  // pelo curto (barba feita, cabelo raspado): so no shader
        float area = 0.5f * len(cross(tv[1]->pos - tv[0]->pos, tv[2]->pos - tv[0]->pos));
        float L0 = (tv[0]->compPelo + tv[1]->compPelo + tv[2]->compPelo) / 3.f;
        float w0 = std::clamp(L0 * 0.6f, 0.0042f, 0.006f);
        bool cabelo = tv[0]->dirPelo.z < -0.6f;           // cabelo (cresce para tras): cobertura menor
        float esperado = area * pel * (cabelo ? 0.55f : (L0 < 0.0045f ? 0.8f : 2.6f)) / (w0 * L0);
        int n = int(esperado);
        if (r01() < esperado - n) n++;
        for (int k = 0; k < n; k++) {
            if (m.v.size() + 4 >= 65000) return;  // limite dos indices de 16 bits
            float a = r01(), b = r01();
            if (a + b > 1) { a = 1 - a; b = 1 - b; }
            float bc[3] = {1 - a - b, a, b};
            int maior = bc[0] > bc[1] ? (bc[0] > bc[2] ? 0 : 2) : (bc[1] > bc[2] ? 1 : 2);
            VertPele raiz = *tv[maior];
            raiz.pos = tv[0]->pos * bc[0] + tv[1]->pos * bc[1] + tv[2]->pos * bc[2];
            Vec3f n3 = normalizar(tv[0]->normal * bc[0] + tv[1]->normal * bc[1] + tv[2]->normal * bc[2]);
            Vec3f dir = tv[0]->dirPelo * bc[0] + tv[1]->dirPelo * bc[1] + tv[2]->dirPelo * bc[2];
            float L = (tv[0]->compPelo * bc[0] + tv[1]->compPelo * bc[1] + tv[2]->compPelo * bc[2]) * (0.75f + 0.5f * r01());
            Vec3f d = dir - n3 * dot(dir, n3);
            if (len(d) < 1e-4f) continue;
            d = normalizar(d);
            float giro = (r01() - 0.5f) * 0.32f;  // fios quase paralelos (barba penteada)
            Vec3f lado0 = normalizar(cross(n3, d));
            Vec3f dd = normalizar(d * std::cos(giro) + lado0 * std::sin(giro));
            Vec3f lado = normalizar(cross(n3, dd));
            float w = std::clamp(L * 0.6f, 0.0042f, 0.006f);
            float incl = 0.12f + 0.2f * r01();  // quanto o fio levanta da pele (quase deitado)
            Vec3f eixo = normalizar(dd * std::cos(incl) + n3 * std::sin(incl));
            Vec3f base = raiz.pos + n3 * 0.0002f;
            Vec3f ponta = base + eixo * L;
            const Vec3f pos[4] = {base - lado * (w * 0.5f), base + lado * (w * 0.5f), ponta + lado * (w * 0.4f), ponta - lado * (w * 0.4f)};
            const float us[4] = {0, 1, 1, 0}, vs[4] = {0, 0, 1, 1};
            uint16_t id0 = uint16_t(m.v.size());
            for (int q = 0; q < 4; q++) {
                VertPele c = raiz;
                c.pos = pos[q];
                c.normal = n3;  // luz como a da pele de baixo (truque padrao de cartoes de cabelo)
                c.material = MAT_CARTAO;
                c.cor = corPelo;
                c.pelos = us[q];
                c.tv = vs[q];
                c.tang = eixo;
                c.compPelo = L;
                c.dirPelo = {};
                m.v.push_back(c);
            }
            Vec3f gn = cross(pos[1] - pos[0], pos[2] - pos[0]);
            if (dot(gn, n3) >= 0) m.idx.insert(m.idx.end(), {id0, uint16_t(id0 + 1), uint16_t(id0 + 2), id0, uint16_t(id0 + 2), uint16_t(id0 + 3)});
            else m.idx.insert(m.idx.end(), {id0, uint16_t(id0 + 2), uint16_t(id0 + 1), id0, uint16_t(id0 + 3), uint16_t(id0 + 2)});
        }
    }
}

void cascasDePelo(MalhaPele& m, Rgb corPelo) {
    // camadas: barba (5 mm) 4 cascas; sobrancelha, cabelo e cilios 2 cascas
    size_t nTri = m.idx.size();
    std::vector<int> mapa2, mapa4;  // copias por numero de camadas (a altura da casca muda)
    for (int camada = 0; camada < 4; camada++) {
        mapa2.assign(m.v.size(), -1);
        mapa4.assign(m.v.size(), -1);
        for (size_t t = 0; t < nTri; t += 3) {
            uint16_t ia = m.idx[t], ib = m.idx[t + 1], ic = m.idx[t + 2];
            const VertPele &a = m.v[ia], &b = m.v[ib], &c = m.v[ic];
            if (a.material >= MAT_PELO || a.pelos <= 0.02f || b.pelos <= 0.02f || c.pelos <= 0.02f) continue;
            float comp = std::max(a.compPelo, std::max(b.compPelo, c.compPelo));
            int nCam = comp >= 0.0045f ? 4 : 2;
            if (camada >= nCam) continue;
            float h = (camada + 1) / float(nCam);
            uint16_t novo[3];
            uint16_t orig[3] = {ia, ib, ic};
            for (int j = 0; j < 3; j++) {
                int& slot = (nCam == 4 ? mapa4 : mapa2)[orig[j]];
                if (slot < 0) {
                    VertPele v = m.v[orig[j]];
                    float cp = std::max(v.compPelo, 0.0015f);
                    v.pos = v.pos + v.normal * (cp * h);
                    v.material = MAT_PELO + h * 3.f;
                    v.cor = corPelo;
                    v.compPelo = cp;
                    slot = int(m.v.size());
                    m.v.push_back(v);
                }
                novo[j] = uint16_t(slot);
            }
            m.idx.insert(m.idx.end(), {novo[0], novo[1], novo[2]});
        }
    }
}

}  // namespace loft
