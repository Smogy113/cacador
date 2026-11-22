#include "render/terrain_mesh.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "core/noise.h"
#include "raymath.h"

namespace {
const float PASSOS[3] = {1.5f, 4.f, 8.f};  // perto: 1,5 m (chao com mais detalhe)
const float DIST_LOD[2] = {110.f, 240.f};

struct Buf {
    std::vector<float> v, n, t;
    std::vector<unsigned char> c;
    void tri(Vec3f a, Vec3f b, Vec3f d, Rgb cor, float agua = 0) { tri3(a, b, d, cor, cor, cor, agua); }
    void tri3(Vec3f a, Vec3f b, Vec3f d, Rgb ca, Rgb cb, Rgb cd, float agua = 0) {
        Vec3f nn = normalizar(cross(b - a, d - a));
        const Rgb cs[3] = {ca, cb, cd};
        int k = 0;
        for (Vec3f p : {a, b, d}) {
            Rgb cor = cs[k++];
            v.insert(v.end(), {p.x, p.y, p.z});
            n.insert(n.end(), {nn.x, nn.y, nn.z});
            t.insert(t.end(), {1.f, agua});
            auto c8 = [](float x) { return (unsigned char)(std::clamp(x, 0.f, 1.f) * 255.f + .5f); };
            c.insert(c.end(), {c8(cor.r), c8(cor.g), c8(cor.b), 255});
        }
    }
    Mesh subir() {
        Mesh m{};
        m.vertexCount = int(v.size() / 3);
        m.triangleCount = m.vertexCount / 3;
        if (!m.vertexCount) return m;
        m.vertices = (float*)MemAlloc(v.size() * sizeof(float));
        m.normals = (float*)MemAlloc(n.size() * sizeof(float));
        m.texcoords = (float*)MemAlloc(t.size() * sizeof(float));
        m.colors = (unsigned char*)MemAlloc(c.size());
        std::copy(v.begin(), v.end(), m.vertices);
        std::copy(n.begin(), n.end(), m.normals);
        std::copy(t.begin(), t.end(), m.texcoords);
        std::copy(c.begin(), c.end(), m.colors);
        UploadMesh(&m, false);
        return m;
    }
};

Rgb mix(Rgb a, Rgb b, float t) { return lerp(a, b, clamp01(t)); }
}  // namespace

// Cor do chao num ponto (cores em sRGB, o shader converte).
Rgb corDoChao(const Terreno& t, float x, float z, float ny) {
    uint32_t s = t.semente();
    float h = t.altura(x, z);
    float r1 = fbm2D(x * 0.02f, z * 0.02f, s + 11, 2) * 0.5f + 0.5f;
    float r2 = ruido2D(x * 0.15f, z * 0.15f, s + 12) * 0.5f + 0.5f;
    Rgb grama1{0.29f, 0.40f, 0.19f}, grama2{0.40f, 0.45f, 0.22f}, seca{0.55f, 0.52f, 0.28f};
    Rgb c = mix(grama1, grama2, r1);
    c = mix(c, seca, smoothstep(0.7f, 0.9f, r1) * 0.6f);
    c = c * (0.92f + r2 * 0.12f);
    // terra da estrada
    Rgb terra{0.55f, 0.42f, 0.28f};
    float estrada = t.mascaraEstrada(x, z);
    Rgb acostamento = mix(seca, terra, 0.45f);
    c = mix(c, acostamento, smoothstep(0.04f, 0.55f, estrada));
    Rgb trilha = mix(terra, Rgb{0.69f, 0.55f, 0.38f}, r1);
    c = mix(c, trilha * (0.90f + r2 * 0.18f), smoothstep(0.42f, 0.94f, estrada));
    // margem/areia perto da agua
    Rgb lama{0.45f, 0.40f, 0.30f};
    c = mix(c, lama, smoothstep(t.nivelAgua() + 1.4f, t.nivelAgua() + 0.3f, h));
    // pedra nas encostas e montanhas
    Rgb pedra = mix(Rgb{0.42f, 0.40f, 0.38f}, Rgb{0.52f, 0.48f, 0.42f}, r2);
    float rocha = std::max(smoothstep(0.85f, 0.7f, ny), smoothstep(40.f, 60.f, h));
    c = mix(c, pedra, rocha);
    // neve no alto
    c = mix(c, Rgb{0.88f, 0.90f, 0.95f}, smoothstep(105.f, 120.f, h) * smoothstep(0.6f, 0.8f, ny));
    return c;
}

