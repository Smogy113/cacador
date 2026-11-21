// Cabeca em alta definicao com proporcoes de um rosto real (antropometria de homem adulto):
// distancia entre pupilas 63 mm, nariz 54 mm (raiz ao subnasal) e 20 mm de projecao, boca 53 mm,
// labio superior 22 mm, queixo 45 mm abaixo da boca. Perfil lateral (testa, glabela, raiz do nariz,
// dorso, ponta, columela, filtro, labios, sulco do queixo, queixo) tirado dessas medidas.
// A malha e mais densa na frente (olhos, nariz, boca) e mais rala atras.
#include <algorithm>
#include <cmath>
#include "core/body.h"
#include "core/loft.h"

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
const Pt PERFIL[] = {{1.555f, .030f}, {1.565f, .036f}, {1.575f, .053f}, {1.585f, .082f}, {1.597f, .097f}, {1.610f, .092f},
                     {1.620f, .099f}, {1.627f, .102f}, {1.632f, .097f}, {1.637f, .103f}, {1.645f, .101f}, {1.654f, .099f},
                     {1.660f, .113f}, {1.667f, .121f}, {1.676f, .115f}, {1.688f, .107f}, {1.700f, .099f}, {1.709f, .093f},
                     {1.720f, .099f}, {1.735f, .096f}, {1.755f, .084f}, {1.775f, .060f}, {1.795f, .025f}};
const Pt BASE[] = {{1.555f, .030f}, {1.565f, .036f}, {1.575f, .053f}, {1.585f, .082f}, {1.597f, .097f}, {1.610f, .092f},
                   {1.620f, .099f}, {1.627f, .102f}, {1.632f, .097f}, {1.637f, .103f}, {1.645f, .101f}, {1.654f, .099f},
                   {1.660f, .100f}, {1.667f, .100f}, {1.676f, .099f}, {1.688f, .097f}, {1.700f, .095f}, {1.709f, .094f},
                   {1.720f, .099f}, {1.735f, .096f}, {1.755f, .084f}, {1.775f, .060f}, {1.795f, .025f}};
// quanto o rosto curva para tras dos lados (z -= a * x^2)
const Pt CURVA[] = {{1.555f, 25}, {1.585f, 22}, {1.612f, 18}, {1.632f, 14}, {1.66f, 9}, {1.69f, 7}, {1.72f, 6.5f}, {1.76f, 7}, {1.795f, 9}};
// largura do nariz
const Pt NARIZ_L[] = {{1.650f, .012f}, {1.656f, .015f}, {1.662f, .017f}, {1.668f, .0145f}, {1.678f, .011f}, {1.690f, .0085f}, {1.709f, .0065f}};
// cranio: raios lateral e frontal e centro em z
const Pt CR_X[] = {{1.53f, .07f}, {1.555f, .066f}, {1.575f, .064f}, {1.59f, .062f}, {1.61f, .064f}, {1.63f, .066f}, {1.66f, .070f}, {1.69f, .074f},
                   {1.72f, .076f}, {1.745f, .075f}, {1.77f, .066f}, {1.79f, .045f}, {1.80f, .02f}};
const Pt CR_Z[] = {{1.53f, .066f}, {1.555f, .066f}, {1.575f, .068f}, {1.59f, .075f}, {1.61f, .082f}, {1.63f, .088f}, {1.66f, .094f}, {1.69f, .098f},
                   {1.72f, .100f}, {1.745f, .098f}, {1.77f, .086f}, {1.79f, .060f}, {1.80f, .03f}};
const Pt CR_C[] = {{1.53f, .006f}, {1.555f, .005f}, {1.575f, .0f}, {1.59f, -.005f}, {1.61f, -.01f}, {1.63f, -.012f}, {1.72f, -.012f}, {1.745f, -.014f}, {1.80f, -.016f}};

