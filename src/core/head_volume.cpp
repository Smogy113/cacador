#include "core/head_volume.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include "core/head_cor.h"
#include "core/pescoco.h"
#include "core/sdf_head.h"

namespace {
// executa f(k) para k em [0, n) dividido entre as threads
template <class F>
void emParalelo(int n, F f) {
    int nt = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
    std::vector<std::thread> ts;
    for (int t = 0; t < nt; t++)
        ts.emplace_back([&, t] {
            for (int k = t; k < n; k += nt) f(k);
        });
    for (auto& th : ts) th.join();
}
int ladoAtlas(int n) { return int(std::ceil(std::sqrt(float(n)))); }
}  // namespace

uint16_t floatParaMeia(float f) {
    uint32_t x;
    std::memcpy(&x, &f, 4);
    uint32_t sinal = (x >> 16) & 0x8000u;
    int e = int((x >> 23) & 0xff) - 127 + 15;
    uint32_t m = x & 0x7fffffu;
    if (e <= 0) {  // subnormal (ou zero)
        if (e < -10) return uint16_t(sinal);
        m |= 0x800000u;
        int s = 14 - e;
        uint32_t r = m >> s;
        if ((m >> (s - 1)) & 1u) r++;
        return uint16_t(sinal | r);
    }
    if (e >= 31) return uint16_t(sinal | 0x7bffu);  // satura no maior finito
    uint32_t r = sinal | (uint32_t(e) << 10) | (m >> 13);
    if (m & 0x1000u) r++;  // arredonda
    return uint16_t(r);
}

float meiaParaFloat(uint16_t h) {
    uint32_t sinal = uint32_t(h & 0x8000u) << 16, e = (h >> 10) & 0x1f, m = h & 0x3ffu, x;
    if (e == 0) {
        float v = std::ldexp(float(m), -24);
        return sinal ? -v : v;
    }
    x = sinal | ((e - 15 + 127) << 23) | (m << 13);
    float f;
    std::memcpy(&f, &x, 4);
    return f;
}