TerrenoRender::TerrenoRender(const Terreno& t) : t_(t) {
    n_ = int(std::ceil(2 * Terreno::META / TAM));
    p_.resize(n_ * n_);
    for (int j = 0; j < n_; j++)
        for (int i = 0; i < n_; i++) {
            Pedaco& p = p_[j * n_ + i];
            p.hmin = 1e9f; p.hmax = -1e9f;
            for (float z = 0; z <= TAM; z += 8)
                for (float x = 0; x <= TAM; x += 8) {
                    float h = t_.altura(-Terreno::META + i * TAM + x, -Terreno::META + j * TAM + z);
                    p.hmin = std::min(p.hmin, h);
                    p.hmax = std::max(p.hmax, h);
                }
        }
    construirHorizonte();
}

void TerrenoRender::construirHorizonte() {
    const float s = 16.f, BAIXAR = 1.5f;
    int k = int(2 * Terreno::META / s);
    Buf b;
    auto P = [&](int i, int j) {
        float x = -Terreno::META + i * s, z = -Terreno::META + j * s;
        return Vec3f{x, t_.altura(x, z) - BAIXAR, z};
    };
    for (int j = 0; j < k; j++)
        for (int i = 0; i < k; i++) {
            Vec3f a = P(i, j), bb = P(i + 1, j), c = P(i + 1, j + 1), d = P(i, j + 1);
            for (auto tr : {std::array<Vec3f, 3>{a, d, c}, std::array<Vec3f, 3>{a, c, bb}}) {
                Vec3f m = (tr[0] + tr[1] + tr[2]) * (1.f / 3.f);
                Vec3f nn = normalizar(cross(tr[1] - tr[0], tr[2] - tr[0]));
                b.tri(tr[0], tr[1], tr[2], corDoChao(t_, m.x, m.z, nn.y));
            }
        }
    horizonte_ = b.subir();
}

void TerrenoRender::desenharHorizonte(Material& mat) { DrawMesh(horizonte_, mat, MatrixIdentity()); }

TerrenoRender::~TerrenoRender() {
    UnloadMesh(horizonte_);
    for (auto& p : p_) {
        for (int l = 0; l < 3; l++)
            if (p.tem[l]) UnloadMesh(p.lod[l]);
        if (p.temAgua) UnloadMesh(p.agua);
    }
}

Vec3f TerrenoRender::centro(int ci, int cj) const {
    const Pedaco& p = p_[cj * n_ + ci];
    return {-Terreno::META + (ci + .5f) * TAM, (p.hmin + p.hmax) * .5f, -Terreno::META + (cj + .5f) * TAM};
}

int TerrenoRender::lodPara(float d) const { return d < DIST_LOD[0] ? 0 : (d < DIST_LOD[1] ? 1 : 2); }

