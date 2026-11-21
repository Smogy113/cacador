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

Shader carregarShaderPos(const char* fs) {
#if defined(PLATFORM_WEB)
    std::string f = std::string("#version 100\nprecision mediump float;\nvarying vec2 fragTexCoord;\nvarying vec4 fragColor;\n#define FRAG gl_FragColor\n#define TEX texture2D\n") + fs;
#else
    std::string f = std::string("#version 330\nin vec2 fragTexCoord;\nin vec4 fragColor;\nout vec4 corSaida;\n#define FRAG corSaida\n#define TEX texture\n") + fs;
#endif
    return LoadShaderFromMemory(nullptr, f.c_str());
}

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
VARY vec2 extra;   // x = oclusao de ambiente, y = tipo (1 agua, 2 chama, 3 janela)
uniform vec3 posCamera;
uniform vec3 ambCeu;
uniform vec3 ambChao;
uniform vec3 dirLua;
uniform vec3 corLua;
uniform vec4 luzPos[8];   // xyz + raio
uniform vec3 luzCor[8];
uniform int numLuzes;
uniform float noite;
uniform float descartarPerto;
uniform float usarSombra;
uniform mat4 matLuz;
uniform sampler2D texSombra;
uniform float texelSombra;
uniform vec3 dirSombra;

uniform float texelMetros;  // tamanho de um texel do mapa de sombras, em metros
uniform vec3 corChao;       // cor media do chao (luz rebatida)

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Sombra suave (PCSS simplificado): procura quem bloqueia a luz e borra mais
// quanto mais longe o bloqueador esta do chao. Perto do pe: nitida; na ponta: borrada.
float sombraEm(vec3 p, vec3 n) {
    if (usarSombra < 0.5) return 1.0;
    float nl = clamp(dot(n, dirSombra), 0.0, 1.0);
    if (nl <= 0.0) return 0.0;
    vec4 pl = matLuz * vec4(p + n * (0.05 + 0.2 * (1.0 - nl)), 1.0);
    vec3 q = pl.xyz / pl.w * 0.5 + 0.5;
    if (q.x <= 0.0 || q.x >= 1.0 || q.y <= 0.0 || q.y >= 1.0 || q.z >= 1.0) return 1.0;
    float bias = 0.00012 + 0.0005 * (1.0 - nl);
    float giro = hash12(p.xz * 7.13) * 6.2831;
    float soma = 0.0, cont = 0.0;
    for (int i = 0; i < 8; i++) {
        float a = float(i) * 2.39996 + giro;
        float r = sqrt((float(i) + 0.5) / 8.0) * 5.0;
        float d = TEX(texSombra, q.xy + vec2(cos(a), sin(a)) * r * texelSombra).r;
        if (d < q.z - bias) { soma += d; cont += 1.0; }
    }
    if (cont < 0.5) return 1.0;
    float distBloq = (q.z - soma / cont) * 1000.0;
    float raio = clamp(distBloq * 0.045 / texelMetros, 0.8, 7.0);
    float s = 0.0;
    for (int i = 0; i < 12; i++) {
        float a = float(i) * 2.39996 + giro;
        float r = sqrt((float(i) + 0.5) / 12.0) * raio;
        float d = TEX(texSombra, q.xy + vec2(cos(a), sin(a)) * r * texelSombra).r;
        s += (q.z - bias > d) ? 0.0 : 1.0;
    }
    s /= 12.0;
    vec2 b = smoothstep(vec2(0.0), vec2(0.08), q.xy) * smoothstep(vec2(1.0), vec2(0.92), q.xy);
    return mix(1.0, s, b.x * b.y);
} // > 0: horizonte, some onde o terreno detalhado ja desenha

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
    // sol e lua (com sombra)
    float sombra = sombraEm(posMundo, n);
    luz += corSol * max(dot(n, dirSol), 0.0) * sombra;
    luz += corLua * max(dot(n, dirLua), 0.0) * sombra;
    // luz rebatida: o chao iluminado pelo sol devolve luz colorida para cima
    float solNoChao = max(dirSol.y, 0.0);
    luz += corSol * corChao * solNoChao * (0.5 - 0.5 * n.y) * 0.9 * ao;
    // contorno: o ceu "abraca" as bordas (luz indireta)
    float borda = pow(1.0 - max(dot(n, v), 0.0), 3.0);
    luz += ambCeu * borda * 0.6 * ao;
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
    if (extra.y > 2.5) {
        // janela: vidro escuro de dia, luz quente a noite
        vec3 vidro = vec3(0.03, 0.035, 0.045) * luz + vec3(0.02);
        c = mix(vidro, albedo * 3.2, noite);
    } else if (extra.y > 1.5) {
        if (noite < 0.05) discard;  // tochas apagadas de dia
        c = albedo * 6.0 * noite;  // chama
    }

    if (extra.y > 0.5 && extra.y < 1.5) {
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

// ---------- passe de profundidade (mapa de sombras) ----------
const char* VS_PROF = R"(
ATTR vec3 vertexPosition;
ATTR vec2 vertexTexCoord;
uniform mat4 mvp;
VARY float tipo;
void main() {
    tipo = vertexTexCoord.y;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";
const char* FS_PROF = R"(
VARY float tipo;
void main() {
    if (tipo > 1.5 && tipo < 2.5) discard;  // chamas nao fazem sombra
    FRAG = vec4(1.0);
}
)";

// ---------- pos-processamento ----------
const char* FS_BRILHO = R"(
uniform sampler2D texture0;
uniform float limiar;
void main() {
    vec3 c = TEX(texture0, fragTexCoord).rgb;
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    FRAG = vec4(c * smoothstep(limiar, limiar + 0.08, l), 1.0);
}
)";
const char* FS_BLUR = R"(
uniform sampler2D texture0;
uniform vec2 direcao;  // (1/largura, 0) ou (0, 1/altura)
void main() {
    vec3 c = TEX(texture0, fragTexCoord).rgb * 0.227;
    c += TEX(texture0, fragTexCoord + direcao * 1.38).rgb * 0.316;
    c += TEX(texture0, fragTexCoord - direcao * 1.38).rgb * 0.316;
    c += TEX(texture0, fragTexCoord + direcao * 3.23).rgb * 0.070;
    c += TEX(texture0, fragTexCoord - direcao * 3.23).rgb * 0.070;
    FRAG = vec4(c, 1.0);
}
)";
const char* FS_RAIOS = R"(
uniform sampler2D texture0;
uniform vec2 posSol;   // posicao do sol na tela (0..1)
uniform float forca;
void main() {
    vec2 uv = fragTexCoord;
    vec2 d = (uv - posSol) * (0.9 / 28.0);
    vec3 c = vec3(0.0);
    float peso = 1.0;
    for (int i = 0; i < 28; i++) {
        uv -= d;
        c += TEX(texture0, uv).rgb * peso;
        peso *= 0.955;
    }
    FRAG = vec4(c * (forca / 28.0), 1.0);
}
)";
const char* FS_JUNTA = R"(
uniform sampler2D texture0;
uniform sampler2D texBrilho;
uniform sampler2D texRaios;
uniform float forcaBrilho;
uniform float usarRaios;
void main() {
    vec3 c = TEX(texture0, fragTexCoord).rgb;
    c += TEX(texBrilho, fragTexCoord).rgb * forcaBrilho;
    if (usarRaios > 0.5) c += TEX(texRaios, fragTexCoord).rgb;
    // vinheta suave
    vec2 v = fragTexCoord - 0.5;
    c *= 1.0 - dot(v, v) * 0.35;
    FRAG = vec4(c, 1.0);
}
)";

