#include "core/terrain.h"
#include <algorithm>
#include <cmath>
#include "core/noise.h"

namespace {
constexpr float NIVEL_AGUA = 3.f;
constexpr float RIO_MEIA_LARGURA = medidas::RIO_LARGURA * .5f;
constexpr float ESTRADA_MEIA_LARGURA = medidas::ESTRADA_LARGURA * .5f;

// Curso do rio (oeste -> leste), passando pela Ponte Velha em (60,-120).
const Vec3f RIO[] = {{-770, 0, -330}, {-520, 0, -240}, {-300, 0, -185}, {-120, 0, -150}, {60, 0, -120},
                     {230, 0, -60},   {420, 0, 10},    {600, 0, 40},    {770, 0, 90}};

struct Seg {
    float ax, az, bx, bz, ha, hb;
};

// distancia de (x,z) ao segmento; t = parametro 0..1
float distSeg(const Seg& s, float x, float z, float& t) {
    float vx = s.bx - s.ax, vz = s.bz - s.az;
    float l2 = vx * vx + vz * vz;
    t = l2 > 0 ? clamp01(((x - s.ax) * vx + (z - s.az) * vz) / l2) : 0;
    float px = s.ax + vx * t - x, pz = s.az + vz * t - z;
    return std::sqrt(px * px + pz * pz);
}

std::vector<Vec3f> catmull(const std::vector<Vec3f>& p, int sub) {
    std::vector<Vec3f> r;
    for (size_t i = 0; i + 1 < p.size(); i++) {
        Vec3f p0 = p[i ? i - 1 : 0], p1 = p[i], p2 = p[i + 1], p3 = p[i + 2 < p.size() ? i + 2 : i + 1];
        for (int k = 0; k < sub; k++) {
            float t = k / float(sub), t2 = t * t, t3 = t2 * t;
            auto f = [&](float a, float b, float c, float d) {
                return 0.5f * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
            };
            r.push_back({f(p0.x, p1.x, p2.x, p3.x), 0, f(p0.z, p1.z, p2.z, p3.z)});
        }
    }
    r.push_back(p.back());
    return r;
}
}  // namespace

RegioesCapim RegioesCapim::proximas(float x0, float z0, float x1, float z1) const {
    RegioesCapim resultado;
    for (Vec3f p : pocos)
        if (p.x >= x0 - 8.f && p.x <= x1 + 8.f && p.z >= z0 - 8.f && p.z <= z1 + 8.f)
            resultado.pocos.push_back(p);
    for (const CercaCapim& c : cercas)
        if (std::max(c.inicio.x, c.fim.x) >= x0 - 3.f && std::min(c.inicio.x, c.fim.x) <= x1 + 3.f &&
            std::max(c.inicio.z, c.fim.z) >= z0 - 3.f && std::min(c.inicio.z, c.fim.z) <= z1 + 3.f)
            resultado.cercas.push_back(c);
    return resultado;
}

AmbienteCapim ambienteCapim(float x, float z, float estrada, float umidade, const RegioesCapim& regioes) {
    float poco = 0.f, cerca = 0.f;
    for (Vec3f p : regioes.pocos) {
        float dx = x - p.x, dz = z - p.z, distancia2 = dx * dx + dz * dz;
        if (distancia2 < 64.f) poco = std::max(poco, 1.f - smoothstep(2.2f, 8.f, std::sqrt(distancia2)));
    }
    for (const CercaCapim& c : regioes.cercas) {
        float dx = c.fim.x - c.inicio.x, dz = c.fim.z - c.inicio.z;
        float tamanho2 = dx * dx + dz * dz;
        float t = tamanho2 > 0.f ? clamp01(((x - c.inicio.x) * dx + (z - c.inicio.z) * dz) / tamanho2) : 0.f;
        float px = x - c.inicio.x - dx * t, pz = z - c.inicio.z - dz * t;
        float distancia2 = px * px + pz * pz;
        if (distancia2 < 9.f) cerca = std::max(cerca, 1.f - smoothstep(.4f, 3.f, std::sqrt(distancia2)));
    }
    float trilha = smoothstep(.03f, .85f, estrada);
    float pisoteio = std::max(trilha, poco);
    float protegida = cerca * (1.f - pisoteio);
    return {1.f - std::max(.93f * trilha, .72f * poco), (1.f + .8f * protegida) * (1.f - .75f * pisoteio),
            1.f + poco, clamp01(umidade)};
}

Rgb corCapimUmido(Rgb cor, float umidade) {
    return lerp(cor, {cor.r * .55f, cor.g * .72f, cor.b * .65f}, clamp01(umidade));
}

float Terreno::nivelAgua() const { return NIVEL_AGUA; }

