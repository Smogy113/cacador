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
    return corNeblina + corSol * (0.16 * pow(s, 5.0) + 0.2 * pow(s, 28.0));  // a nevoa brilha dourada para o lado do sol
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

Shader carregarShader(const char* vs, const char* fs) { return carregarShaderDef(vs, fs, ""); }

Shader carregarShaderDef(const char* vs, const char* fs, const char* defs) {
    std::string v = std::string(CAB_VS) + defs + vs;
    std::string f = std::string(CAB_FS) + defs + COMUM + fs;
    return LoadShaderFromMemory(v.c_str(), f.c_str());
}

const char* VS_LIT = R"(
invariant gl_Position;  // a profundidade do pre-passe precisa coincidir com a do passe de cor
ATTR vec3 vertexPosition;
ATTR vec2 vertexTexCoord;
ATTR vec3 vertexNormal;
ATTR vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform float tempoVento;
VARY vec3 posMundo;
VARY vec3 normalMundo;
VARY vec4 cor;
VARY vec2 extra;
#ifdef PAREDES
ATTR vec2 vertexTexCoord2;   // x = indice da forma da parede + 1 (0: nenhuma)
VARY float formaV;
#elif defined(SOLO_DETALHADO)
ATTR vec2 vertexTexCoord2;   // pesos de terra e rocha no terreno
VARY vec2 pesosSolo;
#endif
void main() {
#ifdef PAREDES
    formaV = vertexTexCoord2.x;
#elif defined(SOLO_DETALHADO)
    pesosSolo = vertexTexCoord2;
#endif
    vec3 vp = vertexPosition;
#ifndef SOLO_DETALHADO
    if (vertexTexCoord.y > 3.5 && vertexTexCoord.y < 4.5) {  // grama: a ponta balanca com o vento (rajadas passando pelo campo)
        float flex = vertexColor.a;
        float onda = sin(tempoVento * 1.6 + vp.x * 0.23 + vp.z * 0.17) * 0.6 + sin(tempoVento * 3.1 + vp.x * 1.3) * 0.25;
        vp.x += onda * 0.08 * flex;
        vp.z += cos(tempoVento * 1.2 + vp.z * 0.31) * 0.045 * flex;
    }
#endif
    vec4 p = matModel * vec4(vp, 1.0);
    posMundo = p.xyz;
    normalMundo = normalize(mat3(matModel) * vertexNormal);
    cor = vertexColor;
    extra = vertexTexCoord;
    gl_Position = mvp * vec4(vp, 1.0);
}
)";

// Formula de distancia do vilarejo e os raios contra ela (paredes no FS_LIT e luz guardada por face)
#define CENA_SDF R"(
// ---- o vilarejo como formula de distancia: as construcoes perto da camera (formas exatas) ----
uniform vec4 formaA[64];    // centro.xyz, rotY
uniform vec4 formaB[64];    // meia.xyz, tipo (0 caixa, 1 prisma, 2 cilindro)
uniform vec3 formaCor[64];
uniform vec2 formaCS[64];   // cos e sen da rotacao (calculados no processador)
uniform float formaR[64];   // raio da esfera que envolve a forma
uniform int numFormas;
float dForma(int i, vec3 p) {
    vec3 d = p - formaA[i].xyz;
    float c = formaCS[i].x, s = formaCS[i].y;
    vec3 q = vec3(d.x * c - d.z * s, d.y, d.x * s + d.z * c);
    vec3 h = formaB[i].xyz;
    if (formaB[i].w < 1.5) {
        vec3 dd = abs(q) - h;
        float cx = length(max(dd, 0.0)) + min(max(dd.x, max(dd.y, dd.z)), 0.0);
        if (formaB[i].w < 0.5) return cx;
        float gz = 1.0 / h.z, gy = 1.0 / (2.0 * h.y);
        float agua = (abs(q.z) * gz + (q.y + h.y) * gy - 1.0) / sqrt(gz * gz + gy * gy);
        return max(max(agua, max(-h.y - q.y, abs(q.x) - h.x)), cx);
    }
    float dr = length(q.xz) - h.x, dy = abs(q.y) - h.y;
    return min(max(dr, dy), 0.0) + length(max(vec2(dr, dy), 0.0));
}
// lista de formas que podem influir num efeito (as outras ficam longe demais para mudar o resultado)
int lista[64];
int nLista = 0;
void listarPerto(vec3 p, float alcance) {          // formas a menos de "alcance" do ponto
    nLista = 0;
    for (int i = 0; i < 64; i++) {
        if (i >= numFormas) break;
        vec3 d = p - formaA[i].xyz;
        float r = formaR[i] + alcance;
        if (dot(d, d) < r * r) { lista[nLista] = i; nLista++; }
    }
}
void listarSegmento(vec3 a, vec3 b, float folga) {  // formas a menos de "folga" do segmento a-b
    nLista = 0;
    vec3 ab = b - a;
    float l2 = dot(ab, ab);
    for (int i = 0; i < 64; i++) {
        if (i >= numFormas) break;
        vec3 c = formaA[i].xyz;
        float t = clamp(dot(c - a, ab) / l2, 0.0, 1.0);
        vec3 d = a + ab * t - c;
        float r = formaR[i] + folga;
        if (dot(d, d) < r * r) { lista[nLista] = i; nLista++; }
    }
}
float mapaLista(vec3 p, out int id) {
    float d = 1e9;
    id = -1;
    for (int k = 0; k < 64; k++) {
        if (k >= nLista) break;
        int i = lista[k];
        // a forma inteira cabe numa esfera: se ate a esfera ja e mais longe que a melhor, pula
        vec3 delta = p - formaA[i].xyz;
        float r = formaR[i] + d;  // d pode ser negativo dentro de uma forma
        if (r <= 0.0 || dot(delta, delta) >= r * r) continue;
        float di = dForma(i, p);
        if (di < d) { d = di; id = i; }
    }
    return d;
}
// sombra suave tracada ate a luz: nitida junto ao que bloqueia, abrindo com a distancia
float sombraCena(vec3 p, vec3 L) {
    float res = 1.0, t = 0.05;
    int id;
    listarSegmento(p, p + L * 32.5, 8.0);  // passo maximo 2,5 m: o raio nunca passa de 32,5 m
    if (nLista == 0) return 1.0;
    for (int i = 0; i < 24; i++) {
        float h = mapaLista(p + L * t, id);
        res = min(res, 7.0 * h / t);
        t += clamp(h, 0.05, 2.5);
        if (res < 0.01 || t > 30.0) break;
    }
    return clamp(res, 0.0, 1.0);
}
// oclusao: o quanto as construcoes em volta (beiral, alicerce, casa vizinha) fecham o ceu
float oclusaoCena(vec3 p, vec3 n) {
    float occ = 0.0, w = 1.0;
    int id;
    // so contam formas a menos de 0,9 m das amostras (alem disso d - dist <= 0 para todas)
    listarPerto(p, 1.8);
    if (nLista == 0) return 1.0;
    for (int k = 1; k <= 5; k++) {
        float d = 0.18 * float(k);
        occ += (d - min(mapaLista(p + n * d, id), 1e3)) * w;
        w *= 0.7;
    }
    return clamp(1.0 - occ * 1.6, 0.25, 1.0);
}
// um rebote: a parede ensolarada em frente devolve luz colorida (5 direcoes fixas, sem ruido)
vec3 reboteCena(vec3 p, vec3 n, vec3 T) {
    vec3 soma = vec3(0.0);
    listarPerto(p, 12.6);  // os raios andam no maximo 12 m
    if (nLista == 0) return soma;
    vec3 dirs[5];
    dirs[0] = n;
    dirs[1] = normalize(n + T * 0.8);
    dirs[2] = normalize(n - T * 0.8);
    dirs[3] = normalize(n + vec3(0.0, 0.7, 0.0));
    dirs[4] = normalize(n + vec3(0.0, -0.35, 0.0));
    for (int k = 0; k < 5; k++) {
        float t = 0.15;
        int id = -1;
        for (int i = 0; i < 16; i++) {
            float h = mapaLista(p + dirs[k] * t, id);
            if (h < 0.03) break;
            t += h;
            if (t > 12.0) { id = -1; break; }
        }
        if (id < 0 || t > 12.0) continue;
        vec3 q = p + dirs[k] * t;
        vec2 e = vec2(0.03, 0.0);
        vec3 nh = normalize(vec3(dForma(id, q + e.xyy) - dForma(id, q - e.xyy), dForma(id, q + e.yxy) - dForma(id, q - e.yxy),
                                 dForma(id, q + e.yyx) - dForma(id, q - e.yyx)));
        float sol = max(dot(nh, dirSol), 0.0) * max(dot(-dirs[k], nh), 0.0);
        soma += pow(formaCor[id], vec3(2.2)) * corSol * sol / (1.0 + t * 0.25);
    }
    return soma * (1.3 / 5.0);
}
)"

#define CORTE_MECHA R"(
bool foraDaMecha(vec2 uv) {
    float borda = abs(uv.x - 0.5) * 2.0;
    float ponta = smoothstep(0.55, 1.0, uv.y);
    float dente = 0.5 + 0.5 * sin(uv.x * 31.4 + uv.y * 3.0);
    return borda > 1.0 - 0.75 * ponta * dente;
}
)"

const char* FS_LIT = CORTE_MECHA R"(
#ifdef RAYMARCH
#define ENTRADA
#else
#define ENTRADA VARY
#endif
ENTRADA vec3 posMundo;
ENTRADA vec3 normalMundo;
ENTRADA vec4 cor;
ENTRADA vec2 extra;   // x = oclusao de ambiente, y = tipo (1 agua, 2 chama, 3 janela)
uniform vec3 posCamera;
uniform vec3 ambCeu;
uniform vec3 ambChao;
uniform vec3 dirLua;
uniform vec3 corLua;
uniform vec4 luzPos[8];   // xyz + raio
uniform vec3 luzCor[8];
uniform int numLuzes;
uniform float noite;
uniform float tempoVento;
uniform float descartarPerto;
uniform float raioGrama;
uniform float neblinaBaixa;
uniform float usarSombra;
uniform mat4 matLuz;
uniform sampler2D texSombra;
uniform float texelSombra;
uniform vec3 dirSombra;

uniform float texelMetros;  // tamanho de um texel do mapa de sombras, em metros
uniform vec3 corChao;       // cor media do chao (luz rebatida)
#if !defined(GL_ES)
uniform float iluminacaoAlta;
uniform float usarFiltroSombra;
uniform sampler2DShadow texSombraFiltrada;
uniform vec3 passoSombraX;
uniform vec3 passoSombraY;
uniform float metrosProfSombra;
#else
const float iluminacaoAlta = 0.0;
#endif

#ifdef PERSONAGEM
uniform float argila;
uniform float heroi;  // 1 = o cacador (luz de personagem, olhos de cinema)
uniform sampler2D texSombraPerto;
uniform mat4 matLuzPerto;
uniform float usarSombraPerto;
uniform float texelSombraPerto;
#else
const float usarSombraPerto = 0.0;
#endif
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Sombra suave (PCSS simplificado): procura quem bloqueia a luz e borra mais
// quanto mais longe o bloqueador esta do chao. Perto do pe: nitida; na ponta: borrada.
#if !defined(GL_ES)
// Discos unitarios com angulo i * 2.39996 e raio sqrt((i + 0.5) / N).
// A rotacao aleatoria e a mesma para todas as amostras de um fragmento.
const vec2 discoBloq[8] = vec2[8](
    vec2(0.250000000, 0.000000000), vec2(-0.319289148, 0.292496890),
    vec2(0.048868917, -0.556876838), vec2(0.402449489, 0.524913669),
    vec2(-0.738536775, -0.130627036), vec2(0.699597657, -0.445042878),
    vec2(-0.233987495, 0.870488286), vec2(-0.446290076, -0.859258533));
const vec2 discoSombra[12] = vec2[12](
    vec2(0.204124153, 0.000000000), vec2(-0.260698497, 0.238822713),
    vec2(0.039901301, -0.454688042), vec2(0.328598619, 0.428590238),
    vec2(-0.603012800, -0.106656536), vec2(0.571219087, -0.363375992),
    vec2(-0.191050008, 0.710750699), vec2(-0.364394337, -0.701581657),
    vec2(0.790564001, 0.288689852), vec2(-0.822432637, 0.339516312),
    vec2(0.396443814, -0.847249866), vec2(0.293014765, 0.934064090));
