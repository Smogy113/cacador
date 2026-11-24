#include "core/collision.h"
#include <algorithm>
#include <cmath>

void MundoColisao::adicionar(AABB c) {
    int id = int(caixas_.size());
    caixas_.push_back(c);
    int i0 = int(std::floor(c.min.x / CEL)), i1 = int(std::floor(c.max.x / CEL));
    int j0 = int(std::floor(c.min.z / CEL)), j1 = int(std::floor(c.max.z / CEL));
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) grade_[chave(i, j)].push_back(id);
}

namespace {
// ponto do mundo -> espaco do retangulo girado (inverso de rotY)
inline void paraLocal(const CaixaGirada& c, float x, float z, float& lx, float& lz) {
    float dx = x - c.centro.x, dz = z - c.centro.z, co = std::cos(c.rot), si = std::sin(c.rot);
    lx = dx * co - dz * si;
    lz = dx * si + dz * co;
}
inline void paraMundo(const CaixaGirada& c, float lx, float lz, float& x, float& z) {
    float co = std::cos(c.rot), si = std::sin(c.rot);
    x = c.centro.x + lx * co + lz * si;
    z = c.centro.z - lx * si + lz * co;
}
AABB envoltoria(const CaixaGirada& c) {
    float co = std::abs(std::cos(c.rot)), si = std::abs(std::sin(c.rot));
    float x = c.meiaX * co + c.meiaZ * si, z = c.meiaX * si + c.meiaZ * co;
    return {{c.centro.x - x, c.y0, c.centro.z - z}, {c.centro.x + x, c.y1, c.centro.z + z}};
}
}  // namespace

void MundoColisao::adicionarGirada(const CaixaGirada& c) {
    int id = int(giradas_.size());
    giradas_.push_back(c);
    AABB b = envoltoria(c);
    int i0 = int(std::floor(b.min.x / CEL)), i1 = int(std::floor(b.max.x / CEL));
    int j0 = int(std::floor(b.min.z / CEL)), j1 = int(std::floor(b.max.z / CEL));
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) gradeGirada_[chave(i, j)].push_back(id);
}

void MundoColisao::adicionarPisoGirado(const CaixaGirada& p, float decliveZ) { pisosGirados_.push_back({p, decliveZ}); }

Vec3f MundoColisao::resolver(Vec3f p, float raio, float altura) const {
    for (int iter = 0; iter < 3; iter++) {
        int ci = int(std::floor(p.x / CEL)), cj = int(std::floor(p.z / CEL));
        bool mexeu = false;
        for (int j = cj - 1; j <= cj + 1; j++)
            for (int i = ci - 1; i <= ci + 1; i++) {
                auto it = grade_.find(chave(i, j));
                if (it == grade_.end()) continue;
                for (int id : it->second) {
                    const AABB& b = caixas_[id];
                    if (p.y >= b.max.y - 0.3f || p.y + altura < b.min.y) continue;
                    float qx = std::clamp(p.x, b.min.x, b.max.x), qz = std::clamp(p.z, b.min.z, b.max.z);
                    float dx = p.x - qx, dz = p.z - qz, d = std::sqrt(dx * dx + dz * dz);
                    if (d >= raio) continue;
                    if (d > 1e-5f) {
                        p.x = qx + dx / d * raio;
                        p.z = qz + dz / d * raio;
                    } else {  // centro dentro da caixa: sai pelo lado mais proximo
                        float e[4] = {p.x - b.min.x, b.max.x - p.x, p.z - b.min.z, b.max.z - p.z};
                        int k = int(std::min_element(e, e + 4) - e);
                        if (k == 0) p.x = b.min.x - raio;
                        else if (k == 1) p.x = b.max.x + raio;
                        else if (k == 2) p.z = b.min.z - raio;
                        else p.z = b.max.z + raio;
                    }
                    mexeu = true;
                }
            }
        auto empurra = [&](const CaixaGirada& b) {
            if (p.y >= b.y1 - 0.3f || p.y + altura < b.y0) return;
            float lx, lz;
            paraLocal(b, p.x, p.z, lx, lz);
            float qx = std::clamp(lx, -b.meiaX, b.meiaX), qz = std::clamp(lz, -b.meiaZ, b.meiaZ);
            float dx = lx - qx, dz = lz - qz, d = std::sqrt(dx * dx + dz * dz);
            if (d >= raio) return;
            if (d > 1e-5f) {
                lx = qx + dx / d * raio;
                lz = qz + dz / d * raio;
            } else {
                float e[4] = {lx + b.meiaX, b.meiaX - lx, lz + b.meiaZ, b.meiaZ - lz};
                int k = int(std::min_element(e, e + 4) - e);
                if (k == 0) lx = -b.meiaX - raio;
                else if (k == 1) lx = b.meiaX + raio;
                else if (k == 2) lz = -b.meiaZ - raio;
                else lz = b.meiaZ + raio;
            }
            paraMundo(b, lx, lz, p.x, p.z);
            mexeu = true;
        };
        for (auto& b : dinamicas_) empurra(b);
        for (int j = cj - 1; j <= cj + 1; j++)
            for (int i = ci - 1; i <= ci + 1; i++) {
                auto it = gradeGirada_.find(chave(i, j));
                if (it == gradeGirada_.end()) continue;
                for (int id : it->second) empurra(giradas_[id]);
            }
        if (!mexeu) break;
    }
    float borda = std::max(0.f, LIMITE - raio);
    p.x = std::clamp(p.x, -borda, borda);
    p.z = std::clamp(p.z, -borda, borda);
    return p;
}