constexpr float OLHO_X = 0.0315f, OLHO_Y = 1.701f, OLHO_R = 0.0122f;
constexpr float ALM_W = 0.0148f, ALM_H = 0.0058f;  // meia abertura do olho

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
    z -= 0.0055f * g(ax, OLHO_X, 0.02f) * g(y, OLHO_Y, 0.014f);                         // orbita (funda)
    z += 0.0045f * g(y, 1.719f, 0.006f) * g(ax, 0.03f, 0.022f);                         // arco da sobrancelha
    z += 0.0075f * g(y, 1.683f, 0.012f) * g(ax, 0.056f, 0.014f);                        // macas do rosto
    z -= 0.0022f * g(y, 1.688f, 0.005f) * g(ax, 0.03f, 0.012f);                         // olheira
    float dn = distSeg2D(ax, y, 0.020f, 1.660f, 0.034f, 1.632f);                        // sulco nariz-boca
    z -= 0.0028f * std::exp(-(dn / 0.0035f) * (dn / 0.0035f));
    z -= 0.0015f * g(x, 0, 0.0038f) * g(y, 1.646f, 0.005f);                             // filtro
    z += 0.0007f * g(ax, 0.0055f, 0.0018f) * g(y, 1.646f, 0.005f);
    z -= 0.0045f * g(ax, 0.0275f, 0.004f) * g(y, 1.632f, 0.004f);                       // cantos da boca
    z -= 0.0035f * std::max(0.f, ax - 0.026f) * 60.f * g(y, 1.6315f, 0.007f);          // labios terminam
    z += 0.003f * g(y, 1.598f, 0.008f) * g(ax, 0, 0.018f);                              // ponta do queixo
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
    if (f < -0.35f) return 0.f;
    float bochecha = 1.700f - (0.072f - std::clamp(ax, 0.028f, 0.072f)) * 0.95f;  // linha alta da bochecha
    float b = smoothstep(bochecha + 0.002f, bochecha - 0.004f, y);
    float boca = (ax / 0.03f) * (ax / 0.03f) + ((y - 1.6315f) / 0.0095f) * ((y - 1.6315f) / 0.0095f);
    b *= smoothstep(0.85f, 1.15f, boca);                                          // labios sem pelos
    float bigodeAlto = smoothstep(1.6525f, 1.649f, y) + smoothstep(0.03f, 0.05f, ax);  // nao sobe no nariz
    b *= std::min(1.f, bigodeAlto);
    b *= smoothstep(-0.35f, -0.1f, f);                                            // nao vai para a nuca
    if (y < 1.585f) b *= smoothstep(1.555f, 1.575f, y);                          // termina no pescoco
    return std::clamp(b, 0.f, 1.f);
}

float sobrancelha(float ax, float y, float f) {
    if (f < 0.2f) return 0.f;
    float t = (ax - 0.033f) / 0.024f;
    float yc = 1.7165f + 0.0035f * (1.f - t * t) - 0.0015f * std::max(0.f, t);
    float espessura = 0.0032f * (1.f - 0.45f * std::max(0.f, t));
    float b = smoothstep(espessura, espessura * 0.4f, std::abs(y - yc)) * smoothstep(1.15f, 0.85f, std::abs(t));
    return b * 0.95f;
}