void TerrenoRender::construir(int ci, int cj, int lod) {
    // o passo e ajustado para dividir o pedaco exatamente: sem fenda na borda entre pedacos
    int k = std::max(1, int(std::lround(TAM / PASSOS[lod])));
    float s = TAM / float(k);
    float x0 = -Terreno::META + ci * TAM, z0 = -Terreno::META + cj * TAM;
    Buf b;
    auto P = [&](int i, int j) {
        float x = x0 + i * s, z = z0 + j * s;
        return Vec3f{x, t_.altura(x, z), z};
    };
    for (int j = 0; j < k; j++)
        for (int i = 0; i < k; i++) {
            Vec3f a = P(i, j), bb = P(i + 1, j), c = P(i + 1, j + 1), d = P(i, j + 1);
            // diagonais alternadas deixam o relevo mais natural
            bool alt = ((i + j) & 1) != 0;
            Vec3f t1[3], t2[3];
            if (alt) { t1[0] = a; t1[1] = d; t1[2] = bb; t2[0] = bb; t2[1] = d; t2[2] = c; }
            else { t1[0] = a; t1[1] = d; t1[2] = c; t2[0] = a; t2[1] = c; t2[2] = bb; }
            for (auto* tr : {t1, t2}) {
                Vec3f nn = normalizar(cross(tr[1] - tr[0], tr[2] - tr[0]));
                // sombreamento leve por face mantem o visual facetado
                float f = 0.96f + 0.08f * hash32(uint32_t(int(tr[0].x * 7 + tr[1].z * 13 + tr[2].x * 3))) / 4294967296.f;
                b.tri3(tr[0], tr[1], tr[2], corDoChao(t_, tr[0].x, tr[0].z, nn.y) * f, corDoChao(t_, tr[1].x, tr[1].z, nn.y) * f,
                       corDoChao(t_, tr[2].x, tr[2].z, nn.y) * f);
            }
        }
    // "saias" nas bordas escondem frestas entre niveis de detalhe
    const float SAIA = 4.f;
    for (int e = 0; e < 4; e++)
        for (int q = 0; q < k; q++) {
            Vec3f a, c;
            if (e == 0) { a = P(q, 0); c = P(q + 1, 0); }
            else if (e == 1) { a = P(k, q); c = P(k, q + 1); }
            else if (e == 2) { a = P(q + 1, k); c = P(q, k); }
            else { a = P(0, q + 1); c = P(0, q); }
            Vec3f ab = a - Vec3f{0, SAIA, 0}, cb = c - Vec3f{0, SAIA, 0};
            Rgb cor = corDoChao(t_, a.x, a.z, t_.normal(a.x, a.z).y);
            b.tri(a, c, cb, cor);
            b.tri(a, cb, ab, cor);
            // normal para cima: a saia recebe a mesma luz do chao ao lado
            for (size_t q2 = b.n.size() - 18; q2 < b.n.size(); q2 += 3) { b.n[q2] = 0; b.n[q2 + 1] = 1; b.n[q2 + 2] = 0; }
        }
    Pedaco& p = p_[cj * n_ + ci];
    p.lod[lod] = b.subir();
    p.tem[lod] = true;
    p.uso[lod] = tempo_ + 0.001f;  // conta como usado agora: se nunca for desenhado, ainda e descartado
}

void TerrenoRender::construirAgua(int ci, int cj) {
    Pedaco& p = p_[cj * n_ + ci];
    p.aguaFeita = true;
    if (p.hmin > t_.nivelAgua() + 0.5f) return;
    float x0 = -Terreno::META + ci * TAM, z0 = -Terreno::META + cj * TAM, s = 4.f, y = t_.nivelAgua();
    int k = int(TAM / s);
    Buf b;
    Rgb agua{0.10f, 0.22f, 0.26f};
    for (int j = 0; j < k; j++)
        for (int i = 0; i < k; i++) {
            float x = x0 + i * s, z = z0 + j * s;
            bool algum = false;
            for (float dz : {0.f, s})
                for (float dx : {0.f, s})
                    if (t_.altura(x + dx, z + dz) < y + 0.2f) algum = true;
            if (!algum) continue;
            Vec3f a{x, y, z}, bb{x + s, y, z}, c{x + s, y, z + s}, d{x, y, z + s};
            b.tri(a, d, c, agua, 1.f);
            b.tri(a, c, bb, agua, 1.f);
        }
    if (b.v.empty()) return;
    p.agua = b.subir();
    p.temAgua = true;
}