VolumeCabeca gerarVolume(float h, float hc, const Visual& vis) {
    VolumeCabeca v;
    const float folga = 2.f * h;
    v.lo = {-0.1f - folga, 1.49f - 2.f * h - folga, -0.13f - folga};
    v.hi = {0.1f + folga, 1.815f + folga, 0.135f + folga};
    v.h = h;
    v.nx = int(std::ceil((v.hi.x - v.lo.x) / h)) + 1;
    v.ny = int(std::ceil((v.hi.y - v.lo.y) / h)) + 1;
    v.nz = int(std::ceil((v.hi.z - v.lo.z) / h)) + 1;
    v.hi = v.lo + Vec3f{(v.nx - 1) * h, (v.ny - 1) * h, (v.nz - 1) * h};
    v.dist.assign(size_t(v.nx) * v.ny * v.nz, 0.f);
    // blocos de B voxels: o centro e avaliado; perto da superficie todos os voxels do bloco,
    // longe um valor conservador (distancia do centro menos o afastamento ate o centro)
    const int B = 8;
    const float banda = 0.87f * h * B + 0.007f;
    const int bx = (v.nx + B - 1) / B, by = (v.ny + B - 1) / B, bz = (v.nz + B - 1) / B;
    emParalelo(bz, [&](int kb) {
        for (int jb = 0; jb < by; jb++)
            for (int ib = 0; ib < bx; ib++) {
                int i0 = ib * B, j0 = jb * B, k0 = kb * B;
                int i1 = std::min(i0 + B, v.nx), j1 = std::min(j0 + B, v.ny), k1 = std::min(k0 + B, v.nz);
                Vec3f c = v.lo + Vec3f{(i0 + i1 - 1) * 0.5f * h, (j0 + j1 - 1) * 0.5f * h, (k0 + k1 - 1) * 0.5f * h};
                float dc = sdf::cabeca(c);
                bool perto = std::abs(dc) < banda;
                for (int k = k0; k < k1; k++)
                    for (int j = j0; j < j1; j++)
                        for (int i = i0; i < i1; i++) {
                            Vec3f p = v.lo + Vec3f{i * h, j * h, k * h};
                            float d;
                            if (perto) d = sdf::cabeca(p);
                            else {
                                float r = len(p - c);
                                d = dc > 0 ? dc - r : std::min(dc + r, -h);
                            }
                            v.dist[(size_t(k) * v.ny + j) * v.nx + i] = d;
                        }
            }
    });
    // ---- cor: projeta cada voxel perto da pele na superficie e pinta como os vertices ----
    v.hc = hc;
    v.cx = int(std::ceil((v.hi.x - v.lo.x) / hc)) + 1;
    v.cy = int(std::ceil((v.hi.y - v.lo.y) / hc)) + 1;
    v.cz = int(std::ceil((v.hi.z - v.lo.z) / hc)) + 1;
    v.cor.assign(size_t(v.cx) * v.cy * v.cz * 4, 0);
    auto c8 = [](float f) { return uint8_t(std::clamp(f, 0.f, 1.f) * 255.f + 0.5f); };
    emParalelo(v.cz, [&](int k) {
        for (int j = 0; j < v.cy; j++)
            for (int i = 0; i < v.cx; i++) {
                Vec3f p = v.lo + Vec3f{i * hc, j * hc, k * hc};
                float d = v.amostra(p);
                Rgb cor = vis.pele;
                float a = 0.5f;
                if (std::abs(d) < 0.007f) {
                    Vec3f q = p;
                    for (int it = 0; it < 3; it++) q = q - sdf::gradiente(q) * sdf::cabeca(q);
                    CorPele cp = corDaPele(q, sdf::gradiente(q), vis);
                    cor = cp.cor;
                    if (cp.labio) a = 1.f;
                    else a = 0.5f - 0.5f * cp.pelos;
                }
                uint8_t* o = &v.cor[((size_t(k) * v.cy + j) * v.cx + i) * 4];
                o[0] = c8(cor.r), o[1] = c8(cor.g), o[2] = c8(cor.b), o[3] = c8(a);
            }
    });
    v.colunas = ladoAtlas(v.nz);
    v.linhas = (v.nz + v.colunas - 1) / v.colunas;
    v.colunasCor = ladoAtlas(v.cz);
    v.linhasCor = (v.cz + v.colunasCor - 1) / v.colunasCor;
    return v;
}

float VolumeCabeca::amostra(Vec3f p) const {
    Vec3f q{std::clamp(p.x, lo.x, hi.x), std::clamp(p.y, lo.y, hi.y), std::clamp(p.z, lo.z, hi.z)};
    float fora = len(p - q);
    float fx = (q.x - lo.x) / h, fy = (q.y - lo.y) / h, fz = (q.z - lo.z) / h;
    int i = std::min(int(fx), nx - 2), j = std::min(int(fy), ny - 2), k = std::min(int(fz), nz - 2);
    float u = fx - i, w = fy - j, t = fz - k;
    auto D = [&](int a, int b, int c) { return dist[(size_t(k + c) * ny + (j + b)) * nx + (i + a)]; };
    float c00 = lerp(D(0, 0, 0), D(1, 0, 0), u), c10 = lerp(D(0, 1, 0), D(1, 1, 0), u);
    float c01 = lerp(D(0, 0, 1), D(1, 0, 1), u), c11 = lerp(D(0, 1, 1), D(1, 1, 1), u);
    return lerp(lerp(c00, c10, w), lerp(c01, c11, w), t) + fora;
}