#endif
float gGrama = 0.0;  // uma leitura de sombra por fragmento da grama
#if !defined(GL_ES)
float sombraFiltrada(vec3 p, vec3 n) {
    float nl = dot(n, dirSombra);
    if (nl <= 0.0 && gGrama < 0.5) return 0.0;
    // A projecao da luz e ortografica. Corrige a profundidade de cada amostra
    // no plano da superficie, para a penumbra acompanhar encostas e paredes.
    vec3 q = (matLuz * vec4(p + n * (0.2 * texelMetros), 1.0)).xyz * 0.5 + 0.5;
    float borda = min(min(q.x, 1.0 - q.x), min(q.y, 1.0 - q.y));
    if (borda <= 0.0 || q.z <= 0.0 || q.z >= 1.0) return 1.0;

    vec2 grad = vec2(dot(n, passoSombraX), dot(n, passoSombraY)) / max(nl, 0.22);
    float z = q.z - 0.000018 - 0.65 * (abs(grad.x) + abs(grad.y));
    float peso = smoothstep(0.0, 0.08, borda);
    if (gGrama > 0.5) return mix(1.0, TEX(texSombraFiltrada, vec3(q.xy, z)), peso);

    float separacao = 0.0, bloqueadores = 0.0;
    for (int i = 0; i < 8; i++) {
        vec2 desloc = discoBloq[i] * 4.0;
        float d = z + dot(grad, desloc) - TEX(texSombra, q.xy + desloc * texelSombra).r;
        separacao += max(d, 0.0);
        bloqueadores += float(d > 0.0);
    }
    if (bloqueadores == 0.0) return mix(1.0, TEX(texSombraFiltrada, vec3(q.xy, z)), peso);
    float dist = separacao / bloqueadores * metrosProfSombra;
#ifdef PERSONAGEM
    if (usarSombraPerto > 0.5 && dist < 0.8) return 1.0;
#endif
    // Quanto mais distante o bloqueador, maior a fonte aparente no mapa.
    // Cada leitura faz quatro comparacoes bilineares na GPU.
    float raio = clamp(dist * 0.032 / texelMetros, 0.8, 7.0);
    float vis = 0.0;
    for (int i = 0; i < 8; i++) {
        vec2 desloc = discoBloq[i] * raio;
        vis += TEX(texSombraFiltrada, vec3(q.xy + desloc * texelSombra, z + dot(grad, desloc)));
    }
    return mix(1.0, vis * 0.125, peso);
}
#endif
float sombraEm(vec3 p, vec3 n) {
    if (usarSombra < 0.5) return 1.0;
#if !defined(GL_ES)
    if (usarFiltroSombra > 0.5) return sombraFiltrada(p, n);
#endif
    float nl = clamp(dot(n, dirSombra), 0.0, 1.0);
    if (nl <= 0.0) return 0.0;
    vec4 pl = matLuz * vec4(p + n * (0.05 + 0.2 * (1.0 - nl)), 1.0);
    vec3 q = pl.xyz / pl.w * 0.5 + 0.5;
    if (q.x <= 0.0 || q.x >= 1.0 || q.y <= 0.0 || q.y >= 1.0 || q.z >= 1.0) return 1.0;
    float bias = 0.00012 + 0.0005 * (1.0 - nl);
    if (gGrama > 0.5) return (q.z - bias > TEX(texSombra, q.xy).r) ? 0.0 : 1.0;
    float giro = hash12(p.xz * 7.13) * 6.2831;
#if !defined(GL_ES)
    float cg = cos(giro), sg = sin(giro);
    mat2 rotacao = mat2(cg, sg, -sg, cg);
#endif
    float soma = 0.0, cont = 0.0;
    for (int i = 0; i < 8; i++) {
#if !defined(GL_ES)
        vec2 desloc = (rotacao * discoBloq[i]) * (5.0 * texelSombra);
#else
        float a = float(i) * 2.39996 + giro;
        float r = sqrt((float(i) + 0.5) / 8.0) * 5.0;
        vec2 desloc = vec2(cos(a), sin(a)) * r * texelSombra;
#endif
        float d = TEX(texSombra, q.xy + desloc).r;
        if (d < q.z - bias) { soma += d; cont += 1.0; }
    }
    if (cont < 0.5) return 1.0;
    float distBloq = (q.z - soma / cont) * 1000.0;
#ifdef PERSONAGEM
    if (usarSombraPerto > 0.5 && distBloq < 0.8) return 1.0;  // o proprio corpo: fica com o mapa de perto
#endif
    float raio = clamp(distBloq * 0.045 / texelMetros, 0.8, 7.0);
    float s = 0.0;
    for (int i = 0; i < 12; i++) {
#if !defined(GL_ES)
        vec2 desloc = (rotacao * discoSombra[i]) * (raio * texelSombra);
#else
        float a = float(i) * 2.39996 + giro;
        float r = sqrt((float(i) + 0.5) / 12.0) * raio;
        vec2 desloc = vec2(cos(a), sin(a)) * r * texelSombra;
#endif
        float d = TEX(texSombra, q.xy + desloc).r;
        s += (q.z - bias > d) ? 0.0 : 1.0;
    }
    s /= 12.0;
    vec2 b = smoothstep(vec2(0.0), vec2(0.08), q.xy) * smoothstep(vec2(1.0), vec2(0.92), q.xy);
    return mix(1.0, s, b.x * b.y);
} // > 0: horizonte, some onde o terreno detalhado ja desenha


float hashSolo(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float ruidoSolo(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hashSolo(i), hashSolo(i + vec2(1,0)), f.x),
               mix(hashSolo(i + vec2(0,1)), hashSolo(i + vec2(1,1)), f.x), f.y);
}
#ifdef PERSONAGEM
ENTRADA vec4 tanRep;   // cartao: direcao do fio + v; pele do rosto: coordenadas do rosto
ENTRADA vec3 tanMundo;
ENTRADA vec3 posRep;   // posicao de repouso (o detalhe acompanha o corpo)
ENTRADA vec3 nRep;
ENTRADA vec3 mc0;      // colunas da matriz que leva normais do repouso para o mundo
ENTRADA vec3 mc1;
ENTRADA vec3 mc2;
float h3(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.71, 0.113, 0.419));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float vnoise(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = mix(mix(h3(i), h3(i + vec3(1, 0, 0)), f.x), mix(h3(i + vec3(0, 1, 0)), h3(i + vec3(1, 1, 0)), f.x), f.y);
    float b = mix(mix(h3(i + vec3(0, 0, 1)), h3(i + vec3(1, 0, 1)), f.x), mix(h3(i + vec3(0, 1, 1)), h3(i + vec3(1, 1, 1)), f.x), f.y);
    return mix(a, b, f.z);
}
// relevo fino: inclina a normal pelo gradiente do ruido (escala k por eixo)
vec3 relevo(vec3 n, vec3 p, vec3 k, float amp) {
    vec3 q = p * k;
    float c = vnoise(q);
    vec3 g = vec3(vnoise(q + vec3(0.3, 0.0, 0.0)) - c, vnoise(q + vec3(0.0, 0.3, 0.0)) - c, vnoise(q + vec3(0.0, 0.0, 0.3)) - c) / 0.3;
    g -= n * dot(g, n);
    return normalize(n - g * amp);  // amp = inclinacao maxima da normal
}
#endif

#ifdef RAYMARCH
// ---- cabeca do heroi desenhada pixel a pixel: o raio anda pelo volume de distancias (atlas de
// fatias) ate a pele e preenche as mesmas variaveis que a malha traria do vertex shader ----
VARY vec3 posCaixa;
uniform mat4 matModel;
uniform mat4 ossos[17];
uniform mat4 invModelo;
uniform mat4 matVP;
uniform sampler2D texDist;
uniform sampler2D texCor;
uniform vec3 volLo;
uniform vec3 volHi;
uniform vec3 volN;       // voxels de distancia (x, y, z)
uniform vec3 corN;       // voxels de cor
uniform vec4 volInfo;    // h, hCor, colunas (dist), colunas (cor)
uniform vec4 atlasTam;   // largura e altura dos dois atlas
uniform vec3 escalaRep;  // escultura -> modelo
uniform float passosMax;
uniform float tamPixel;   // tamanho de um pixel a 1 m da camera (m)
uniform float economico;  // Medio: sem sombra propria e sem translucidez
mat4 Mc, Mp, iC, iP;
mat3 mnG;   // repouso -> mundo (normais)
vec3 nqG;   // normal na escultura
mat4 inversaR(mat4 m) {
    mat3 rt = transpose(mat3(m));
    return mat4(vec4(rt[0], 0.0), vec4(rt[1], 0.0), vec4(rt[2], 0.0), vec4(-(rt * m[3].xyz), 1.0));
}
float pesoCab(float y) { return y < 1.585 ? smoothstep(1.53, 1.585, y) : 1.0; }
float dPeso(float y) {
    if (y <= 1.53 || y >= 1.585) return 0.0;
    float t = (y - 1.53) / 0.055;
    return 6.0 * t * (1.0 - t) / 0.055;
}
// ponto da pose atual (modelo) -> pose de repouso; Newton com a derivada do peso (como pescoco.cpp)
vec3 paraRep(vec3 p) {
    vec3 r = (iC * vec4(p, 1.0)).xyz;
    if (r.y > 1.592 * escalaRep.y) return r;
    vec3 rn = (iP * vec4(p, 1.0)).xyz;
    if (rn.y < 1.55 * escalaRep.y) r = rn;
    for (int it = 0; it < 4; it++) {
        float y = r.y / escalaRep.y, w = pesoCab(y), dw = dPeso(y) / escalaRep.y;
        vec3 cr = (Mc * vec4(r, 1.0)).xyz, nr = (Mp * vec4(r, 1.0)).xyz;
        vec3 e = p - mix(nr, cr, w);
        vec3 ae = mat3(iC) * e * w + mat3(iP) * e * (1.0 - w);
        vec3 au = mat3(iC) * (cr - nr) * w + mat3(iP) * (cr - nr) * (1.0 - w);
        float den = 1.0 + dw * au.y;
        r += ae - au * (dw * ae.y / (abs(den) > 0.05 ? den : 0.05));
    }
    return r;
}
vec2 naFatia(vec2 xy, float k, vec3 n, float colunas, vec2 tam) {
    return (vec2(mod(k, colunas) * n.x, floor(k / colunas) * n.y) + xy + 0.5) / tam;
}
float distVol(vec3 q) {  // q: coordenadas da escultura
    vec3 c = clamp(q, volLo, volHi);
    vec3 f = (c - volLo) / volInfo.x;
    float k = min(floor(f.z), volN.z - 2.0);
    float a = TEX(texDist, naFatia(f.xy, k, volN, volInfo.z, atlasTam.xy)).r;
    float b = TEX(texDist, naFatia(f.xy, k + 1.0, volN, volInfo.z, atlasTam.xy)).r;
    return mix(a, b, f.z - k) + length(q - c);
}
vec4 corVol(vec3 q) {
    vec3 f = clamp((q - volLo) / volInfo.y, vec3(0.0), corN - 1.001);
    float k = floor(f.z);
    vec4 a = TEX(texCor, naFatia(f.xy, k, corN, volInfo.w, atlasTam.zw));
    vec4 b = TEX(texCor, naFatia(f.xy, k + 1.0, corN, volInfo.w, atlasTam.zw));
    return mix(a, b, f.z - k);
}
vec3 qHit;  // ponto da pele (escultura), para a luz do rosto
void marchar() {
    Mc = ossos[4]; Mp = ossos[3];  // CABECA, PESCOCO
    iC = inversaR(Mc); iP = inversaR(Mp);
    vec3 ro = (invModelo * vec4(posCamera, 1.0)).xyz;
    vec3 rd = (invModelo * vec4(posCaixa, 1.0)).xyz - ro;  // a caixa e desenhada pelas faces de tras: posCaixa e a saida
    float tFim = length(rd);
    rd /= tFim;
    float esc = min(escalaRep.x, escalaRep.y);
    float t = max(tFim - 0.6, 0.0);  // a caixa tem ~0,5 m de diagonal; com a camera dentro, comeca na camera
    // raio rente a pele gasta os passos andando devagar: se acabarem sem ter saido da caixa e
    // perto da superficie, fica com o ponto (senao aparecem fios do fundo nas bordas do nariz/queixo)
    bool achou = false, saiu = false;
    float d = 1.0;
    for (int i = 0; i < 96; i++) {
        if (float(i) >= passosMax) break;
        d = distVol(paraRep(ro + rd * t) / escalaRep) * esc;
        if (d < 0.00005 + t * tamPixel * 0.3) { achou = true; break; }
        t += d * 0.85;
        if (t > tFim + 0.01) { saiu = true; break; }
    }
    if (!achou && (saiu || d > t * tamPixel * 2.0)) discard;  // passou rente: so fica se a pele esta a menos de ~2 pixels
    for (int k = 0; k < 2; k++) t += distVol(paraRep(ro + rd * t) / escalaRep) * esc;  // refino
    vec3 p = ro + rd * t;
    vec3 r = paraRep(p);
    vec3 q = r / escalaRep;
    qHit = q;
    float h = volInfo.x;
    vec3 g = vec3(distVol(q + vec3(h, 0.0, 0.0)) - distVol(q - vec3(h, 0.0, 0.0)),
                  distVol(q + vec3(0.0, h, 0.0)) - distVol(q - vec3(0.0, h, 0.0)),
                  distVol(q + vec3(0.0, 0.0, h)) - distVol(q - vec3(0.0, 0.0, h)));
    vec3 nq = normalize(g);
    nRep = normalize(nq / escalaRep);
    float w = pesoCab(q.y);
    mat3 mn = mat3(matModel) * (mat3(Mc) * w + mat3(Mp) * (1.0 - w));
    mnG = mn;
    nqG = nq;
    mc0 = mn[0]; mc1 = mn[1]; mc2 = mn[2];
    posMundo = (matModel * vec4(p, 1.0)).xyz;
    normalMundo = normalize(mn * nRep);
    posRep = r;
    tanRep = vec4(q.x, q.y, 1.0, 0.0);
    tanMundo = vec3(0.0, 1.0, 0.0);
    // oclusao pela propria forma (como a malha: 5 amostras ao longo da normal)
    float occ = 0.0, peso = 1.0;
    for (int s = 1; s <= 5; s++) {
        float dist = 0.0025 * float(s);
        occ += (dist - distVol(q + nq * dist)) * peso;
        peso *= 0.75;
    }
    float ao = clamp(1.0 - occ * 55.0, 0.3, 1.0);
    vec4 c = corVol(q);
    cor = vec4(c.rgb, clamp((0.5 - c.a) * 2.0, 0.0, 1.0));
    extra = vec2(ao, c.a > 0.75 ? 18.0 : 10.0);
    vec4 cp = matVP * vec4(posMundo, 1.0);
    gl_FragDepth = (cp.z / cp.w) * 0.5 + 0.5;
}
// direcao do mundo -> escultura
vec3 paraEscultura(vec3 d) { return normalize((transpose(mnG) * d) / escalaRep); }
// sombra do proprio rosto (nariz na bochecha, sobrancelha no olho): marcha curta ate o sol;
// a penumbra vem do menor afastamento da pele ao longo do raio
float sombraPropria(vec3 q, vec3 L) {
    q += nqG * 0.0015;  // sai da pele (o ponto achado pode estar um pouco dentro)
    float res = 1.0, t = 0.002;
    for (int i = 0; i < 16; i++) {
        float h = distVol(q + L * t);
        res = min(res, 4.5 * h / t);  // penumbra larga: a pele espalha a luz por dentro
        t += clamp(h, 0.0015, 0.02);
        if (res < 0.02 || t > 0.12) break;
    }
    return clamp(res, 0.0, 1.0);
}
// espessura local (andando para dentro pela normal): orelha e asas do nariz sao finas (a luz
// atravessa), o resto da cabeca e grosso. Nao depende da direcao do sol (sem faixas no limite da sombra).
float finura(vec3 q) {
    float e = 0.0;
    for (int k = 1; k <= 4; k++) e += max(-distVol(q - nqG * (0.0025 * float(k))), 0.0);
    return exp(-e * 300.0);
}
#endif