namespace {
bool segmentoNaGirada(const CaixaGirada& c, Vec3f a, Vec3f b) {
    float ax, az, bx, bz;
    paraLocal(c, a.x, a.z, ax, az);
    paraLocal(c, b.x, b.z, bx, bz);
    float o[3] = {ax, a.y, az}, dd[3] = {bx - ax, b.y - a.y, bz - az};
    float mn[3] = {-c.meiaX, c.y0, -c.meiaZ}, mx[3] = {c.meiaX, c.y1, c.meiaZ};
    float tmin = 0, tmax = 1;
    for (int e = 0; e < 3; e++) {
        if (std::abs(dd[e]) < 1e-8f) {
            if (o[e] < mn[e] || o[e] > mx[e]) return false;
        } else {
            float t1 = (mn[e] - o[e]) / dd[e], t2 = (mx[e] - o[e]) / dd[e];
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    return true;
}
}  // namespace

bool MundoColisao::raioLivre(Vec3f a, Vec3f b) const {
    for (auto& c : dinamicas_)
        if (segmentoNaGirada(c, a, b)) return false;
    Vec3f d = b - a;
    int i0 = int(std::floor(std::min(a.x, b.x) / CEL)), i1 = int(std::floor(std::max(a.x, b.x) / CEL));
    int j0 = int(std::floor(std::min(a.z, b.z) / CEL)), j1 = int(std::floor(std::max(a.z, b.z) / CEL));
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) {
            static const std::vector<int> vazio;
            auto it = grade_.find(chave(i, j));
            for (int id : (it == grade_.end() ? vazio : it->second)) {
                const AABB& bx = caixas_[id];
                float tmin = 0, tmax = 1;
                const float* o = &a.x; const float* dd = &d.x;
                const float* mn = &bx.min.x; const float* mx = &bx.max.x;
                bool acerta = true;
                for (int e = 0; e < 3 && acerta; e++) {
                    if (std::abs(dd[e]) < 1e-8f) {
                        if (o[e] < mn[e] || o[e] > mx[e]) acerta = false;
                    } else {
                        float t1 = (mn[e] - o[e]) / dd[e], t2 = (mx[e] - o[e]) / dd[e];
                        if (t1 > t2) std::swap(t1, t2);
                        tmin = std::max(tmin, t1);
                        tmax = std::min(tmax, t2);
                        if (tmin > tmax) acerta = false;
                    }
                }
                if (acerta) return false;
            }
            auto ig = gradeGirada_.find(chave(i, j));
            if (ig == gradeGirada_.end()) continue;
            for (int id : ig->second) {
                const CaixaGirada& c = giradas_[id];
                float ax, az, bx, bz;
                paraLocal(c, a.x, a.z, ax, az);
                paraLocal(c, b.x, b.z, bx, bz);
                float o[3] = {ax, a.y, az}, dd[3] = {bx - ax, b.y - a.y, bz - az};
                float mn[3] = {-c.meiaX, c.y0, -c.meiaZ}, mx[3] = {c.meiaX, c.y1, c.meiaZ};
                float tmin = 0, tmax = 1;
                bool acerta = true;
                for (int e = 0; e < 3 && acerta; e++) {
                    if (std::abs(dd[e]) < 1e-8f) {
                        if (o[e] < mn[e] || o[e] > mx[e]) acerta = false;
                    } else {
                        float t1 = (mn[e] - o[e]) / dd[e], t2 = (mx[e] - o[e]) / dd[e];
                        if (t1 > t2) std::swap(t1, t2);
                        tmin = std::max(tmin, t1);
                        tmax = std::min(tmax, t2);
                        if (tmin > tmax) acerta = false;
                    }
                }
                if (acerta) return false;
            }
        }
    return true;
}

namespace {
// Teste de lajes que tambem diz por qual face o segmento entrou. o/d/mn/mx: origem, deslocamento e
// limites nos tres eixos. Devolve false se nao toca ou se ja comeca dentro (normal indefinida).
bool entradaNaCaixa(const float o[3], const float d[3], const float mn[3], const float mx[3], float& t, int& eixo, float& sinal) {
    float tmin = 0, tmax = 1;
    eixo = -1;
    for (int e = 0; e < 3; e++) {
        if (std::abs(d[e]) < 1e-8f) {
            if (o[e] < mn[e] || o[e] > mx[e]) return false;
            continue;
        }
        float t1 = (mn[e] - o[e]) / d[e], t2 = (mx[e] - o[e]) / d[e];
        float s = -1.f;  // entrando pela face minima, a normal aponta para -e
        if (t1 > t2) {
            std::swap(t1, t2);
            s = 1.f;
        }
        if (t1 > tmin) {
            tmin = t1;
            eixo = e;
            sinal = s;
        }
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    if (eixo < 0) return false;
    t = tmin;
    return true;
}
}  // namespace

bool MundoColisao::primeiroContato(Vec3f a, Vec3f b, float& tSaida, Vec3f& normalSaida) const {
    bool achou = false;
    float melhor = 2.f;
    const Vec3f d = b - a;
    auto testarGirada = [&](const CaixaGirada& c) {
        float ax, az, bx, bz;
        paraLocal(c, a.x, a.z, ax, az);
        paraLocal(c, b.x, b.z, bx, bz);
        const float o[3] = {ax, a.y, az}, dd[3] = {bx - ax, b.y - a.y, bz - az};
        const float mn[3] = {-c.meiaX, c.y0, -c.meiaZ}, mx[3] = {c.meiaX, c.y1, c.meiaZ};
        float t, sinal = 0;
        int eixo;
        if (!entradaNaCaixa(o, dd, mn, mx, t, eixo, sinal) || t >= melhor) return;
        melhor = t;
        achou = true;
        // normal local -> mundo (so gira: paraMundo sem a translacao)
        float lx = eixo == 0 ? sinal : 0.f, lz = eixo == 2 ? sinal : 0.f;
        float co = std::cos(c.rot), si = std::sin(c.rot);
        normalSaida = {lx * co + lz * si, eixo == 1 ? sinal : 0.f, -lx * si + lz * co};
    };
    for (const CaixaGirada& c : dinamicas_) testarGirada(c);
    int i0 = int(std::floor(std::min(a.x, b.x) / CEL)), i1 = int(std::floor(std::max(a.x, b.x) / CEL));
    int j0 = int(std::floor(std::min(a.z, b.z) / CEL)), j1 = int(std::floor(std::max(a.z, b.z) / CEL));
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) {
            auto it = grade_.find(chave(i, j));
            if (it != grade_.end())
                for (int id : it->second) {
                    const AABB& bx = caixas_[id];
                    float t, sinal = 0;
                    int eixo;
                    if (!entradaNaCaixa(&a.x, &d.x, &bx.min.x, &bx.max.x, t, eixo, sinal) || t >= melhor) continue;
                    melhor = t;
                    achou = true;
                    normalSaida = {eixo == 0 ? sinal : 0.f, eixo == 1 ? sinal : 0.f, eixo == 2 ? sinal : 0.f};
                }
            auto ig = gradeGirada_.find(chave(i, j));
            if (ig != gradeGirada_.end())
                for (int id : ig->second) testarGirada(giradas_[id]);
        }
    if (achou) tSaida = melhor;
    return achou;
}

void MundoColisao::adicionarPiso(AABB p) { pisos_.push_back(p); }

float MundoColisao::alturaPiso(float x, float z, float y) const {
    float melhor = -1e9f;
    for (auto& p : pisos_)
        if (x >= p.min.x && x <= p.max.x && z >= p.min.z && z <= p.max.z && p.max.y <= y + 0.7f) melhor = std::max(melhor, p.max.y);
    for (const auto& piso : pisosGirados_) {
        const auto& p = piso.caixa;
        float variacao = std::abs(piso.decliveZ) * p.meiaZ;
        if (p.y1 + variacao <= melhor || p.y1 - variacao > y + .7f) continue;
        float alcance = p.meiaX + p.meiaZ;
        if (std::abs(x - p.centro.x) > alcance || std::abs(z - p.centro.z) > alcance) continue;
        float lx, lz;
        paraLocal(p, x, z, lx, lz);
        if (std::abs(lx) > p.meiaX + 1e-4f || std::abs(lz) > p.meiaZ + 1e-4f) continue;
        float topo = p.y1 + lz * piso.decliveZ;
        if (topo <= y + 0.7f) melhor = std::max(melhor, topo);
    }
    return melhor;
}

std::vector<AABB> MundoColisao::caixasPerto(Vec3f p, float r) const {
    std::vector<AABB> res;
    std::vector<int> vistos, vistosGirados;
    int i0 = int(std::floor((p.x - r) / CEL)), i1 = int(std::floor((p.x + r) / CEL));
    int j0 = int(std::floor((p.z - r) / CEL)), j1 = int(std::floor((p.z + r) / CEL));
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) {
            auto it = grade_.find(chave(i, j));
            auto incluir = [&](int id, const AABB& b, std::vector<int>& lista) {
                if (b.max.x < p.x - r || b.min.x > p.x + r || b.max.z < p.z - r || b.min.z > p.z + r) return;
                if (std::find(lista.begin(), lista.end(), id) != lista.end()) return;
                lista.push_back(id);
                res.push_back(b);
            };
            if (it != grade_.end()) for (int id : it->second) incluir(id, caixas_[id], vistos);
            auto ig = gradeGirada_.find(chave(i, j));
            if (ig != gradeGirada_.end()) for (int id : ig->second) incluir(id, envoltoria(giradas_[id]), vistosGirados);
        }
    return res;
}
