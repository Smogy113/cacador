#include "render/grama.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <vector>
#include "core/noise.h"
#include "core/terrain.h"
#include "raymath.h"
#include "render/frustum.h"
#include "render/terrain_mesh.h"

void Grama::configurar(float raio, float densidade) {
    if (raio == raio_ && densidade == densidade_) return;
    descarregar();  // mudou o nivel: refaz tudo
    raio_ = raio;
    densidade_ = densidade;
}

void Grama::descarregar() {
    for (auto& [k, b] : blocos_)
        if (b.malha.vertexCount > 0) UnloadMesh(b.malha);
    blocos_.clear();
}

namespace {
constexpr int FOLHAS_BASE = 6, FOLHAS_EXTRAS = 8;
constexpr float RAIO_DETALHE = 12.f;  // mesmo alcance da faixa extra no FS_LIT
constexpr float GERAR_DETALHE = 25.f, DESCARTAR_DETALHE = 32.f;

struct MalhaCapim {
    std::vector<float> pos, nor, tex;
    std::vector<unsigned char> cor;
    std::vector<unsigned short> base, detalhe;

    void folha(Vec3f raiz, Vec3f rumo, Vec3f lateral, Vec3f normal, float h, float largura,
               float curva, Rgb corRaiz, Rgb corPonta, float sorte, bool extra) {
        auto& indices = extra ? detalhe : base;
        const auto primeiro = static_cast<unsigned short>(pos.size() / 3);
        const int segmentos = extra ? 3 : 1;
        const float tipo = (extra ? 4.2f : 4.f) + 0.1f * sorte;
        auto vertice = [&](Vec3f p, float t) {
            pos.insert(pos.end(), {p.x, p.y, p.z});
            nor.insert(nor.end(), {normal.x, normal.y, normal.z});
            tex.insert(tex.end(), {lerp(0.58f, 0.96f, t), tipo});
            Rgb c = lerp(corRaiz, corPonta, t);
            auto c8 = [](float v) { return (unsigned char)(clamp01(v) * 255.f + 0.5f); };
            float flex = t * t * clamp01(h / 0.35f);
            cor.insert(cor.end(), {c8(c.r), c8(c.g), c8(c.b), c8(flex)});
        };
        for (int k = 0; k <= segmentos; k++) {
            float t = float(k) / segmentos;
            Vec3f ponto = raiz + rumo * (curva * t * t) + Vec3f{0, h * t * (1.18f - 0.18f * t), 0};
            if (k == segmentos) {
                vertice(ponto, t);
                break;
            }
            Vec3f lado = lateral * (largura * (1.f - t * t));
            vertice(ponto - lado, t);
            vertice(ponto + lado, t);
        }
        for (int k = 0; k < segmentos; k++) {
            auto i = static_cast<unsigned short>(primeiro + 2 * k);
            auto j = static_cast<unsigned short>(i + 2);
            auto lado = static_cast<unsigned short>(i + 1);
            indices.insert(indices.end(), {i, lado, j});
            if (k + 1 < segmentos) indices.insert(indices.end(), {lado, static_cast<unsigned short>(j + 1), j});
        }
    }

    Mesh subir() const {
        Mesh m{};
        m.vertexCount = int(pos.size() / 3);
        m.triangleCount = int((base.size() + detalhe.size()) / 3);
        if (!m.vertexCount) return m;
        auto copiar = [](const auto& v, auto*& destino) {
            destino = static_cast<typename std::decay_t<decltype(v)>::value_type*>(MemAlloc(v.size() * sizeof(v[0])));
            std::copy(v.begin(), v.end(), destino);
        };
        copiar(pos, m.vertices);
        copiar(nor, m.normals);
        copiar(tex, m.texcoords);
        copiar(cor, m.colors);
        m.indices = (unsigned short*)MemAlloc((base.size() + detalhe.size()) * sizeof(unsigned short));
        auto fimBase = std::copy(base.begin(), base.end(), m.indices);
        std::copy(detalhe.begin(), detalhe.end(), fimBase);
        UploadMesh(&m, false);
        return m;
    }
};
}  // namespace