#if !defined(GL_ES)
// ---- dentro de casa: o ceu so entra pela porta e janelas; a noite, luz quente de vela ----
uniform vec4 interiorA[16];   // centro xyz, cos(giro)
uniform vec4 interiorB[16];   // meia xyz, abertura da porta (0..1)
uniform vec4 interiorP[16];   // centro do vao da porta, sen(giro)
uniform vec3 interioresMin;
uniform vec3 interioresMax;
uniform int numInteriores;
float fatorInterior(vec3 p) {
    if (any(lessThan(p, interioresMin)) || any(greaterThan(p, interioresMax))) return 1.0;
    float f = 1.0;
    for (int i = 0; i < numInteriores; i++) {
        vec3 d = p - interiorA[i].xyz;
        vec3 m = interiorB[i].xyz + vec3(0.08);
        if (d.y <= -m.y - 0.3 || d.y >= m.y) continue;
        float c = interiorA[i].w, s = interiorP[i].w;
        vec2 q = vec2(d.x * c - d.z * s, d.x * s + d.z * c);
        if (abs(q.x) < m.x && abs(q.y) < m.z) {
            float dp = length(p - interiorP[i].xyz);
            float k = 0.26 + interiorB[i].w * 0.7 * exp(-dp * 0.55);  // janelas abertas + porta (se aberta)
            f = min(f, clamp(k, 0.0, 1.0));
        }
    }
    return f;
}
#endif

#ifdef PAREDES
// ---- paredes das casas: relevo desenhado pixel a pixel (o raio entra num campo de alturas por
// material) com sombra propria do relevo; coordenadas = posicao no plano da face (sem UV) ----
uniform float paredes;  // 0 desligado, 1 Medio, 2 Alto
float sombraRelevo = 1.0;
float vn2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), f.x), mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), f.x), f.y);
}
// material assado em texturas na abertura (FS_ASSAR_MATERIAL): repete a cada 4 m
uniform sampler2D texAltura;
uniform sampler2D texFator;
uniform sampler2D texPeso;
float lodMat = 0.0;
float hParede(vec2 uv, float mat) {
    vec3 a = textureLod(texAltura, uv * 0.25, lodMat).rgb;
    return -0.03 * (mat < 5.5 ? a.r : (mat < 6.5 ? a.g : a.b));
}
// ---- telhados: palha penteada, telhas em fileiras, tabuas sobrepostas. So normal, oclusao, cor e a
// sombra dos degraus (sem raio andando no relevo): poucas leituras de textura por pixel ----
uniform sampler2D texTelhado;   // FS_ASSAR_TELHADO (1024 texels em 4 m)
vec2 matTelhado(vec2 uv, float mat) {  // altura (m, <= 0) e fator de cor
    float lt = max(lodMat - 1.0, 0.0);
    if (mat < 8.5) { vec2 t = textureLod(texTelhado, uv * 0.25, lt).rg; return vec2(-0.045 * t.x, t.y * 2.0); }
    if (mat < 9.5) { vec2 t = textureLod(texTelhado, uv * 0.25, lt).ba; return vec2(-0.04 * t.x, t.y * 2.0); }
    // tabuas: a madeira das paredes (fileiras de 0,2 m) com cada tabua inclinada sobre a de baixo
    // o degrau e calculado (sem mipmap): some quando o pixel fica do tamanho da tabua (sem moire)
    float degrau = 0.014 * (1.0 - smoothstep(2.5, 4.0, lodMat));
    float h = -0.03 * textureLod(texAltura, uv * 0.25, lodMat).g - degrau * fract(uv.y * 5.0);
    return vec2(h, textureLod(texFator, uv * 0.25, lodMat).g * 2.0);
}
vec3 nParede = vec3(0.0);   // normal da face (antes do relevo)
float aoCena = 1.0;
// luz guardada por face (atlas): tabela das formas e as duas imagens de luz
VARY float formaV;
uniform sampler2D texFaces;   // por forma: (centro, cos rot), (meia, sen rot), 4 retangulos do atlas
uniform sampler2D texLuz1;    // sombra tracada, oclusao, -, calculada?
uniform sampler2D texLuz2;    // rebote / 2
uniform float tamAtlas;
uniform float metroTexel;
uniform float usarAtlas;   // 0 ate o atlas existir (sem ele, os amostradores leriam a textura branca)
bool luzGuardada(vec3 p, vec3 n, out vec4 L1, out vec3 L2) {
    L1 = vec4(0.0);
    L2 = vec3(0.0);
    int fid = int(formaV + 0.5) - 1;
    if (fid < 0 || usarAtlas < 0.5) return false;
    vec4 cr = texelFetch(texFaces, ivec2(0, fid), 0);
    vec4 mh = texelFetch(texFaces, ivec2(1, fid), 0);
    float c = cr.w, s = mh.w;
    vec3 d = p - cr.xyz;
    vec3 q = vec3(d.x * c - d.z * s, d.y, d.x * s + d.z * c);
    vec3 nl = vec3(n.x * c - n.z * s, n.y, n.x * s + n.z * c);
    int k;
    float u;
    if (abs(nl.x) > abs(nl.z)) { k = nl.x > 0.0 ? 0 : 1; u = nl.x > 0.0 ? q.z + mh.z : mh.z - q.z; }
    else { k = nl.z > 0.0 ? 2 : 3; u = nl.z > 0.0 ? q.x + mh.x : mh.x - q.x; }
    float v = q.y + mh.y;
    vec4 r = texelFetch(texFaces, ivec2(2 + k, fid), 0);
    if (r.z < 0.5) return false;
    vec2 t = clamp(vec2(u, v) / metroTexel, vec2(0.0), r.zw - 2.0);
    vec2 auv = (r.xy + 1.0 + t) / tamAtlas;
    L1 = textureLod(texLuz1, auv, 0.0);
    if (L1.a < 0.5) return false;   // ainda nao calculada
    if (paredes > 1.5) L2 = textureLod(texLuz2, auv, 0.0).rgb * 2.0;
    return true;
}
// janelas acesas a noite clareiam a parede em volta
uniform vec3 janelaP[12];
uniform vec3 janelaN[12];
uniform int numJanelas;
vec3 luzJanelas(vec3 p, vec3 n) {
    vec3 soma = vec3(0.0);
    for (int i = 0; i < 12; i++) {
        if (i >= numJanelas) break;
        vec3 d = janelaP[i] - p;
        float dd = length(d);
        if (dd > 5.0) continue;
        // a luz sai pela janela (mais forte para fora) e espalha pela propria parede, caindo suave
        float lado = 0.35 + 0.65 * max(dot(janelaN[i], -d / max(dd, 0.001)), 0.0);
        float queda = exp(-dd * 1.5) * (1.0 - smoothstep(3.0, 5.0, dd));
        soma += vec3(1.0, 0.62, 0.3) * queda * lado * (0.5 + 0.5 * max(dot(n, d / max(dd, 0.001)), 0.0));
    }
    return soma * noite * 1.6;
}
// aplica o relevo: muda uv, normal, albedo e oclusao; guarda a sombra do relevo
void relevoParede(inout vec3 n, inout vec3 albedo, inout float ao, vec3 v, float dist, float mat) {
    vec3 T = normalize(cross(vec3(0.0, 1.0, 0.0), n));
    vec3 B = vec3(0.0, 1.0, 0.0);
    vec3 N0 = n;
    vec2 uv = vec2(dot(posMundo, T), posMundo.y);
    float prof = 1.0 - smoothstep(18.0, 25.0, dist);    // relevo ate 25 m; depois so a normal
    float d = 0.0;                                       // profundidade achada (m, positiva)
    if (prof > 0.01) {
        vec3 vt = vec3(dot(v, T), dot(v, B), dot(v, N0));
        prof *= smoothstep(0.05, 0.25, vt.z);            // olhando rente: relevo raso (sem estourar)
        float alto = paredes > 1.5 ? 1.0 : 0.0;
        float nPassos = mix(mix(12.0, 24.0, alto), mix(5.0, 8.0, alto), vt.z);  // de frente precisa de menos passos
        float dz = 0.03 / nPassos;
        vec2 passo = -vt.xy / max(vt.z, 0.25) * dz * prof;
        vec2 uvA = uv;
        float camA = 0.0, cam = 0.0;
        for (int i = 0; i < 24; i++) {
            if (float(i) >= nPassos) break;
            if (cam >= -hParede(uv, mat) * prof) break;
            uvA = uv; camA = cam;
            uv += passo; cam += dz;
        }
        for (int k = 0; k < 4; k++) {  // bissecao entre o ultimo ponto fora e o primeiro dentro
            vec2 um = (uvA + uv) * 0.5;
            float cm = (camA + cam) * 0.5;
            if (cm >= -hParede(um, mat) * prof) { uv = um; cam = cm; } else { uvA = um; camA = cm; }
        }
        d = cam;
        // sombra do proprio relevo (tabua sobre tabua, pedra sobre argamassa): so no Alto
        vec3 Lt = vec3(dot(dirSombra, T), dot(dirSombra, B), dot(dirSombra, N0));
        if (alto > 0.5 && Lt.z > 0.05 && prof > 0.05) {
            float sh = 1.0;
            for (int k = 1; k <= 10; k++) {
                float s2 = float(k) * 0.0035;
                if (s2 > d) break;
                vec2 uk = uv + Lt.xy / Lt.z * s2;
                float acima = (d - s2) - (-hParede(uk, mat) * prof);  // > 0: o relevo esta acima do raio
                sh = min(sh, clamp(1.0 - acima * 160.0, 0.0, 1.0) * 0.5 + 0.5 * step(acima, 0.0));
            }
            sombraRelevo = sh;
        }
    }
    // normal pelo campo de alturas (ate 45 m)
    float kn = 1.0 - smoothstep(35.0, 45.0, dist);
    float e = 0.004;
    float h0 = hParede(uv, mat), hx = hParede(uv + vec2(e, 0.0), mat), hy = hParede(uv + vec2(0.0, e), mat);
    vec3 nt = normalize(vec3(-(hx - h0) / e * kn, -(hy - h0) / e * kn, 1.0));
    n = normalize(T * nt.x + B * nt.y + N0 * nt.z);
    // cor e oclusao das cavidades (fatores e pesos assados)
    vec3 corPedra = vec3(0.46, 0.44, 0.41);
    vec4 F = textureLod(texFator, uv * 0.25, lodMat) * 2.0;
    if (mat < 5.5) {
        vec4 P = textureLod(texPeso, uv * 0.25, lodMat);
        albedo *= F.r;
        albedo = mix(albedo, albedo * vec3(0.93, 0.92, 0.88), P.a);   // pintura gasta
        albedo = mix(albedo, corPedra * (P.g * 2.0), P.r);            // reboco caido: pedra exposta
    } else if (mat < 6.5) {
        float lumM = dot(albedo, vec3(0.3, 0.59, 0.11));
        albedo = mix(albedo, vec3(lumM) * vec3(1.08, 0.95, 0.82), 0.45);   // madeira envelhecida: menos saturada
        albedo *= F.g;
    } else {
        float peso = textureLod(texPeso, uv * 0.25, lodMat).b;
        albedo = mix(albedo * F.b, vec3(0.42, 0.39, 0.34) * 0.75, peso);   // argamassa
    }
    ao *= mix(0.45, 1.0, smoothstep(-0.03, -0.004, h0));             // fundo das frestas escuro
}
#endif
#ifdef PAREDES
void relevoTelhado(inout vec3 n, inout vec3 albedo, inout float ao, float dist, float mat) {
    vec3 N0 = n;
    vec3 T = abs(N0.y) > 0.999 ? vec3(1.0, 0.0, 0.0) : normalize(cross(vec3(0.0, 1.0, 0.0), N0));  // ao longo da cumeeira
    vec3 B = cross(N0, T);                                                                          // sobe a agua
    vec2 uv = vec2(dot(posMundo, T), dot(posMundo, B));
    vec2 m0 = matTelhado(uv, mat);
    float fundo = mat < 8.5 ? 0.045 : (mat < 9.5 ? 0.04 : 0.044);
    float kn = 1.0 - smoothstep(35.0, 60.0, dist);  // longe: so a cor (a mipmap ja alisou o relevo)
    if (kn > 0.0) {
        float e = 0.006;
        float hx = matTelhado(uv + vec2(e, 0.0), mat).x, hy = matTelhado(uv + vec2(0.0, e), mat).x;
        vec3 nt = normalize(vec3(-(hx - m0.x) / e * kn, -(hy - m0.x) / e * kn, 1.0));
        n = normalize(T * nt.x + B * nt.y + N0 * nt.z);
        // sombra dos degraus (ponta da palha, boca da telha, borda da tabua): uma amostra rumo ao sol
        vec3 Lt = vec3(dot(dirSombra, T), dot(dirSombra, B), dot(dirSombra, N0));
        if (paredes > 1.5 && Lt.z > 0.02 && dist < 30.0) {
            float lx = max(length(Lt.xy), 1e-3), d = 0.035;
            float hs = matTelhado(uv + Lt.xy / lx * d, mat).x;
            float raio = m0.x + d * Lt.z / lx;  // altura do raio de luz ali
            sombraRelevo = mix(1.0, 0.35, smoothstep(0.0, 0.008, hs - raio) * (1.0 - smoothstep(20.0, 30.0, dist)));
        }
    }
    albedo *= m0.y;
    if (mat < 8.5) {  // palha seca ao sol: menos saturada, mais cinza onde e velha
        float lum = dot(albedo, vec3(0.3, 0.59, 0.11));
        albedo = mix(albedo, vec3(lum) * vec3(1.05, 0.95, 0.8), 0.3 + clamp(0.9 - m0.y, 0.0, 0.4));
    }
    ao *= mix(0.5, 1.0, smoothstep(-fundo, -fundo * 0.35, m0.x));  // fundo entre fios, telhas e tabuas
}
#endif
#ifdef SOLO_DETALHADO
VARY vec2 pesosSolo;
uniform sampler2D corGramaSolo, normalGramaSolo;
uniform sampler2D corTerraSolo, normalTerraSolo;
uniform sampler2D corRochaSolo, normalRochaSolo;

void amostraSolo(sampler2D texCor, sampler2D texNormal, vec2 p, vec2 dx, vec2 dy,
                 float peso, inout vec3 detalhe, inout vec2 relevo) {
    detalhe += peso * textureGrad(texCor, p, dx, dy).rgb;
    relevo += peso * (textureGrad(texNormal, p, dx, dy).rg * 2.0 - 1.0);
}

