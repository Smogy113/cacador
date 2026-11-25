#include "render/skinned.h"
#include <algorithm>
#include "raymath.h"
#include "rlgl.h"

Matrix paraRaylib(const Mat4& m) {
    Matrix r;
    r.m0 = m.a[0][0]; r.m4 = m.a[0][1]; r.m8 = m.a[0][2]; r.m12 = m.a[0][3];
    r.m1 = m.a[1][0]; r.m5 = m.a[1][1]; r.m9 = m.a[1][2]; r.m13 = m.a[1][3];
    r.m2 = m.a[2][0]; r.m6 = m.a[2][1]; r.m10 = m.a[2][2]; r.m14 = m.a[2][3];
    r.m3 = m.a[3][0]; r.m7 = m.a[3][1]; r.m11 = m.a[3][2]; r.m15 = m.a[3][3];
    return r;
}

static unsigned char c8(float v) { return (unsigned char)(std::clamp(v, 0.f, 1.f) * 255.f + 0.5f); }

namespace {

struct UniformesCabelo {
    unsigned int shader = 0;
    int ativo, curvas, pontasYZ, centro, raios, volumes;
};

const UniformesCabelo& uniformesCabelo(Shader shader) {
    // Um programa no passe de cor e outro na profundidade.
    static std::array<UniformesCabelo, 2> cache{};
    static size_t proximo = 0;
    for (const auto& loc : cache) if (loc.shader == shader.id) return loc;
    auto& loc = cache[proximo++ % cache.size()];
    loc = {shader.id, GetShaderLocation(shader, "cabeloAtivo"), GetShaderLocation(shader, "cabeloCurvas[0]"),
           GetShaderLocation(shader, "cabeloPontasYZ[0]"), GetShaderLocation(shader, "cabeloCentro"),
           GetShaderLocation(shader, "cabeloRaios"), GetShaderLocation(shader, "cabeloVolumes[0]")};
    return loc;
}

// Apoio de cada vértice de fita (VertPele::apoio*) nos atributos de skinning da raylib, que este
// projeto não usa de outro jeito: boneIds.x = osso da pele; boneWeights = normal (xyz) e folga (w).
// Sem nenhum apoio na malha, nada é enviado e o shader lê zero (sem apoio).
void carregarApoios(const MalhaPele& p, Mesh& m) {
    const bool temApoio = std::any_of(p.v.begin(), p.v.end(), [](const VertPele& v) { return v.apoioFolga > 0.f; });
    if (!temApoio) return;
    const size_t n = p.v.size();
    m.boneIds = (unsigned char*)MemAlloc(4 * n);
    m.boneWeights = (float*)MemAlloc(sizeof(float) * 4 * n);
    for (size_t i = 0; i < n; i++) {
        const VertPele& v = p.v[i];
        m.boneIds[i * 4] = v.apoioOsso;
        m.boneIds[i * 4 + 1] = m.boneIds[i * 4 + 2] = m.boneIds[i * 4 + 3] = 0;
        m.boneWeights[i * 4] = v.apoioNormal.x;
        m.boneWeights[i * 4 + 1] = v.apoioNormal.y;
        m.boneWeights[i * 4 + 2] = v.apoioNormal.z;
        m.boneWeights[i * 4 + 3] = v.apoioFolga;
    }
}

void enviarCabelo(Shader shader, const UniformesCabelo& loc, const FisicaCabelo& cabelo) {
    float ativo = 1.f;
    const auto& deformacao = cabelo.deformacao();
    const auto& perfil = cabelo.perfil();
    // Seis componentes por guia em 1,5 vec4. A paleta respeita o orçamento
    // de uniforms de cada backend, incluindo os ossos.
    float curvas[MAX_GUIAS_CABELO][4], pontasYZ[MAX_GUIAS_CABELO / 2][4];
    size_t pares = (perfil.quantidade + 1) / 2;
    for (size_t i = 0; i < pares; ++i) {
        size_t a = 2 * i, b = std::min(a + 1, perfil.quantidade - 1);
        Vec3f pa = deformacao.ponta[a], pb = deformacao.ponta[b];
        pontasYZ[i][0] = pa.y; pontasYZ[i][1] = pa.z;
        pontasYZ[i][2] = pb.y; pontasYZ[i][3] = pb.z;
    }
    for (size_t i = 0; i < perfil.quantidade; ++i) {
        Vec3f meio = deformacao.meio[i];
        curvas[i][0] = meio.x; curvas[i][1] = meio.y; curvas[i][2] = meio.z;
        curvas[i][3] = deformacao.ponta[i].x;
    }
    Vec3f centro = perfil.colisores.centroCabeca, raios = perfil.colisores.raiosCabeca;
    float centroShader[] = {centro.x, centro.y, centro.z}, raiosShader[] = {raios.x, raios.y, raios.z};
    SetShaderValue(shader, loc.ativo, &ativo, SHADER_UNIFORM_FLOAT);
    SetShaderValueV(shader, loc.curvas, curvas, SHADER_UNIFORM_VEC4, int(perfil.quantidade));
    SetShaderValueV(shader, loc.pontasYZ, pontasYZ, SHADER_UNIFORM_VEC4, int(pares));
    SetShaderValue(shader, loc.centro, centroShader, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, loc.raios, raiosShader, SHADER_UNIFORM_VEC3);
    const std::array<float, 32> volumes = volumesParaShader(perfil.colisores);
    SetShaderValueV(shader, loc.volumes, volumes.data(), SHADER_UNIFORM_VEC4, 8);
}

}  // namespace