Grama::Bloco Grama::montar(int bi, int bj, bool detalhado) const {
    const Terreno& t = *t_;
    MalhaCapim malha;
    uint32_t rng = hash32(uint32_t(bi) * 92821u + uint32_t(bj) * 68917u + 7u);
    // Cada raiz gera varias folhas. O limite tambem garante indices de 16 bits por bloco.
    constexpr int VERTICES_TUFO = FOLHAS_BASE * 3 + FOLHAS_EXTRAS * 7;
    int n = std::min(int(BLOCO * BLOCO * densidade_ / 3.f), 65536 / VERTICES_TUFO);
    float x0 = bi * BLOCO, z0 = bj * BLOCO;
    RegioesCapim regioes = regioes_->proximas(x0, z0, x0 + BLOCO, z0 + BLOCO);
    std::vector<InteriorMundo> casas;
    for (auto casa : *interiores_) {
        casa.meia.x += 0.38f;
        casa.meia.z += 0.38f;
        float alcance = std::hypot(casa.meia.x, casa.meia.z) + BLOCO * 0.5f;
        if (std::abs(casa.centro.x - (x0 + BLOCO * 0.5f)) <= alcance &&
            std::abs(casa.centro.z - (z0 + BLOCO * 0.5f)) <= alcance)
            casas.push_back(casa);
    }
    for (int k = 0; k < n; k++) {
        float x = x0 + aleatorio01(rng) * BLOCO, z = z0 + aleatorio01(rng) * BLOCO;
        uint32_t folhas = hash32(rng);
        float escolha = aleatorio01(rng);
        if (!t.dentroDoJogavel(x, z)) continue;
        float y = t.altura(x, z), estrada = t.mascaraEstrada(x, z);
        if (t.mascaraRio(x, z) > 0.05f || y < t.nivelAgua() + 0.3f) continue;
        AmbienteCapim ambiente = ambienteCapim(x, z, estrada, 0.f, regioes);
        float mancha = smoothstep(-0.65f, 0.45f, ruido2D(x * 0.36f, z * 0.36f, t.semente() + 251u));
        if (escolha > ambiente.densidade * lerp(0.64f, 1.f, mancha)) continue;
        Vec3f nn = t.normal(x, z);
        if (nn.y < 0.86f) continue;
        ambiente.umidade = t.umidadeMargem(x, z, y);
        Rgb chao = corDoChao(t, x, z, nn.y);
        if (!(chao.g > chao.r * 1.05f && chao.g > chao.b * 1.3f) && estrada < .03f && ambiente.umidade < .05f) continue;
        if (dentroDeInterior(casas, {x, y + 0.60f, z}) >= 0) continue;
        Vec3f livre = col_->resolver({x, y, z}, 0.32f);
        if (std::hypot(livre.x - x, livre.z - z) > 0.01f) continue;

        float altura = lerp(medidas::CAPIM_ALTURA_MIN, medidas::CAPIM_ALTURA_MAX, mancha) * ambiente.altura;
        float azimute = aleatorio01(folhas) * 6.2831853f;
        Rgb verdeTufo = lerp(Rgb{0.25f, 0.39f, 0.13f}, chao, 0.32f);
        int quantas = FOLHAS_BASE + (detalhado ? FOLHAS_EXTRAS : 0);
        for (int i = 0; i < quantas; i++) {
            bool extra = i >= FOLHAS_BASE;
            float ang = azimute + 2.399963f * i + (aleatorio01(folhas) - 0.5f) * 0.85f;
            float ca = std::cos(ang), sa = std::sin(ang);
            Vec3f rumo{ca, 0, sa}, lateral{-sa, 0, ca};
            float h = altura * lerp(0.56f, 1.30f, aleatorio01(folhas));
            float largura = lerp(0.004f, 0.01f, aleatorio01(folhas)) * (extra ? 1.f : 1.42f);
            float abertura = 0.095f * aleatorio01(folhas);
            float curva = std::min(0.22f, h * lerp(0.24f, 0.86f, aleatorio01(folhas)) * ambiente.inclinacao);
            float sorte = aleatorio01(folhas);
            float seca = smoothstep(0.76f, 1.f, sorte) * (1.f - ambiente.umidade);
            Rgb verde = corCapimUmido(lerp(verdeTufo, {0.55f, 0.49f, 0.29f}, seca * 0.65f), ambiente.umidade);
            Vec3f raiz = Vec3f{x, y - 0.016f, z} + rumo * abertura;
            raiz.y = t.altura(raiz.x, raiz.z) - 0.016f;
            Vec3f normal = normalizar(nn + rumo * lerp(-0.25f, 0.35f, sorte));
            malha.folha(raiz, rumo, lateral, normal, h, largura, curva, verde * 0.83f,
                         lerp(verde * 1.23f, {0.56f, 0.53f, 0.34f}, seca * 0.5f), sorte, extra);
        }
    }
    Bloco bloco;
    bloco.malha = malha.subir();
    bloco.triangulosBase = int(malha.base.size() / 3);
    bloco.detalhado = detalhado;
    if (bloco.malha.vertexCount) {
        bloco.limites = GetMeshBoundingBox(bloco.malha);
        // Folga para o deslocamento maximo no VS_LIT.
        bloco.limites.min.x -= 0.08f;
        bloco.limites.max.x += 0.08f;
        bloco.limites.min.z -= 0.05f;
        bloco.limites.max.z += 0.05f;
    }
    return bloco;
}

