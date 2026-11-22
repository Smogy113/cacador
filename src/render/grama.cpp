#include "render/grama.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "core/noise.h"
#include "core/terrain.h"
#include "raymath.h"
#include "render/terrain_mesh.h"

void Grama::configurar(float raio, float densidade) {
    if (raio == raio_ && densidade == densidade_) return;
    descarregar();  // mudou o nivel: refaz tudo
    raio_ = raio;
    densidade_ = densidade;
}

void Grama::descarregar() {
    for (auto& [k, m] : blocos_)
        if (m.vertexCount > 0) UnloadMesh(m);
    blocos_.clear();
}

Mesh Grama::montar(int bi, int bj) const {
    const Terreno& t = *t_;
    std::vector<float> pos, nor, tex;
    std::vector<unsigned char> cor;
    uint32_t rng = hash32(uint32_t(bi * 92821 + bj * 68917 + 7));
    int n = int(BLOCO * BLOCO * densidade_);
    float x0 = bi * BLOCO, z0 = bj * BLOCO;
    RegioesCapim regioes = regioes_->proximas(x0, z0, x0 + BLOCO, z0 + BLOCO);
    std::vector<InteriorMundo> casas;
    for (auto casa : *interiores_) {
        // Folhas inclinadas ou balancando podem entrar pela borda do piso.
        casa.meia.x += 0.35f;
        casa.meia.z += 0.35f;
        float alcance = std::hypot(casa.meia.x, casa.meia.z) + BLOCO * 0.5f;
        if (std::abs(casa.centro.x - (x0 + BLOCO * 0.5f)) <= alcance &&
            std::abs(casa.centro.z - (z0 + BLOCO * 0.5f)) <= alcance)
            casas.push_back(casa);
    }
    for (int k = 0; k < n; k++) {
        float x = x0 + aleatorio01(rng) * BLOCO, z = z0 + aleatorio01(rng) * BLOCO;
        float r1 = aleatorio01(rng), r2 = aleatorio01(rng), r3 = aleatorio01(rng), r4 = aleatorio01(rng);
        if (!t.dentroDoJogavel(x, z)) continue;
        float y = t.altura(x, z), estrada = t.mascaraEstrada(x, z);
        if (t.mascaraRio(x, z) > 0.05f || y < t.nivelAgua() + 0.3f) continue;
        AmbienteCapim ambiente = ambienteCapim(x, z, estrada, 0.f, regioes);
        uint32_t sorte = rng ^ 0x9e3779b9u;
        if (ambiente.densidade < 1.f && aleatorio01(sorte) > ambiente.densidade) continue;
        Vec3f nn = t.normal(x, z);
        if (nn.y < 0.86f) continue;
        ambiente.umidade = t.umidadeMargem(x, z, y);
        Rgb chao = corDoChao(t, x, z, nn.y);
        if (!(chao.g > chao.r * 1.05f && chao.g > chao.b * 1.3f) && estrada < .03f && ambiente.umidade < .05f) continue;
        if (dentroDeInterior(casas, {x, y + 0.72f, z}) >= 0) continue;
        Vec3f livre = col_->resolver({x, y, z}, 0.25f);
        if (std::hypot(livre.x - x, livre.z - z) > 0.01f) continue;
        float mancha = 0.5f + 0.5f * std::sin(x * 0.21f + std::sin(z * 0.17f) * 2.f) * std::cos(z * 0.13f);
        float h = (0.18f + 0.32f * mancha + 0.18f * r1) * ambiente.altura, w = 0.025f + 0.025f * r2;
        float a = r3 * 6.2831f, inc = (0.05f + 0.2f * r4) * std::min(1.f, ambiente.altura * 1.7f) * ambiente.inclinacao;
        Vec3f lado{std::cos(a) * w, 0, std::sin(a) * w};
        Vec3f base{x, y - 0.02f, z}, ponta = base + Vec3f{std::sin(a + 1.57f) * inc, h, std::cos(a + 1.57f) * inc};
        chao = lerp(chao, Rgb{.30f, .40f, .18f}, std::max(smoothstep(.03f, .85f, estrada), ambiente.umidade));
        Rgb cb = corCapimUmido(chao * (0.7f + 0.15f * r2), ambiente.umidade);
        Rgb cp = corCapimUmido({chao.r * (1.05f + 0.35f * mancha), chao.g * (1.1f + 0.2f * mancha), chao.b * 0.9f}, ambiente.umidade);
        Vec3f vs[3] = {base - lado, base + lado, ponta};
        Rgb cs[3] = {cb, cb, cp};
        for (int v = 0; v < 3; v++) {
            pos.insert(pos.end(), {vs[v].x, vs[v].y, vs[v].z});
            nor.insert(nor.end(), {nn.x * 0.6f, 0.8f, nn.z * 0.6f});
            tex.insert(tex.end(), {v == 2 ? 1.f : 0.6f, 4.f});
            auto c8 = [](float f) { return (unsigned char)std::clamp(f * 255.f, 0.f, 255.f); };
            cor.insert(cor.end(), {c8(cs[v].r), c8(cs[v].g), c8(cs[v].b), (unsigned char)(v == 2 ? 255 : 0)});
        }
    }
    Mesh m{};
    m.vertexCount = int(pos.size() / 3);
    m.triangleCount = m.vertexCount / 3;
    if (m.vertexCount == 0) return m;
    m.vertices = (float*)MemAlloc(pos.size() * sizeof(float));
    m.normals = (float*)MemAlloc(nor.size() * sizeof(float));
    m.texcoords = (float*)MemAlloc(tex.size() * sizeof(float));
    m.colors = (unsigned char*)MemAlloc(cor.size());
    std::copy(pos.begin(), pos.end(), m.vertices);
    std::copy(nor.begin(), nor.end(), m.normals);
    std::copy(tex.begin(), tex.end(), m.texcoords);
    std::copy(cor.begin(), cor.end(), m.colors);
    UploadMesh(&m, false);
    return m;
}