void materialDoSolo(inout vec3 n, inout vec3 albedo, float dist) {
    vec2 p = posMundo.xz;
    // Gradientes antes das ramificacoes para manter os mipmaps nas transicoes.
    vec2 dx = dFdx(p), dy = dFdy(p);
    float alcance = (1.0 - smoothstep(45.0, 100.0, dist)) * cor.a;
    if (extra.y > -0.5 || alcance <= 0.0) return;

    float rocha = smoothstep(0.12, 0.82, pesosSolo.y);
    float terra = smoothstep(0.10, 0.84, pesosSolo.x) * (1.0 - rocha);
    float grama = max(1.0 - terra - rocha, 0.0);
    vec3 detalhe = vec3(0.0);
    vec2 relevo = vec2(0.0);
    // As escalas sao em metros e continuam iguais dos dois lados da borda do pedaco.
    if (grama > 0.0) amostraSolo(corGramaSolo, normalGramaSolo, p / 3.8, dx / 3.8, dy / 3.8, grama, detalhe, relevo);
    if (terra > 0.0) amostraSolo(corTerraSolo, normalTerraSolo, p / 4.6, dx / 4.6, dy / 4.6, terra, detalhe, relevo);
    if (rocha > 0.0) amostraSolo(corRochaSolo, normalRochaSolo, p / 5.2, dx / 5.2, dy / 5.2, rocha, detalhe, relevo);
    // Os mapas de cor foram equilibrados em torno de 0,45 sRGB para manter a paleta do terreno.
    albedo *= mix(vec3(1.0), pow(detalhe / 0.45, vec3(2.2)), alcance * 0.82);
    vec3 t = normalize(vec3(n.y, -n.x, 0.0));
    vec3 b = cross(n, t);  // +Y do mapa OpenGL aponta para -Z do chao
    n = normalize(n + (t * relevo.x + b * relevo.y) * alcance * 0.62);
}
#endif
void main() {
#ifdef RAYMARCH
    marchar();
#endif
#ifdef PAREDES
    if (paredes > 0.5) {  // tamanho de um pixel em metros (texel = 4 m / 2048)
        float px = max(length(dFdx(posMundo)), length(dFdy(posMundo)));
        lodMat = clamp(log2(max(px, 1e-5) / (4.0 / 2048.0)), 0.0, 11.0);
    }
#endif
    vec3 n = normalize(normalMundo);
#if !defined(GL_ES)
    vec3 normalSuperficie = n;
#endif
    vec3 albedo = pow(cor.rgb, vec3(2.2));
    float ao = extra.x;
    vec3 paraCam = posCamera - posMundo;
    float dist = length(paraCam);
    vec3 v = paraCam / max(dist, 0.001);
#ifndef SOLO_DETALHADO
    if (descartarPerto > 0.0 && length(paraCam.xz) < descartarPerto) discard;
#endif

#ifdef SOLO_DETALHADO
    // Esta variante recebe apenas a malha opaca do solo. A agua e o horizonte usam o shader geral.
    materialDoSolo(n, albedo, dist);
#else
    if (extra.y < -0.5 && dist < 120.0) {
        vec2 p = posMundo.xz;
        float alcance = 1.0 - smoothstep(65.0, 120.0, dist);
        float manchas = ruidoSolo(p * 1.7);
        float detalhe = 0.38 * (manchas - 0.5);
        if (dist < 65.0) {
            float grao = ruidoSolo(p * 13.0 + vec2(17.0, 5.0));
            float pedrinhas = smoothstep(0.72, 0.9, grao);
            detalhe += (0.55 * (grao - 0.5) + 0.30 * pedrinhas) * (1.0 - smoothstep(18.0, 65.0, dist));
        }
        if (dist < 30.0) {
            float areia = ruidoSolo(p * 66.0);
            detalhe += 0.45 * (areia - 0.5) * (1.0 - smoothstep(9.0, 30.0, dist));
        }
        albedo *= 1.0 + alcance * detalhe;
    }
#endif
    // parametros de material (mundo: tudo zero)
    float brilho = 0.0, aspereza = 1.0, sss = 0.0, veludo = 0.0, metalico = 0.0;
    float kk = 0.0;               // cabelo (Kajiya-Kay)
    float olhoHeroi = 0.0;        // brilho de cinema no olho do heroi
    vec3 Tcab = vec3(0.0, 1.0, 0.0);
#ifndef SOLO_DETALHADO
    if (extra.y > 3.5 && extra.y < 4.5) {  // grama: some aos poucos na borda do campo desenhado
        float dg = length(posMundo.xz - posCamera.xz);
        bool extraFolha = extra.y > 4.15;
        // A semente e constante em toda a folha, inclusive quando ela balanca.
        float sorte = clamp((extra.y - (extraFolha ? 4.2 : 4.0)) * 10.0, 0.0, 1.0);
        float alcance = extraFolha ? min(raioGrama, 12.0) : raioGrama;
        if (dg > alcance * (1.0 - (extraFolha ? 0.48 : 0.25) * sorte)) discard;
        gGrama = 1.0;
    }
#endif
#ifdef PAREDES
    bool ehParede = paredes > 0.5 && extra.y > 4.5 && extra.y < 7.5 && abs(n.y) < 0.3;
    float alcance = 1.0 - smoothstep(40.0, 70.0, dist);  // luz tracada: ate 40 m, some ate 70 m
    vec4 luz1 = vec4(0.0);
    vec3 luz2 = vec3(0.0);
    bool guardada = false;
    if (ehParede) {
        nParede = n;
        if (alcance > 0.0) {
            guardada = luzGuardada(posMundo, n, luz1, luz2);
            aoCena = mix(1.0, guardada ? luz1.g : 1.0, alcance);  // sem luz guardada (1o quadro): sem oclusao extra
            ao *= aoCena;
        }
        if (dist < 45.0) relevoParede(n, albedo, ao, v, dist, floor(extra.y + 0.5));
    }
    if (paredes > 0.5 && extra.y > 7.5 && extra.y < 10.5 && dist < 90.0) relevoTelhado(n, albedo, ao, dist, floor(extra.y + 0.5));
#endif
#ifdef PERSONAGEM
    float m = extra.y;
    if (argila > 0.5 && m > 23.5) discard;  // argila: sem fios
    if (m > 9.5) {
        vec3 nr = normalize(nRep);
        vec3 pr = posRep;
        if (m > 24.25) {         // cabelo comprido: mecha cheia (sem buracos entre os fios -> sem serrilhado);
            float u = cor.a, vv = tanRep.w;  // os fios sao listras de cor; so a borda e a ponta recortam
            float borda = abs(u - 0.5) * 2.0;               // 0 no meio da mecha, 1 na lateral
            if (foraDaMecha(vec2(u, vv))) discard;
            float listra = 0.5 + 0.5 * sin(u * 62.8 + sin(vv * 9.0 + u * 7.0) * 1.5);
            float listra2 = 0.5 + 0.5 * sin(u * 23.0 - vv * 4.0);
            float fio = sin(u * 303.0 + 1.8 * sin(vv * 19.0 + u * 11.0));
            float detalhe = (0.10 * fio) * (1.0 - smoothstep(1.4, 3.0, dist));
            albedo *= mix(0.66, 1.0, vv) * (0.78 + 0.15 * listra + 0.10 * listra2 + detalhe);
            ao *= mix(0.6, 1.0, vv) * (1.0 - 0.25 * borda * borda);
            kk = 0.68;
            Tcab = normalize(tanMundo);
        } else if (m > 23.5) {   // cartao de pelo: ~7 fios por fita, finos na ponta, raiz escura
            float u = cor.a, vv = tanRep.w;
            float N = 9.0;
            float fio = floor(u * N);
            float dentro = fract(u * N);
            vec3 sem = floor(posRep * 350.0);
            float r = h3(sem + fio * 1.7);
            float compr = 0.7 + 0.3 * r;
            float onda = sin(vv * 7.0 + r * 6.2831) * 0.1;
            float larg = 0.44 * (1.0 - 0.8 * smoothstep(0.0, compr, vv));
            if (vv > compr || abs(dentro - 0.5 + onda) > larg) discard;
            albedo *= mix(0.6, 0.95, vv / compr) * (0.8 + 0.35 * h3(sem + fio * 3.1));
            if (h3(sem + fio * 5.3) > 0.988) albedo = mix(albedo, vec3(0.45, 0.43, 0.4), 0.6);  // fio grisalho
            ao *= mix(0.55, 1.0, vv);
            kk = 1.0;
            Tcab = normalize(tanMundo);
        } else if (m > 19.5) {   // casca de pelo (barba, sobrancelha, cabelo, cilios): recorta em fios
            float h = (m - 20.0) / 3.0;                    // altura da casca: 0..1
            // fios espalhados: duas grades giradas (angulos e escalas diferentes) no plano da pele,
            // sem regularidade -> sem moire
            vec3 an = abs(nr);
            vec2 uv0 = an.x > an.y && an.x > an.z ? pr.zy : (an.z > an.y ? pr.xy : pr.xz);
            float cobertura = 0.0;
            vec3 cel = vec3(0.0);
            for (int gi = 0; gi < 2; gi++) {
                float rot = gi == 0 ? 0.52 : 1.63;
                float esc = gi == 0 ? 1150.0 : 1530.0;
                vec2 uv = mat2(cos(rot), sin(rot), -sin(rot), cos(rot)) * uv0 * esc;
                uv.y += h * 0.6;  // fios inclinados
                vec3 c3 = vec3(floor(uv), float(gi) * 17.0);
                vec2 dd = fract(uv) - 0.5 + (vec2(h3(c3 + 7.0), h3(c3 + 13.0)) - 0.5) * 0.5;
                float tem = step(h3(c3), cor.a * 0.75) * step(length(dd), 0.4 * (1.0 - h * 0.65));
                if (tem > cobertura) { cobertura = tem; cel = c3; }
            }
            if (cobertura < 0.5) discard;
            albedo *= (0.55 + 0.6 * h) * (0.8 + 0.4 * h3(cel + 3.0));
            nr = relevo(nr, pr, vec3(1500.0, 120.0, 1500.0), 0.3);
            brilho = 0.18; aspereza = 0.58;
            ao *= 0.6 + 0.4 * h;
        } else if (m > 18.5) {   // olho de cavalo: a malha fornece a iris e a pupila
            brilho = 0.98; aspereza = 0.19;
        } else if (m > 17.5) {   // labios: marcas verticais, umidos
            nr = relevo(nr, pr, vec3(120.0, 900.0, 120.0), 0.07);
            brilho = 0.3; aspereza = 0.4; sss = 1.0;
        } else if (m > 16.5) {   // pelagem curta, com o grao preso ao modelo
#if !defined(GL_ES)
            if (dist < 10.0) {
                vec3 fibra = cross(nr, tanRep.xyz);
                float detalhe = 1.0 - smoothstep(3.0, 10.0, dist);
                float grao = vnoise(pr * 76.0) - 0.5;
                float fase = dot(pr, fibra) * 920.0 + grao * 4.0;
                float fio = sin(fase) * (1.0 - smoothstep(0.8, 3.0, fwidth(fase)));
                albedo *= 1.0 + detalhe * (0.09 * grao + 0.025 * fio);
                nr = normalize(nr + fibra * (fio * detalhe * 0.036));
            }
#endif
            brilho = 0.32; aspereza = 0.54; veludo = 0.26;
        } else if (m < 10.5) {   // pele: poros, sardas, manchas, barba por fazer, luz que atravessa
            nr = relevo(nr, pr, vec3(420.0), 0.045);
            nr = relevo(nr, pr, vec3(1100.0), 0.025);
            albedo *= 0.94 + 0.1 * vnoise(pr * 35.0) + 0.05 * (vnoise(pr * 140.0) - 0.5);
            if (h3(floor(pr * 520.0)) > 0.993) albedo *= mix(vec3(0.78, 0.7, 0.65), vec3(1.0), smoothstep(0.3, 0.45, length(fract(pr * 520.0) - 0.5)));  // sardas (redondas)
            // barba curta: tracos finos (~1 mm) apontando para baixo, desenhados por pixel na pele
            float pel = cor.a;
            if (pel > 0.01) {
                vec3 anp = abs(nr);
                vec2 uv0 = anp.x > anp.y && anp.x > anp.z ? pr.zy : (anp.z > anp.y ? pr.xy : pr.xz);
                float tinta = 0.0;
                // sobrancelha (acima dos olhos): tracos mais longos para os lados; barba: curtos para baixo
                bool sob = tanRep.z > 0.5 && tanRep.y > 1.706 && tanRep.y < 1.732;
                vec2 d0 = sob ? normalize(vec2(tanRep.x > 0.0 ? 1.0 : -1.0, abs(tanRep.x) < 0.019 ? 2.5 : 0.25)) : vec2(0.0, -1.0);
                for (int gi = 0; gi < 2; gi++) {
                    float esc = (gi == 0 ? 2200.0 : 3000.0) * (sob ? 0.62 : 1.0);
                    float rot = gi == 0 ? 0.35 : 1.95;
                    mat2 R = mat2(cos(rot), sin(rot), -sin(rot), cos(rot));
                    vec2 uv = R * uv0 * esc;
                    vec2 cel = floor(uv);
                    vec2 f = fract(uv) - 0.5;
                    float rr = h3(vec3(cel, float(gi) * 5.0));
                    vec2 perp = vec2(-d0.y, d0.x);
                    vec2 dirS = normalize(R * (d0 + perp * 0.35 * (h3(vec3(cel, 9.0)) - 0.5)));
                    float t = clamp(dot(f, dirS), -0.45, 0.45);
                    float d = length(f - dirS * t);
                    tinta = max(tinta, step(rr, pel * 0.95) * smoothstep(0.16, 0.06, d));
                }
                albedo = mix(albedo, albedo * vec3(0.5, 0.49, 0.53), pel * 0.75);    // sombra da barba feita
                albedo = mix(albedo, albedo * (sob ? 0.18 : 0.3), mix(tinta, 0.28, smoothstep(0.8, 2.8, dist)) * (sob ? 0.95 : 0.75) * pel);  // os pelinhos
                nr = relevo(nr, pr, vec3(1800.0), 0.08 * pel * (1.0 - smoothstep(0.8, 2.8, dist)));
            }
            if (tanRep.z > 0.5) {
                // rugas finas por pixel (coordenadas do rosto, em metros)
                float fx = tanRep.x, fy = tanRep.y, ax = abs(fx);
                float ry = 0.0;
                for (int k = 0; k < 3; k++) {  // linhas da testa
                    float d = fy - (1.729 + float(k) * 0.0082 + 0.002 * sin(fx * 90.0 + float(k)));
                    ry += -d / 0.0009 * exp(-(d / 0.0009) * (d / 0.0009)) * smoothstep(0.05, 0.028, ax) * 0.5;
                }
                float dxo = ax - 0.047, dyo = fy - 1.7025;  // pes de galinha no canto do olho
                float ro = sqrt(dxo * dxo + dyo * dyo);
                float ango = atan(dyo, max(dxo, 0.0001));
                float pg = sin(ango * 9.0) * smoothstep(0.003, 0.006, ro) * smoothstep(0.018, 0.01, ro) * step(0.0, dxo);
                ry += pg * 0.35;
                float db = fy - (1.6935 - 5.0 * (ax - 0.0315) * (ax - 0.0315));  // bolsa sob o olho
                ry += -db / 0.0012 * exp(-(db / 0.0012) * (db / 0.0012)) * smoothstep(0.018, 0.008, abs(ax - 0.0315)) * 0.4;
                nr = normalize(nr + vec3(0.0, ry * 0.18, 0.0));
                float nariz = smoothstep(0.018, 0.008, ax) * smoothstep(1.65, 1.66, fy) * smoothstep(1.71, 1.70, fy);
                nr = relevo(nr, pr, vec3(700.0), 0.05 * nariz);                              // poros abertos no nariz
                albedo *= 1.0 + 0.05 * (vnoise(pr * 60.0) - 0.5);
            }
            brilho = 0.26; aspereza = 0.5; sss = 1.0;
        } else if (m < 11.5) {   // tecido: trama + fiapos
            nr = relevo(nr, pr, vec3(220.0), 0.12);
            float trama = sin(pr.x * 700.0 + pr.z * 700.0) * sin(pr.y * 700.0);
            albedo *= 0.9 + 0.08 * trama + 0.12 * vnoise(pr * 18.0);
            brilho = 0.05; aspereza = 0.9; veludo = 0.45;
        } else if (m < 12.5) {   // couro: granulado, gasto nas bordas
            nr = relevo(nr, pr, vec3(160.0), 0.1);
            float gasto = smoothstep(0.55, 0.85, vnoise(pr * 22.0));
            albedo = mix(albedo, albedo * 1.6 + 0.02, gasto * 0.5);
            brilho = m > 12.1 ? 0.21 : 0.55;
            aspereza = (m > 12.1 ? 0.56 : 0.35) + 0.3 * gasto;
        } else if (m < 13.5) {   // metal
            nr = relevo(nr, pr, vec3(90.0), 0.03);
            brilho = 1.4; aspereza = 0.18; metalico = 1.0;
        } else if (m > 14.1 && m < 14.5) {   // crina opaca: estrias com brilho na direcao dos fios
            float u = cor.a, t = tanRep.w;
            float fase = u * 520.0 + 2.6 * sin(11.0 * t + u * 21.0);
#if !defined(GL_ES)
            float filtro = 1.0 - smoothstep(0.85, 3.0, fwidth(fase));
#else
            float filtro = 1.0 - smoothstep(2.0, 5.0, dist);
#endif
            albedo *= 0.94 + 0.14 * cos(fase) * filtro + 0.10 * sin(u * 107.0 + t * 5.0);
            brilho = 0.16; aspereza = 0.64;
            kk = 0.29;
            Tcab = normalize(tanMundo);
        } else if (m < 14.5) {   // cabelo e barba: fios
            nr = relevo(nr, pr, vec3(900.0, 120.0, 900.0), 0.18);
            albedo *= 0.8 + 0.4 * vnoise(pr * vec3(900.0, 90.0, 900.0));
            brilho = 0.15; aspereza = 0.6;
        } else if (m < 15.5) {   // olho: esclera, iris com fibras, pupila; cornea molhada
            vec2 e = nr.xy;
            float rr = length(e);
            float frente = step(0.0, nr.z);
            float ang = atan(e.y, e.x);
            float fibras = vnoise(vec3(ang * 14.0, rr * 40.0, 0.0)) * 0.6 + vnoise(vec3(ang * 40.0, rr * 90.0, 3.0)) * 0.4;
            vec3 irisCor = albedo * (0.55 + 0.9 * fibras);
            irisCor = mix(irisCor, irisCor * 1.6 + vec3(0.05, 0.04, 0.0), smoothstep(0.3, 0.22, rr) * 0.6);  // anel claro perto da pupila
            irisCor *= mix(1.0, 0.35, smoothstep(0.42, 0.49, rr));                                         // anel escuro da borda
            vec3 esclera = vec3(0.78, 0.74, 0.7);
            esclera = mix(esclera, vec3(0.8, 0.55, 0.5), smoothstep(0.7, 0.95, rr) * 0.5);                // canto rosado
            esclera = mix(esclera, vec3(0.6, 0.2, 0.18), step(0.9, vnoise(vec3(ang * 25.0, rr * 8.0, 1.0))) * smoothstep(0.55, 0.9, rr) * 0.5);  // veias
            float naIris = smoothstep(0.5, 0.47, rr) * frente;
            albedo = mix(esclera, irisCor, naIris);
            albedo = mix(albedo, vec3(0.01), smoothstep(0.175, 0.15, rr) * frente);                       // pupila
            albedo *= mix(1.0, 0.45, smoothstep(0.15, 0.55, nr.y));                                       // sombra da palpebra
            if (heroi > 0.5) {
                albedo *= mix(1.0, 0.8, smoothstep(0.2, 0.42, nr.y));                                     // palpebra de cima mais funda
                float umida = smoothstep(-0.27, -0.31, nr.y) * smoothstep(-0.37, -0.33, nr.y) * frente * (1.0 - naIris);  // linha umida embaixo (so na esclera)
                albedo = mix(albedo, vec3(0.85, 0.78, 0.76), umida * 0.55);
                olhoHeroi = frente;
            }
            brilho = 2.2; aspereza = 0.04;
        } else {                 // feltro do chapeu
            nr = relevo(nr, pr, vec3(500.0), 0.08);
            albedo *= 0.92 + 0.1 * vnoise(pr * 60.0);
            brilho = 0.04; aspereza = 0.95; veludo = 0.7;
        }
        n = normalize(mc0 * nr.x + mc1 * nr.y + mc2 * nr.z);
        if (argila > 0.5) {  // material de argila cinza (como uma escultura)
            albedo = vec3(0.2);
            brilho = 0.35; aspereza = 0.4; sss = 0.0; veludo = 0.0; metalico = 0.0; kk = 0.0;
        }
    }
#endif

    // ceu e chao (hemisferica) com oclusao de ambiente
    float hemi = n.y * 0.5 + 0.5;
#if !defined(GL_ES)
    float fInt = fatorInterior(posMundo);  // 1 fora de casa
#else
    float fInt = 1.0;
#endif
    vec3 luzAmbiente = mix(ambChao, ambCeu, hemi);
    if (iluminacaoAlta > 0.5 && fInt < 0.999) {
        // Preenchimento difuso das aberturas ao dia, mais forte perto da porta.
        float dia = smoothstep(-0.04, 0.22, dirSol.y);
        float preenchimento = dia * (0.15 + 0.32 * max(fInt - 0.26, 0.0));
        luzAmbiente += ambCeu * preenchimento;
        fInt += (1.0 - fInt) * 0.08 * dia;
    }
    vec3 luz = luzAmbiente * ao * fInt;
    if (fInt < 0.999) luz += vec3(1.0, 0.72, 0.48) * 0.14 * noite * ao;  // dentro, a noite: vela/lareira
    // sol e lua (com sombra)
#if defined(SOLO_DETALHADO)
    vec3 normalSombra = normalSuperficie;
#elif !defined(GL_ES)
    vec3 normalSombra = usarFiltroSombra > 0.5 ? normalSuperficie : n;
#else
    vec3 normalSombra = n;
#endif
#ifdef PAREDES
    // Com o filtro de profundidade ativo, o atlas fornece oclusao e luz rebatida.
    bool usaSombraAtlas = paredes > 1.5 && guardada && usarFiltroSombra < 0.5;
    float sombra = usaSombraAtlas && alcance >= 1.0 ? smoothstep(0.08, 0.92, luz1.r) : sombraEm(posMundo, normalSombra);
    if (usaSombraAtlas && alcance < 1.0) sombra = mix(sombra, smoothstep(0.08, 0.92, luz1.r), alcance);
#else
    float sombra = sombraEm(posMundo, normalSombra);
#endif
#ifdef PERSONAGEM
#ifdef RAYMARCH
    // no Alto o rosto usa a sombra propria exata (abaixo) no lugar do mapa de perto (serrilhado de 3 mm)
    if (usarSombraPerto > 0.5 && economico > 0.5) {
#else
    if (usarSombraPerto > 0.5) {  // sombra de perto
#endif
        // 3 mm por texel (chapeu no rosto, braco no corpo)
        vec4 pl2 = matLuzPerto * vec4(posMundo + n * 0.004, 1.0);
        vec3 q2 = pl2.xyz / pl2.w * 0.5 + 0.5;
        if (q2.x > 0.0 && q2.x < 1.0 && q2.y > 0.0 && q2.y < 1.0 && q2.z < 1.0) {
            // PCF bilinear numa grade 3x3 (sem ruido: a penumbra do nariz e do chapeu fica lisa, sem pontilhado)
            vec2 uvT = q2.xy / texelSombraPerto - 0.5;
            vec2 fr = fract(uvT);
            vec2 base = (floor(uvT) + 0.5) * texelSombraPerto;
            // As 9 amostras bilineares compartilham 16 texels. Soma os pesos por eixo.
            float s2 = 0.0;
            for (int y = 0; y < 4; y++) {
                float wy = y == 0 ? 1.0 - fr.y : (y == 3 ? fr.y : 1.0);
                for (int x = 0; x < 4; x++) {
                    float wx = x == 0 ? 1.0 - fr.x : (x == 3 ? fr.x : 1.0);
                    vec2 uv = base + (vec2(float(x), float(y)) - 1.0) * texelSombraPerto;
                    float lit = q2.z - 0.00004 > TEX(texSombraPerto, uv).r ? 0.0 : 1.0;
                    s2 += lit * wx * wy;
                }
            }
            sombra = min(sombra, s2 / 9.0);
        }
    }
#endif
#ifdef RAYMARCH
    vec3 Lq = paraEscultura(dirSol);
    if (economico < 0.5 && sombra > 0.01) sombra = min(sombra, mix(0.22, 1.0, sombraPropria(qHit, Lq)));  // a luz rebatida na pele nao deixa a sombra preta
#endif
#ifdef PAREDES
    sombra *= sombraRelevo;
#endif
    float ndl = dot(n, dirSol);
    float difSol = mix(max(ndl, 0.0), clamp((ndl + 0.45) / 1.45, 0.0, 1.0), sss);
    if (iluminacaoAlta > 0.5 && gGrama > 0.5) {
        float contraluz = max(dot(-v, dirSol), 0.0);
        difSol += 0.18 * contraluz * contraluz * (1.0 - 0.5 * max(ndl, 0.0));
    }
    if (kk > 0.0) {  // cabelo: a luz depende da direcao do fio, nao da normal
        float TL = dot(Tcab, dirSol);
        difSol = mix(sqrt(max(1.0 - TL * TL, 0.0)) * 0.55, max(ndl, 0.0), 0.45) * smoothstep(-0.35, 0.2, ndl);
    }
    luz += corSol * difSol * mix(sombra, max(sombra, 0.35), sss * 0.5);
    luz += corLua * max(dot(n, dirLua), 0.0) * sombra;
    // luz rebatida: o chao iluminado pelo sol devolve luz colorida para cima
    float solNoChao = max(dirSol.y, 0.0);
    float reboteChao = iluminacaoAlta > 0.5 ? mix(0.38, 1.0, sombra) : 1.0;
    luz += corSol * corChao * solNoChao * (0.5 - 0.5 * n.y) * 0.9 * ao * fInt * reboteChao;
    // contorno: o ceu "abraca" as bordas (luz indireta)
    float borda = pow(1.0 - max(dot(n, v), 0.0), 3.0);
    luz += ambCeu * borda * 0.25 * (1.0 - abs(n.y)) * ao * fInt;
#ifdef PAREDES
    if (ehParede && alcance > 0.0) {
        if (paredes > 1.5 && guardada) luz += luz2 * alcance * aoCena;
        if (noite > 0.0) luz += luzJanelas(posMundo, n) * alcance;
    }
#endif
    // tochas, fogueiras, lanterna
    for (int i = 0; i < 8; i++) {
        if (i >= numLuzes) break;
        vec3 d = luzPos[i].xyz - posMundo;
        float r = luzPos[i].w;
        float dd = length(d);
        if (dd >= r) continue;
        float at = clamp(1.0 - (dd * dd) / (r * r), 0.0, 1.0);
        at = at * at / (1.0 + dd * 0.35);
        float lamb = max(dot(n, d / max(dd, 0.001)), 0.0) * 0.8 + 0.2;
        luz += luzCor[i] * at * lamb * mix(1.0, ao, 0.5);
    }
#ifdef PERSONAGEM
    if (heroi > 0.5) {
        // luz de personagem (cinema): contraluz na borda, mais forte e da cor do sol quando ele esta
        // atras; e um preenchimento suave vindo da camera que abre as sombras do rosto
        float nv = max(dot(n, v), 0.0);
        float atrasSol = max(dot(-v, dirSol), 0.0) * smoothstep(-0.05, 0.1, dirSol.y);
        vec3 corBorda = mix(ambCeu * 1.3, corSol * 0.8, atrasSol);
        luz += corBorda * pow(1.0 - nv, 3.0) * (0.3 + 0.7 * atrasSol) * 0.55 * ao;
        luz += (ambCeu * 0.7 + vec3(0.015)) * nv * nv * 0.22 * ao;
    }
#endif
    vec3 c = albedo * luz * (1.0 - metalico * 0.6);
    if (brilho > 0.0) {
        vec3 h = normalize(dirSol + v);
        float expo = mix(8.0, 900.0, pow(1.0 - aspereza, 2.0));
        float fres = 0.04 + 0.96 * pow(1.0 - max(dot(h, v), 0.0), 5.0);
        vec3 f0 = mix(vec3(fres), albedo * 1.5, metalico);
        float esp = pow(max(dot(n, h), 0.0), expo) * (expo + 8.0) / 25.0;
        c += corSol * f0 * esp * brilho * max(ndl, 0.0) * sombra * 0.25;
        // reflexo do ceu (forte no metal e nos olhos)
        vec3 r = reflect(-v, n);
        vec3 ceu = mix(corNeblina, ambCeu * 2.0, clamp(r.y * 1.5 + 0.3, 0.0, 1.0));
        float fv = 0.04 + 0.96 * pow(1.0 - max(dot(n, v), 0.0), 5.0);
        c += ceu * mix(vec3(fv), albedo, metalico) * brilho * (1.0 - aspereza) * 0.35 * ao;
        // tochas tambem brilham na superficie
        for (int i = 0; i < 8; i++) {
            if (i >= numLuzes) break;
            vec3 d = luzPos[i].xyz - posMundo;
            float dd = length(d);
            if (dd >= luzPos[i].w) continue;
            vec3 hl = normalize(d / max(dd, 0.001) + v);
            float at = clamp(1.0 - (dd * dd) / (luzPos[i].w * luzPos[i].w), 0.0, 1.0);
            c += luzCor[i] * at * at * pow(max(dot(n, hl), 0.0), expo) * brilho * 0.15;
        }
    }
    if (kk > 0.0) {
        vec3 Hk = normalize(dirSol + v);
        float TH = dot(Tcab, Hk);
        float s1 = pow(sqrt(max(1.0 - TH * TH, 0.0)), 110.0);
        float TH2 = TH + 0.14;
        float s2 = pow(sqrt(max(1.0 - TH2 * TH2, 0.0)), 32.0);
        c += corSol * sombra * smoothstep(-0.2, 0.3, ndl) * (vec3(s1 * 0.05 * kk) + albedo * s2 * 0.22);
    }
    if (olhoHeroi > 0.5) {
        // reflexo da luz de preenchimento (um ponto de luz no olho, como nas cenas de cinema) e do ceu
        vec3 hc = normalize(v + vec3(0.0, 0.25, 0.0) + dirSol * 0.15);
        c += (ambCeu * 2.0 + corSol * 0.3 + vec3(0.25)) * pow(max(dot(n, hc), 0.0), 1400.0) * 0.9;
    }
    if (veludo > 0.0) c += (ambCeu * 0.5 + corSol * max(ndl, 0.0) * sombra * 0.15) * albedo * veludo * pow(1.0 - max(dot(n, v), 0.0), 2.0);
    if (sss > 0.0) c += corSol * sombra * vec3(0.55, 0.14, 0.07) * albedo * sss * (1.0 - abs(ndl)) * 0.45;
#ifdef RAYMARCH
    {
        // brilho oleoso na zona T (testa e nariz): segundo lobulo, estreito
        float ax = abs(qHit.x);
        float oleo = smoothstep(0.045, 0.02, ax) * smoothstep(1.72, 1.735, qHit.y) * smoothstep(1.79, 1.765, qHit.y)
                   + smoothstep(0.014, 0.006, ax) * smoothstep(1.652, 1.665, qHit.y) * smoothstep(1.712, 1.695, qHit.y);
        vec3 h2 = normalize(dirSol + v);
        float fres2 = 0.04 + 0.96 * pow(1.0 - max(dot(h2, v), 0.0), 5.0);
        c += corSol * fres2 * pow(max(dot(n, h2), 0.0), 260.0) * 2.2 * oleo * max(ndl, 0.0) * sombra;
        // luz atravessando: pouca espessura + sol atras -> vermelho de sangue (orelhas, narinas, nariz)
        if (economico < 0.5 && ndl < 0.2) {
            vec2 dOlho = vec2(abs(qHit.x) - 0.0315, qHit.y - 1.703);
            float tr = finura(qHit) * smoothstep(0.012, 0.022, length(dOlho));  // palpebras: nao (olho "vermelho")
            float atras = 0.3 + 0.7 * pow(max(dot(-v, dirSol), 0.0), 3.0);
            float solAtras = sombraEm(posMundo + dirSol * 0.02, dirSol);
            c += corSol * solAtras * albedo * vec3(0.9, 0.3, 0.17) * tr * atras * smoothstep(0.2, -0.3, ndl) * 0.75;
        }
    }
#endif
#ifndef SOLO_DETALHADO
    if (extra.y > 2.5 && extra.y < 3.5) {
        // janela: vidro escuro de dia, luz quente a noite
        vec3 vidro = vec3(0.03, 0.035, 0.045) * luz + vec3(0.02);
        c = mix(vidro, albedo * 3.2, noite);
    } else if (extra.y > 1.5 && extra.y < 2.5) {
        if (noite < 0.05) discard;  // tochas apagadas de dia
        c = albedo * 6.0 * noite;  // chama
    }

    if (extra.y > 0.5 && extra.y < 1.5) {
        // agua: reflete o ceu (fresnel) + brilho do sol
        vec3 na = n;
        if (iluminacaoAlta > 0.5) {
            vec2 p = posMundo.xz;
            float fase = sin(dot(p, vec2(0.56, 0.37)) - tempoVento * 0.52);
            float onda = sin(dot(p, vec2(2.1, 1.7)) - tempoVento * 1.9 + fase * 1.25);
            float transversal = sin(dot(p, vec2(-3.5, 2.9)) - tempoVento * 2.3);
            float fina = sin(dot(p, vec2(6.6, -2.2)) - tempoVento * 3.1 + fase);
            vec2 incl = vec2(0.019, 0.015) * onda + vec2(-0.024, 0.020) * transversal;
            incl += vec2(0.015, -0.005) * fina * (1.0 - smoothstep(20.0, 55.0, dist));
            na = normalize(n + vec3(incl.x, 0.0, incl.y));
        }
        vec3 r = reflect(-v, na);
        float nv = 1.0 - clamp(dot(na, v), 0.0, 1.0);
        float fres = iluminacaoAlta > 0.5 ? 0.02 + 0.98 * pow(nv, 5.0) : pow(nv, 4.0) * 0.8 + 0.1;
        float alturaReflexo = iluminacaoAlta > 0.5 ? smoothstep(0.0, 0.72, r.y) : clamp(r.y * 2.0, 0.0, 1.0);
        vec3 ceu = mix(corNeblina, ambCeu * 2.2, alturaReflexo);
        c = mix(c, ceu, fres);
        float reflexoLuz = iluminacaoAlta > 0.5 ? sombra : 1.0;
        c += corSol * pow(max(dot(r, dirSol), 0.0), 120.0) * 1.5 * reflexoLuz;
        c += corLua * pow(max(dot(r, dirLua), 0.0), 80.0) * reflexoLuz;
    }
#endif

    float f = neblinaFator(dist);
    c = mix(c, neblinaCor(-v), f);
    // neblina baixa: acumula nos vales e nas partes mais baixas que a camera (forte ao amanhecer/entardecer)
    float baixa = exp(-max(posMundo.y - (posCamera.y - 3.0), 0.0) * 0.12);
    float fb = (1.0 - exp(-dist * 0.006 * neblinaBaixa)) * baixa;
    c = mix(c, neblinaCor(-v), fb * 0.6);
    FRAG = vec4(finalizar(c), 1.0);
}
)";

