#include "render/lighting.h"
#include <algorithm>
#include <cmath>
#include "core/noise.h"
#include "render/shaders.h"
#include "rlgl.h"

namespace {
void setV3(Shader s, const char* nome, Vec3f v) {
    float f[3] = {v.x, v.y, v.z};
    SetShaderValue(s, GetShaderLocation(s, nome), f, SHADER_UNIFORM_VEC3);
}
void setRgb(Shader s, const char* nome, Rgb c) { setV3(s, nome, {c.r, c.g, c.b}); }
void setF(Shader s, const char* nome, float v) { SetShaderValue(s, GetShaderLocation(s, nome), &v, SHADER_UNIFORM_FLOAT); }
}  // namespace

void Iluminacao::carregar(Nivel nivel) {
    nivel_ = nivel;
    ef_ = efeitosDe(nivel);
    if (carregado_) return;
    lit_ = carregarShader(VS_LIT, FS_LIT);
    lit_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(lit_, "matModel");
    ceu_ = carregarShader(VS_CEU, FS_CEU);
    mat_ = LoadMaterialDefault();
    mat_.shader = lit_;
    esfera_ = LoadModelFromMesh(GenMeshSphere(400.f, 24, 32));
    esfera_.materials[0].shader = ceu_;
    carregado_ = true;
}

void Iluminacao::descarregar() {
    if (!carregado_) return;
    UnloadModel(esfera_);
    UnloadShader(lit_);
    UnloadShader(ceu_);
    carregado_ = false;
}

void Iluminacao::uniformesComuns(Shader s, const Paleta& p, Vec3f sol, float noite) {
    setRgb(s, "corNeblina", p.neblina);
    // o sol so ilumina acima do horizonte (desvanece no por do sol)
    float acima = smoothstep(-0.05f, 0.12f, sol.y);
    setRgb(s, "corSol", p.sol * acima);
    setV3(s, "dirSol", sol);
    setF(s, "densNeblina", p.densidadeNeblina);
    setF(s, "distVisao", ef_.distanciaVisao);
    setF(s, "exposicao", p.exposicao);
    setRgb(s, "grading", p.grading);
    setV3(s, "dirLua", lua_);
    (void)noite;
}

void Iluminacao::comecarCena(const Camera3D& cam, const DayClock& relogio, const std::vector<LuzPontual>& todas, Vec3f foco, float tempo) {
    paleta_ = relogio.paleta();
    sol_ = relogio.direcaoSol();
    lua_ = relogio.direcaoLua();
    noite_ = relogio.intensidadeTochas();
    uniformesComuns(lit_, paleta_, sol_, noite_);
    setV3(lit_, "posCamera", {cam.position.x, cam.position.y, cam.position.z});
    setRgb(lit_, "ambCeu", paleta_.ambienteCeu);
    setRgb(lit_, "ambChao", paleta_.ambienteChao);
    float luaAcima = smoothstep(-0.05f, 0.15f, lua_.y);
    setRgb(lit_, "corLua", Rgb{0.10f, 0.13f, 0.22f} * (luaAcima * noite_));
    setF(lit_, "noite", noite_);

    // as 8 luzes mais proximas do foco
    std::vector<const LuzPontual*> ord;
    ord.reserve(todas.size());
    for (auto& l : todas) ord.push_back(&l);
    auto d2 = [&](const LuzPontual* l) { Vec3f d = l->pos - foco; return dot(d, d); };
    size_t n = std::min<size_t>(MAX_LUZES, ord.size());
    std::partial_sort(ord.begin(), ord.begin() + n, ord.end(), [&](auto a, auto b) { return d2(a) < d2(b); });
    float pos[MAX_LUZES * 4] = {0}, cor[MAX_LUZES * 3] = {0};
    for (size_t i = 0; i < n; i++) {
        const LuzPontual& l = *ord[i];
        float tremor = 1.f;
        if (l.tremor > 0) {
            float semente = l.pos.x * 0.37f + l.pos.z * 0.71f;
            tremor = 1.f + l.tremor * (0.12f * std::sin(tempo * 13.f + semente) + 0.10f * ruido2D(tempo * 6.f, semente, 3));
        }
        pos[i * 4 + 0] = l.pos.x; pos[i * 4 + 1] = l.pos.y; pos[i * 4 + 2] = l.pos.z; pos[i * 4 + 3] = l.raio;
        cor[i * 3 + 0] = l.cor.r * tremor; cor[i * 3 + 1] = l.cor.g * tremor; cor[i * 3 + 2] = l.cor.b * tremor;
    }
    int ni = int(n);
    SetShaderValueV(lit_, GetShaderLocation(lit_, "luzPos"), pos, SHADER_UNIFORM_VEC4, MAX_LUZES);
    SetShaderValueV(lit_, GetShaderLocation(lit_, "luzCor"), cor, SHADER_UNIFORM_VEC3, MAX_LUZES);
    SetShaderValue(lit_, GetShaderLocation(lit_, "numLuzes"), &ni, SHADER_UNIFORM_INT);

    uniformesComuns(ceu_, paleta_, sol_, noite_);
    setRgb(ceu_, "ceuTopo", paleta_.ceuTopo);
    setRgb(ceu_, "ceuHorizonte", paleta_.ceuHorizonte);
    setF(ceu_, "noite", noite_);
}

void Iluminacao::modoHorizonte(float d) { setF(lit_, "descartarPerto", d); }

void Iluminacao::desenharCeu(const Camera3D& cam) {
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawModel(esfera_, cam.position, 1.f, WHITE);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}
