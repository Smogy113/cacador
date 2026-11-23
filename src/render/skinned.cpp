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
    int ativo, curvas, pontasYZ, centro, raios;
};

const UniformesCabelo& uniformesCabelo(Shader shader) {
    // Um programa no passe de cor e outro na profundidade.
    static std::array<UniformesCabelo, 2> cache{};
    static size_t proximo = 0;
    for (const auto& loc : cache) if (loc.shader == shader.id) return loc;
    auto& loc = cache[proximo++ % cache.size()];
    loc = {shader.id, GetShaderLocation(shader, "cabeloAtivo"), GetShaderLocation(shader, "cabeloCurvas[0]"),
           GetShaderLocation(shader, "cabeloPontasYZ[0]"), GetShaderLocation(shader, "cabeloCentro"),
           GetShaderLocation(shader, "cabeloRaios")};
    return loc;
}

void enviarCabelo(Shader shader, const UniformesCabelo& loc, const FisicaCabelo& cabelo) {
    float ativo = 1.f;
    const auto& deformacao = cabelo.deformacao();
    const auto& perfil = cabelo.perfil();
    // Seis componentes por guia em 1,5 vec4. Incluindo os ossos, o shader
    // fica dentro do limite mínimo de 128 vetores de uniforms do WebGL 1.
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
}

}  // namespace

void MalhaEsqueleto::carregar(const MalhaPele& p) {
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
            // Sinal negativo marca uma fita presa ao osso A. Codifica o par
            // de guias no inteiro e a mistura na fração, sem outro atributo.
            int par = v.guiaCabelo[0] * MAX_GUIAS_CABELO + v.guiaCabelo[1];
            m.texcoords2[i * 2 + 1] = -float(1 + par) - v.misturaGuias * (.99f / 255.f);
        } else {
            m.texcoords2[i * 2 + 1] = float(v.osso[1]) + std::clamp(v.peso, 0.f, 1.f) * .99f;
        }
        m.colors[i * 4] = c8(v.cor.r); m.colors[i * 4 + 1] = c8(v.cor.g); m.colors[i * 4 + 2] = c8(v.cor.b); m.colors[i * 4 + 3] = c8(v.pelos);
    }
    std::copy(p.idx.begin(), p.idx.end(), m.indices);
    UploadMesh(&m, false);
    mesh_ = m;
    cabelo_ = p.cabelo.quantidade ? std::make_unique<FisicaCabelo>(p.cabelo) : nullptr;
    ok_ = true;
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