const char* VS_CEU = R"(
ATTR vec3 vertexPosition;
uniform mat4 mvp;
VARY vec3 dirLocal;
void main() {
    dirLocal = vertexPosition;
    vec4 pc = mvp * vec4(vertexPosition, 1.0);
    gl_Position = vec4(pc.xy, pc.w * 0.99999, pc.w);  // no fundo: so os pixels onde o ceu aparece sao calculados
}
)";

const char* FS_CEU = R"(
VARY vec3 dirLocal;
uniform vec3 ceuTopo;
uniform vec3 ceuHorizonte;
uniform vec3 dirLua;
uniform float noite;
uniform vec3 posCamera;
uniform float tempoVento;
uniform float passosNuvem;   // 0 = nuvens planas (Baixo); 12 Medio; 24 Alto
uniform float iluminacaoAlta;
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
uniform sampler2D texture0;  // ruido (G = R deslocado): ruido 3D com uma leitura de textura
float vruido(vec3 x) {
    vec3 p = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    vec2 uv = (p.xy + vec2(37.0, 17.0) * p.z) + f.xy;
    vec2 rg = TEX(texture0, (uv + 0.5) / 256.0).yx;
    return mix(rg.x, rg.y, f.z);
}
// nuvens cumulus: base reta, topo arredondado, bordas recortadas por ruido fino
const float NUV_BASE = 1200.0, NUV_TOPO = 2300.0;
float formaNuvem(vec3 p) {  // so as formas grandes (barato: usado tambem para a luz)
    float h = (p.y - NUV_BASE) / (NUV_TOPO - NUV_BASE);
    if (h < 0.0 || h > 1.0) return 0.0;
    vec3 q = p * 0.00045 + vec3(tempoVento * 0.0025, 0.0, tempoVento * 0.001);
    float n = vruido(q) * 0.62 + vruido(q * 2.1 + 3.7) * 0.38;
    float perfil = smoothstep(0.0, 0.07, h) * smoothstep(1.0, 0.35, h);  // base reta, topo redondo
    return clamp((n - 0.56) * 4.0 * perfil - (1.0 - perfil) * 0.25, 0.0, 1.0);
}
float densNuvem(vec3 p, int detalhe) {
    float b = formaNuvem(p);
    if (b <= 0.0 || detalhe == 0) return b;
    vec3 q = p * 0.004 + vec3(tempoVento * 0.01, tempoVento * 0.004, 0.0);
    float e = vruido(q) * 0.6 + vruido(q * 2.3) * 0.4;  // erosao nas bordas (fiapos)
    return clamp((b - e * 0.32 * (1.0 - b)) * 1.6, 0.0, 1.0);
}
void main() {
    vec3 d = normalize(dirLocal);
    float h = max(d.y, 0.0);
    vec3 c = mix(ceuHorizonte, ceuTopo, pow(h, 0.55));
    // brilho e disco do sol
    float s = max(dot(d, dirSol), 0.0);
    float disco = iluminacaoAlta > 0.5 ? smoothstep(0.999976, 0.999990, s) : smoothstep(0.9993, 0.9996, s);
    c += corSol * (pow(s, 8.0) * 0.12 + pow(s, 200.0) * 0.6 + disco * 6.0);
    // lua
    float l = max(dot(d, dirLua), 0.0);
    c += vec3(0.7, 0.75, 0.9) * (smoothstep(0.9990, 0.9993, l) * 1.6 + pow(l, 60.0) * 0.05) * noite;
    // estrelas
    vec3 q = floor(d * 280.0);
    float e = step(0.9965, hash(q)) * smoothstep(0.0, 0.25, d.y);
    c += vec3(e) * 0.9 * noite;
    // ---- nuvens volumetricas ----
    if (d.y > 0.01 && passosNuvem > -0.5) {
        vec3 cam = posCamera;
        float t0 = (NUV_BASE - cam.y) / d.y, t1 = min((NUV_TOPO - cam.y) / d.y, 30000.0);
        float cosSol = dot(d, dirSol);
        float g = 0.55;  // espalhamento para a frente: borda prateada contra o sol
        float fase = mix(1.0, (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * cosSol, 1.5), 0.5);
        if (iluminacaoAlta > 0.5) fase = min(fase, 2.7);
        vec3 luzSol = corSol * (iluminacaoAlta > 0.5 ? 1.05 : 1.6);
        vec3 ambCeuN = mix(ceuHorizonte, ceuTopo, 0.6) * 0.9 + vec3(0.6, 0.65, 0.8) * 0.06 * noite;
        float T = 1.0;
        vec3 acc = vec3(0.0);
        if (passosNuvem < 0.5) {  // Baixo: uma amostra no meio da camada
            vec3 p = cam + d * mix(t0, t1, 0.35);
            float dn = formaNuvem(p);
            acc = mix(ambCeuN * 0.8, luzSol * 0.8, 0.55 + 0.3 * dirSol.y) * dn;
            T = 1.0 - dn * 0.9;
        } else {
            float passo = (t1 - t0) / passosNuvem;
            float jit = hash(d * 937.0 + tempoVento);
            for (int i = 0; i < 24; i++) {
                if (float(i) >= passosNuvem || T < 0.04) break;
                vec3 p = cam + d * (t0 + (float(i) + jit) * passo);
                float dn = densNuvem(p, 1);
                if (dn > 0.002) {
                    // luz: nuvem entre o ponto e o sol (Beer) + "powder" (bordas internas escuras)
                    float od = formaNuvem(p + dirSol * 260.0) * 2.2;
                    float luz = exp(-od * 2.2) * (1.0 - exp(-dn * 4.0)) * 1.6;
                    float alt = clamp((p.y - NUV_BASE) / (NUV_TOPO - NUV_BASE), 0.0, 1.0);
                    vec3 col = luzSol * luz * fase + ambCeuN * (0.35 + 0.65 * alt);
                    float a = 1.0 - exp(-dn * passo * 0.012);
                    acc += col * a * T;
                    T *= 1.0 - a;
                }
            }
        }
        // longe, a nuvem some no ar (perspectiva aerea)
        float longe = smoothstep(0.01, 0.1, d.y);
        vec3 comNuvem = acc + c * T;
        c = mix(c, comNuvem, longe);
    }
    // abaixo do horizonte: neblina
    float abaixo = smoothstep(0.02, -0.12, d.y);
    c = mix(c, corNeblina, max(abaixo, smoothstep(0.25, 0.0, d.y) * 0.65));
    FRAG = vec4(finalizar(c), 1.0);
}
)";

