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
    for (size_t i = 0; i < n; i++) {
        const VertPele& v = p.v[i];
        m.vertices[i * 3] = v.pos.x; m.vertices[i * 3 + 1] = v.pos.y; m.vertices[i * 3 + 2] = v.pos.z;
        m.normals[i * 3] = v.normal.x; m.normals[i * 3 + 1] = v.normal.y; m.normals[i * 3 + 2] = v.normal.z;
        m.texcoords[i * 2] = v.ao; m.texcoords[i * 2 + 1] = 0;
        m.texcoords2[i * 2] = float(v.osso[0]);
        m.texcoords2[i * 2 + 1] = float(v.osso[1]) + std::clamp(v.peso, 0.f, 1.f) * 0.99f;
        m.colors[i * 4] = c8(v.cor.r); m.colors[i * 4 + 1] = c8(v.cor.g); m.colors[i * 4 + 2] = c8(v.cor.b); m.colors[i * 4 + 3] = 255;
    }
    std::copy(p.idx.begin(), p.idx.end(), m.indices);
    UploadMesh(&m, false);
    mesh_ = m;
    ok_ = true;
}

void MalhaEsqueleto::descarregar() {
    if (ok_) UnloadMesh(mesh_);
    ok_ = false;
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
    rlEnableShader(mat.shader.id);
    rlSetUniformMatrices(GetShaderLocation(mat.shader, "ossos"), buf, 17);
    DrawMesh(mesh_, mat, modelo);
}