// ---------- personagens com esqueleto (deformados na placa de video) ----------
// vertexTexCoord2: x = osso A, y = osso B + 0.99 * peso de A
#define CORPO_SKIN R"(
ATTR vec3 vertexPosition;
ATTR vec2 vertexTexCoord;
ATTR vec2 vertexTexCoord2;
ATTR vec3 vertexNormal;
ATTR vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 ossos[17];
mat4 matrizPele() {
    int a = int(vertexTexCoord2.x + 0.5);
    float fy = vertexTexCoord2.y;
    int b = int(floor(fy));
    float w = fract(fy) / 0.99;
    return ossos[a] * w + ossos[b] * (1.0 - w);
}
)"

const char* VS_PELE = CORPO_SKIN R"(
VARY vec3 posMundo;
VARY vec3 normalMundo;
VARY vec4 cor;
VARY vec2 extra;
void main() {
    mat4 S = matrizPele();
    vec4 pl = S * vec4(vertexPosition, 1.0);
    vec4 p = matModel * pl;
    posMundo = p.xyz;
    normalMundo = normalize(mat3(matModel) * (mat3(S) * vertexNormal));
    cor = vertexColor;
    extra = vertexTexCoord;
    gl_Position = mvp * pl;
}
)";

const char* VS_PROF_PELE = CORPO_SKIN R"(
VARY float tipo;
void main() {
    tipo = 0.0;
    gl_Position = mvp * (matrizPele() * vec4(vertexPosition, 1.0));
}
)";