// ---------- passe de profundidade (mapa de sombras) ----------
const char* VS_PROF = R"(
invariant gl_Position;
ATTR vec3 vertexPosition;
ATTR vec2 vertexTexCoord;
uniform mat4 mvp;
VARY float tipo;
VARY vec2 mechaUV;
void main() {
    tipo = vertexTexCoord.y;
    mechaUV = vec2(0.0);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";
const char* FS_PROF = CORTE_MECHA R"(
VARY float tipo;
VARY vec2 mechaUV;
void main() {
#ifndef PROFUNDIDADE_OPACA
    if (tipo > 1.5 && tipo < 2.5) discard;
    if (tipo > 23.5 && (tipo < 24.25 || tipo > 24.75 || foraDaMecha(mechaUV))) discard;
#endif
    FRAG = vec4(1.0);
}
)";


// ---------- material das paredes, assado em texturas na abertura (repete a cada 4 m) ----------
// saida 0: alturas (R reboco, G madeira, B pedra; 0 = face, 1 = 3 cm de fundo)
// saida 1: fatores de cor / 2 (R reboco, G madeira, B pedra)
// saida 2: pesos (R reboco caido, G tom da pedra exposta / 2, B argamassa, A desgaste da pintura)
const char* FS_ASSAR_MATERIAL = R"(
uniform float saida;
uniform float tamTex;
float hashA(vec2 p) {
    p += vec2(271.3, 517.9);  // o hash e ruim com inteiros pequenos (0..per): desloca para longe da origem
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
// ruido periodico: o reticulado da a volta a cada "per" celulas
float vp(vec2 p, vec2 per) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    vec2 i0 = mod(i, per), i1 = mod(i + 1.0, per);
    return mix(mix(hashA(i0), hashA(vec2(i1.x, i0.y)), f.x), mix(hashA(vec2(i0.x, i1.y)), hashA(i1), f.x), f.y);
}
float vpf(vec2 uv, float fr, vec2 desl) { return vp(uv * fr + desl, vec2(fr * 4.0)); }
float hTabua(vec2 uv, out float id) {
    float alt = 0.2, row = floor(uv.y / alt), fy = fract(uv.y / alt);
    float rowM = mod(row, 20.0);
    float x = uv.x + hashA(vec2(rowM, 7.0)) * 3.0;
    float seg = floor(x / 2.0), fx = fract(x / 2.0);
    id = hashA(vec2(rowM, mod(seg, 2.0) + 31.0));
    float face = smoothstep(0.0, 0.04, fy) * smoothstep(1.0, 0.96, fy) * smoothstep(0.0, 0.004, fx) * smoothstep(1.0, 0.996, fx);
    float topo = -0.005 + 0.0035 * sin(fy * 3.1416) - 0.003 * id + 0.0012 * vp(vec2(x * 2.0, uv.y * 70.0 + id * 9.0), vec2(8.0, 280.0));
    return mix(-0.026, topo, face);
}
float hPedra(vec2 uv, out float id) {
    vec2 tam = vec2(0.4, 4.0 / 15.0);
    float row = floor(uv.y / tam.y);
    float rowM = mod(row, 15.0);
    vec2 s = vec2(uv.x / tam.x + hashA(vec2(rowM, 3.0)) * 0.9 + mod(row, 2.0) * 0.5, uv.y / tam.y);
    s.x += (vp(vec2(s.x * 0.8, rowM * 3.0), vec2(8.0, 45.0)) - 0.5) * 0.7;
    vec2 c = floor(s), f = fract(s);
    id = hashA(mod(c, vec2(10.0, 15.0)) + 17.0);
    vec2 e = min(f, 1.0 - f) * tam;
    float borda = min(e.x, e.y) + (vpf(uv, 9.0, vec2(0.0)) - 0.5) * 0.014;
    float m = smoothstep(0.003, 0.018, borda);
    float face = -0.014 + 0.011 * smoothstep(0.0, 0.07, borda) - 0.004 * vpf(uv, 7.0, vec2(id * 13.0)) - 0.0015 * vpf(uv, 30.0, vec2(0.0));
    return mix(-0.029, face, m);
}
float hReboco(vec2 uv, out float caiu, out float id) {
    float h = -0.0025 * vpf(uv, 1.75, vec2(0.0)) - 0.0009 * vpf(uv, 14.0, vec2(0.0));
    float rach = abs(vpf(uv, 3.25, vec2(0.0)) + 0.5 * vpf(uv, 7.0, vec2(3.0)) - 0.75);
    h -= 0.004 * smoothstep(0.007, 0.0, rach) * smoothstep(0.6, 0.75, vpf(uv, 2.25, vec2(9.0))) * step(0.6, vpf(uv, 1.0, vec2(5.0)));
    // deslocamento e limiar escolhidos para o ladrilho ter a proporcao de reboco caido do original (~4%)
    float pc = vpf(uv, 0.75, vec2(26.0, 9.62)) * 0.7 + vpf(uv, 3.5, vec2(0.0)) * 0.3;
    caiu = smoothstep(0.77, 0.80, pc);
    id = 0.0;
    if (caiu <= 0.0) return h;
    float hp = hPedra(uv, id);
    return mix(h, -0.012 + hp * 0.6, caiu);
}
void main() {
    vec2 uv = gl_FragCoord.xy / tamTex * 4.0;  // metros dentro do ladrilho de 4 m
    float idR, caiu, idM, idP;
    float hR = hReboco(uv, caiu, idR), hM = hTabua(uv, idM), hP = hPedra(uv, idP);
    if (saida < 0.5) { FRAG = vec4(-hR / 0.03, -hM / 0.03, -hP / 0.03, 1.0); return; }
    if (saida < 1.5) {
        float fR = (0.86 + 0.2 * vpf(uv, 0.5, vec2(0.0)) - 0.1 * vpf(uv, 2.25, vec2(0.0)) + 0.06 * vpf(uv, 9.0, vec2(0.0)))
                 * (1.0 - 0.14 * smoothstep(0.55, 0.9, vp(vec2(uv.x * 7.0, uv.y * 0.25), vec2(28.0, 1.0))));
        float fM = (0.72 + 0.5 * idM) * (0.8 + 0.32 * vp(vec2(uv.x * 2.0, uv.y * 60.0 + idM * 7.0), vec2(8.0, 240.0)));
        float fP = (0.72 + 0.5 * idP) * (0.9 + 0.2 * vpf(uv, 11.0, vec2(0.0)));
        FRAG = vec4(fR * 0.5, fM * 0.5, fP * 0.5, 1.0);
        return;
    }
    float tomP = mix(1.25, 0.8 + 0.4 * idR, smoothstep(-0.026, -0.018, hR + 0.012));
    float arg = smoothstep(-0.02, -0.026, hP);
    float gasto = smoothstep(0.4, 0.8, vpf(uv, 0.25, vec2(40.0)));
    FRAG = vec4(caiu, tomP * 0.5, arg, gasto);
}
)";