void MalhaEsqueleto::carregar(const MalhaPele& p, bool dinamica) {
    Mesh m{};
    size_t n = p.v.size();
    m.vertexCount = int(n);
    m.triangleCount = int(p.idx.size() / 3);
    m.vertices = (float*)MemAlloc(sizeof(float) * 3 * n);
    m.normals = (float*)MemAlloc(sizeof(float) * 3 * n);
    m.texcoords = (float*)MemAlloc(sizeof(float) * 2 * n);
    m.texcoords2 = (float*)MemAlloc(sizeof(float) * 2 * n);
    m.colors = (unsigned char*)MemAlloc(4 * n);
    m.indices = (unsigned short*)MemAlloc(sizeof(unsigned short) * p.idx.size());
    m.tangents = (float*)MemAlloc(sizeof(float) * 4 * n);
    for (size_t i = 0; i < n; i++) {
        const VertPele& v = p.v[i];
        m.vertices[i * 3] = v.pos.x; m.vertices[i * 3 + 1] = v.pos.y; m.vertices[i * 3 + 2] = v.pos.z;
        m.normals[i * 3] = v.normal.x; m.normals[i * 3 + 1] = v.normal.y; m.normals[i * 3 + 2] = v.normal.z;
        m.texcoords[i * 2] = v.ao; m.texcoords[i * 2 + 1] = v.material;
        m.tangents[i * 4] = v.tang.x; m.tangents[i * 4 + 1] = v.tang.y; m.tangents[i * 4 + 2] = v.tang.z; m.tangents[i * 4 + 3] = v.tv;
        m.texcoords2[i * 2] = float(v.osso[0]);
        if (v.guiaCabelo[0] != SEM_GUIA_CABELO) {
            // Fita rígida no osso A. A fração de x guarda o comprimento livre
            // comprimido. Em y, o inteiro guarda o par e a fração, a mistura.
            const auto& guias = p.cabelo.guias;
            float peso = v.misturaGuias / 255.f;
            float comprimento = lerp(guias[v.guiaCabelo[0]].comprimentoLivre,
                                     guias[v.guiaCabelo[1]].comprimentoLivre, peso);
            m.texcoords2[i * 2] += comprimento / (1.f + comprimento);
            int par = v.guiaCabelo[0] * MAX_GUIAS_CABELO + v.guiaCabelo[1];
            m.texcoords2[i * 2 + 1] = -float(1 + par) - peso * .99f;
        } else {
            m.texcoords2[i * 2 + 1] = float(v.osso[1]) + std::clamp(v.peso, 0.f, 1.f) * .99f;
        }
        m.colors[i * 4] = c8(v.cor.r); m.colors[i * 4 + 1] = c8(v.cor.g); m.colors[i * 4 + 2] = c8(v.cor.b); m.colors[i * 4 + 3] = c8(v.pelos);
    }
    std::copy(p.idx.begin(), p.idx.end(), m.indices);
    carregarApoios(p, m);
    UploadMesh(&m, dinamica);
    mesh_ = m;
    cabelo_ = p.cabelo.quantidade ? std::make_unique<FisicaCabelo>(p.cabelo) : nullptr;
    ok_ = true;
}