void Grama::atualizar(Vec3f foco, int maxNovos) {
    if (raio_ <= 0 || !t_) return;
    // descarta os que ficaram longe
    for (auto it = blocos_.begin(); it != blocos_.end();) {
        float cx = (it->first.first + 0.5f) * BLOCO, cz = (it->first.second + 0.5f) * BLOCO;
        if (std::hypot(cx - foco.x, cz - foco.z) > raio_ + BLOCO * 2.f) {
            if (it->second.vertexCount > 0) UnloadMesh(it->second);
            it = blocos_.erase(it);
        } else {
            ++it;
        }
    }
    // monta os que faltam, do mais perto para o mais longe
    std::vector<std::pair<float, std::pair<int, int>>> fila;
    int r = int(std::ceil(raio_ / BLOCO)) + 1, ci = int(std::floor(foco.x / BLOCO)), cj = int(std::floor(foco.z / BLOCO));
    for (int j = cj - r; j <= cj + r; j++)
        for (int i = ci - r; i <= ci + r; i++) {
            float d = std::hypot((i + 0.5f) * BLOCO - foco.x, (j + 0.5f) * BLOCO - foco.z);
            if (d <= raio_ + BLOCO && !blocos_.count({i, j})) fila.push_back({d, {i, j}});
        }
    std::sort(fila.begin(), fila.end());
    for (int k = 0; k < int(fila.size()) && k < maxNovos; k++) blocos_[fila[k].second] = montar(fila[k].second.first, fila[k].second.second);
}

void Grama::desenhar(const Camera3D& cam, Material& mat) {
    tris_ = 0;
    chamadas_ = 0;
    if (raio_ <= 0) return;
    Vec3f c{cam.position.x, cam.position.y, cam.position.z};
    Vec3f f = normalizar(Vec3f{cam.target.x - c.x, 0, cam.target.z - c.z});
    Matrix id = MatrixIdentity();
    for (auto& [k, m] : blocos_) {
        if (m.vertexCount == 0) continue;
        Vec3f centro{(k.first + 0.5f) * BLOCO, c.y, (k.second + 0.5f) * BLOCO};
        Vec3f d = centro - c;
        float dist = std::hypot(d.x, d.z);
        if (dist > raio_ + BLOCO) continue;
        if (dist > BLOCO * 1.5f && (d.x * f.x + d.z * f.z) < -BLOCO) continue;  // atras da camera
        DrawMesh(m, mat, id);
        tris_ += size_t(m.triangleCount);
        chamadas_++;
    }
}