// Coberturas de telhado assadas numa textura RGBA (ladrilho de 4 m, 1024 texels): palha (R altura,
// G cor) e telhas (B altura, A cor). Coordenadas: x ao longo da cumeeira, y subindo a agua (m).
// Altura guardada como profundidade 0..1 (0 = mais alto); cor como fator / 2.
const char* FS_ASSAR_TELHADO = R"(
uniform float tamTex;
float hashA(vec2 p) {
    p += vec2(271.3, 517.9);
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float vp(vec2 p, vec2 per) {  // ruido periodico: o reticulado da a volta a cada "per" celulas
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    vec2 i0 = mod(i, per), i1 = mod(i + 1.0, per);
    return mix(mix(hashA(i0), hashA(vec2(i1.x, i0.y)), f.x), mix(hashA(vec2(i0.x, i1.y)), hashA(i1), f.x), f.y);
}
// Palha penteada: fios ao longo da agua, em camadas de 0,4 m; a ponta de cada camada fica saliente
// e desfiada (borda irregular), o fundo logo abaixo dela fica na sombra.
vec2 palha(vec2 uv) {
    float fio = vp(vec2(uv.x * 55.0, uv.y * 1.2), vec2(220.0, 4.8));
    float fio2 = vp(vec2(uv.x * 130.0 + fio * 3.0, uv.y * 2.5), vec2(520.0, 10.0));
    // borda da camada: ondula devagar e desfia pouco (fios de comprimentos diferentes)
    float camada = uv.y / 0.4 + (vp(vec2(uv.x * 1.25, 0.0), vec2(5.0, 1.0)) - 0.5) * 0.5 + (fio - 0.5) * 0.12;
    float fy = fract(camada), id = hashA(vec2(mod(floor(camada), 10.0), 3.0));
    float degrau = (1.0 - fy) * smoothstep(0.0, 0.12, fy);    // 1 na ponta da camada, 0 onde a de cima cobre
    float alto = 0.45 * degrau + 0.35 * fio + 0.2 * fio2;
    float manchas = vp(uv * 1.5, vec2(6.0));                    // palha velha (acinzentada) em manchas
    float cor = (0.72 + 0.26 * fio2 + 0.14 * fio) * (0.92 + 0.16 * id) * mix(1.0, 0.75, smoothstep(0.5, 0.85, manchas));
    cor *= mix(0.78, 1.0, smoothstep(0.0, 0.3, fy));            // a ponta da camada de cima escurece a de baixo
    return vec2(1.0 - alto, cor * 0.5);
}
// Telhas capa e canal em fileiras de 0,4 m: a capa (convexa) cobre a junta de dois canais
// (concavos); cada telha e mais alta na boca, que faz sombra na fileira de baixo.
vec2 telha(vec2 uv) {
    float t = fract(uv.x / 0.5), col = mod(floor(uv.x / 0.5), 8.0);
    bool capa = t < 0.42;
    float desl = (hashA(vec2(col, capa ? 1.0 : 2.0)) - 0.5) * 0.06 + (capa ? 0.2 : 0.0);  // capa alterna a fileira
    float fila = (uv.y + desl) / 0.4, fy = fract(fila);
    float id = hashA(vec2(col * 2.0 + (capa ? 0.0 : 1.0), mod(floor(fila), 10.0)));
    float perfil;
    if (capa) { float q = t / 0.42 * 2.0 - 1.0; perfil = 0.45 + 0.55 * sqrt(max(1.0 - q * q, 0.0)); }
    else { float q = (t - 0.42) / 0.58 * 2.0 - 1.0; perfil = 0.35 * (1.0 - sqrt(max(1.0 - q * q, 0.0))); }
    float boca = (1.0 - fy) * smoothstep(0.0, 0.05, fy);        // borda de baixo de cada telha
    float alto = 0.6 * perfil + 0.4 * boca + 0.03 * vp(uv * 40.0, vec2(160.0));
    float cor = (0.78 + 0.42 * id) * (0.92 + 0.12 * vp(uv * 9.0, vec2(36.0)));
    if (!capa) cor *= 0.86;                                     // sujeira escorre no canal
    cor *= mix(0.5, 1.0, smoothstep(0.0, 0.22, fy));            // sombra da telha de cima
    cor = mix(cor, cor * 0.7, smoothstep(0.7, 0.9, vp(uv * 0.9, vec2(3.6))) * (1.0 - perfil * 0.5));  // limo
    return vec2(1.0 - alto, cor * 0.5);
}
void main() {
    vec2 uv = gl_FragCoord.xy / tamTex * 4.0;
    FRAG = vec4(palha(uv), telha(uv));
}
)";

// ---------- luz das paredes guardada por face: desenhada no atlas, algumas faces por quadro ----------
const char* FS_LUZ_PAREDE = R"(
uniform vec3 dirSol;
uniform vec3 corSol;
uniform vec3 dirSombra;
uniform vec3 faceO;
uniform vec3 faceU;
uniform vec3 faceN;
uniform vec2 faceTam;     // largura, altura (m)
uniform vec4 faceRet;     // retangulo no atlas (texels, com borda)
uniform float metroTexel;
uniform float saida;      // 0: sombra e oclusao; 1: rebote
uniform float calcularSombra;
)" CENA_SDF R"(
void main() {
    vec2 t = gl_FragCoord.xy - faceRet.xy - 1.0;            // texel dentro da face (a borda repete a beirada)
    vec2 m = clamp(t * metroTexel, vec2(0.0), faceTam);
    vec3 p = faceO + faceU * m.x + vec3(0.0, m.y, 0.0);
    if (saida < 0.5) {
        float sc = calcularSombra > 0.5 && dot(faceN, dirSombra) > 0.0 ? sombraCena(p + faceN * 0.03, dirSombra) : 0.0;
        FRAG = vec4(sc, oclusaoCena(p + faceN * 0.02, faceN), 0.0, 1.0);
    } else {
        vec3 r = dot(corSol, corSol) > 0.0 ? reboteCena(p + faceN * 0.03, faceN, normalize(cross(vec3(0.0, 1.0, 0.0), faceN))) : vec3(0.0);
        FRAG = vec4(r * 0.5, 1.0);
    }
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
// copia do fundo para a profundidade de campo: so o que esta longe entra (alfa = peso), para o
// heroi e o que esta perto nao "vazarem" sobre o fundo desfocado (halo)
#define PROFUNDIDADE_CAMERA R"(
uniform vec2 profCena;  // m10 e m14 da matriz de projecao ativa
float profLinear(float d) {
    return profCena.y / (d * 2.0 - 1.0 + profCena.x);
}
)"
const char* FS_LONGE = PROFUNDIDADE_CAMERA R"(
uniform sampler2D texture0;
uniform sampler2D texProf;
uniform float distFoco;
void main() {
    float dist = profLinear(TEX(texProf, fragTexCoord).r);
    float w = smoothstep(distFoco + 12.0, distFoco + 25.0, dist);
    if (w <= 0.0) { FRAG = vec4(0.0); return; }
    FRAG = vec4(TEX(texture0, fragTexCoord).rgb * w, w);
}
)";
// desfoque que conserva o alfa (pesos do fundo)
const char* FS_BLUR4 = R"(
uniform sampler2D texture0;
uniform vec2 direcao;
void main() {
    vec4 c = TEX(texture0, fragTexCoord) * 0.227;
    c += TEX(texture0, fragTexCoord + direcao * 1.38) * 0.316;
    c += TEX(texture0, fragTexCoord - direcao * 1.38) * 0.316;
    c += TEX(texture0, fragTexCoord + direcao * 3.23) * 0.070;
    c += TEX(texture0, fragTexCoord - direcao * 3.23) * 0.070;
    FRAG = c;
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
uniform sampler2D texture0;   // cena
uniform sampler2D texProf;    // profundidade (ceu = 1)
uniform float usarProf;
uniform vec2 posSol;          // posicao do sol na tela (0..1)
uniform float aspecto;
uniform vec3 corRaio;
uniform float forca;
// fonte dos raios: so o ceu (o que tapa o ceu vira sombra no facho), mais forte perto do sol
vec3 fonte(vec2 uv) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec3(0.0);
    if (usarProf > 0.5 && TEX(texProf, uv).r < 0.99995) return vec3(0.0);
    vec3 c = TEX(texture0, uv).rgb;
    float ceu = usarProf > 0.5 ? 1.0 : smoothstep(0.55, 0.9, dot(c, vec3(0.3, 0.59, 0.11)));
    if (ceu <= 0.0) return vec3(0.0);
    vec2 dd = (uv - posSol) * vec2(aspecto, 1.0);
    float perto = exp(-dot(dd, dd) * 9.0);
    return (c * 0.55 + corRaio * 0.25) * ceu * perto;
}
void main() {
    vec2 uv = fragTexCoord;
    vec2 d = (uv - posSol) * (1.0 / 40.0);
    vec3 c = vec3(0.0);
    float peso = 1.0;
    for (int i = 0; i < 40; i++) {
        c += fonte(uv) * peso;
        uv -= d;
        peso *= 0.968;
    }
    FRAG = vec4(c * (forca / 40.0), 1.0);
}
)";
// Espalhamento do sol no ar. A marcha termina na primeira superficie da cena.
const char* FS_RAIOS_VOLUME = PROFUNDIDADE_CAMERA R"(
uniform sampler2D texProf;
uniform sampler2DShadow texSombraFiltrada;
uniform mat4 invCameraVP;
uniform mat4 matLuz;
uniform vec3 posCamera;
uniform vec3 dirSol;
uniform vec3 corRaio;
uniform float alcanceVolume;

