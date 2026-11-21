#include "render/cabeca_raio.h"
#include "raymath.h"
#include "rlgl.h"

namespace {
Texture2D carregarAtlas(const void* dados, int w, int h, int formato) {
    Texture2D t{};
    t.id = rlLoadTexture(dados, w, h, formato, 1);
    t.width = w;
    t.height = h;
    t.mipmaps = 1;
    t.format = formato;
    if (t.id) {
        SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
    }
    return t;
}
void setV3(Shader s, const char* nome, Vec3f v) {
    float f[3] = {v.x, v.y, v.z};
    SetShaderValue(s, GetShaderLocation(s, nome), f, SHADER_UNIFORM_VEC3);
}
}  // namespace

void CabecaRaio::carregar(const VolumeCabeca& v, float sxz, float sy) {
    descarregar();
    auto d = atlasMeiaPrecisao(v);
    auto c = atlasCor(v);
    int wd = v.colunas * v.nx, hd = v.linhas * v.ny, wc = v.colunasCor * v.cx, hc = v.linhasCor * v.cy;
    dist_ = carregarAtlas(d.data(), wd, hd, PIXELFORMAT_UNCOMPRESSED_R16);
    cor_ = carregarAtlas(c.data(), wc, hc, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (!dist_.id || !cor_.id) {
        descarregar();
        return;
    }
    caixa_.carregar(caixaCabeca(v, sxz, sy));
    lo_ = v.lo, hi_ = v.hi;
    n_ = {float(v.nx), float(v.ny), float(v.nz)};
    nc_ = {float(v.cx), float(v.cy), float(v.cz)};
    escala_ = {sxz, sy, sxz};
    float info[4] = {v.h, v.hc, float(v.colunas), float(v.colunasCor)};
    float tam[4] = {float(wd), float(hd), float(wc), float(hc)};
    for (int i = 0; i < 4; i++) info_[i] = info[i], tam_[i] = tam[i];
    ok_ = true;
}

void CabecaRaio::descarregar() {
    if (dist_.id) UnloadTexture(dist_);
    if (cor_.id) UnloadTexture(cor_);
    dist_ = cor_ = Texture2D{};
    caixa_.descarregar();
    ok_ = false;
}

void CabecaRaio::desenhar(Material& mat, const std::vector<Mat4>& ossos, Matrix modelo) const {
    if (!ok_) return;
    Shader s = mat.shader;
    mat.maps[MATERIAL_MAP_NORMAL].texture = dist_;
    mat.maps[MATERIAL_MAP_EMISSION].texture = cor_;
    setV3(s, "volLo", lo_);
    setV3(s, "volHi", hi_);
    setV3(s, "volN", n_);
    setV3(s, "corN", nc_);
    setV3(s, "escalaRep", escala_);
    SetShaderValue(s, GetShaderLocation(s, "volInfo"), info_, SHADER_UNIFORM_VEC4);
    SetShaderValue(s, GetShaderLocation(s, "atlasTam"), tam_, SHADER_UNIFORM_VEC4);
    SetShaderValueMatrix(s, GetShaderLocation(s, "invModelo"), MatrixInvert(modelo));
    Matrix proj = rlGetMatrixProjection();
    SetShaderValueMatrix(s, GetShaderLocation(s, "matVP"), MatrixMultiply(rlGetMatrixModelview(), proj));
    float tamPixel = 2.f / (proj.m5 * float(GetRenderHeight() > 0 ? GetRenderHeight() : 720));  // m5 = 1 / tan(fov / 2)
    SetShaderValue(s, GetShaderLocation(s, "tamPixel"), &tamPixel, SHADER_UNIFORM_FLOAT);
    rlSetCullFace(RL_CULL_FACE_FRONT);  // faces de tras: funciona tambem com a camera dentro da caixa
    caixa_.desenhar(mat, ossos, modelo);
    rlDrawRenderBatchActive();
    rlSetCullFace(RL_CULL_FACE_BACK);
}
