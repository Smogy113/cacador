#include "render/lighting.h"
#include <algorithm>
#include <cmath>
#include "core/noise.h"
#include "render/shaders.h"
#include "raymath.h"
#include "rlgl.h"

namespace {
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
        for (Shader sh : personagens()) SetShaderValue(sh, GetShaderLocation(sh, "usarSombraPerto"), &zero, SHADER_UNIFORM_FLOAT);
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
    mat_.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
    float zero = 0;
    for (Shader sh : sombreados()) SetShaderValue(sh, GetShaderLocation(sh, "usarSombra"), &zero, SHADER_UNIFORM_FLOAT);
    matPele_.maps[MATERIAL_MAP_SPECULAR].texture = matCabeca_.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
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
    } else {
        lit_ = carregarShader(VS_LIT, FS_LIT);
    lit_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(lit_, "matModel");
    ceu_ = carregarShader(VS_CEU, FS_CEU);
    mat_ = LoadMaterialDefault();
    mat_.shader = lit_;
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
    lit_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(lit_, "texSombra");
    pele_ = carregarShaderDef(VS_PELE, FS_LIT, "#define PERSONAGEM\n");
    pele_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(pele_, "matModel");
    pele_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(pele_, "texSombra");
    pele_.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(pele_, "texSombraPerto");
    matPele_ = LoadMaterialDefault();
    matPele_.shader = pele_;
#if !defined(PLATFORM_WEB)
    cabeca_ = carregarShaderDef(VS_CABECA, FS_LIT, "#define PERSONAGEM\n#define RAYMARCH\n");
    cabecaOk_ = cabeca_.id > 0 && cabeca_.id != rlGetShaderIdDefault();
    if (cabecaOk_) {
        cabeca_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(cabeca_, "matModel");
        cabeca_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(cabeca_, "texSombra");
        cabeca_.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(cabeca_, "texSombraPerto");
        cabeca_.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(cabeca_, "texDist");
        cabeca_.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(cabeca_, "texCor");
        matCabeca_ = LoadMaterialDefault();
        matCabeca_.shader = cabeca_;
    }
#endif
    profPele_ = carregarShader(VS_PROF_PELE, FS_PROF);
    matProfPele_ = LoadMaterialDefault();
    matProfPele_.shader = profPele_;
    junta_.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(junta_, "texBrilho");
    carregado_ = true;
    }
    if (ef_.sombras) {
        sombraRT_ = criarMapaSombra(ef_.resolucaoSombra);
        sombraOk_ = sombraRT_.id > 0;
        sombraPertoRT_ = criarMapaSombra(1024);
        sombraPertoOk_ = sombraPertoRT_.id > 0;
        if (sombraPertoOk_) {
            matPele_.maps[MATERIAL_MAP_ROUGHNESS].texture = matCabeca_.maps[MATERIAL_MAP_ROUGHNESS].texture = sombraPertoRT_.depth;
            float t = 1.f / 1024.f;
            for (Shader sh : personagens()) setF(sh, "texelSombraPerto", t);
        }
        if (sombraOk_) {
            mat_.maps[MATERIAL_MAP_SPECULAR].texture = sombraRT_.depth;
            matPele_.maps[MATERIAL_MAP_SPECULAR].texture = matCabeca_.maps[MATERIAL_MAP_SPECULAR].texture = sombraRT_.depth;
            float texel = 1.f / ef_.resolucaoSombra;
            setF(lit_, "texelSombra", texel);
            for (Shader sh : personagens()) setF(sh, "texelSombra", texel);
            float tm = 2 * AREA_SOMBRA / ef_.resolucaoSombra;
            setF(lit_, "texelMetros", tm);
            for (Shader sh : personagens()) setF(sh, "texelMetros", tm);
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
    for (Shader sh : {prof_, brilho_, blur_, raios_, junta_, pele_, profPele_, ssao_}) UnloadShader(sh);
    if (cabecaOk_) {
        for (int i = 0; i < 12; i++) matCabeca_.maps[i].texture = Texture2D{};  // as texturas sao de outros donos
        UnloadShader(cabeca_);
        cabecaOk_ = false;
    }
    esfera_.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = Texture2D{};
    UnloadModel(esfera_);
    UnloadTexture(ruidoNuvem_);
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
    setF(s, "exposicao", p.exposicao * dbg.exposicao);
    if (dbg.semNeblina) setF(s, "densNeblina", 0.f);
    setRgb(s, "grading", p.grading);
    setV3(s, "dirLua", lua_);
    (void)noite;
}

void Iluminacao::comecarCena(const Camera3D& cam, const DayClock& relogio, const std::vector<LuzPontual>& todas, Vec3f foco, float tempo) {
    paleta_ = relogio.paleta();
    sol_ = relogio.direcaoSol();
    lua_ = relogio.direcaoLua();
    noite_ = relogio.intensidadeTochas();
    for (Shader sh : sombreados()) {
        uniformesComuns(sh, paleta_, sol_, noite_);
        setV3(sh, "posCamera", {cam.position.x, cam.position.y, cam.position.z});
        setF(sh, "tempoVento", tempo);
        // neblina baixa: forte com o sol baixo, fraca ao meio-dia, media a noite
        setF(sh, "neblinaBaixa", sol_.y > 0.f ? 0.25f + 0.85f * (1.f - smoothstep(0.05f, 0.45f, sol_.y)) : 0.5f);
        setRgb(sh, "ambCeu", paleta_.ambienteCeu);
        setRgb(sh, "ambChao", paleta_.ambienteChao);
        float luaAcima = smoothstep(-0.05f, 0.15f, lua_.y);
        setRgb(sh, "corLua", Rgb{0.10f, 0.13f, 0.22f} * (luaAcima * noite_));
        setF(sh, "noite", noite_);
        setRgb(sh, "corChao", Rgb{0.10f, 0.15f, 0.05f});

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
        SetShaderValueV(sh, GetShaderLocation(sh, "luzPos"), pos, SHADER_UNIFORM_VEC4, MAX_LUZES);
        SetShaderValueV(sh, GetShaderLocation(sh, "luzCor"), cor, SHADER_UNIFORM_VEC3, MAX_LUZES);
        SetShaderValue(sh, GetShaderLocation(sh, "numLuzes"), &ni, SHADER_UNIFORM_INT);
    }
    uniformesComuns(ceu_, paleta_, sol_, noite_);
    setRgb(ceu_, "ceuTopo", paleta_.ceuTopo);
    setRgb(ceu_, "ceuHorizonte", paleta_.ceuHorizonte);
    setF(ceu_, "noite", noite_);
    setV3(ceu_, "posCamera", {cam.position.x, cam.position.y, cam.position.z});
    setF(ceu_, "tempoVento", tempo);
    float pn = nivel_ == Nivel::Alto ? 22.f : (nivel_ == Nivel::Medio ? 12.f : 0.f);
    if (dbg.passosNuvem >= 0) pn = dbg.passosNuvem;
    setF(ceu_, "passosNuvem", dbg.semNuvens ? -1.f : pn);
}

void Iluminacao::modoHorizonte(float d) { setF(lit_, "descartarPerto", d); }

void Iluminacao::modoEstudio(Vec3f dir, bool argila) {
    for (Shader sh : sombreados()) {
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
    for (Shader sh : personagens()) setF(sh, "argila", argila ? 1.f : 0.f);
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
    setV3(lit_, "dirSombra", dir);
    for (Shader sh : personagens()) setV3(sh, "dirSombra", dir);
    BeginTextureMode(sombraRT_);
    ClearBackground(WHITE);
    rlClearScreenBuffers();
    BeginMode3D(cl);
    luzVP_ = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

void Iluminacao::terminarSombra() {
    EndMode3D();
    EndTextureMode();
    float um = 1;
    for (Shader sh : sombreados()) {
        SetShaderValueMatrix(sh, GetShaderLocation(sh, "matLuz"), luzVP_);
        SetShaderValue(sh, GetShaderLocation(sh, "usarSombra"), &um, SHADER_UNIFORM_FLOAT);
    }
}

void Iluminacao::comecarPos() {
    if (GetScreenWidth() != largPos_ || GetScreenHeight() != altPos_) carregar(nivel_);  // janela mudou de tamanho
    BeginTextureMode(cena_);
}

static void desenharTela(RenderTexture2D& origem, int w, int h) {
    DrawTexturePro(origem.texture, {0, 0, float(origem.texture.width), float(-origem.texture.height)}, {0, 0, float(w), float(h)}, {0, 0}, 0, WHITE);
}

void Iluminacao::terminarPos(const Camera3D& cam) {
    EndTextureMode();
    int mw = largPos_ / 2, mh = altPos_ / 2;
    // 1) partes claras da cena (meia resolucao)
    BeginTextureMode(meiaA_);
    BeginShaderMode(brilho_);
    float limiar = 0.85f;
    SetShaderValue(brilho_, GetShaderLocation(brilho_, "limiar"), &limiar, SHADER_UNIFORM_FLOAT);
    desenharTela(cena_, mw, mh);
    EndShaderMode();
    EndTextureMode();
    // 2) raios de luz a partir do sol
    bool raios = false;
    if (ef_.raiosDeLuz && !dbg.semRaios && sol_.y > -0.02f) {
        Vector3 ps{cam.position.x + sol_.x * 300, cam.position.y + sol_.y * 300, cam.position.z + sol_.z * 300};
        Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
        float olhaSol = Vector3DotProduct(fwd, {sol_.x, sol_.y, sol_.z});
        if (olhaSol > 0.05f) {
            Vector2 t = GetWorldToScreenEx(ps, cam, largPos_, altPos_);
            posSolTela_[0] = t.x / largPos_;
            posSolTela_[1] = 1.f - t.y / altPos_;
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
    }
    // 3) desfoca o brilho (horizontal e vertical, 2 vezes)
    for (int k = 0; k < 2 && ef_.bloom && !dbg.semBloom; k++) {
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
    // 3b) oclusao de ambiente em tela (meia resolucao, desfocada)
    bool ao = ef_.ssao && cenaComProfundidade_ && !dbg.semSSAO;
    if (ao) {
        BeginTextureMode(aoA_);
        BeginShaderMode(ssao_);
        float tx[2] = {1.f / largPos_, 1.f / altPos_};
        float escala = altPos_ / (2.f * std::tan(cam.fovy * 0.5f * DEG2RAD));
        SetShaderValue(ssao_, GetShaderLocation(ssao_, "texel"), tx, SHADER_UNIFORM_VEC2);
        SetShaderValue(ssao_, GetShaderLocation(ssao_, "escalaProj"), &escala, SHADER_UNIFORM_FLOAT);
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
            BeginShaderMode(blur_);
            SetShaderValue(blur_, GetShaderLocation(blur_, "direcao"), d, SHADER_UNIFORM_VEC2);
            DrawTexturePro(de.texture, {0, 0, float(mw), float(mh)}, {0, 0, float(mw), float(mh)}, {0, 0}, 0, WHITE);
            EndShaderMode();
            EndTextureMode();
        }
    }
    // 3c) camera de cinema (Alto): copia desfocada da cena para a profundidade de campo
    bool cinema = nivel_ == Nivel::Alto && cenaComProfundidade_;
    bool foco = cinema && !dbg.semFoco;
    if (foco) {
        BeginTextureMode(focoA_);
        desenharTela(cena_, mw, mh);
        EndTextureMode();
        for (int k = 0; k < 4; k++) {
            float esp = k < 2 ? 1.f : 2.2f;
            float d[2] = {k % 2 == 0 ? esp / mw : 0.f, k % 2 == 0 ? 0.f : esp / mh};
            RenderTexture2D& de = k % 2 == 0 ? focoA_ : focoB_;
            RenderTexture2D& para = k % 2 == 0 ? focoB_ : focoA_;
            BeginTextureMode(para);
            BeginShaderMode(blur_);
            SetShaderValue(blur_, GetShaderLocation(blur_, "direcao"), d, SHADER_UNIFORM_VEC2);
            DrawTexturePro(de.texture, {0, 0, float(mw), float(mh)}, {0, 0, float(mw), float(mh)}, {0, 0}, 0, WHITE);
            EndShaderMode();
            EndTextureMode();
        }
    }
    // 4) junta tudo na tela
    BeginShaderMode(junta_);
    {
        float uf = foco ? 1.f : 0.f, ug = cinema && !dbg.semGrao ? 1.f : 0.f, t = float(GetTime());
        // mais desfoque com a camera perto do heroi (close, mira); de longe quase nada
        float forca = 0.35f + 0.5f * (1.f - smoothstep(1.2f, 4.5f, distFoco_));
        float tx[2] = {1.f / largPos_, 1.f / altPos_};
        SetShaderValue(junta_, GetShaderLocation(junta_, "usarFoco"), &uf, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "usarGrao"), &ug, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "distFoco"), &distFoco_, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "forcaFoco"), &forca, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "tempo"), &t, SHADER_UNIFORM_FLOAT);
        SetShaderValue(junta_, GetShaderLocation(junta_, "texelTela"), tx, SHADER_UNIFORM_VEC2);
        if (foco) {
            SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texProf"), cena_.depth);
            SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texDesfoque"), focoA_.texture);
        }
    }
    float fb = ef_.bloom && !dbg.semBloom ? 0.65f : 0.f, ur = raios ? 1.f : 0.f;
    float aspJ = float(largPos_) / float(altPos_);
    Vector3 crJ{paleta_.sol.r, paleta_.sol.g, paleta_.sol.b};
    SetShaderValue(junta_, GetShaderLocation(junta_, "posSol"), posSolTela_, SHADER_UNIFORM_VEC2);
    SetShaderValue(junta_, GetShaderLocation(junta_, "aspecto"), &aspJ, SHADER_UNIFORM_FLOAT);
    SetShaderValue(junta_, GetShaderLocation(junta_, "corRaio"), &crJ, SHADER_UNIFORM_VEC3);
    SetShaderValue(junta_, GetShaderLocation(junta_, "forcaBrilho"), &fb, SHADER_UNIFORM_FLOAT);
    SetShaderValue(junta_, GetShaderLocation(junta_, "usarRaios"), &ur, SHADER_UNIFORM_FLOAT);
    SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texBrilho"), meiaA_.texture);
    SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texRaios"), raiosRT_.texture);
    float ua = ao ? 1.f : 0.f;
    SetShaderValue(junta_, GetShaderLocation(junta_, "usarAO"), &ua, SHADER_UNIFORM_FLOAT);
    if (ao) SetShaderValueTexture(junta_, GetShaderLocation(junta_, "texAO"), aoA_.texture);
    desenharTela(cena_, GetScreenWidth(), GetScreenHeight());
    EndShaderMode();
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
    rlClearScreenBuffers();
    BeginMode3D(cl);
    luzVPPerto_ = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

void Iluminacao::terminarSombraPerto() {
    EndMode3D();
    EndTextureMode();
    float um = 1;
    for (Shader sh : personagens()) {
        SetShaderValueMatrix(sh, GetShaderLocation(sh, "matLuzPerto"), luzVPPerto_);
        SetShaderValue(sh, GetShaderLocation(sh, "usarSombraPerto"), &um, SHADER_UNIFORM_FLOAT);
    }
}
std::vector<Shader> Iluminacao::personagens() const {
    std::vector<Shader> v{pele_};
    if (cabecaOk_) v.push_back(cabeca_);
    return v;
}
std::vector<Shader> Iluminacao::sombreados() const {
    std::vector<Shader> v{lit_, pele_};
    if (cabecaOk_) v.push_back(cabeca_);
    return v;
}
bool Iluminacao::usaCabecaRaio() const { return cabecaOk_ && nivel_ != Nivel::Baixo && !dbg.semCabecaRaio; }
void Iluminacao::definirHeroi(bool sim) {
    for (Shader sh : personagens()) setF(sh, "heroi", sim && !dbg.semLuzPersonagem ? 1.f : 0.f);
}
void Iluminacao::definirFoco(float d) { distFoco_ = d; }
void Iluminacao::definirRaioGrama(float r) { setF(lit_, "raioGrama", r); }
void Iluminacao::definirCabelo(Vec3f d) { SetShaderValue(pele_, GetShaderLocation(pele_, "cabeloDesl"), &d, SHADER_UNIFORM_VEC3); }