void main() {
    float z = TEX(texProf, fragTexCoord).r;
    vec4 fim = invCameraVP * vec4(fragTexCoord * 2.0 - 1.0, z * 2.0 - 1.0, 1.0);
    vec3 raio = fim.xyz / fim.w - posCamera;
    float distancia = length(raio);
    vec3 dir = raio / max(distancia, 0.001);
    float alcance = min(distancia, alcanceVolume);
    float passo = alcance * 0.1;

    vec3 q = (matLuz * vec4(posCamera, 1.0)).xyz * 0.5 + 0.5;
    vec3 dq = (matLuz * vec4(dir * passo, 0.0)).xyz * 0.5;
    q += dq * 0.5;
    float queda = exp(-max(dir.y, 0.0) * passo * 0.06);
    float peso = sqrt(queda), luz = 0.0;
    for (int i = 0; i < 10; i++) {
        luz += TEX(texSombraFiltrada, q) * peso;
        q += dq;
        peso *= queda;
    }
    // Henyey-Greenstein com g=0,6, limitado para conter o brilho perto do disco.
    float fase = min(3.0, 0.64 / pow(1.36 - 1.2 * dot(dir, dirSol), 1.5));
    float dens = mix(0.00235, 0.0009, smoothstep(0.15, 0.75, dirSol.y));
    float espalhamento = min(luz * passo * dens * fase, 0.16);
    float prof = profLinear(z) * (alcance / max(distancia, 0.001));
    FRAG = vec4(espalhamento * corRaio, clamp(prof / alcanceVolume, 0.0, 1.0));
}
)";

const char* FS_JUNTA = PROFUNDIDADE_CAMERA R"(
uniform sampler2D texture0;
uniform sampler2D texBrilho;
uniform sampler2D texRaios;
uniform sampler2D texAO;
uniform float forcaBrilho;
uniform float usarRaios;
uniform float usarAO;
uniform float alcanceAO;
#if !defined(GL_ES)
uniform float usarVolume;
uniform float alcanceVolume;
#else
const float usarVolume = 0.0;
#endif
uniform vec2 posSol;
uniform float aspecto;
uniform vec3 corRaio;
// camera de cinema (Alto): profundidade de campo, granulacao e aberracao cromatica
uniform sampler2D texProf;      // profundidade da cena
uniform sampler2D texDesfoque;  // cena desfocada (meia resolucao)
uniform float usarFoco;
uniform float distFoco;         // distancia da camera ao heroi
uniform float forcaFoco;
uniform float usarGrao;
uniform float tempo;
uniform vec2 texelTela;
float hashJ(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    vec3 c = TEX(texture0, fragTexCoord).rgb;
    if (usarGrao > 0.5) {  // aberracao: vermelho e azul se separam um pouco nas bordas da tela
        vec2 dc = fragTexCoord - 0.5;
        vec2 off = dc * dot(dc, dc) * 1.8 * texelTela.x;
        c.r = TEX(texture0, fragTexCoord + off).r;
        c.b = TEX(texture0, fragTexCoord - off).b;
    }
    float prof = 0.0;
    if (usarFoco > 0.5 || usarAO > 0.5 || usarVolume > 0.5)
        prof = profLinear(TEX(texProf, fragTexCoord).r);
    if (usarFoco > 0.5) {  // o fundo distante desfoca; o heroi e o que esta perto dele nunca
        float coc = smoothstep(distFoco + 25.0, distFoco + 150.0, prof) * forcaFoco;
        if (coc > 0.0) {
            vec4 bf = TEX(texDesfoque, fragTexCoord);  // fundo desfocado, pre-multiplicado pelo peso (alfa)
            if (bf.a > 0.02) c = mix(c, bf.rgb / bf.a, coc * smoothstep(0.02, 0.35, bf.a));
        }
    }
    if (usarAO > 0.5 && prof < alcanceAO) {
        vec2 ao = TEX(texAO, fragTexCoord).rg;
        float peso = 1.0 - smoothstep(0.3, 1.2, abs(prof - ao.g * alcanceAO));
        c *= mix(1.0, ao.r, peso);
    }
    if (forcaBrilho > 0.0) c += TEX(texBrilho, fragTexCoord).rgb * forcaBrilho;
    if (usarRaios > 0.5) {
        vec4 raio = TEX(texRaios, fragTexCoord);
#if !defined(GL_ES)
        if (usarVolume > 0.5) {
            // Evita que o facho da meia resolucao vaze sobre silhuetas em primeiro plano.
            float peso = 1.0 - smoothstep(0.4, 1.8, raio.a * alcanceVolume - prof);
            c = sqrt(c * c + raio.rgb * peso);
        } else
#endif
            c += raio.rgb;
    }
    if (usarRaios > 0.5 && all(greaterThanEqual(posSol, vec2(0.0))) && all(lessThanEqual(posSol, vec2(1.0)))) {
        // reflexos da lente: o sol visivel deixa "fantasmas" na linha que passa pelo centro da tela
        vec3 solTela = TEX(texture0, clamp(posSol, 0.001, 0.999)).rgb;
        float vis = smoothstep(0.75, 0.98, dot(solTela, vec3(0.33)));
        vec2 eixo = vec2(0.5) - posSol;
        for (int k = 0; k < 4; k++) {
            float fk = k == 0 ? 0.45 : (k == 1 ? 0.9 : (k == 2 ? 1.35 : 1.75));
            float rk = k == 0 ? 0.035 : (k == 1 ? 0.06 : (k == 2 ? 0.025 : 0.09));
            vec3 ck = k == 0 ? vec3(0.9, 0.6, 0.3) : (k == 1 ? vec3(0.4, 0.8, 0.5) : (k == 2 ? vec3(0.7, 0.5, 0.9) : vec3(0.3, 0.5, 0.9)));
            vec2 g = (fragTexCoord - (posSol + eixo * fk)) * vec2(aspecto, 1.0);
            float dg = length(g);
            c += ck * corRaio * 0.035 * vis * (1.0 - smoothstep(rk * 0.6, rk, dg));
        }
        vec2 h = (fragTexCoord - posSol) * vec2(aspecto, 1.0);  // halo em volta do sol
        c += corRaio * vis * 0.065 * exp(-length(h) * 6.0);
    }
    // Contraste suave para manter detalhes na sombra e no ceu claro.
    c = clamp(c, 0.0, 1.0);
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(lum), c, 0.92);
    c = mix(c, c * c * (3.0 - 2.0 * c), 0.17);
    // vinheta suave
    vec2 v = fragTexCoord - 0.5;
    c *= 1.0 - dot(v, v) * 0.15;
    if (usarGrao > 0.5) {  // granulacao de filme: fina, mais visivel nos meios-tons
        float g = hashJ(fragTexCoord / texelTela + fract(tempo * 7.31) * 113.0) - 0.5;
        float lum2 = dot(c, vec3(0.3, 0.59, 0.11));
        c += g * 0.019 * (1.0 - abs(lum2 - 0.5) * 1.4);
    }
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
ATTR vec4 vertexTangent;
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

#define FISICA_MECHA R"(
uniform vec3 cabeloMeio[3], cabeloPonta[3];
uniform float escalaCabelo;
vec3 deslocarMecha(float w) {
    if (escalaCabelo <= 0.0 || w <= 0.08) return vec3(0.0);
    vec3 p = vertexPosition / escalaCabelo;
    // Mistura os dois guias laterais com o da nuca no espaco de repouso.
    float esquerda = smoothstep(-0.025, 0.025, p.x);
    float nuca = 1.0 - smoothstep(-0.036, 0.071, p.z);
    vec3 meio = mix(mix(cabeloMeio[1], cabeloMeio[0], esquerda), cabeloMeio[2], nuca);
    vec3 ponta = mix(mix(cabeloPonta[1], cabeloPonta[0], esquerda), cabeloPonta[2], nuca);
    float t = smoothstep(0.08, 0.63, w);
    vec3 desloc = mix(meio * (2.0 * t), mix(meio, ponta, 2.0 * t - 1.0), smoothstep(0.42, 0.58, t));

    // Limita o movimento de cada vertice contra o cranio sem alterar o encaixe em repouso.
    vec3 raio = vec3(0.077, 0.095, 0.103);
    vec3 q0 = (p - vec3(0.0, 1.698, 0.055)) / raio;
    vec3 q = q0 + desloc / (escalaCabelo * raio);
    float base = min(1.0, dot(q0, q0)), d = dot(q, q);
    if (d < base) desloc = (q * sqrt(base / max(1e-5, d)) - q0) * raio * escalaCabelo;
    return desloc;
}
)"

const char* VS_PELE = CORPO_SKIN FISICA_MECHA R"(
VARY vec4 tanRep;
VARY vec3 tanMundo;
VARY vec3 posRep;
VARY vec3 nRep;
VARY vec3 mc0;
VARY vec3 mc1;
VARY vec3 mc2;
VARY vec3 posMundo;
VARY vec3 normalMundo;
VARY vec4 cor;
VARY vec2 extra;
void main() {
    mat4 S = matrizPele();
    vec4 pl = S * vec4(vertexPosition, 1.0);
    vec3 tangente = mat3(S) * vertexTangent.xyz;
    vec3 normal = normalize(mat3(S) * vertexNormal);
    if (vertexTexCoord.y > 24.25 && vertexTexCoord.y < 24.75) {
        float w = clamp(length(vertexTangent.xyz) - 0.05, 0.0, 1.0);
        vec3 desloc = mat3(S) * deslocarMecha(w);
        pl.xyz += desloc;
        vec3 fio = normalize(tangente + desloc * 2.8);
        normal = normalize(normal - fio * dot(normal, fio) * w);
        tangente = fio;
    }
    vec4 p = matModel * pl;
    posMundo = p.xyz;
    mat3 mn = mat3(matModel) * mat3(S);
    normalMundo = normalize(mat3(matModel) * normal);
    cor = vertexColor;
    extra = vertexTexCoord;
    posRep = vertexPosition;
    nRep = vertexNormal;
    tanRep = vertexTangent;
    tanMundo = mat3(matModel) * tangente;
    mc0 = mn[0];
    mc1 = mn[1];
    mc2 = mn[2];
    gl_Position = mvp * pl;
}
)";

// caixa da cabeca (raymarching): so leva a posicao ao mundo; o fragment shader faz o resto
const char* VS_CABECA = CORPO_SKIN R"(
VARY vec3 posCaixa;
void main() {
    vec4 pl = matrizPele() * vec4(vertexPosition, 1.0);
    posCaixa = (matModel * pl).xyz;
    gl_Position = mvp * pl;
}
)";

const char* VS_PROF_PELE = CORPO_SKIN FISICA_MECHA R"(
VARY float tipo;
VARY vec2 mechaUV;
void main() {
    tipo = vertexTexCoord.y;
    mechaUV = vec2(vertexColor.a, vertexTangent.w);
    mat4 S = matrizPele();
    vec4 pl = S * vec4(vertexPosition, 1.0);
    if (tipo > 24.25 && tipo < 24.75) {
        float w = clamp(length(vertexTangent.xyz) - 0.05, 0.0, 1.0);
        pl.xyz += mat3(S) * deslocarMecha(w);
    }
    gl_Position = mvp * pl;
}
)";

// ---------- oclusao de ambiente em tela (SSAO) ----------
const char* FS_SSAO = PROFUNDIDADE_CAMERA R"(
uniform sampler2D texture0;   // profundidade da cena
uniform vec2 texel;           // 1 / resolucao da profundidade
uniform float escalaProj;     // pixels por metro a 1 m de distancia
uniform vec2 tanMeio;         // tan(fov/2) * (aspecto, 1)
uniform float alcanceAO;
vec3 posVista(vec2 uv, float z) {
    return vec3((uv * 2.0 - 1.0) * tanMeio * z, -z);
}
vec3 posVista(vec2 uv) { return posVista(uv, profLinear(TEX(texture0, uv).r)); }
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    vec2 uv = fragTexCoord;
    float z0 = profLinear(TEX(texture0, uv).r);
    if (z0 >= alcanceAO) { FRAG = vec4(1.0); return; }
    vec3 P = posVista(uv, z0);
    // normal reconstruida pelos vizinhos (usa o lado com menor salto de profundidade)
    vec2 t3 = texel * 3.0;
    vec3 px1 = posVista(uv + vec2(t3.x, 0.0)) - P, px2 = P - posVista(uv - vec2(t3.x, 0.0));
    vec3 py1 = posVista(uv + vec2(0.0, t3.y)) - P, py2 = P - posVista(uv - vec2(0.0, t3.y));
    vec3 dx = abs(px1.z) < abs(px2.z) ? px1 : px2;
    vec3 dy = abs(py1.z) < abs(py2.z) ? py1 : py2;
    vec3 n = normalize(cross(dx, dy));
    if (n.z < 0.0) n = -n;
    float raio = min(0.7 * escalaProj / z0, 80.0);
    float giro = hash(uv * 913.7) * 6.2831;
    float soma = 0.0;
    for (int i = 0; i < 12; i++) {
        float a = float(i) * 2.39996 + giro;
        float r = sqrt((float(i) + 0.5) / 12.0) * raio;
        vec3 v = posVista(uv + vec2(cos(a), sin(a)) * r * texel) - P;
        float dist = length(v);
        float alcance = 1.0 - smoothstep(0.7, 1.4, dist);
        soma += max(0.0, dot(v, n) - 0.03 * z0) / (dot(v, v) + 0.02) * alcance;
    }
    float ao = clamp(1.0 - soma * (1.05 / 12.0), 0.0, 1.0);
    float peso = 1.0 - smoothstep(18.0, alcanceAO, z0);
    FRAG = vec4(mix(1.0, ao * ao, peso), z0 / alcanceAO, 0.0, 1.0);
}
)";

// O canal verde guarda a profundidade da amostra. O desfoque respeita silhuetas.
const char* FS_BLUR_AO = R"(
uniform sampler2D texture0;
uniform vec2 direcao;
uniform float alcanceAO;
void main() {
    vec2 centro = TEX(texture0, fragTexCoord).rg;
    if (centro.g >= 1.0) { FRAG = vec4(1.0); return; }
    float soma = centro.r * 0.4, pesos = 0.4;
    for (int i = -2; i <= 2; i++) {
        if (i == 0) continue;
        vec2 a = TEX(texture0, fragTexCoord + direcao * float(i)).rg;
        float w = (abs(float(i)) == 1.0 ? 0.24 : 0.06) * (1.0 - smoothstep(0.2, 1.0, abs(a.g - centro.g) * alcanceAO));
        soma += a.r * w;
        pesos += w;
    }
    FRAG = vec4(soma / pesos, centro.g, 0.0, 1.0);
}
)";
