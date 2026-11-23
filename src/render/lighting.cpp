#include "render/lighting.h"
#include "core/hair_physics.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include "core/noise.h"
#include "render/perfil.h"
#include "render/shaders.h"
#include "raymath.h"
#include "rlgl.h"
#if !defined(PLATFORM_WEB)
#include "external/glad.h"
#endif

namespace {
constexpr float ALCANCE_AO = 48.f;
#if !defined(PLATFORM_WEB)
// Slot livre nas malhas e acima dos slots que a raylib usa nos efeitos de tela.
constexpr int UNIDADE_SOMBRA_FILTRADA = 11;
constexpr float ALCANCE_VOLUME = 56.f;
#endif
void setV3(Shader s, const char* nome, Vec3f v) {
    float f[3] = {v.x, v.y, v.z};
    SetShaderValue(s, GetShaderLocation(s, nome), f, SHADER_UNIFORM_VEC3);
}
void setRgb(Shader s, const char* nome, Rgb c) { setV3(s, nome, {c.r, c.g, c.b}); }
void setF(Shader s, const char* nome, float v) { SetShaderValue(s, GetShaderLocation(s, nome), &v, SHADER_UNIFORM_FLOAT); }
}  // namespace

static RenderTexture2D criarMapaSombra(int tam) {
    RenderTexture2D t{};
    t.id = rlLoadFramebuffer();
    t.texture.width = tam;
    t.texture.height = tam;
    if (t.id > 0) {
        rlEnableFramebuffer(t.id);
        t.depth.id = rlLoadTextureDepth(tam, tam, false);
        t.depth.width = tam;
        t.depth.height = tam;
        t.depth.format = 19;  // PIXELFORMAT_DEPTH (raylib usa 19 nesse caso)
        t.depth.mipmaps = 1;
        rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
        if (!rlFramebufferComplete(t.id)) {
            rlDisableFramebuffer();
            rlUnloadFramebuffer(t.id);
            return RenderTexture2D{};
        }
        rlDisableFramebuffer();
    }
    return t;
}