bool Terreno::dentroDoJogavel(float x, float z) const {
    return std::abs(x) <= medidas::MEIO_AREA_JOGAVEL && std::abs(z) <= medidas::MEIO_AREA_JOGAVEL;
}

static inline int idx(int i, int j) { return j * Terreno::RES + i; }

Terreno::Terreno(const Mapa& mapa, uint32_t semente) : semente_(semente) {
    const int R = RES;
    alt_.assign(R * R, 0.f);
    estrada_.assign(R * R, 0.f);
    rio_.assign(R * R, 0.f);
    auto wx = [](int i) { return -META + float(i); };

    // 1) morros suaves + montanhas na borda
    for (int j = 0; j < R; j++)
        for (int i = 0; i < R; i++) {
            float x = wx(i), z = wx(j);
            float base = 4.f + (fbm2D(x * 0.0035f, z * 0.0035f, semente, 4) * 0.5f + 0.5f) * 24.f;
            base += fbm2D(x * 0.03f, z * 0.03f, semente + 7, 2) * 0.8f;  // irregularidade pequena
            float r = std::max(std::abs(x), std::abs(z));
            float m = smoothstep(630.f, 750.f, r);
            float picos = 110.f + fbm2D(x * 0.012f, z * 0.012f, semente + 3, 3) * 40.f;
            alt_[idx(i, j)] = base + m * m * picos;
        }

    // 2) rio: escava um leito e marca a agua
    std::vector<Vec3f> curso = catmull(std::vector<Vec3f>(std::begin(RIO), std::end(RIO)), 8);
    for (size_t k = 0; k + 1 < curso.size(); k++) {
        Seg s{curso[k].x, curso[k].z, curso[k + 1].x, curso[k + 1].z, 0, 0};
        const float alcance = 30.f;
        int i0 = std::max(0, int(std::min(s.ax, s.bx) - alcance + META)), i1 = std::min(R - 1, int(std::max(s.ax, s.bx) + alcance + META));
        int j0 = std::max(0, int(std::min(s.az, s.bz) - alcance + META)), j1 = std::min(R - 1, int(std::max(s.az, s.bz) + alcance + META));
        for (int j = j0; j <= j1; j++)
            for (int i = i0; i <= i1; i++) {
                float t, d = distSeg(s, wx(i), wx(j), t);
                float agua = 1.f - smoothstep(RIO_MEIA_LARGURA - 1.5f, RIO_MEIA_LARGURA + 1.5f, d);
                rio_[idx(i, j)] = std::max(rio_[idx(i, j)], agua);
            }
    }
    // leito: mistura em direcao a NIVEL_AGUA-2 pela distancia (usa mascara alargada)
    for (int j = 0; j < R; j++)
        for (int i = 0; i < R; i++) {
            float a = rio_[idx(i, j)];
            if (a <= 0) continue;
            float& h = alt_[idx(i, j)];
            h = lerp(h, NIVEL_AGUA - 2.f, a);
        }
    // margens: rampa suave ate 25 m do centro
    for (size_t k = 0; k + 1 < curso.size(); k++) {
        Seg s{curso[k].x, curso[k].z, curso[k + 1].x, curso[k + 1].z, 0, 0};
        const float alcance = 28.f;
        int i0 = std::max(0, int(std::min(s.ax, s.bx) - alcance + META)), i1 = std::min(R - 1, int(std::max(s.ax, s.bx) + alcance + META));
        int j0 = std::max(0, int(std::min(s.az, s.bz) - alcance + META)), j1 = std::min(R - 1, int(std::max(s.az, s.bz) + alcance + META));
        for (int j = j0; j <= j1; j++)
            for (int i = i0; i <= i1; i++) {
                float t, d = distSeg(s, wx(i), wx(j), t);
                if (d < RIO_MEIA_LARGURA || d > alcance) continue;
                float w = 1.f - smoothstep(RIO_MEIA_LARGURA, alcance, d);
                float& h = alt_[idx(i, j)];
                float alvo = NIVEL_AGUA + 0.6f + (d - RIO_MEIA_LARGURA) * 0.15f;
                if (h > alvo) h = lerp(h, alvo, w * 0.85f);
            }
    }

    // 3) aplainar onde ha lugares (menos floresta e ponte)
    for (auto& l : mapa.lugares) {
        if (l.tipo == TipoLugar::Floresta || l.tipo == TipoLugar::Ponte) continue;
        float alvo = std::max(altura(l.x, l.z), NIVEL_AGUA + 1.5f);
        float ext = l.raio * 1.3f;
        int i0 = std::max(0, int(l.x - ext + META)), i1 = std::min(R - 1, int(l.x + ext + META) + 1);
        int j0 = std::max(0, int(l.z - ext + META)), j1 = std::min(R - 1, int(l.z + ext + META) + 1);
        for (int j = j0; j <= j1; j++)
            for (int i = i0; i <= i1; i++) {
                float dx = wx(i) - l.x, dz = wx(j) - l.z;
                float d = std::sqrt(dx * dx + dz * dz);
                float w = 1.f - smoothstep(l.raio, ext, d);
                if (w <= 0) continue;
                alt_[idx(i, j)] = lerp(alt_[idx(i, j)], alvo, w);
                rio_[idx(i, j)] *= (1.f - w);
            }
    }

    // 4) estradas: curvas suaves entre os centros, alisando o terreno
    int estradaN = 0;
    for (auto& e : mapa.estradas) {
        const Lugar* a = mapa.achar(e.de);
        const Lugar* b = mapa.achar(e.para);
        if (!a || !b) continue;
        float dx = b->x - a->x, dz = b->z - a->z;
        float comp = std::sqrt(dx * dx + dz * dz);
        if (comp < 1e-3f) continue;
        float px = -dz / comp, pz = dx / comp;
        int n = std::max(4, int(comp / 20.f));
        std::vector<Vec3f> pts;
        for (int k = 0; k <= n; k++) {
            float t = k / float(n);
            float desvio = ruido2D(t * 3.f, estradaN * 13.7f, semente + 55) * 14.f * std::sin(t * 3.14159f);
            float x = a->x + dx * t + px * desvio, z = a->z + dz * t + pz * desvio;
            pts.push_back({x, std::max(altura(x, z), NIVEL_AGUA + 0.8f), z});
        }
        estradaN++;
        for (size_t k = 0; k + 1 < pts.size(); k++) {
            Seg s{pts[k].x, pts[k].z, pts[k + 1].x, pts[k + 1].z, pts[k].y, pts[k + 1].y};
            const float alcance = 7.f;
            int i0 = std::max(0, int(std::min(s.ax, s.bx) - alcance + META)), i1 = std::min(R - 1, int(std::max(s.ax, s.bx) + alcance + META) + 1);
            int j0 = std::max(0, int(std::min(s.az, s.bz) - alcance + META)), j1 = std::min(R - 1, int(std::max(s.az, s.bz) + alcance + META) + 1);
            for (int j = j0; j <= j1; j++)
                for (int i = i0; i <= i1; i++) {
                    float t, d = distSeg(s, wx(i), wx(j), t);
                    float m = 1.f - smoothstep(ESTRADA_MEIA_LARGURA, ESTRADA_MEIA_LARGURA + 1.5f, d);
                    float& em = estrada_[idx(i, j)];
                    if (rio_[idx(i, j)] > 0.05f) continue;  // a ponte cobre o rio
                    em = std::max(em, m);
                    float w = 1.f - smoothstep(ESTRADA_MEIA_LARGURA + 1.f, alcance, d);
                    float& h = alt_[idx(i, j)];
                    h = lerp(h, lerp(s.ha, s.hb, t), w * 0.8f);
                }
        }
    }
}