CorVoxel VolumeCabeca::amostraCor(Vec3f p) const {
    float fx = std::clamp((p.x - lo.x) / hc, 0.f, cx - 1.001f), fy = std::clamp((p.y - lo.y) / hc, 0.f, cy - 1.001f),
          fz = std::clamp((p.z - lo.z) / hc, 0.f, cz - 1.001f);
    int i = int(fx), j = int(fy), k = int(fz);
    float u = fx - i, w = fy - j, t = fz - k;
    float r[4] = {0, 0, 0, 0};
    for (int c = 0; c < 2; c++)
        for (int b = 0; b < 2; b++)
            for (int a = 0; a < 2; a++) {
                float peso = (a ? u : 1 - u) * (b ? w : 1 - w) * (c ? t : 1 - t);
                const uint8_t* o = &cor[((size_t(k + c) * cy + (j + b)) * cx + (i + a)) * 4];
                for (int q = 0; q < 4; q++) r[q] += peso * o[q] / 255.f;
            }
    return {r[0], r[1], r[2], r[3]};
}

std::vector<uint16_t> atlasMeiaPrecisao(const VolumeCabeca& v) {
    size_t W = size_t(v.colunas) * v.nx, H = size_t(v.linhas) * v.ny;
    std::vector<uint16_t> a(W * H, floatParaMeia(1.f));
    for (int k = 0; k < v.nz; k++) {
        size_t x0 = size_t(k % v.colunas) * v.nx, y0 = size_t(k / v.colunas) * v.ny;
        for (int j = 0; j < v.ny; j++)
            for (int i = 0; i < v.nx; i++) a[(y0 + j) * W + x0 + i] = floatParaMeia(v.dist[(size_t(k) * v.ny + j) * v.nx + i]);
    }
    return a;
}

std::vector<uint8_t> atlasCor(const VolumeCabeca& v) {
    size_t W = size_t(v.colunasCor) * v.cx, H = size_t(v.linhasCor) * v.cy;
    std::vector<uint8_t> a(W * H * 4, 0);
    for (int k = 0; k < v.cz; k++) {
        size_t x0 = size_t(k % v.colunasCor) * v.cx, y0 = size_t(k / v.colunasCor) * v.cy;
        for (int j = 0; j < v.cy; j++)
            std::memcpy(&a[((y0 + j) * W + x0) * 4], &v.cor[((size_t(k) * v.cy + j) * v.cx) * 4], size_t(v.cx) * 4);
    }
    return a;
}

MalhaPele caixaCabeca(const VolumeCabeca& v, float sxz, float sy) {
    const float pad = 0.008f;
    const float ys[] = {v.lo.y, 1.53f, 1.545f, 1.5575f, 1.57f, 1.585f, v.hi.y + pad};
    const float xs[2] = {v.lo.x - pad, v.hi.x + pad}, zs[2] = {v.lo.z - pad, v.hi.z + pad};
    const int cantos[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    MalhaPele m;
    for (float y : ys)
        for (auto& c : cantos) {
            VertPele p;
            p.pos = {xs[c[0]] * sxz, y * sy, zs[c[1]] * sxz};
            p.osso[0] = CABECA;
            p.osso[1] = PESCOCO;
            p.peso = pesoCabeca(y);
            p.material = MAT_PELE;
            m.v.push_back(p);
        }
    const int n = int(sizeof(ys) / sizeof(ys[0]));
    Vec3f centro{0, (ys[0] + ys[n - 1]) * 0.5f * sy, (zs[0] + zs[1]) * 0.5f * sxz};
    auto tri = [&](int a, int b, int c) {
        Vec3f pa = m.v[a].pos, pb = m.v[b].pos, pc = m.v[c].pos;
        if (dot(cross(pb - pa, pc - pa), (pa + pb + pc) * (1.f / 3.f) - centro) < 0) std::swap(b, c);  // sempre para fora
        m.idx.insert(m.idx.end(), {uint16_t(a), uint16_t(b), uint16_t(c)});
    };
    for (int r = 0; r + 1 < n; r++)
        for (int k = 0; k < 4; k++) {
            int a = r * 4 + k, b = r * 4 + (k + 1) % 4, c = a + 4, d = b + 4;
            tri(a, b, d);
            tri(a, d, c);
        }
    for (int top : {0, n - 1}) {
        int b0 = top * 4;
        tri(b0, b0 + 1, b0 + 2);
        tri(b0, b0 + 2, b0 + 3);
    }
    for (auto& p : m.v) p.normal = normalizar(p.pos - centro);
    return m;
}
