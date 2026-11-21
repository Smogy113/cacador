#include "core/sdf_head.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sdf {
namespace {

inline Vec3f V(float x, float y, float z) { return {x, y, z}; }

float elips(Vec3f p, Vec3f c, Vec3f r) {
    Vec3f q = p - c;
    Vec3f a{q.x / r.x, q.y / r.y, q.z / r.z};
    Vec3f b{q.x / (r.x * r.x), q.y / (r.y * r.y), q.z / (r.z * r.z)};
    float k0 = len(a), k1 = len(b);
    if (k1 < 1e-9f) return -std::min(r.x, std::min(r.y, r.z));
    return k0 * (k0 - 1.f) / k1;
}
float esfera(Vec3f p, Vec3f c, float r) { return len(p - c) - r; }

// cone arredondado (capsula com raios diferentes nas pontas)
float coneArred(Vec3f p, Vec3f a, Vec3f b, float r1, float r2) {
    Vec3f ba = b - a;
    float l2 = dot(ba, ba), rr = r1 - r2, a2 = l2 - rr * rr, il2 = 1.f / l2;
    Vec3f pa = p - a;
    float y = dot(pa, ba), z = y - l2;
    Vec3f xv = pa * l2 - ba * y;
    float x2 = dot(xv, xv), y2 = y * y * l2, z2 = z * z * l2;
    float k = (rr >= 0 ? 1.f : -1.f) * rr * rr * x2;
    if ((z >= 0 ? 1.f : -1.f) * a2 * z2 > k) return std::sqrt(x2 + z2) * il2 - r2;
    if ((y >= 0 ? 1.f : -1.f) * a2 * y2 < k) return std::sqrt(x2 + y2) * il2 - r1;
    return (std::sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

inline float smin(float a, float b, float k) {
    float h = clamp01(0.5f + 0.5f * (b - a) / k);
    return lerp(b, a, h) - k * h * (1.f - h);
}
inline float smax(float a, float b, float k) { return -smin(-a, -b, k); }

// abertura do olho (prisma em forma de amendoa na frente do globo): < 0 dentro
float abertura(Vec3f q) {
    float dx = q.x - OLHO_X;
    float dy = q.y - OLHO_Y - 0.0013f * (dx / ABERT_W);  // canto de fora mais alto
    float H = dy > 0 ? ABERT_HC : ABERT_HB;
    float e = std::sqrt((dx / ABERT_W) * (dx / ABERT_W) + (dy / H) * (dy / H));
    float d2 = (e - 1.f) * H;
    return std::max(d2, -(q.z - (OLHO_Z - 0.002f)));
}

// vinco da palpebra de cima: uma faixa fina acima da abertura, na frente do globo (> 0 fora do vinco)
float vinco(Vec3f q) {
    float dx = q.x - OLHO_X;
    float dy = q.y - OLHO_Y - 0.0012f;
    float W = ABERT_W * 1.12f, H = ABERT_HC * 1.85f;
    float e = std::sqrt((dx / W) * (dx / W) + (dy / H) * (dy / H));
    float banda = std::abs(e - 1.f) * H - 0.00045f;
    return std::max(banda, std::max(-dy, -(q.z - OLHO_Z)));
}

}  // namespace

float cabeca(Vec3f p) {
    Vec3f q{std::abs(p.x), p.y, p.z};  // simetria
    // ---- cranio e testa ----
    float d = elips(p, V(0, 1.7155f, -0.014f), V(0.0765f, 0.0865f, 0.099f));
    d = smin(d, elips(p, V(0, 1.724f, 0.03f), V(0.066f, 0.054f, 0.07f)), 0.02f);
    d = smin(d, elips(q, V(0.028f, 1.7195f, 0.083f), V(0.026f, 0.0085f, 0.0145f)), 0.011f);  // arco superciliar
    d = smin(d, elips(p, V(0, 1.712f, 0.088f), V(0.012f, 0.009f, 0.009f)), 0.008f);          // glabela
    // ---- meio do rosto ----
    d = smin(d, elips(q, V(0.051f, 1.687f, 0.05f), V(0.02f, 0.017f, 0.028f)), 0.025f);     // zigomatico (maca)
    d = smin(d, elips(p, V(0, 1.669f, 0.04f), V(0.05f, 0.042f, 0.06f)), 0.018f);           // maxila
    d = smin(d, elips(q, V(0.031f, 1.691f, 0.07f), V(0.021f, 0.011f, 0.022f)), 0.01f);    // sob os olhos
    d = smin(d, elips(q, V(0.037f, 1.667f, 0.032f), V(0.022f, 0.028f, 0.048f)), 0.022f); // lateral do rosto (gordura e musculo sob a maca)
    // ---- mandibula, queixo e boca ----
    d = smin(d, elips(p, V(0, 1.614f, 0.026f), V(0.0535f, 0.031f, 0.068f)), 0.02f);        // corpo da mandibula
    d = smin(d, elips(q, V(0.05f, 1.615f, -0.015f), V(0.012f, 0.027f, 0.021f)), 0.014f);   // ramo e angulo
    d = smin(d, elips(q, V(0.047f, 1.633f, 0.004f), V(0.009f, 0.018f, 0.02f)), 0.014f);    // masseter
    d = smin(d, elips(p, V(0, 1.598f, 0.077f), V(0.027f, 0.017f, 0.02f)), 0.012f);         // queixo
    d = smin(d, elips(q, V(0.009f, 1.5965f, 0.085f), V(0.0115f, 0.012f, 0.011f)), 0.008f); // queixo quadrado
    d = smin(d, elips(p, V(0, 1.633f, 0.069f), V(0.028f, 0.018f, 0.024f)), 0.012f);        // regiao da boca
    // labios: superior (com arco do cupido) e inferior
    float lab = elips(p, V(0, 1.6366f, 0.0915f), V(0.0238f, 0.0044f, 0.0058f));
    lab = smin(lab, elips(q, V(0.005f, 1.638f, 0.0932f), V(0.006f, 0.0028f, 0.0046f)), 0.002f);
    lab = smin(lab, elips(p, V(0, 1.6272f, 0.0905f), V(0.0218f, 0.005f, 0.0066f)), 0.003f);
    d = smin(d, lab, 0.0045f);
    d = smax(d, -elips(p, V(0, 1.6322f, 0.097f), V(0.0255f, 0.0008f, 0.012f)), 0.0012f);  // linha da boca
    d = smax(d, -elips(q, V(0.0255f, 1.6322f, 0.084f), V(0.004f, 0.003f, 0.006f)), 0.003f);  // cantos da boca
    d = smax(d, -elips(p, V(0, 1.6455f, 0.0985f), V(0.0034f, 0.0055f, 0.0022f)), 0.002f);  // filtro
    d = smax(d, -elips(p, V(0, 1.6135f, 0.0895f), V(0.016f, 0.0035f, 0.006f)), 0.006f);   // sulco do queixo
    // ---- nariz ----
    float nar = coneArred(p, V(0, 1.706f, 0.0895f), V(0, 1.671f, 0.1075f), 0.0068f, 0.0078f);  // dorso
    nar = smin(nar, esfera(p, V(0, 1.6635f, 0.1085f), 0.0086f), 0.006f);                          // ponta
    nar = smin(nar, elips(q, V(0.0112f, 1.6605f, 0.0975f), V(0.006f, 0.0058f, 0.0074f)), 0.005f);   // asas
    nar = smin(nar, coneArred(p, V(0, 1.662f, 0.106f), V(0, 1.6555f, 0.0995f), 0.0040f, 0.0034f), 0.003f);   // columela
    d = smin(d, nar, 0.007f);
    d = smax(d, -elips(q, V(0.0064f, 1.6568f, 0.0995f), V(0.003f, 0.0013f, 0.004f)), 0.0015f);    // narinas (vistas de baixo)
    d = smax(d, -elips(q, V(0.0175f, 1.661f, 0.093f), V(0.0022f, 0.0055f, 0.0035f)), 0.003f);     // sulco da asa
    // ---- olhos: orbita, palpebras ----
    d = smax(d, -elips(q, V(OLHO_X, 1.7045f, 0.095f), V(0.019f, 0.011f, 0.011f)), 0.006f);
    float palp = std::max(esfera(q, V(OLHO_X, OLHO_Y, OLHO_Z), PALP_R), -abertura(q));
    d = smin(d, palp, 0.003f);
    d = smax(d, -vinco(q), 0.0008f);
    // ---- buracos e sulcos ----
        // ---- orelhas: base, aba (helix), concha e lobulo ----
    {
        float base = elips(q, V(0.079f, 1.69f, -0.012f), V(0.0075f, 0.0305f, 0.0185f));
        float wy = (q.y - 1.692f) / 0.027f, wz = (q.z + 0.013f) / 0.017f;
        float dE = (std::sqrt(wy * wy + wz * wz) - 1.f) * 0.017f;
        float helix = std::sqrt(dE * dE + (q.x - 0.0855f) * (q.x - 0.0855f)) - 0.0034f;
        helix = std::max(helix, -(q.y - 1.672f));  // so em cima e atras
        float orelha = smin(base, helix, 0.004f);
        orelha = smin(orelha, elips(q, V(0.081f, 1.662f, -0.004f), V(0.0062f, 0.0085f, 0.0072f)), 0.004f);  // lobulo
        orelha = smax(orelha, -elips(q, V(0.0885f, 1.684f, -0.009f), V(0.0065f, 0.0135f, 0.0095f)), 0.0025f);  // concha
        orelha = smax(orelha, -elips(q, V(0.0905f, 1.703f, -0.016f), V(0.004f, 0.009f, 0.004f)), 0.002f);  // fossa
        d = smin(d, orelha, 0.004f);
    }
    // ---- pescoco e musculos ----
    d = smin(d, coneArred(p, V(0, 1.49f, -0.004f), V(0, 1.588f, -0.022f), 0.062f, 0.054f), 0.016f);  // pescoco (abaixo da linha da mandibula)
    d = smin(d, coneArred(q, V(0.056f, 1.655f, -0.028f), V(0.019f, 1.505f, 0.046f), 0.0135f, 0.011f), 0.012f);  // esternocleidomastoideo
    d = smin(d, elips(p, V(0, 1.567f, 0.047f), V(0.009f, 0.012f, 0.009f)), 0.01f);                            // pomo de Adao
    // aberto embaixo (o pescoco entra no tronco)
    d = std::max(d, 1.49f - p.y);
    return d;
}

Vec3f gradiente(Vec3f p) {
    const float e = 0.00025f;
    return normalizar({cabeca(p + V(e, 0, 0)) - cabeca(p - V(e, 0, 0)), cabeca(p + V(0, e, 0)) - cabeca(p - V(0, e, 0)),
                       cabeca(p + V(0, 0, e)) - cabeca(p - V(0, 0, e))});
}

// ---------------------------------------------------------------------------------------------
// Malha por "surface nets": um vertice por celula cortada pela superficie, quadrados nas arestas.
// Avalia primeiro numa grade grossa e so refina perto da superficie (banda estreita).
MalhaSup malhar(float h, float yMin) {
    const Vec3f lo{-0.1f, yMin - h, -0.13f}, hi{0.1f, 1.815f, 0.135f};
    const int nx = int((hi.x - lo.x) / h) + 2, ny = int((hi.y - lo.y) / h) + 2, nz = int((hi.z - lo.z) / h) + 2;
    auto id = [&](int i, int j, int k) { return (size_t(k) * ny + j) * nx + i; };
    const float NAN_ = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> f(size_t(nx) * ny * nz, NAN_);
    auto P = [&](int i, int j, int k) { return Vec3f{lo.x + i * h, lo.y + j * h, lo.z + k * h}; };
    // grade grossa (blocos de B celulas)
    const int B = 4;
    const int bx = (nx + B - 1) / B, by = (ny + B - 1) / B, bz = (nz + B - 1) / B;
    std::vector<float> bloco(size_t(bx) * by * bz, 1.f);  // valor no centro de cada bloco (sinal para os pontos pulados)
    for (int bk = 0; bk < nz; bk += B)
        for (int bj = 0; bj < ny; bj += B)
            for (int bi = 0; bi < nx; bi += B) {
                Vec3f c = P(std::min(bi + B / 2, nx - 1), std::min(bj + B / 2, ny - 1), std::min(bk + B / 2, nz - 1));
                float dc = cabeca(c);
                bloco[(size_t(bk / B) * by + bj / B) * bx + bi / B] = dc;
                if (std::abs(dc) > h * B * 1.0f) continue;  // longe da superficie
                for (int k = bk; k <= std::min(bk + B, nz - 1); k++)
                    for (int j = bj; j <= std::min(bj + B, ny - 1); j++)
                        for (int i = bi; i <= std::min(bi + B, nx - 1); i++) {
                            float& v = f[id(i, j, k)];
                            if (std::isnan(v)) v = cabeca(P(i, j, k));
                        }
            }
    auto val = [&](int i, int j, int k) {
        float v = f[id(i, j, k)];
        if (!std::isnan(v)) return v;
        float b = bloco[(size_t(std::min(k / B, bz - 1)) * by + std::min(j / B, by - 1)) * bx + std::min(i / B, bx - 1)];
        return b < 0 ? -1.f : 1.f;  // nao avaliado: longe da superficie, com o sinal do bloco
    };
    // vertices: um por celula com troca de sinal
    MalhaSup m;
    std::vector<int32_t> vid(size_t(nx) * ny * nz, -1);
    const int cantos[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
    const int arestas[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (int k = 0; k + 1 < nz; k++)
        for (int j = 0; j + 1 < ny; j++)
            for (int i = 0; i + 1 < nx; i++) {
                float c[8];
                int dentro = 0;
                for (int q = 0; q < 8; q++) {
                    c[q] = val(i + cantos[q][0], j + cantos[q][1], k + cantos[q][2]);
                    dentro += c[q] < 0;
                }
                if (dentro == 0 || dentro == 8) continue;
                Vec3f soma{};
                int n = 0;
                for (auto& a : arestas) {
                    float va = c[a[0]], vb = c[a[1]];
                    if ((va < 0) == (vb < 0)) continue;
                    float t = va / (va - vb);
                    Vec3f pa{float(cantos[a[0]][0]), float(cantos[a[0]][1]), float(cantos[a[0]][2])};
                    Vec3f pb{float(cantos[a[1]][0]), float(cantos[a[1]][1]), float(cantos[a[1]][2])};
                    soma = soma + pa + (pb - pa) * t;
                    n++;
                }
                Vec3f p = P(i, j, k) + soma * (h / n);
                // projeta na superficie (2 passos de Newton)
                for (int it = 0; it < 2; it++) {
                    float dd = cabeca(p);
                    p = p - gradiente(p) * dd;
                }
                vid[id(i, j, k)] = int32_t(m.v.size());
                m.v.push_back({p, gradiente(p), 1.f});
            }
    // quadrados: cada aresta da grade com troca de sinal liga as 4 celulas em volta
    auto quad = [&](int a, int b, int c, int d, bool inverte) {
        if (a < 0 || b < 0 || c < 0 || d < 0) return;
        // a tampa do fundo do pescoco fica dentro do tronco: nao gera
        if (m.v[a].pos.y < yMin + 0.004f && m.v[b].pos.y < yMin + 0.004f && m.v[c].pos.y < yMin + 0.004f && m.v[d].pos.y < yMin + 0.004f) return;
        if (inverte) std::swap(b, d);
        m.idx.insert(m.idx.end(), {uint32_t(a), uint32_t(b), uint32_t(c), uint32_t(a), uint32_t(c), uint32_t(d)});
    };
    for (int k = 1; k + 1 < nz; k++)
        for (int j = 1; j + 1 < ny; j++)
            for (int i = 1; i + 1 < nx; i++) {
                float v0 = val(i, j, k);
                bool s0 = v0 < 0;
                if ((val(i + 1, j, k) < 0) != s0)  // aresta em x: celulas (i, j-1..j, k-1..k)
                    quad(vid[id(i, j - 1, k - 1)], vid[id(i, j, k - 1)], vid[id(i, j, k)], vid[id(i, j - 1, k)], !s0);
                if ((val(i, j + 1, k) < 0) != s0)  // aresta em y
                    quad(vid[id(i - 1, j, k - 1)], vid[id(i - 1, j, k)], vid[id(i, j, k)], vid[id(i, j, k - 1)], !s0);
                if ((val(i, j, k + 1) < 0) != s0)  // aresta em z
                    quad(vid[id(i - 1, j - 1, k)], vid[id(i, j - 1, k)], vid[id(i, j, k)], vid[id(i - 1, j, k)], !s0);
            }
    // oclusao de ambiente pela propria forma (cavidades escurecem)
    for (auto& v : m.v) {
        float occ = 0, peso = 1;
        for (int s = 1; s <= 5; s++) {
            float dist = 0.0025f * s;
            float dd = cabeca(v.pos + v.normal * dist);
            occ += (dist - dd) * peso;
            peso *= 0.75f;
        }
        v.ao = std::clamp(1.f - occ * 55.f, 0.3f, 1.f);
    }
    return m;
}

}  // namespace sdf