float cabelo(float y, float f) {
    float frente = smoothstep(1.756f, 1.768f, y + 0.03f * f);
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

void cabecaHD(Construtor& k, float s, float ms, const Visual& vis) {
    MalhaPele& m = k.m;
    auto W = [&](Vec3f p) { return Vec3f{p.x * s * ms, p.y * s, p.z * s * ms}; };
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
    Rgb labio{vis.pele.r * 0.86f, vis.pele.g * 0.7f, vis.pele.b * 0.68f};

    // ---- aneis em y: denso no rosto (2,2 mm), ralo em cima e no pescoco ----
    std::vector<float> ys;
    for (float y = 1.53f; y < 1.612f; y += 0.0055f) ys.push_back(y);
    for (float y = 1.612f; y < 1.732f; y += 0.0027f) ys.push_back(y);
    for (float y = 1.732f; y < 1.797f; y += 0.0055f) ys.push_back(y);
    ys.push_back(1.797f);
    // ---- angulos: 64 lados, 3,6x mais densos na frente ----
    const int N = 56;
    std::vector<float> angs(N);
    {
        const int F = 4000;
        std::vector<float> acum(F + 1, 0.f);
        for (int i = 0; i < F; i++) {
            float a = -PI_F / 2 + 2 * PI_F * (i + 0.5f) / F;  // comeca atras (-90 graus)
            float sn = std::max(0.f, std::sin(a));
            acum[i + 1] = acum[i] + 1.f + 2.6f * sn * sn;
        }
        int j = 0;
        for (int q = 0; q < N; q++) {
            float alvo = acum[F] * q / N;
            while (j < F && acum[j + 1] < alvo) j++;
            angs[q] = -PI_F / 2 + 2 * PI_F * (j + 0.5f) / F;
        }
    }
    const float cz = rostoSemCorte(OLHO_X, OLHO_Y) - 0.0035f - OLHO_R;  // centro do globo ocular (z)

    size_t base = m.v.size();
    for (float y : ys) {
        float rx = tabela(CR_X, y), rz = tabela(CR_Z, y), zc = tabela(CR_C, y);
        float mascRosto = smoothstep(1.56f, 1.585f, y) * (1.f - smoothstep(1.765f, 1.79f, y));
        for (float a : angs) {
            float l = std::cos(a), f = std::sin(a);
            float x = rx * l, z = zc + rz * f;
            float b = smoothstep(0.1f, 0.55f, f) * mascRosto;
            if (b > 0) z = lerp(z, std::max(rosto(x, y), zc), b);
            float ax = std::abs(x);
            VertPele v;
            v.pos = W({x, y, z});
            v.material = MAT_PELE;
            ossos(y, v);
            // ---- cor, pelos e oclusao ----
            Rgb c = vis.pele;
            c = lerp(c, Rgb{c.r * 1.03f, c.g * 0.86f, c.b * 0.84f}, 0.6f * g(y, 1.667f, 0.008f) * g(x, 0, 0.012f) * (f > 0));  // ponta do nariz rosada
            c = lerp(c, Rgb{c.r * 1.02f, c.g * 0.88f, c.b * 0.86f}, 0.4f * g(y, 1.678f, 0.014f) * g(ax, 0.052f, 0.016f) * (f > 0));  // bochechas
            float corte = corteOlho(x, y) * b;
            v.ao = 1.f - std::min(0.15f, corte * 10.f) - 0.12f * g(y, 1.655f, 0.003f) * g(ax, 0.009f, 0.006f);
            float lab = (ax / 0.0265f) * (ax / 0.0265f) + ((y - 1.6315f) / 0.0072f) * ((y - 1.6315f) / 0.0072f);
            if (lab < 1.f && f > 0.3f) {
                v.material = MAT_LABIO;
                c = lerp(c, labio, smoothstep(1.f, 0.6f, lab));
                if (std::abs(y - 1.632f) < 0.0012f) { c = c * 0.45f; v.ao = 0.6f; }  // linha da boca
            }
            if (std::abs(y - 1.6555f) < 0.0016f && ax > 0.004f && ax < 0.012f && f > 0.5f) { c = c * 0.3f; v.ao = 0.5f; }  // narinas
            float pb = vis.barba ? barba(ax, y, f) : 0.f;
            float ps = sobrancelha(ax, y, f) * b;
            float pc = cabelo(y, f);
            if (vis.chapeuAba && y > 1.768f) pc = 0.f;  // escondido pelo chapeu: sem cascas
            float pel = std::max(pb, std::max(ps, pc));
            if (pel > 0.02f) {
                v.pelos = pel;
                v.compPelo = pb >= std::max(ps, pc) ? 0.005f : (ps >= pc ? 0.0025f : 0.004f);
                c = lerp(c, vis.cabelo, 0.3f * smoothstep(0.35f, 0.9f, pel));  // so onde o pelo e denso (sem manchas nas bordas)
            }
            v.cor = c;
            m.v.push_back(v);
        }
    }
    int nr = int(ys.size());
    for (int i = 0; i + 1 < nr; i++)
        for (int q = 0; q < N; q++) {
            uint16_t a = uint16_t(base + i * N + q), b2 = uint16_t(base + i * N + (q + 1) % N);
            uint16_t c = uint16_t(base + (i + 1) * N + (q + 1) % N), d = uint16_t(base + (i + 1) * N + q);
            m.idx.insert(m.idx.end(), {a, c, b2, a, d, c});
        }
    {  // topo da cabeca
        VertPele v;
        v.pos = W({0, 1.80f, tabela(CR_C, 1.80f)});
        v.cor = vis.cabelo;
        v.material = MAT_PELE;
        v.pelos = 1;
        v.compPelo = 0.004f;
        ossos(1.8f, v);
        uint16_t ci = uint16_t(m.v.size());
        m.v.push_back(v);
        for (int q = 0; q < N; q++) m.idx.insert(m.idx.end(), {ci, uint16_t(base + (nr - 1) * N + (q + 1) % N), uint16_t(base + (nr - 1) * N + q)});
    }

    // ---- globos oculares: a iris e desenhada no shader a partir da normal ----
    Rgb iris = vis.cabelo.r > 0.4f ? Rgb{0.28f, 0.42f, 0.52f} : Rgb{0.36f, 0.23f, 0.12f};
    for (float sx : {1.f, -1.f}) {
        Vec3f c = W({sx * OLHO_X, OLHO_Y, cz});
        float r = OLHO_R * s;
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

    // ---- palpebras (pele sobre o globo) e cilios ----
    auto faixa = [&](const std::vector<std::vector<Vec3f>>& linhas, float material, Rgb cor, float pelos, float comp, float aoBase) {
        size_t b0 = m.v.size();
        int cols = int(linhas[0].size());
        for (size_t r = 0; r < linhas.size(); r++)
            for (int q = 0; q < cols; q++) {
                VertPele v;
                v.pos = W(linhas[r][q]);
                v.cor = cor;
                v.material = material;
                v.ao = aoBase + (1 - aoBase) * (r / float(linhas.size() - 1));
                v.pelos = pelos;
                v.compPelo = comp;
                ossos(1.7f, v);
                m.v.push_back(v);
            }
        for (size_t r = 0; r + 1 < linhas.size(); r++)
            for (int q = 0; q + 1 < cols; q++) {
                uint16_t a = uint16_t(b0 + r * cols + q), b2 = uint16_t(b0 + r * cols + q + 1);
                uint16_t c = uint16_t(b0 + (r + 1) * cols + q + 1), d = uint16_t(b0 + (r + 1) * cols + q);
                // frente da face sempre para +z (para fora do rosto)
                Vec3f n = cross(m.v[b2].pos - m.v[a].pos, m.v[c].pos - m.v[a].pos);
                if (n.z >= 0) m.idx.insert(m.idx.end(), {a, b2, c, a, c, d});
                else m.idx.insert(m.idx.end(), {a, c, b2, a, d, c});
            }
    };
    const int M = 14;
    for (float sx : {1.f, -1.f}) {
        std::vector<std::vector<Vec3f>> cima(4), baixo(3), cil(2), cilB(2);
        for (int i = 0; i <= M; i++) {
            float u = -1.f + 2.f * i / M;
            Vec3f bc = bordaPalpebra(sx, u, true, cz), bb = bordaPalpebra(sx, u * 0.98f, false, cz);
            // palpebra de cima: borda arredondada, dobra e pele ate a orbita
            cima[0].push_back(bc + Vec3f{0, -0.0007f, 0.0004f});
            cima[1].push_back(bc + Vec3f{0, 0.0012f, 0.0019f});
            float yd = OLHO_Y + (ALM_H + 0.0035f) * std::pow(std::max(0.f, 1 - u * u), 0.5f) + 0.0016f * u;
            float xd = sx * (OLHO_X + u * (ALM_W + 0.0012f));
            cima[2].push_back({xd, yd, rostoSemCorte(xd, yd) - 0.0022f});  // vinco da palpebra
            float yo = OLHO_Y + (ALM_H + 0.0062f) * std::pow(std::max(0.f, 1 - u * u), 0.5f) + 0.0018f * u;
            float xo = sx * (OLHO_X + u * (ALM_W + 0.0032f));
            cima[3].push_back({xo, yo, rostoSemCorte(xo, yo) - corteOlho(xo, yo) + 0.0003f});
            // de baixo
            baixo[0].push_back(bb + Vec3f{0, 0.0005f, 0.0003f});
            float yb = OLHO_Y - (ALM_H * 0.78f + 0.0032f) * std::pow(std::max(0.f, 1 - u * u), 0.7f) + 0.0014f * u;
            float xb = sx * (OLHO_X + u * (ALM_W + 0.001f));
            baixo[1].push_back({xb, yb, rostoSemCorte(xb, yb) - 0.0012f});
            float yb2 = OLHO_Y - (ALM_H * 0.78f + 0.0065f) * std::pow(std::max(0.f, 1 - u * u), 0.7f) + 0.0014f * u;
            float xb2 = sx * (OLHO_X + u * (ALM_W + 0.003f));
            baixo[2].push_back({xb2, yb2, rostoSemCorte(xb2, yb2) - corteOlho(xb2, yb2) + 0.0003f});
            // cilios: saem da borda, curvam para cima e para a frente
            float h = std::pow(std::max(0.f, 1 - u * u), 0.3f);
            cil[0].push_back(bc + Vec3f{0, 0.0002f, 0.0018f});
            cil[1].push_back(bc + Vec3f{sx * 0.0012f * u, 0.0045f * h, 0.0055f * h + 0.0018f});
            cilB[0].push_back(bb + Vec3f{0, -0.0002f, 0.0012f});
            cilB[1].push_back(bb + Vec3f{0, -0.0022f * h, 0.0028f * h + 0.0012f});
        }
        faixa(cima, MAT_PELE, vis.pele * 0.97f, 0, 0, 0.7f);
        faixa(baixo, MAT_PELE, vis.pele * 0.97f, 0, 0, 0.75f);
        faixa(cil, MAT_PELO + 3.f, vis.cabelo * 0.6f, 1.f, 0.0015f, 0.8f);
        faixa(cilB, MAT_PELO + 3.f, vis.cabelo * 0.6f, 0.7f, 0.0015f, 0.8f);
    }

    // ---- orelhas: aba (helix) em volta de uma concha ----
    for (float sx : {1.f, -1.f}) {
        float yc = 1.686f, zc0 = -0.016f;
        float xr = tabela(CR_X, yc) + 0.002f;
        std::vector<Anel> helix;
        for (int i = 0; i <= 14; i++) {
            float t = -0.25f * PI_F + 1.45f * PI_F * i / 14.f;  // da frente-cima, por cima, descendo atras ate o lobulo
            float y = yc + 0.029f * std::cos(t - 0.15f) * (t > PI_F ? 0.9f : 1.f);
            float z = zc0 - 0.017f * std::sin(t);
            float rr = (i >= 12 ? 0.0055f : 0.0032f);
            helix.push_back(Anel{W({sx * (xr + 0.006f + 0.004f * std::sin(t)), y, z}), rr * s, rr * s * 0.8f, CABECA, CABECA, 1});
        }
        k.material = MAT_PELE;
        k.tubo(helix, 7, {sx, 0, 0}, true, true, [&](int, float, Vec3f&, Vec3f, Vec3f, float& ao) -> Rgb { ao = 0.9f; return vis.pele * 0.97f; }, 0.002f * s, 0.003f * s);
        std::vector<Anel> concha = {Anel{W({sx * xr, yc - 0.001f, zc0}), 0.016f * s, 0.026f * s, CABECA, CABECA, 1},
                                    Anel{W({sx * (xr + 0.0075f), yc, zc0 - 0.002f}), 0.012f * s, 0.021f * s, CABECA, CABECA, 1}};
        k.tubo(concha, 12, {0, 1, 0}, false, true, [&](int, float, Vec3f&, Vec3f, Vec3f, float& ao) -> Rgb { ao = 0.7f; return vis.pele * 0.92f; }, 0.f, 0.f);
    }
}

void cascasDePelo(MalhaPele& m, Rgb corPelo) {
    // camadas: barba (5 mm) 4 cascas; sobrancelha, cabelo e cilios 2 cascas
    size_t nTri = m.idx.size();
    std::vector<int> mapa;
    for (int camada = 0; camada < 4; camada++) {
        mapa.assign(m.v.size(), -1);
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
                int& slot = mapa[orig[j]];
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
