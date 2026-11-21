#include "render/shaders.h"
#include <string>

#if defined(PLATFORM_WEB)
static const char* CAB_VS = "#version 100\n#define ATTR attribute\n#define VARY varying\n";
static const char* CAB_FS = "#version 100\nprecision highp float;\n#define VARY varying\n#define FRAG gl_FragColor\n#define TEX texture2D\n";
#else
static const char* CAB_VS = "#version 330\n#define ATTR in\n#define VARY out\n";
static const char* CAB_FS = "#version 330\n#define VARY in\nout vec4 corSaida;\n#define FRAG corSaida\n#define TEX texture\n";
#endif

// Funcoes comuns: tone mapping (ACES), grading e neblina.
static const char* COMUM = R"(
uniform vec3 corNeblina;
uniform vec3 corSol;
uniform vec3 dirSol;
uniform float densNeblina;
uniform float distVisao;
uniform float exposicao;
uniform vec3 grading;

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
vec3 finalizar(vec3 c) {
    c = aces(c * exposicao) * grading;
    return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
}
vec3 neblinaCor(vec3 dirVista) {
    float s = max(dot(dirVista, dirSol), 0.0);
    return corNeblina + corSol * 0.18 * pow(s, 6.0);
}
float neblinaFator(float d) {
    return clamp(1.0 - exp(-pow(d * densNeblina, 2.0)), 0.0, 1.0);
}
)";

Shader carregarShader(const char* vs, const char* fs) {
    std::string v = std::string(CAB_VS) + vs;
    std::string f = std::string(CAB_FS) + COMUM + fs;
    return LoadShaderFromMemory(v.c_str(), f.c_str());
}

const char* VS_LIT = R"(
ATTR vec3 vertexPosition;
ATTR vec2 vertexTexCoord;
ATTR vec3 vertexNormal;
ATTR vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
VARY vec3 posMundo;
VARY vec3 normalMundo;
VARY vec4 cor;
VARY vec2 extra;
void main() {
    vec4 p = matModel * vec4(vertexPosition, 1.0);
    posMundo = p.xyz;
    normalMundo = normalize(mat3(matModel) * vertexNormal);
    cor = vertexColor;
    extra = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* FS_LIT = R"(
VARY vec3 posMundo;
VARY vec3 normalMundo;
VARY vec4 cor;
VARY vec2 extra;   // x = oclusao de ambiente, y = 1 para agua
uniform vec3 posCamera;
uniform vec3 ambCeu;
uniform vec3 ambChao;
uniform vec3 dirLua;
uniform vec3 corLua;
uniform vec4 luzPos[8];   // xyz + raio
uniform vec3 luzCor[8];
uniform int numLuzes;
uniform float descartarPerto; // > 0: horizonte, some onde o terreno detalhado ja desenha

void main() {
    vec3 n = normalize(normalMundo);
    vec3 albedo = pow(cor.rgb, vec3(2.2));
    float ao = extra.x;
    vec3 paraCam = posCamera - posMundo;
    float dist = length(paraCam);
    vec3 v = paraCam / max(dist, 0.001);
    if (descartarPerto > 0.0 && length(paraCam.xz) < descartarPerto) discard;

    // ceu e chao (hemisferica) com oclusao de ambiente
    float hemi = n.y * 0.5 + 0.5;
    vec3 luz = mix(ambChao, ambCeu, hemi) * ao;
    // sol e lua
    luz += corSol * max(dot(n, dirSol), 0.0);
    luz += corLua * max(dot(n, dirLua), 0.0);
    // tochas, fogueiras, lanterna
    for (int i = 0; i < 8; i++) {
        if (i >= numLuzes) break;
        vec3 d = luzPos[i].xyz - posMundo;
        float r = luzPos[i].w;
        float dd = length(d);
        float at = clamp(1.0 - (dd * dd) / (r * r), 0.0, 1.0);
        at = at * at / (1.0 + dd * 0.35);
        float lamb = max(dot(n, d / max(dd, 0.001)), 0.0) * 0.8 + 0.2;
        luz += luzCor[i] * at * lamb * mix(1.0, ao, 0.5);
    }
    vec3 c = albedo * luz;

    if (extra.y > 0.5) {
        // agua: reflete o ceu (fresnel) + brilho do sol
        vec3 r = reflect(-v, n);
        float fres = pow(1.0 - max(dot(n, v), 0.0), 4.0) * 0.8 + 0.1;
        vec3 ceu = mix(corNeblina, ambCeu * 2.2, clamp(r.y * 2.0, 0.0, 1.0));
        c = mix(c, ceu, fres);
        c += corSol * pow(max(dot(r, dirSol), 0.0), 120.0) * 1.5;
        c += corLua * pow(max(dot(r, dirLua), 0.0), 80.0) * 1.0;
    }

    float f = neblinaFator(dist);
    c = mix(c, neblinaCor(-v), f);
    FRAG = vec4(finalizar(c), 1.0);
}
)";

const char* VS_CEU = R"(
ATTR vec3 vertexPosition;
uniform mat4 mvp;
VARY vec3 dirLocal;
void main() {
    dirLocal = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* FS_CEU = R"(
VARY vec3 dirLocal;
uniform vec3 ceuTopo;
uniform vec3 ceuHorizonte;
uniform vec3 dirLua;
uniform float noite;
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
void main() {
    vec3 d = normalize(dirLocal);
    float h = max(d.y, 0.0);
    vec3 c = mix(ceuHorizonte, ceuTopo, pow(h, 0.55));
    // brilho e disco do sol
    float s = max(dot(d, dirSol), 0.0);
    c += corSol * (pow(s, 8.0) * 0.12 + pow(s, 200.0) * 0.6 + smoothstep(0.9993, 0.9996, s) * 6.0);
    // lua
    float l = max(dot(d, dirLua), 0.0);
    c += vec3(0.7, 0.75, 0.9) * (smoothstep(0.9990, 0.9993, l) * 1.6 + pow(l, 60.0) * 0.05) * noite;
    // estrelas
    vec3 q = floor(d * 280.0);
    float e = step(0.9965, hash(q)) * smoothstep(0.0, 0.25, d.y);
    c += vec3(e) * 0.9 * noite;
    // abaixo do horizonte: neblina
    float abaixo = smoothstep(0.02, -0.12, d.y);
    c = mix(c, corNeblina, max(abaixo, smoothstep(0.25, 0.0, d.y) * 0.65));
    FRAG = vec4(finalizar(c), 1.0);
}
)";
