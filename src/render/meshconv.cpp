#include "render/meshconv.h"
#include <cstdlib>
#include <cstring>

static unsigned char cor8(float v) {
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    return (unsigned char)(v * 255.f + 0.5f);
}

Mesh paraMesh(const MeshBuilder& b) {
    const auto& vs = b.verts();
    Mesh m{};
    m.vertexCount = int(vs.size());
    m.triangleCount = int(vs.size() / 3);
    m.vertices = (float*)MemAlloc(sizeof(float) * 3 * vs.size());
    m.normals = (float*)MemAlloc(sizeof(float) * 3 * vs.size());
    m.texcoords = (float*)MemAlloc(sizeof(float) * 2 * vs.size());
    m.colors = (unsigned char*)MemAlloc(4 * vs.size());
    for (size_t i = 0; i < vs.size(); i++) {
        const Vert& v = vs[i];
        m.vertices[i * 3 + 0] = v.pos.x; m.vertices[i * 3 + 1] = v.pos.y; m.vertices[i * 3 + 2] = v.pos.z;
        m.normals[i * 3 + 0] = v.normal.x; m.normals[i * 3 + 1] = v.normal.y; m.normals[i * 3 + 2] = v.normal.z;
        m.texcoords[i * 2 + 0] = v.ao; m.texcoords[i * 2 + 1] = 0;
        m.colors[i * 4 + 0] = cor8(v.cor.r); m.colors[i * 4 + 1] = cor8(v.cor.g); m.colors[i * 4 + 2] = cor8(v.cor.b);
        m.colors[i * 4 + 3] = cor8(v.ao);
    }
    UploadMesh(&m, false);
    return m;
}