void TerrenoRender::atualizar(Vec3f foco, float distMax, float tempo, int maxConstrucoes) {
    tempo_ = tempo;
    int feitas = 0;
    // constroi primeiro o que esta mais perto
    std::vector<std::pair<float, int>> fila;
    for (int j = 0; j < n_; j++)
        for (int i = 0; i < n_; i++) {
            Vec3f c = centro(i, j);
            float d = std::hypot(c.x - foco.x, c.z - foco.z) - TAM * 0.7f;
            if (d > distMax) continue;
            Pedaco& p = p_[j * n_ + i];
            int l = lodPara(std::max(d, 0.f));
            if (!p.tem[l] || !p.aguaFeita) fila.push_back({d, j * n_ + i});
        }
    std::sort(fila.begin(), fila.end());
    for (auto& [d, id] : fila) {
        if (feitas >= maxConstrucoes) break;
        int i = id % n_, j = id / n_;
        Pedaco& p = p_[id];
        int l = lodPara(std::max(d, 0.f));
        if (!p.tem[l]) { construir(i, j, l); feitas++; }
        if (!p.aguaFeita) construirAgua(i, j);
    }
    // descarta niveis que nao sao usados ha 10 s
    for (auto& p : p_)
        for (int l = 0; l < 3; l++)
            if (p.tem[l] && tempo - p.uso[l] > 10.f && p.uso[l] > 0) {
                UnloadMesh(p.lod[l]);
                p.tem[l] = false;
                p.uso[l] = 0;
            }
}

void TerrenoRender::desenhar(const Camera3D& cam, float distMax, Material& mat) {
    tris_ = 0;
    chamadas_ = 0;
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float cosMeio = std::cos((cam.fovy * 0.5f * 1.9f) * DEG2RAD);  // margem para a largura da tela
    Matrix id = MatrixIdentity();
    for (int j = 0; j < n_; j++)
        for (int i = 0; i < n_; i++) {
            Pedaco& p = p_[j * n_ + i];
            Vec3f c = centro(i, j);
            float raio = TAM * 0.72f + (p.hmax - p.hmin) * 0.5f;
            Vector3 v{c.x - cam.position.x, c.y - cam.position.y, c.z - cam.position.z};
            float d = Vector3Length(v);
            float dh = std::hypot(v.x, v.z) - TAM * 0.7f;
            if (dh > distMax) continue;
            if (d > raio) {
                float cosAng = Vector3DotProduct(Vector3Scale(v, 1.f / d), fwd);
                float folga = std::asin(std::min(1.f, raio / d));
                if (std::acos(std::clamp(cosAng, -1.f, 1.f)) - folga > std::acos(cosMeio)) continue;
            }
            int l = lodPara(std::max(dh, 0.f));
            int usar = -1;
            for (int k = l; k < 3 && usar < 0; k++) if (p.tem[k]) usar = k;
            for (int k = l - 1; k >= 0 && usar < 0; k--) if (p.tem[k]) usar = k;
            if (usar < 0) continue;
            p.uso[usar] = tempo_ + 0.001f;
            DrawMesh(p.lod[usar], mat, id);
            tris_ += p.lod[usar].triangleCount;
            chamadas_++;
            if (p.temAgua) {
                DrawMesh(p.agua, mat, id);
                tris_ += p.agua.triangleCount;
                chamadas_++;
            }
        }
}

void TerrenoRender::desenharArea(Vec3f centro, float raio, Material& mat) {
    Matrix id = MatrixIdentity();
    for (int j = 0; j < n_; j++)
        for (int i = 0; i < n_; i++) {
            Pedaco& p = p_[j * n_ + i];
            Vec3f c = this->centro(i, j);
            if (std::hypot(c.x - centro.x, c.z - centro.z) > raio + TAM * 0.72f) continue;
            for (int k = 0; k < 3; k++)
                if (p.tem[k]) {
                    DrawMesh(p.lod[k], mat, id);
                    break;
                }
        }
}