void MalhaEsqueleto::alternarForma(size_t inicio, std::vector<float>& posicoes, std::vector<float>& normais) {
    size_t deslocamento = inicio * 3;
    std::swap_ranges(posicoes.begin(), posicoes.end(), mesh_.vertices + deslocamento);
    std::swap_ranges(normais.begin(), normais.end(), mesh_.normals + deslocamento);
    int bytes = int(posicoes.size() * sizeof(float)), offset = int(deslocamento * sizeof(float));
    // Posições e normais ocupam os buffers 0 e 2 da malha da raylib.
    UpdateMeshBuffer(mesh_, 0, mesh_.vertices + deslocamento, bytes, offset);
    UpdateMeshBuffer(mesh_, 2, mesh_.normals + deslocamento, bytes, offset);
}

void MalhaEsqueleto::descarregar() {
    if (ok_) UnloadMesh(mesh_);
    cabelo_.reset();
    ok_ = false;
}

void MalhaEsqueleto::atualizarCabelo(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f vento, float detalhe) {
    if (cabelo_) cabelo_->atualizar(dt, cabeca, pescoco, peito, vento, detalhe);
}

void MalhaEsqueleto::reiniciarCabelo() {
    if (cabelo_) cabelo_->reiniciar();
}

void MalhaEsqueleto::desenhar(Material& mat, const std::vector<Mat4>& ossos, Matrix modelo) const {
    Matrix buf[17];
    int n = std::min<int>(17, int(ossos.size()));
    for (int i = 0; i < n; i++) buf[i] = paraRaylib(ossos[i]);
    for (int i = n; i < 17; i++) buf[i] = paraRaylib(Mat4::id());
#if defined(PLATFORM_WEB)
    // rlSetUniformMatrices transpoe no PC mas nao no WebGL (que nao aceita transpose=true)
    for (int i = 0; i < 17; i++) buf[i] = MatrixTranspose(buf[i]);
#endif
    // cache da posicao do uniform por shader (evita glGetUniformLocation a cada desenho)
    static unsigned int ultimoShader = 0;
    static int ultimoLoc = -1;
    if (mat.shader.id != ultimoShader) {
        ultimoShader = mat.shader.id;
        ultimoLoc = GetShaderLocation(mat.shader, "ossos");
    }
    rlEnableShader(mat.shader.id);
    rlSetUniformMatrices(ultimoLoc, buf, 17);
    if (cabelo_ && cabelo_->iniciada()) {
        const auto& loc = uniformesCabelo(mat.shader);
        enviarCabelo(mat.shader, loc, *cabelo_);
        DrawMesh(mesh_, mat, modelo);
        const float desligado = 0.f;
        SetShaderValue(mat.shader, loc.ativo, &desligado, SHADER_UNIFORM_FLOAT);
    } else {
        DrawMesh(mesh_, mat, modelo);
    }
}