// Render target com cor + profundidade em textura (o SSAO le a profundidade).
static RenderTexture2D criarCenaComProfundidade(int w, int h) {
    RenderTexture2D t{};
    t.id = rlLoadFramebuffer();
    if (!t.id) return t;
    rlEnableFramebuffer(t.id);
    t.texture.id = rlLoadTexture(nullptr, w, h, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    t.texture.width = w; t.texture.height = h; t.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8; t.texture.mipmaps = 1;
    t.depth.id = rlLoadTextureDepth(w, h, false);
    t.depth.width = w; t.depth.height = h; t.depth.format = 19; t.depth.mipmaps = 1;
    rlFramebufferAttach(t.id, t.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(t.id, t.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    bool ok = rlFramebufferComplete(t.id);
    rlDisableFramebuffer();
    if (!ok) {
        rlUnloadTexture(t.texture.id);
        rlUnloadTexture(t.depth.id);
        rlUnloadFramebuffer(t.id);
        return RenderTexture2D{};
    }
    return t;
}

void Iluminacao::liberarEfeitos() {
    if (sombraPertoOk_) {
        rlUnloadTexture(sombraPertoRT_.depth.id);
        rlUnloadFramebuffer(sombraPertoRT_.id);
        sombraPertoOk_ = false;
    }
    matPele_.maps[MATERIAL_MAP_ROUGHNESS].texture = matCabeca_.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
    {
        float zero = 0;
        for (Shader sh : personagens_) SetShaderValue(sh, GetShaderLocation(sh, "usarSombraPerto"), &zero, SHADER_UNIFORM_FLOAT);
    }
    if (sombraOk_) {
        rlUnloadTexture(sombraRT_.depth.id);
        rlUnloadFramebuffer(sombraRT_.id);
        sombraOk_ = false;
    }
    if (posOk_) {
        if (cenaComProfundidade_) {
            rlUnloadTexture(cena_.texture.id);
            rlUnloadTexture(cena_.depth.id);
            rlUnloadFramebuffer(cena_.id);
            UnloadRenderTexture(aoA_);
            UnloadRenderTexture(aoB_);
            UnloadRenderTexture(focoA_);
            UnloadRenderTexture(focoB_);
        } else
            UnloadRenderTexture(cena_);
        cenaComProfundidade_ = false;
        UnloadRenderTexture(meiaA_);
        UnloadRenderTexture(meiaB_);
        UnloadRenderTexture(raiosRT_);
        posOk_ = false;
    }
    mat_.maps[MATERIAL_MAP_SPECULAR].texture = matConstr_.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
    if (matSolo_.maps) matSolo_.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
    float zero = 0;
    for (Shader sh : sombreados_) SetShaderValue(sh, GetShaderLocation(sh, "usarSombra"), &zero, SHADER_UNIFORM_FLOAT);
    matPele_.maps[MATERIAL_MAP_SPECULAR].texture = matCabeca_.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
}

void Iluminacao::carregarSolo() {
#if !defined(PLATFORM_WEB)
    soloTentado_ = true;
    Shader sh = carregarShaderDef(VS_LIT, FS_LIT, "#define SOLO_DETALHADO\n");
    if (sh.id == 0 || sh.id == rlGetShaderIdDefault()) return;
    Material mat = LoadMaterialDefault();
    mat.shader = sh;
    struct MapaSolo { int slot; const char* uniforme; const char* arquivo; };
    constexpr MapaSolo mapas[] = {
        {MATERIAL_MAP_ALBEDO, "corGramaSolo", DATA_DIR "/solo/grama.jpg"},
        {MATERIAL_MAP_NORMAL, "normalGramaSolo", DATA_DIR "/solo/grama-normal.jpg"},
        {MATERIAL_MAP_ROUGHNESS, "corTerraSolo", DATA_DIR "/solo/terra.jpg"},
        {MATERIAL_MAP_OCCLUSION, "normalTerraSolo", DATA_DIR "/solo/terra-normal.jpg"},
        {MATERIAL_MAP_EMISSION, "corRochaSolo", DATA_DIR "/solo/rocha.jpg"},
        {MATERIAL_MAP_HEIGHT, "normalRochaSolo", DATA_DIR "/solo/rocha-normal.jpg"},
    };
    for (const auto& mapa : mapas) {
        Texture2D& tex = mat.maps[mapa.slot].texture;
        tex = LoadTexture(mapa.arquivo);
        if (!tex.id) {
            UnloadMaterial(mat);
            return;
        }
        GenTextureMipmaps(&tex);
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
        SetTextureFilter(tex, TEXTURE_FILTER_ANISOTROPIC_4X);
        SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
        sh.locs[SHADER_LOC_MAP_ALBEDO + mapa.slot] = GetShaderLocation(sh, mapa.uniforme);
    }
    sh.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(sh, "matModel");
    sh.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(sh, "texSombra");
    SetShaderValue(sh, GetShaderLocation(sh, "texSombraFiltrada"), &UNIDADE_SOMBRA_FILTRADA, SHADER_UNIFORM_INT);
    matSolo_ = mat;
    sombreados_.push_back(sh);
#endif
}

void Iluminacao::carregar(Nivel nivel) {
    nivel_ = nivel;
    ef_ = efeitosDe(nivel);
#if defined(PLATFORM_WEB)
    ef_.sombras = false;  // a versao de teste no navegador fica sem os efeitos pesados
    ef_.bloom = ef_.raiosDeLuz = false;
    ef_.ssao = false;
#endif
    if (carregado_) {
        liberarEfeitos();
        limparLuzParedes();  // Medio nao guarda o rebote; luz velha nao aparece ao voltar
    } else {
        lit_ = carregarShader(VS_LIT, FS_LIT);
    lit_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(lit_, "matModel");
    ceu_ = carregarShader(VS_CEU, FS_CEU);
    mat_ = LoadMaterialDefault();
    mat_.shader = lit_;
    // construcoes: o mesmo shader com o bloco das paredes (o resto do mundo, grama inclusive, fica
    // com o shader menor, que roda mais pixels ao mesmo tempo na placa)
    matConstr_ = LoadMaterialDefault();
    matConstr_.shader = lit_;
#if !defined(PLATFORM_WEB)
    litParede_ = carregarShaderDef(VS_LIT, FS_LIT, "#define PAREDES\n");
    paredeOk_ = litParede_.id > 0 && litParede_.id != rlGetShaderIdDefault();
    if (paredeOk_) {
        litParede_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(litParede_, "matModel");
        litParede_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(litParede_, "texSombra");
        matConstr_.shader = litParede_;
        locJanelaP_ = GetShaderLocation(litParede_, "janelaP");
        locJanelaN_ = GetShaderLocation(litParede_, "janelaN");
        locNumJanelas_ = GetShaderLocation(litParede_, "numJanelas");
    }
#endif
    esfera_ = LoadModelFromMesh(GenMeshSphere(400.f, 24, 32));
    esfera_.materials[0].shader = ceu_;
    {   // textura de ruido: G(x,y) = R(x-37, y-17) -> ruido 3D com uma leitura (camadas em z deslocadas)
        Image img = GenImageColor(256, 256, BLACK);
        Color* px = (Color*)img.data;
        std::vector<unsigned char> r(256 * 256);
        uint32_t st = 12345u;
        for (auto& v : r) { st = st * 1664525u + 1013904223u; v = (unsigned char)(st >> 24); }
        for (int y = 0; y < 256; y++)
            for (int x = 0; x < 256; x++)
                px[y * 256 + x] = Color{r[y * 256 + x], r[((y - 17) & 255) * 256 + ((x - 37) & 255)], 0, 255};
        ruidoNuvem_ = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(ruidoNuvem_, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(ruidoNuvem_, TEXTURE_WRAP_REPEAT);
        esfera_.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = ruidoNuvem_;
    }
    prof_ = carregarShader(VS_PROF, FS_PROF);
    matProf_ = LoadMaterialDefault();
    matProf_.shader = prof_;
    brilho_ = carregarShaderPos(FS_BRILHO);
    blur_ = carregarShaderPos(FS_BLUR);
    raios_ = carregarShaderPos(FS_RAIOS);
    junta_ = carregarShaderPos(FS_JUNTA);
    ssao_ = carregarShaderPos(FS_SSAO);
    blurAO_ = carregarShaderPos(FS_BLUR_AO);
    blur4_ = carregarShaderPos(FS_BLUR4);
    longe_ = carregarShaderPos(FS_LONGE);
    lit_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(lit_, "texSombra");
    pele_ = carregarShaderDef(VS_PELE, FS_LIT, "#define PERSONAGEM\n");
    pele_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(pele_, "matModel");
    pele_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(pele_, "texSombra");
    pele_.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(pele_, "texSombraPerto");
    matPele_ = LoadMaterialDefault();
    matPele_.shader = pele_;
    matCabeca_ = LoadMaterialDefault();  // sempre existe (as texturas de sombra sao atribuidas mesmo sem o shader)
#if !defined(PLATFORM_WEB)
    cabeca_ = carregarShaderDef(VS_CABECA, FS_LIT, "#define PERSONAGEM\n#define RAYMARCH\n");
    cabecaOk_ = cabeca_.id > 0 && cabeca_.id != rlGetShaderIdDefault() && !getenv("CACADOR_SEM_CABECA");  // variavel de teste: simula placa sem o shader da cabeca
    if (cabecaOk_) {
        cabeca_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(cabeca_, "matModel");
        cabeca_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(cabeca_, "texSombra");
        cabeca_.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(cabeca_, "texSombraPerto");
        cabeca_.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(cabeca_, "texDist");
        cabeca_.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(cabeca_, "texCor");
        matCabeca_.shader = cabeca_;
    }
#endif
    profPele_ = carregarShader(VS_PROF_PELE, FS_PROF);
    int indiceCabelo = 0;
    for (Shader sh : {pele_, profPele_})
        locCabelo_[indiceCabelo++] = {GetShaderLocation(sh, "cabeloMeio[0]"), GetShaderLocation(sh, "cabeloPonta[0]"),
                                     GetShaderLocation(sh, "escalaCabelo")};
    matProfPele_ = LoadMaterialDefault();
    matProfPele_.shader = profPele_;
    junta_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(junta_, "texBrilho");
    personagens_ = {pele_};
    sombreados_ = {lit_, pele_};
    if (paredeOk_) sombreados_.push_back(litParede_);
    if (cabecaOk_) {
        personagens_.push_back(cabeca_);
        sombreados_.push_back(cabeca_);
    }
#if !defined(PLATFORM_WEB)
    for (Shader sh : sombreados_)
        SetShaderValue(sh, GetShaderLocation(sh, "texSombraFiltrada"), &UNIDADE_SOMBRA_FILTRADA, SHADER_UNIFORM_INT);
#endif
    carregado_ = true;
    }
#if !defined(PLATFORM_WEB)
    if (nivel_ != Nivel::Baixo && !soloTentado_) carregarSolo();
#endif
    if (paredeOk_ && nivel_ != Nivel::Baixo && !materiaisAssados_) {
        assarMateriais();
        materiaisAssados_ = true;
    }
    if (ef_.sombras) {
        sombraRT_ = criarMapaSombra(ef_.resolucaoSombra);
        sombraOk_ = sombraRT_.id > 0;
#if !defined(PLATFORM_WEB)
        if (nivel_ == Nivel::Alto && sombraOk_ && !samplerSombra_ && glGenSamplers) {
            glGenSamplers(1, &samplerSombra_);
            if (samplerSombra_) {
                glSamplerParameteri(samplerSombra_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glSamplerParameteri(samplerSombra_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glSamplerParameteri(samplerSombra_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glSamplerParameteri(samplerSombra_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glSamplerParameteri(samplerSombra_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glSamplerParameteri(samplerSombra_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                raiosVolume_ = carregarShaderPos(FS_RAIOS_VOLUME);
                if (raiosVolume_.id == rlGetShaderIdDefault()) raiosVolume_ = {};
                if (raiosVolume_.id)
                    SetShaderValue(raiosVolume_, GetShaderLocation(raiosVolume_, "texSombraFiltrada"),
                                   &UNIDADE_SOMBRA_FILTRADA, SHADER_UNIFORM_INT);
            }
        }
#endif
        sombraPertoRT_ = criarMapaSombra(1024);
        sombraPertoOk_ = sombraPertoRT_.id > 0;
        if (sombraPertoOk_) {
            matPele_.maps[MATERIAL_MAP_ROUGHNESS].texture = matCabeca_.maps[MATERIAL_MAP_ROUGHNESS].texture = sombraPertoRT_.depth;
            float t = 1.f / 1024.f;
            for (Shader sh : personagens_) setF(sh, "texelSombraPerto", t);
        }
        if (sombraOk_) {
            mat_.maps[MATERIAL_MAP_SPECULAR].texture = matConstr_.maps[MATERIAL_MAP_SPECULAR].texture = sombraRT_.depth;
            matPele_.maps[MATERIAL_MAP_SPECULAR].texture = matCabeca_.maps[MATERIAL_MAP_SPECULAR].texture = sombraRT_.depth;
            if (matSolo_.maps) matSolo_.maps[MATERIAL_MAP_SPECULAR].texture = sombraRT_.depth;
            float texel = 1.f / ef_.resolucaoSombra;
            float tm = 2 * AREA_SOMBRA / ef_.resolucaoSombra;
            for (Shader sh : sombreados_) {
                setF(sh, "texelSombra", texel);
                setF(sh, "texelMetros", tm);
            }
        }
    }
    if (ef_.bloom || ef_.raiosDeLuz || ef_.ssao) {
        largPos_ = GetScreenWidth();
        altPos_ = GetScreenHeight();
        if (ef_.ssao) {
            cena_ = criarCenaComProfundidade(largPos_, altPos_);
            cenaComProfundidade_ = cena_.id > 0;
            if (!cenaComProfundidade_) ef_.ssao = false;
            else {
                aoA_ = LoadRenderTexture(largPos_ / 2, altPos_ / 2);
                aoB_ = LoadRenderTexture(largPos_ / 2, altPos_ / 2);
                SetTextureFilter(aoA_.texture, TEXTURE_FILTER_BILINEAR);
                SetTextureFilter(aoB_.texture, TEXTURE_FILTER_BILINEAR);
                focoA_ = LoadRenderTexture(largPos_ / 2, altPos_ / 2);
                focoB_ = LoadRenderTexture(largPos_ / 2, altPos_ / 2);
                SetTextureFilter(focoA_.texture, TEXTURE_FILTER_BILINEAR);
                SetTextureFilter(focoB_.texture, TEXTURE_FILTER_BILINEAR);
            }
        }
        if (!cenaComProfundidade_) cena_ = LoadRenderTexture(largPos_, altPos_);
        meiaA_ = LoadRenderTexture(largPos_ / 2, altPos_ / 2);
        meiaB_ = LoadRenderTexture(largPos_ / 2, altPos_ / 2);
        raiosRT_ = LoadRenderTexture(largPos_ / 2, altPos_ / 2);
        for (auto* t : {&cena_, &meiaA_, &meiaB_, &raiosRT_}) SetTextureFilter(t->texture, TEXTURE_FILTER_BILINEAR);
        posOk_ = cena_.id > 0;
    }
    if (cabecaOk_) {
        setF(cabeca_, "passosMax", nivel_ == Nivel::Alto ? 64.f : 40.f);
        setF(cabeca_, "economico", nivel_ == Nivel::Alto ? 0.f : 1.f);
    }
}

void Iluminacao::descarregar() {
    if (!carregado_) return;
    liberarEfeitos();
    if (matSolo_.maps) UnloadMaterial(matSolo_);
    matSolo_ = {};
    soloTentado_ = false;
#if !defined(PLATFORM_WEB)
    if (raiosVolume_.id) UnloadShader(raiosVolume_);
    raiosVolume_ = {};
    if (samplerSombra_) glDeleteSamplers(1, &samplerSombra_);
    samplerSombra_ = 0;
#endif
    for (Shader sh : {prof_, brilho_, blur_, raios_, junta_, pele_, profPele_, ssao_, blurAO_, blur4_, longe_}) UnloadShader(sh);
    if (cabecaOk_) {
        for (int i = 0; i < 12; i++) matCabeca_.maps[i].texture = Texture2D{};  // as texturas sao de outros donos
        UnloadShader(cabeca_);
        cabecaOk_ = false;
    }
    esfera_.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = Texture2D{};
    UnloadModel(esfera_);
    UnloadTexture(ruidoNuvem_);
    UnloadShader(lit_);
    liberarLuzParedes();
    for (auto& t : texMat_)
        if (t.id) UnloadRenderTexture(t);
    for (auto& t : texMat_) t = RenderTexture2D{};
    materiaisAssados_ = false;
    if (paredeOk_) {
        matConstr_.shader = lit_;
        for (int m : {MATERIAL_MAP_ALBEDO, MATERIAL_MAP_NORMAL, MATERIAL_MAP_ROUGHNESS, MATERIAL_MAP_OCCLUSION}) matConstr_.maps[m].texture = Texture2D{};
        matConstr_.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
        UnloadShader(litParede_);
        paredeOk_ = false;
    }
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
    setF(s, "exposicao", p.exposicao * dbg.exposicao);
    if (dbg.semNeblina) setF(s, "densNeblina", 0.f);
    setRgb(s, "grading", p.grading);
    setV3(s, "dirLua", lua_);
    (void)noite;
}

void Iluminacao::comecarCena(const Camera3D& cam, const DayClock& relogio, const std::vector<LuzPontual>& todas, Vec3f foco, float tempo) {
    // A nova cena e os mapas de sombra precisam estar prontos antes dos passes do quadro.
    if (usaPos() && (GetScreenWidth() != largPos_ || GetScreenHeight() != altPos_)) carregar(nivel_);
    paleta_ = relogio.paleta();
    sol_ = relogio.direcaoSol();
    lua_ = relogio.direcaoLua();
    noite_ = relogio.intensidadeTochas();

    bool alto = false;
#if !defined(PLATFORM_WEB)
    alto = nivel_ == Nivel::Alto;
#endif
    if (alto) {
        float dia = smoothstep(-0.04f, 0.24f, sol_.y);
        Rgb sol = paleta_.sol;
        paleta_.sol = lerp(sol, {sol.r, sol.g * 0.84f + sol.r * 0.16f, sol.b * 0.84f + sol.r * 0.16f}, dia) * 0.88f;
        Rgb ceu = paleta_.ambienteCeu;
        paleta_.ambienteCeu = lerp(ceu, {ceu.r * 0.8f, ceu.g * 0.91f, ceu.b * 0.99f}, dia);
        paleta_.ambienteChao = paleta_.ambienteChao * lerp(1.f, 0.88f, dia);
        paleta_.grading = lerp(paleta_.grading, {1.f, 1.f, 1.f}, 0.65f);
    }

    // A selecao e o tremor das luzes sao iguais para todos os materiais.
    std::vector<const LuzPontual*> ord;
    ord.reserve(todas.size());
    for (const auto& l : todas) ord.push_back(&l);
    auto d2 = [&](const LuzPontual* l) { Vec3f d = l->pos - foco; return dot(d, d); };
    int n = int(std::min<size_t>(MAX_LUZES, ord.size()));
    std::partial_sort(ord.begin(), ord.begin() + n, ord.end(), [&](auto a, auto b) { return d2(a) < d2(b); });
    float pos[MAX_LUZES * 4], cor[MAX_LUZES * 3];
    for (int i = 0; i < n; i++) {
        const LuzPontual& l = *ord[i];
        float tremor = 1.f;
        if (l.tremor > 0) {
            float semente = l.pos.x * 0.37f + l.pos.z * 0.71f;
            tremor = 1.f + l.tremor * (0.12f * std::sin(tempo * 13.f + semente) + 0.10f * ruido2D(tempo * 6.f, semente, 3));
        }
        pos[i * 4 + 0] = l.pos.x; pos[i * 4 + 1] = l.pos.y; pos[i * 4 + 2] = l.pos.z; pos[i * 4 + 3] = l.raio;
        cor[i * 3 + 0] = l.cor.r * tremor; cor[i * 3 + 1] = l.cor.g * tremor; cor[i * 3 + 2] = l.cor.b * tremor;
    }

    float luaAcima = smoothstep(-0.05f, 0.15f, lua_.y);
    for (Shader sh : sombreados_) {
        if (nivel_ == Nivel::Baixo && sh.id == matSolo_.shader.id) continue;
        uniformesComuns(sh, paleta_, sol_, noite_);
        setV3(sh, "posCamera", {cam.position.x, cam.position.y, cam.position.z});
        setF(sh, "tempoVento", tempo);
        // neblina baixa: forte com o sol baixo, fraca ao meio-dia, media a noite
        setF(sh, "neblinaBaixa", sol_.y > 0.f ? 0.25f + 0.85f * (1.f - smoothstep(0.05f, 0.45f, sol_.y)) : 0.5f);
        setRgb(sh, "ambCeu", paleta_.ambienteCeu);
        setRgb(sh, "ambChao", paleta_.ambienteChao);
        setRgb(sh, "corLua", Rgb{0.10f, 0.13f, 0.22f} * (luaAcima * noite_));
        setF(sh, "noite", noite_);
        setRgb(sh, "corChao", Rgb{0.10f, 0.15f, 0.05f});
#if !defined(PLATFORM_WEB)
        setF(sh, "iluminacaoAlta", alto ? 1.f : 0.f);
        setF(sh, "usarFiltroSombra", usaFiltroSombra() ? 1.f : 0.f);
        if (alto && !usaSombras()) {
            setF(sh, "usarSombra", 0.f);
            setF(sh, "usarSombraPerto", 0.f);
        }
#endif

        if (n) {
            SetShaderValueV(sh, GetShaderLocation(sh, "luzPos"), pos, SHADER_UNIFORM_VEC4, n);
            SetShaderValueV(sh, GetShaderLocation(sh, "luzCor"), cor, SHADER_UNIFORM_VEC3, n);
        }
        SetShaderValue(sh, GetShaderLocation(sh, "numLuzes"), &n, SHADER_UNIFORM_INT);
    }
#if !defined(PLATFORM_WEB)
    if (paredeOk_) setF(litParede_, "paredes", dbg.semParedes || nivel_ == Nivel::Baixo ? 0.f : (nivel_ == Nivel::Alto ? 2.f : 1.f));
#endif
    uniformesComuns(ceu_, paleta_, sol_, noite_);
    setRgb(ceu_, "ceuTopo", paleta_.ceuTopo);
    setRgb(ceu_, "ceuHorizonte", paleta_.ceuHorizonte);
    setF(ceu_, "noite", noite_);
    setV3(ceu_, "posCamera", {cam.position.x, cam.position.y, cam.position.z});
    setF(ceu_, "tempoVento", tempo);
    setF(ceu_, "iluminacaoAlta", alto ? 1.f : 0.f);
    float pn = nivel_ == Nivel::Alto ? 22.f : (nivel_ == Nivel::Medio ? 12.f : 0.f);
    if (dbg.passosNuvem >= 0) pn = dbg.passosNuvem;
    setF(ceu_, "passosNuvem", dbg.semNuvens ? -1.f : pn);
}

void Iluminacao::modoHorizonte(float d) {
    setF(lit_, "descartarPerto", d);
    if (paredeOk_) setF(litParede_, "descartarPerto", d);
}

void Iluminacao::modoEstudio(Vec3f dir, bool argila) {
    for (Shader sh : sombreados_) {
        setV3(sh, "dirSol", dir);
        setRgb(sh, "corSol", Rgb{1.45f, 1.42f, 1.38f});
        setRgb(sh, "ambCeu", Rgb{0.16f, 0.165f, 0.18f});
        setRgb(sh, "ambChao", Rgb{0.07f, 0.07f, 0.075f});
        setRgb(sh, "corLua", Rgb{0, 0, 0});
        setRgb(sh, "corChao", Rgb{0.3f, 0.3f, 0.3f});
        setRgb(sh, "corNeblina", Rgb{0.02f, 0.02f, 0.02f});
        setF(sh, "densNeblina", 0.f);
        setF(sh, "exposicao", 1.f);
        setRgb(sh, "grading", Rgb{1, 1, 1});
        setV3(sh, "dirSombra", dir);
        int zero = 0;
        SetShaderValue(sh, GetShaderLocation(sh, "numLuzes"), &zero, SHADER_UNIFORM_INT);
    }
    sol_ = dir;
    dirSombra_ = dir;
    for (Shader sh : personagens_) setF(sh, "argila", argila ? 1.f : 0.f);
}

void Iluminacao::desenharCeu(const Camera3D& cam) {
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawModel(esfera_, cam.position, 1.f, WHITE);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void Iluminacao::comecarSombra(Vec3f centro) {
    // luz que faz sombra: sol de dia, lua de noite
    Vec3f dir = sol_.y > 0.05f ? sol_ : lua_;
    if (dir.y < 0.05f) dir = {0.3f, 0.9f, 0.2f};
    dir = normalizar(dir);
    // prende o centro na grade de texels: a sombra nao "tremula" ao andar
    Vec3f dirX = normalizar(cross({0, 1, 0}, dir));
    Vec3f dirY = cross(dir, dirX);
    float texel = 2 * AREA_SOMBRA / ef_.resolucaoSombra;
    float px = std::floor(dot(centro, dirX) / texel) * texel, py = std::floor(dot(centro, dirY) / texel) * texel, pz = dot(centro, dir);
    Vec3f c = dirX * px + dirY * py + dir * pz;
    Camera3D cl{};
    cl.position = {c.x + dir.x * 200.f, c.y + dir.y * 200.f, c.z + dir.z * 200.f};
    cl.target = {c.x, c.y, c.z};
    cl.up = {0, 1, 0};
    cl.fovy = 2 * AREA_SOMBRA;
    cl.projection = CAMERA_ORTHOGRAPHIC;
    dirSombra_ = dir;
    for (Shader sh : sombreados_) setV3(sh, "dirSombra", dir);
    BeginTextureMode(sombraRT_);
    ClearBackground(WHITE);
    BeginMode3D(cl);
    Matrix projecao = rlGetMatrixProjection();
    luzVP_ = MatrixMultiply(rlGetMatrixModelview(), projecao);
    if (usaFiltroSombra()) {
        float profundidadePorMetro = -0.5f * projecao.m10;
        for (Shader sh : sombreados_) {
            setV3(sh, "passoSombraX", dirX * (texel * profundidadePorMetro));
            setV3(sh, "passoSombraY", dirY * (texel * profundidadePorMetro));
            setF(sh, "metrosProfSombra", 1.f / profundidadePorMetro);
        }
    }
}

void Iluminacao::terminarSombra() {
    EndMode3D();
    EndTextureMode();
    float um = 1;
    for (Shader sh : sombreados_) {
        SetShaderValueMatrix(sh, GetShaderLocation(sh, "matLuz"), luzVP_);
        SetShaderValue(sh, GetShaderLocation(sh, "usarSombra"), &um, SHADER_UNIFORM_FLOAT);
    }
}

void Iluminacao::comecarPos() {
    BeginTextureMode(cena_);
}

void Iluminacao::comecarPassePrincipal() {
#if !defined(PLATFORM_WEB)
    if (cenaComProfundidade_) {
        Matrix projecao = rlGetMatrixProjection();
        profCena_[0] = projecao.m10;
        profCena_[1] = projecao.m14;
        if (usaSombras() && usaFiltroSombra() && !dbg.semRaios)
            invCameraVP_ = MatrixInvert(MatrixMultiply(rlGetMatrixModelview(), projecao));
    }
    if (!usaSombras() || !usaFiltroSombra()) return;
    // A mesma textura e lida sem filtro no slot do material para procurar bloqueadores.
    rlActiveTextureSlot(UNIDADE_SOMBRA_FILTRADA);
    rlEnableTexture(sombraRT_.depth.id);
    glBindSampler(UNIDADE_SOMBRA_FILTRADA, samplerSombra_);
    rlActiveTextureSlot(0);
#endif
}

void Iluminacao::terminarPassePrincipal() {
#if !defined(PLATFORM_WEB)
    if (!usaSombras() || !usaFiltroSombra()) return;
    glBindSampler(UNIDADE_SOMBRA_FILTRADA, 0);
    rlActiveTextureSlot(UNIDADE_SOMBRA_FILTRADA);
    rlDisableTexture();
    rlActiveTextureSlot(0);
#endif
}

static void desenharTela(RenderTexture2D& origem, int w, int h) {
    DrawTexturePro(origem.texture, {0, 0, float(origem.texture.width), float(-origem.texture.height)}, {0, 0, float(w), float(h)}, {0, 0}, 0, WHITE);
}

void Iluminacao::desenharRaiosVolume(const Camera3D& cam, float intensidade) {
#if !defined(PLATFORM_WEB)
    Shader sh = raiosVolume_;
    BeginTextureMode(raiosRT_);
    BeginShaderMode(sh);
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
    SetShaderValueTexture(sh, GetShaderLocation(sh, "texProf"), cena_.depth);
    SetShaderValueMatrix(sh, GetShaderLocation(sh, "invCameraVP"), invCameraVP_);
    SetShaderValueMatrix(sh, GetShaderLocation(sh, "matLuz"), luzVP_);
    SetShaderValue(sh, GetShaderLocation(sh, "profCena"), profCena_, SHADER_UNIFORM_VEC2);
    setV3(sh, "posCamera", {cam.position.x, cam.position.y, cam.position.z});
    setV3(sh, "dirSol", sol_);
    setRgb(sh, "corRaio", paleta_.sol * intensidade);
    setF(sh, "alcanceVolume", ALCANCE_VOLUME);
    desenharTela(cena_, raiosRT_.texture.width, raiosRT_.texture.height);
    EndBlendMode();
    EndShaderMode();
    EndTextureMode();
#else
    (void)cam;
    (void)intensidade;
#endif
}

void Iluminacao::terminarPos(const Camera3D& cam, float tempoGrao, PerfilGPU* perfil) {
    auto comecar = [&](ParteQuadro p) { if (perfil) perfil->comecar(p); };
    auto terminar = [&]() { if (perfil) perfil->terminar(); };
    EndTextureMode();
    int mw = largPos_ / 2, mh = altPos_ / 2;
    bool bloom = ef_.bloom && !dbg.semBloom;
    comecar(P_POS_BRILHO);
    // 1) partes claras da cena (meia resolucao)
    if (bloom) {
        BeginTextureMode(meiaA_);
        BeginShaderMode(brilho_);
        setF(brilho_, "limiar", 0.85f);
        desenharTela(cena_, mw, mh);
        EndShaderMode();
        EndTextureMode();
    }
    terminar();
    comecar(P_POS_RAIOS);
    // 2) raios de luz a partir do sol
    bool raios = false;
    bool volume = false;
    posSolTela_[0] = posSolTela_[1] = -1.f;
    if (ef_.raiosDeLuz && !dbg.semRaios && sol_.y > -0.02f) {
        Vector3 ps{cam.position.x + sol_.x * 300, cam.position.y + sol_.y * 300, cam.position.z + sol_.z * 300};
        Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
        float olhaSol = Vector3DotProduct(fwd, {sol_.x, sol_.y, sol_.z});
        if (olhaSol > 0.05f) {
            Vector2 t = GetWorldToScreenEx(ps, cam, largPos_, altPos_);
            posSolTela_[0] = t.x / largPos_;
            posSolTela_[1] = 1.f - t.y / altPos_;
        }
        bool podeUsarVolume = raiosVolume_.id && cenaComProfundidade_ && usaSombras() && usaFiltroSombra();
        if (podeUsarVolume) {
            float intensidade = smoothstep(-0.3f, 0.08f, olhaSol) * smoothstep(0.05f, 0.16f, sol_.y);
            if (intensidade > 0.f) {
                desenharRaiosVolume(cam, intensidade);
                volume = true;
            }
        } else if (olhaSol > 0.05f) {
            // fachos fortes com o sol baixo (manha e fim de tarde), mais suaves ao meio-dia
            float forca = (0.75f * (1.f - smoothstep(0.25f, 0.8f, sol_.y)) + 0.25f) * smoothstep(0.05f, 0.45f, olhaSol);
            float asp = float(largPos_) / float(altPos_), up = cenaComProfundidade_ ? 1.f : 0.f;
            BeginTextureMode(raiosRT_);
            BeginShaderMode(raios_);
            SetShaderValue(raios_, GetShaderLocation(raios_, "posSol"), posSolTela_, SHADER_UNIFORM_VEC2);
            SetShaderValue(raios_, GetShaderLocation(raios_, "forca"), &forca, SHADER_UNIFORM_FLOAT);
            SetShaderValue(raios_, GetShaderLocation(raios_, "aspecto"), &asp, SHADER_UNIFORM_FLOAT);
            SetShaderValue(raios_, GetShaderLocation(raios_, "usarProf"), &up, SHADER_UNIFORM_FLOAT);
            Vector3 cr{paleta_.sol.r, paleta_.sol.g, paleta_.sol.b};
            SetShaderValue(raios_, GetShaderLocation(raios_, "corRaio"), &cr, SHADER_UNIFORM_VEC3);
            if (cenaComProfundidade_) SetShaderValueTexture(raios_, GetShaderLocation(raios_, "texProf"), cena_.depth);
            DrawTexturePro(cena_.texture, {0, 0, float(largPos_), float(-altPos_)}, {0, 0, float(mw), float(mh)}, {0, 0}, 0, WHITE);
            EndShaderMode();
            EndTextureMode();
            raios = true;
        }
        raios |= volume;
    }
    terminar();
    comecar(P_POS_BRILHO_DESF);
    // 3) desfoca o brilho (horizontal e vertical, 2 vezes)
    for (int k = 0; k < 2 && bloom; k++) {
        float dh[2] = {1.f / mw * (k + 1), 0}, dv[2] = {0, 1.f / mh * (k + 1)};
        BeginTextureMode(meiaB_);
        BeginShaderMode(blur_);
        SetShaderValue(blur_, GetShaderLocation(blur_, "direcao"), dh, SHADER_UNIFORM_VEC2);
        DrawTexturePro(meiaA_.texture, {0, 0, float(mw), float(mh)}, {0, 0, float(mw), float(mh)}, {0, 0}, 0, WHITE);
        EndShaderMode();
        EndTextureMode();
        BeginTextureMode(meiaA_);
        BeginShaderMode(blur_);
        SetShaderValue(blur_, GetShaderLocation(blur_, "direcao"), dv, SHADER_UNIFORM_VEC2);
        DrawTexturePro(meiaB_.texture, {0, 0, float(mw), float(mh)}, {0, 0, float(mw), float(mh)}, {0, 0}, 0, WHITE);
        EndShaderMode();
        EndTextureMode();
    }
    terminar();
    comecar(P_POS_SSAO);
    // 3b) oclusao de ambiente em tela (meia resolucao, desfocada)
    bool ao = ef_.ssao && cenaComProfundidade_ && !dbg.semSSAO;
    if (ao) {
        BeginTextureMode(aoA_);
        BeginShaderMode(ssao_);
        float tx[2] = {1.f / largPos_, 1.f / altPos_};
        float escala = altPos_ / (2.f * std::tan(cam.fovy * 0.5f * DEG2RAD));
        SetShaderValue(ssao_, GetShaderLocation(ssao_, "texel"), tx, SHADER_UNIFORM_VEC2);
        SetShaderValue(ssao_, GetShaderLocation(ssao_, "escalaProj"), &escala, SHADER_UNIFORM_FLOAT);
        SetShaderValue(ssao_, GetShaderLocation(ssao_, "profCena"), profCena_, SHADER_UNIFORM_VEC2);
        setF(ssao_, "alcanceAO", ALCANCE_AO);
        float th = std::tan(cam.fovy * 0.5f * DEG2RAD);
        float tm[2] = {th * float(largPos_) / float(altPos_), th};
        SetShaderValue(ssao_, GetShaderLocation(ssao_, "tanMeio"), tm, SHADER_UNIFORM_VEC2);
        DrawTexturePro(cena_.depth, {0, 0, float(largPos_), float(-altPos_)}, {0, 0, float(mw), float(mh)}, {0, 0}, 0, WHITE);
        EndShaderMode();
        EndTextureMode();
        for (int k = 0; k < 2; k++) {
            float d[2] = {k == 0 ? 1.f / mw : 0.f, k == 0 ? 0.f : 1.f / mh};
            RenderTexture2D& de = k == 0 ? aoA_ : aoB_;
            RenderTexture2D& para = k == 0 ? aoB_ : aoA_;
            BeginTextureMode(para);
            BeginShaderMode(blurAO_);
            setF(blurAO_, "alcanceAO", ALCANCE_AO);
            SetShaderValue(blurAO_, GetShaderLocation(blurAO_, "direcao"), d, SHADER_UNIFORM_VEC2);
            desenharTela(de, mw, mh);
            EndShaderMode();
            EndTextureMode();
        }
    }
    terminar();
    comecar(P_POS_FOCO);
    // 3c) camera de cinema (Alto): copia desfocada da cena para a profundidade de campo
    bool cinema = nivel_ == Nivel::Alto && cenaComProfundidade_;
    bool foco = cinema && !dbg.semFoco;
    if (foco) {
        BeginTextureMode(focoA_);
        ClearBackground(BLANK);
        BeginShaderMode(longe_);
        SetShaderValue(longe_, GetShaderLocation(longe_, "distFoco"), &distFoco_, SHADER_UNIFORM_FLOAT);
        SetShaderValue(longe_, GetShaderLocation(longe_, "profCena"), profCena_, SHADER_UNIFORM_VEC2);
        SetShaderValueTexture(longe_, GetShaderLocation(longe_, "texProf"), cena_.depth);
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);  // grava o alfa como esta (sem misturar)
        BeginBlendMode(BLEND_CUSTOM);
        desenharTela(cena_, mw, mh);
        EndBlendMode();
        EndShaderMode();
        EndTextureMode();
        for (int k = 0; k < 4; k++) {
            float esp = k < 2 ? 1.f : 2.2f;
            float d[2] = {k % 2 == 0 ? esp / mw : 0.f, k % 2 == 0 ? 0.f : esp / mh};
            RenderTexture2D& de = k % 2 == 0 ? focoA_ : focoB_;
            RenderTexture2D& para = k % 2 == 0 ? focoB_ : focoA_;
            BeginTextureMode(para);
            BeginShaderMode(blur4_);
            rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM);
            SetShaderValue(blur4_, GetShaderLocation(blur4_, "direcao"), d, SHADER_UNIFORM_VEC2);
            DrawTexturePro(de.texture, {0, 0, float(mw), float(mh)}, {0, 0, float(mw), float(mh)}, {0, 0}, 0, WHITE);
            EndBlendMode();
            EndShaderMode();
            EndTextureMode();
        }
    }
    terminar();
    comecar(P_POS_JUNTA);
    // 4) junta tudo na tela
    BeginShaderMode(junta_);
    {
        float uf = foco ? 1.f : 0.f, ug = cinema && !dbg.semGrao ? 1.f : 0.f;
        // mais desfoque com a camera perto do heroi (close, mira); de longe quase nada
        float forca = 0.35f + 0.5f * (1.f - smoothstep(1.2f, 4.5f, distFoco_));
        float tx[2] = {1.f / largPos_, 1.f / altPos_};
        SetShaderValue(junta_, GetShaderLocation(junta_, "usarFoco"), &uf, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "usarGrao"), &ug, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "distFoco"), &distFoco_, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "forcaFoco"), &forca, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "tempo"), &tempoGrao, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "texelTela"), tx, SHADER_UNIFORM_VEC2);
        if (foco || ao || volume) {
            SetShaderValue(junta_, GetShaderLocation(junta_, "profCena"), profCena_, SHADER_UNIFORM_VEC2);
            SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texProf"), cena_.depth);
        }
        if (foco) {
            SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texDesfoque"), focoA_.texture);
        }
    }
    float fb = bloom ? 0.42f : 0.f, ur = raios ? 1.f : 0.f;
    float aspJ = float(largPos_) / float(altPos_);
    Vector3 crJ{paleta_.sol.r, paleta_.sol.g, paleta_.sol.b};
    SetShaderValue(junta_, GetShaderLocation(junta_, "posSol"), posSolTela_, SHADER_UNIFORM_VEC2);
    SetShaderValue(junta_, GetShaderLocation(junta_, "aspecto"), &aspJ, SHADER_UNIFORM_FLOAT);
    SetShaderValue(junta_, GetShaderLocation(junta_, "corRaio"), &crJ, SHADER_UNIFORM_VEC3);
    SetShaderValue(junta_, GetShaderLocation(junta_, "forcaBrilho"), &fb, SHADER_UNIFORM_FLOAT);
    SetShaderValue(junta_, GetShaderLocation(junta_, "usarRaios"), &ur, SHADER_UNIFORM_FLOAT);
#if !defined(PLATFORM_WEB)
    setF(junta_, "usarVolume", volume ? 1.f : 0.f);
    if (volume) setF(junta_, "alcanceVolume", ALCANCE_VOLUME);
#endif
    if (bloom) SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texBrilho"), meiaA_.texture);
    if (raios) SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texRaios"), raiosRT_.texture);
    float ua = ao ? 1.f : 0.f;
    SetShaderValue(junta_, GetShaderLocation(junta_, "usarAO"), &ua, SHADER_UNIFORM_FLOAT);
    if (ao) {
        setF(junta_, "alcanceAO", ALCANCE_AO);
        SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texAO"), aoA_.texture);
    }
    desenharTela(cena_, GetScreenWidth(), GetScreenHeight());
    EndShaderMode();
    terminar();
}

void Iluminacao::comecarSombraPerto(Vec3f centro) {
    Vec3f dir = dirSombra_;
    Camera3D cl{};
    cl.position = {centro.x + dir.x * 20.f, centro.y + dir.y * 20.f, centro.z + dir.z * 20.f};
    cl.target = {centro.x, centro.y, centro.z};
    cl.up = {0, 1, 0};
    if (std::abs(dir.y) > 0.98f) cl.up = {0, 0, 1};
    cl.fovy = 3.0f;
    cl.projection = CAMERA_ORTHOGRAPHIC;
    BeginTextureMode(sombraPertoRT_);
    ClearBackground(WHITE);
    BeginMode3D(cl);
    luzVPPerto_ = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

void Iluminacao::terminarSombraPerto() {
    EndMode3D();
    EndTextureMode();
    float um = 1;
    for (Shader sh : personagens_) {
        SetShaderValueMatrix(sh, GetShaderLocation(sh, "matLuzPerto"), luzVPPerto_);
        SetShaderValue(sh, GetShaderLocation(sh, "usarSombraPerto"), &um, SHADER_UNIFORM_FLOAT);
    }
}
// Tabuas, pedra e reboco (2048 texels) e as coberturas dos telhados (palha e telhas, 1024 texels)
// desenhados uma vez em texturas que repetem a cada 4 m (mesmas funcoes de antes no shader):
// as construcoes leem a textura em vez de calcular o ruido em todo pixel.
void Iluminacao::assarMateriais() {
    struct Assar { const char* fs; float saida; int tam, mapa; const char* nome; };
    const Assar lista[4] = {
        {FS_ASSAR_MATERIAL, 0, 2048, MATERIAL_MAP_NORMAL, "texAltura"},
        {FS_ASSAR_MATERIAL, 1, 2048, MATERIAL_MAP_ROUGHNESS, "texFator"},
        {FS_ASSAR_MATERIAL, 2, 2048, MATERIAL_MAP_OCCLUSION, "texPeso"},
        {FS_ASSAR_TELHADO, 0, 1024, MATERIAL_MAP_ALBEDO, "texTelhado"},
    };
    Shader paredes = carregarShaderPos(FS_ASSAR_MATERIAL), telhados = carregarShaderPos(FS_ASSAR_TELHADO);
    for (int k = 0; k < 4; k++) {
        const Assar& a = lista[k];
        Shader sh = a.fs == FS_ASSAR_MATERIAL ? paredes : telhados;
        if (sh.id == 0 || sh.id == rlGetShaderIdDefault()) continue;  // sem a textura: o shader le a branca (sem relevo)
        float tam = float(a.tam);
        texMat_[k] = LoadRenderTexture(a.tam, a.tam);
        BeginTextureMode(texMat_[k]);
        ClearBackground(BLANK);
        BeginShaderMode(sh);
        SetShaderValue(sh, GetShaderLocation(sh, "saida"), &a.saida, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, GetShaderLocation(sh, "tamTex"), &tam, SHADER_UNIFORM_FLOAT);
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);  // grava o alfa como esta
        BeginBlendMode(BLEND_CUSTOM);
        DrawRectangle(0, 0, a.tam, a.tam, WHITE);
        EndBlendMode();
        EndShaderMode();
        EndTextureMode();
        GenTextureMipmaps(&texMat_[k].texture);
        SetTextureFilter(texMat_[k].texture, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(texMat_[k].texture, TEXTURE_WRAP_REPEAT);
        matConstr_.maps[a.mapa].texture = texMat_[k].texture;
        litParede_.locs[SHADER_LOC_MAP_DIFFUSE + a.mapa] = GetShaderLocation(litParede_, a.nome);
    }
    UnloadShader(paredes);
    UnloadShader(telhados);
}

void Iluminacao::limparLuzParedes() {
    if (!luzParedeOk_) return;
    for (auto* rt : {&luz1_, &luz2_}) {
        BeginTextureMode(*rt);
        ClearBackground(BLANK);
        EndTextureMode();
    }
    std::fill(feita_.begin(), feita_.end(), 0);
}

void Iluminacao::liberarLuzParedes() {
    if (!luzParedeOk_) return;
    if (paredeOk_) setF(litParede_, "usarAtlas", 0.f);
    for (int m : {MATERIAL_MAP_EMISSION, MATERIAL_MAP_HEIGHT, MATERIAL_MAP_BRDF}) matConstr_.maps[m].texture = Texture2D{};
    UnloadRenderTexture(luz1_);
    UnloadRenderTexture(luz2_);
    UnloadTexture(texFaces_);
    UnloadShader(luzParede_);
    luzParedeOk_ = false;
}

void Iluminacao::prepararLuzParedes(const std::vector<FormaCena>& fs) {
#if !defined(PLATFORM_WEB)
    liberarLuzParedes();
    if (!paredeOk_ || fs.empty() || fs.size() > 16000) return;
    luzParede_ = carregarShaderPos(FS_LUZ_PAREDE);
    if (luzParede_.id == 0 || luzParede_.id == rlGetShaderIdDefault()) return;
    formasLuz_ = &fs;
    atlas_ = alocarAtlas(fs, METRO_TEXEL, TAM_ATLAS);
    // tabela: por forma, 6 texels RGBA float (centro+cos rot, meia+sen rot, 4 retangulos)
    std::vector<float> tab(fs.size() * 6 * 4, 0.f);
    for (size_t i = 0; i < fs.size(); i++) {
        float* t = &tab[i * 24];
        const FormaCena& f = fs[i];
        t[0] = f.centro.x, t[1] = f.centro.y, t[2] = f.centro.z, t[3] = std::cos(f.rotY);
        t[4] = f.meia.x, t[5] = f.meia.y, t[6] = f.meia.z, t[7] = std::sin(f.rotY);
        for (int k = 0; k < 4; k++) {
            const RetAtlas& r = atlas_[i][k];
            t[8 + k * 4] = float(r.x), t[9 + k * 4] = float(r.y), t[10 + k * 4] = float(r.w), t[11 + k * 4] = float(r.h);
        }
    }
    texFaces_.id = rlLoadTexture(tab.data(), 6, int(fs.size()), PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    texFaces_.width = 6, texFaces_.height = int(fs.size()), texFaces_.mipmaps = 1, texFaces_.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
    SetTextureFilter(texFaces_, TEXTURE_FILTER_POINT);
    luz1_ = LoadRenderTexture(TAM_ATLAS, TAM_ATLAS);
    luz2_ = LoadRenderTexture(TAM_ATLAS, TAM_ATLAS);
    for (auto* rt : {&luz1_, &luz2_}) {
        BeginTextureMode(*rt);
        ClearBackground(BLANK);  // alfa 0: "ainda nao calculada"
        EndTextureMode();
        SetTextureFilter(rt->texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(rt->texture, TEXTURE_WRAP_CLAMP);
    }
    matConstr_.maps[MATERIAL_MAP_EMISSION].texture = luz1_.texture;
    matConstr_.maps[MATERIAL_MAP_HEIGHT].texture = texFaces_;
    matConstr_.maps[MATERIAL_MAP_BRDF].texture = luz2_.texture;
    litParede_.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(litParede_, "texLuz1");
    litParede_.locs[SHADER_LOC_MAP_HEIGHT] = GetShaderLocation(litParede_, "texFaces");
    litParede_.locs[SHADER_LOC_MAP_BRDF] = GetShaderLocation(litParede_, "texLuz2");
    float ta = float(TAM_ATLAS), mt = METRO_TEXEL;
    setF(litParede_, "tamAtlas", ta);
    setF(litParede_, "metroTexel", mt);
    setF(luzParede_, "metroTexel", mt);
    setF(litParede_, "usarAtlas", 1.f);
    pertoFace_.assign(fs.size(), {});
    feita_.assign(fs.size() * 4, 0);
    solFace_.assign(fs.size() * 4, Vec3f{});
    corFace_.assign(fs.size() * 4, Vec3f{});
    luzParedeOk_ = true;
#else
    (void)fs;
#endif
}

void Iluminacao::atualizarLuzParedes(Vec3f cam, int porQuadro) {
    if (!luzParedeOk_ || nivel_ == Nivel::Baixo || dbg.semParedes || porQuadro <= 0) return;
    const auto& fs = *formasLuz_;
    // faces no raio de 70 m; as que entram agora vao para a frente da fila
    // luz que faz sombra (como comecarSombra) e cor do sol do quadro
    Vec3f dir = sol_.y > 0.05f ? sol_ : lua_;
    if (dir.y < 0.05f) dir = {0.3f, 0.9f, 0.2f};
    dir = normalizar(dir);
    Rgb cs = paleta_.sol * smoothstep(-0.05f, 0.12f, sol_.y);
    Vec3f csv{cs.r, cs.g, cs.b};
    std::vector<int> novas, saindo;
    std::vector<std::pair<float, int>> fila;  // ja calculadas, pela distancia
    for (size_t i = 0; i < fs.size(); i++) {
        const FormaCena& forma = fs[i];
        if (!forma.parede) continue;
        // Folga que cobre qualquer rotacao horizontal da caixa.
        float limite = 70.f + forma.meia.x + forma.meia.z + 0.01f;
        float d = limite;
        if (std::abs(cam.x - forma.centro.x) < limite && std::abs(cam.z - forma.centro.z) < limite)
            d = distanciaForma(forma, cam);
        bool perto = d < 70.f;
        for (int k = 0; k < 4; k++) {
            int f = int(i) * 4 + k;
            if (atlas_[i][k].w == 0) continue;
            if (!perto) {
                if (feita_[f]) saindo.push_back(f);  // apaga: nao mostra luz velha quando voltar
                feita_[f] = 0;
                continue;
            }
            if (!feita_[f]) { novas.push_back(f); continue; }
            // so refaz se o sol andou (> ~0,3 grau) ou mudou de cor desde o calculo desta face
            Vec3f dc = corFace_[f] - csv;
            if (dot(solFace_[f], dir) < 0.999986f || std::abs(dc.x) + std::abs(dc.y) + std::abs(dc.z) > 0.015f) fila.push_back({d, f});
        }
    }
    if (!saindo.empty()) {
        BeginTextureMode(luz1_);
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
        for (int f : saindo) {
            const RetAtlas& ra = atlas_[f / 4][f % 4];
            DrawRectangle(ra.x, TAM_ATLAS - ra.y - ra.h, ra.w, ra.h, BLANK);
        }
        EndBlendMode();
        EndTextureMode();
    }
    if (novas.empty() && fila.empty()) return;
    // As desatualizadas mais perto da camera primeiro, ate completar o orcamento.
    size_t nNovas = std::min(novas.size(), size_t(porQuadro));
    size_t nRefazer = std::min(fila.size(), size_t(porQuadro) - nNovas);
    if (nRefazer) std::partial_sort(fila.begin(), fila.begin() + nRefazer, fila.end());
    Shader sh = luzParede_;
    float d3[3] = {dir.x, dir.y, dir.z}, s3[3] = {sol_.x, sol_.y, sol_.z}, c3[3] = {cs.r, cs.g, cs.b};
    SetShaderValue(sh, GetShaderLocation(sh, "dirSombra"), d3, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "dirSol"), s3, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "corSol"), c3, SHADER_UNIFORM_VEC3);
    setF(sh, "calcularSombra", usaFiltroSombra() ? 0.f : 1.f);
    static int loc[12] = {-2};
    static unsigned int locShader = 0;
    if (locShader != sh.id) {
        locShader = sh.id;
        const char* nomes[12] = {"formaA", "formaB", "formaCor", "numFormas", "formaCS", "formaR", "faceO", "faceU", "faceN", "faceTam", "faceRet", "saida"};
        for (int i = 0; i < 12; i++) loc[i] = GetShaderLocation(sh, nomes[i]);
    }
    std::vector<int> escolhidas(novas.begin(), novas.begin() + nNovas);
    for (size_t n = 0; n < nRefazer; n++) escolhidas.push_back(fila[n].second);
    int ultimaForma = -1;
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1 && nivel_ != Nivel::Alto) break;  // o rebote so aparece no Alto
        BeginTextureMode(pass == 0 ? luz1_ : luz2_);
        BeginShaderMode(sh);
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
        float saida = float(pass);
        SetShaderValue(sh, loc[11], &saida, SHADER_UNIFORM_FLOAT);
        ultimaForma = -1;
        for (int f : escolhidas) {
            int i = f / 4, k = f % 4;
            if (i != ultimaForma) {  // formas perto desta casa (calculadas uma vez)
                ultimaForma = i;
                auto& pf = pertoFace_[i];
                if (pf.empty()) pf = formasParaLuz(fs, fs[i].centro, fs[i].grupo, 40.f, 64);
                float a[64 * 4] = {0}, b[64 * 4] = {0}, c[64 * 3] = {0}, csn[64 * 2] = {0}, r[64] = {0};
                int nf = int(pf.size());
                for (int q = 0; q < nf; q++) {
                    const FormaCena& g = fs[pf[q]];
                    a[q * 4] = g.centro.x, a[q * 4 + 1] = g.centro.y, a[q * 4 + 2] = g.centro.z, a[q * 4 + 3] = g.rotY;
                    b[q * 4] = g.meia.x, b[q * 4 + 1] = g.meia.y, b[q * 4 + 2] = g.meia.z, b[q * 4 + 3] = float(int(g.tipo));
                    c[q * 3] = g.cor.r, c[q * 3 + 1] = g.cor.g, c[q * 3 + 2] = g.cor.b;
                    csn[q * 2] = std::cos(g.rotY), csn[q * 2 + 1] = std::sin(g.rotY);
                    r[q] = len(g.meia);
                }
                if (nf) {
                    SetShaderValueV(sh, loc[0], a, SHADER_UNIFORM_VEC4, nf);
                    SetShaderValueV(sh, loc[1], b, SHADER_UNIFORM_VEC4, nf);
                    SetShaderValueV(sh, loc[2], c, SHADER_UNIFORM_VEC3, nf);
                    SetShaderValueV(sh, loc[4], csn, SHADER_UNIFORM_VEC2, nf);
                    SetShaderValueV(sh, loc[5], r, SHADER_UNIFORM_FLOAT, nf);
                }
                SetShaderValue(sh, loc[3], &nf, SHADER_UNIFORM_INT);
            }
            FaceParede fc = faceDe(fs[i], k);
            const RetAtlas& ra = atlas_[i][k];
            float o3[3] = {fc.origem.x, fc.origem.y, fc.origem.z}, u3[3] = {fc.eixoU.x, fc.eixoU.y, fc.eixoU.z}, n3[3] = {fc.normal.x, fc.normal.y, fc.normal.z};
            float tam[2] = {fc.largura, fc.altura}, ret[4] = {float(ra.x), float(ra.y), float(ra.w), float(ra.h)};
            SetShaderValue(sh, loc[6], o3, SHADER_UNIFORM_VEC3);
            SetShaderValue(sh, loc[7], u3, SHADER_UNIFORM_VEC3);
            SetShaderValue(sh, loc[8], n3, SHADER_UNIFORM_VEC3);
            SetShaderValue(sh, loc[9], tam, SHADER_UNIFORM_VEC2);
            SetShaderValue(sh, loc[10], ret, SHADER_UNIFORM_VEC4);
            // raylib desenha com y para baixo; no atlas a linha 0 e a de baixo (coordenada de textura)
            DrawRectangle(ra.x, TAM_ATLAS - ra.y - ra.h, ra.w, ra.h, WHITE);
            rlDrawRenderBatchActive();  // cada face com seus uniformes
        }
        EndBlendMode();
        EndShaderMode();
        EndTextureMode();
    }
    for (int f : escolhidas) {
        feita_[f] = 1;
        solFace_[f] = dir;
        corFace_[f] = csv;
    }
}

bool Iluminacao::usaCabecaRaio() const { return cabecaOk_ && nivel_ != Nivel::Baixo && !dbg.semCabecaRaio; }
void Iluminacao::definirHeroi(bool sim) {
    for (Shader sh : personagens_) setF(sh, "heroi", sim && !dbg.semLuzPersonagem ? 1.f : 0.f);
}
void Iluminacao::definirFoco(float d) { distFoco_ = d; }
void Iluminacao::definirInteriores(const std::vector<InteriorMundo>& is, const std::vector<PortaMundo>& ps, Vec3f foco) {
#if !defined(PLATFORM_WEB)
    std::vector<std::pair<float, int>> ord;
    for (int i = 0; i < int(is.size()); i++) {
        float d = std::hypot(is[i].centro.x - foco.x, is[i].centro.z - foco.z);
        if (d < 80.f) ord.push_back({d, i});
    }
    int n = std::min<int>(16, int(ord.size()));
    std::partial_sort(ord.begin(), ord.begin() + n, ord.end());
    float a[16 * 4], b[16 * 4], c[16 * 4];
    float limite = std::numeric_limits<float>::max();
    Vec3f minimo{limite, limite, limite}, maximo{-limite, -limite, -limite};
    for (int k = 0; k < n; k++) {
        const InteriorMundo& it = is[ord[k].second];
        float cosRot = std::cos(it.rot), senRot = std::sin(it.rot);
        float ab = ord[k].second < int(ps.size()) ? ps[ord[k].second].abertura : 0.f;  // a porta i e a do interior i
        a[k * 4] = it.centro.x, a[k * 4 + 1] = it.centro.y, a[k * 4 + 2] = it.centro.z, a[k * 4 + 3] = cosRot;
        b[k * 4] = it.meia.x, b[k * 4 + 1] = it.meia.y, b[k * 4 + 2] = it.meia.z, b[k * 4 + 3] = ab;
        c[k * 4] = it.porta.x, c[k * 4 + 1] = it.porta.y, c[k * 4 + 2] = it.porta.z, c[k * 4 + 3] = senRot;

        // A caixa envolve os volumes girados, incluindo as folgas do shader.
        Vec3f m = it.meia + Vec3f{0.08f, 0.08f, 0.08f};
        float dx = std::abs(cosRot) * m.x + std::abs(senRot) * m.z + 0.01f;
        float dz = std::abs(senRot) * m.x + std::abs(cosRot) * m.z + 0.01f;
        minimo.x = std::min(minimo.x, it.centro.x - dx);
        minimo.y = std::min(minimo.y, it.centro.y - m.y - 0.31f);
        minimo.z = std::min(minimo.z, it.centro.z - dz);
        maximo.x = std::max(maximo.x, it.centro.x + dx);
        maximo.y = std::max(maximo.y, it.centro.y + m.y + 0.01f);
        maximo.z = std::max(maximo.z, it.centro.z + dz);
    }
    for (Shader sh : sombreados_) {
        if (n) {
            SetShaderValueV(sh, GetShaderLocation(sh, "interiorA"), a, SHADER_UNIFORM_VEC4, n);
            SetShaderValueV(sh, GetShaderLocation(sh, "interiorB"), b, SHADER_UNIFORM_VEC4, n);
            SetShaderValueV(sh, GetShaderLocation(sh, "interiorP"), c, SHADER_UNIFORM_VEC4, n);
            setV3(sh, "interioresMin", minimo);
            setV3(sh, "interioresMax", maximo);
        }
        SetShaderValue(sh, GetShaderLocation(sh, "numInteriores"), &n, SHADER_UNIFORM_INT);
    }
#else
    (void)is, (void)ps, (void)foco;
#endif
}
void Iluminacao::definirJanelas(const std::vector<JanelaLuz>& js, Vec3f foco) {
#if !defined(PLATFORM_WEB)
    if (!paredeOk_ || nivel_ == Nivel::Baixo || dbg.semParedes || noite_ <= 0.f) return;
    // as 12 janelas mais proximas do foco
    std::vector<std::pair<float, int>> ord;
    ord.reserve(64);
    for (int i = 0; i < int(js.size()); i++) {
        Vec3f d = js[i].pos - foco;
        float d2 = dot(d, d);
        if (d2 < 60.f * 60.f) ord.push_back({d2, i});
    }
    int nj = std::min<int>(12, int(ord.size()));
    std::partial_sort(ord.begin(), ord.begin() + nj, ord.end());
    float jp[12 * 3], jn[12 * 3];
    for (int k = 0; k < nj; k++) {
        const JanelaLuz& j = js[ord[k].second];
        jp[k * 3] = j.pos.x, jp[k * 3 + 1] = j.pos.y, jp[k * 3 + 2] = j.pos.z;
        jn[k * 3] = j.normal.x, jn[k * 3 + 1] = j.normal.y, jn[k * 3 + 2] = j.normal.z;
    }
    if (nj) {
        SetShaderValueV(litParede_, locJanelaP_, jp, SHADER_UNIFORM_VEC3, nj);
        SetShaderValueV(litParede_, locJanelaN_, jn, SHADER_UNIFORM_VEC3, nj);
    }
    SetShaderValue(litParede_, locNumJanelas_, &nj, SHADER_UNIFORM_INT);
#else
    (void)js, (void)foco;
#endif
}
void Iluminacao::definirRaioGrama(float r) { setF(lit_, "raioGrama", r); }
void Iluminacao::definirCabelo(const Material& mat, const DeformacaoCabelo& deformacao, float escala) {
    const auto& loc = locCabelo_[mat.shader.id == profPele_.id ? 1 : 0];
    SetShaderValue(mat.shader, loc.escala, &escala, SHADER_UNIFORM_FLOAT);
    if (escala == 0.f) return;
    float meio[3][3], ponta[3][3];
    for (size_t i = 0; i < deformacao.meio.size(); ++i) {
        Vec3f m = deformacao.meio[i], p = deformacao.ponta[i];
        meio[i][0] = m.x; meio[i][1] = m.y; meio[i][2] = m.z;
        ponta[i][0] = p.x; ponta[i][1] = p.y; ponta[i][2] = p.z;
    }
    SetShaderValueV(mat.shader, loc.meio, meio, SHADER_UNIFORM_VEC3, 3);
    SetShaderValueV(mat.shader, loc.ponta, ponta, SHADER_UNIFORM_VEC3, 3);
}