float Terreno::amostra(const std::vector<float>& g, float x, float z) const {
    float fx = std::clamp(x + META, 0.f, float(RES - 1) - 0.001f);
    float fz = std::clamp(z + META, 0.f, float(RES - 1) - 0.001f);
    int i = int(fx), j = int(fz);
    float u = fx - i, v = fz - j;
    float a = g[idx(i, j)], b = g[idx(i + 1, j)], c = g[idx(i, j + 1)], d = g[idx(i + 1, j + 1)];
    return lerp(lerp(a, b, u), lerp(c, d, u), v);
}

float Terreno::altura(float x, float z) const { return amostra(alt_, x, z); }
float Terreno::mascaraEstrada(float x, float z) const { return amostra(estrada_, x, z); }
float Terreno::mascaraRio(float x, float z) const { return amostra(rio_, x, z); }

float Terreno::umidadeMargem(float x, float z, float altura) const {
    if (altura >= NIVEL_AGUA + 3.2f) return 0.f;
    float agua = std::max({mascaraRio(x - 7.f, z), mascaraRio(x + 7.f, z),
                           mascaraRio(x, z - 7.f), mascaraRio(x, z + 7.f)});
    return agua * (1.f - smoothstep(NIVEL_AGUA + 1.2f, NIVEL_AGUA + 3.2f, altura));
}

Vec3f Terreno::normal(float x, float z) const {
    const float e = 1.f;
    float hx = altura(x + e, z) - altura(x - e, z);
    float hz = altura(x, z + e) - altura(x, z - e);
    return normalizar({-hx, 2 * e, -hz});
}