void Grama::atualizar(Vec3f foco, int maxNovos) {
    if (raio_ <= 0 || !t_) return;
    // descarta os que ficaram longe
    for (auto it = blocos_.begin(); it != blocos_.end();) {
        float cx = (it->first.first + 0.5f) * BLOCO, cz = (it->first.second + 0.5f) * BLOCO;
        if (std::hypot(cx - foco.x, cz - foco.z) > raio_ + BLOCO * 2.f) {
            if (it->second.malha.vertexCount > 0) UnloadMesh(it->second.malha);
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
            if (d > raio_ + BLOCO) continue;
            auto it = blocos_.find({i, j});
            if (it == blocos_.end() || (d < GERAR_DETALHE && !it->second.detalhado) ||
                (d > DESCARTAR_DETALHE && it->second.detalhado))
                fila.push_back({d, {i, j}});
        }
    std::sort(fila.begin(), fila.end());
    for (int k = 0; k < int(fila.size()) && k < maxNovos; k++) {
        auto [dist, chave] = fila[k];
        Bloco novo = montar(chave.first, chave.second, dist < GERAR_DETALHE);
        Bloco& bloco = blocos_[chave];
        if (bloco.malha.vertexCount) UnloadMesh(bloco.malha);
        bloco = novo;
    }
}

void Grama::desenhar(const Camera3D& cam, Material& mat) {
    tris_ = 0;
    chamadas_ = 0;
    if (raio_ <= 0) return;
    Vec3f c{cam.position.x, cam.position.y, cam.position.z};
    Vec3f f = normalizar(Vec3f{cam.target.x - c.x, 0, cam.target.z - c.z});
    Matrix id = MatrixIdentity();
    const Frustum frustum = Frustum::doPasse();
    for (auto& [k, b] : blocos_) {
        if (!b.malha.vertexCount) continue;
        Vec3f centro{(k.first + 0.5f) * BLOCO, c.y, (k.second + 0.5f) * BLOCO};
        Vec3f d = centro - c;
        float dist = std::hypot(d.x, d.z);
        if (dist > raio_ + BLOCO) continue;
        if (dist > BLOCO * 1.5f && (d.x * f.x + d.z * f.z) < -BLOCO) continue;  // atras da camera
        if (!frustum.contemParte(b.limites)) continue;
        Mesh malha = b.malha;
        // A caixa ja inclui as folhas inclinadas e o vento.
        float dx = std::max({b.limites.min.x - c.x, c.x - b.limites.max.x, 0.f});
        float dz = std::max({b.limites.min.z - c.z, c.z - b.limites.max.z, 0.f});
        if (dx * dx + dz * dz >= RAIO_DETALHE * RAIO_DETALHE) malha.triangleCount = b.triangulosBase;
        DrawMesh(malha, mat, id);
        tris_ += size_t(malha.triangleCount);
        chamadas_++;
    }
}
