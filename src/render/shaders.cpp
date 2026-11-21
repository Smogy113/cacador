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

Shader carregarShader(const char* vs, const char* fs) { return carregarShaderDef(vs, fs, ""); }

Shader carregarShaderDef(const char* vs, const char* fs, const char* defs) {
    std::string v = std::string(CAB_VS) + defs + vs;
    std::string f = std::string(CAB_FS) + defs + COMUM + fs;
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

#ifdef PERSONAGEM
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
#ifdef PERSONAGEM
    if (usarSombraPerto > 0.5 && distBloq < 0.8) return 1.0;  // o proprio corpo: fica com o mapa de perto
#endif
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


#ifdef PERSONAGEM
VARY vec3 posRep;   // posicao de repouso (o detalhe acompanha o corpo)
VARY vec3 nRep;
VARY vec3 mc0;      // colunas da matriz que leva normais do repouso para o mundo
VARY vec3 mc1;
VARY vec3 mc2;
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
void main() {
    vec3 n = normalize(normalMundo);
    vec3 albedo = pow(cor.rgb, vec3(2.2));
    float ao = extra.x;
    vec3 paraCam = posCamera - posMundo;
    float dist = length(paraCam);
    vec3 v = paraCam / max(dist, 0.001);
    if (descartarPerto > 0.0 && length(paraCam.xz) < descartarPerto) discard;

    // parametros de material (mundo: tudo zero)
    float brilho = 0.0, aspereza = 1.0, sss = 0.0, veludo = 0.0, metalico = 0.0;
#ifdef PERSONAGEM
    float m = extra.y;
    if (m > 9.5) {
        vec3 nr = normalize(nRep);
        vec3 pr = posRep;
        if (m > 19.5) {          // casca de pelo (barba, sobrancelha, cabelo, cilios): recorta em fios
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
            brilho = 0.35; aspereza = 0.4;
            ao *= 0.6 + 0.4 * h;
        } else if (m > 17.5 && m < 18.5) {  // labios: marcas verticais, umidos
            nr = relevo(nr, pr, vec3(120.0, 900.0, 120.0), 0.07);
            brilho = 0.45; aspereza = 0.34; sss = 1.0;
        } else if (m < 10.5) {   // pele: poros, sardas, manchas, barba por fazer, luz que atravessa
            nr = relevo(nr, pr, vec3(420.0), 0.05);
            nr = relevo(nr, pr, vec3(1100.0), 0.025);
            albedo *= 0.94 + 0.1 * vnoise(pr * 35.0) + 0.05 * (vnoise(pr * 140.0) - 0.5);
            if (h3(floor(pr * 520.0)) > 0.993) albedo *= vec3(0.78, 0.7, 0.65);              // sardas
            float barbaRala = step(0.5, h3(floor(pr * 2600.0))) * cor.a;
            albedo *= 1.0 - 0.45 * barbaRala;                                                // pontinhos da barba
            brilho = 0.32; aspereza = 0.45; sss = 1.0;
        } else if (m < 11.5) {   // tecido: trama + fiapos
            nr = relevo(nr, pr, vec3(220.0), 0.12);
            float trama = sin(pr.x * 700.0 + pr.z * 700.0) * sin(pr.y * 700.0);
            albedo *= 0.9 + 0.08 * trama + 0.12 * vnoise(pr * 18.0);
            brilho = 0.05; aspereza = 0.9; veludo = 0.45;
        } else if (m < 12.5) {   // couro: granulado, gasto nas bordas
            nr = relevo(nr, pr, vec3(160.0), 0.1);
            float gasto = smoothstep(0.55, 0.85, vnoise(pr * 22.0));
            albedo = mix(albedo, albedo * 1.6 + 0.02, gasto * 0.5);
            brilho = 0.55; aspereza = 0.35 + 0.3 * gasto;
        } else if (m < 13.5) {   // metal
            nr = relevo(nr, pr, vec3(90.0), 0.03);
            brilho = 1.4; aspereza = 0.18; metalico = 1.0;
        } else if (m < 14.5) {   // cabelo e barba: fios
            nr = relevo(nr, pr, vec3(900.0, 120.0, 900.0), 0.18);
            albedo *= 0.8 + 0.4 * vnoise(pr * vec3(900.0, 90.0, 900.0));
            brilho = 0.4; aspereza = 0.35;
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
            brilho = 2.2; aspereza = 0.04;
        } else {                 // feltro do chapeu
            nr = relevo(nr, pr, vec3(500.0), 0.08);
            albedo *= 0.92 + 0.1 * vnoise(pr * 60.0);
            brilho = 0.04; aspereza = 0.95; veludo = 0.7;
        }
        n = normalize(mc0 * nr.x + mc1 * nr.y + mc2 * nr.z);
    }
#endif

    // ceu e chao (hemisferica) com oclusao de ambiente
    float hemi = n.y * 0.5 + 0.5;
    vec3 luz = mix(ambChao, ambCeu, hemi) * ao;
    // sol e lua (com sombra)
    float sombra = sombraEm(posMundo, n);
#ifdef PERSONAGEM
    if (usarSombraPerto > 0.5) {  // sombra de perto: 3 mm por texel (chapeu no rosto, braco no corpo)
        vec4 pl2 = matLuzPerto * vec4(posMundo + n * 0.004, 1.0);
        vec3 q2 = pl2.xyz / pl2.w * 0.5 + 0.5;
        if (q2.x > 0.0 && q2.x < 1.0 && q2.y > 0.0 && q2.y < 1.0 && q2.z < 1.0) {
            float giro2 = hash12(posMundo.xz * 91.7) * 6.2831;
            float s2 = 0.0;
            for (int i = 0; i < 10; i++) {
                float a2 = float(i) * 2.39996 + giro2;
                float r2 = sqrt((float(i) + 0.5) / 10.0) * 2.2;
                float d2 = TEX(texSombraPerto, q2.xy + vec2(cos(a2), sin(a2)) * r2 * texelSombraPerto).r;
                s2 += (q2.z - 0.00004 > d2) ? 0.0 : 1.0;
            }
            sombra = min(sombra, s2 / 10.0);
        }
    }
#endif
    float ndl = dot(n, dirSol);
    float difSol = mix(max(ndl, 0.0), clamp((ndl + 0.45) / 1.45, 0.0, 1.0), sss);
    luz += corSol * difSol * mix(sombra, max(sombra, 0.35), sss * 0.5);
    luz += corLua * max(dot(n, dirLua), 0.0) * sombra;
    // luz rebatida: o chao iluminado pelo sol devolve luz colorida para cima
    float solNoChao = max(dirSol.y, 0.0);
    luz += corSol * corChao * solNoChao * (0.5 - 0.5 * n.y) * 0.9 * ao;
    // contorno: o ceu "abraca" as bordas (luz indireta)
    float borda = pow(1.0 - max(dot(n, v), 0.0), 3.0);
    luz += ambCeu * borda * 0.25 * (1.0 - abs(n.y)) * ao;
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
            vec3 hl = normalize(d / max(dd, 0.001) + v);
            float at = clamp(1.0 - (dd * dd) / (luzPos[i].w * luzPos[i].w), 0.0, 1.0);
            c += luzCor[i] * at * at * pow(max(dot(n, hl), 0.0), expo) * brilho * 0.15;
        }
    }
    if (veludo > 0.0) c += (ambCeu * 0.5 + corSol * max(ndl, 0.0) * sombra * 0.15) * albedo * veludo * pow(1.0 - max(dot(n, v), 0.0), 2.0);
    if (sss > 0.0) c += corSol * sombra * vec3(0.55, 0.14, 0.07) * albedo * sss * (1.0 - abs(ndl)) * 0.45;
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
uniform sampler2D texAO;
uniform float forcaBrilho;
uniform float usarRaios;
uniform float usarAO;
void main() {
    vec3 c = TEX(texture0, fragTexCoord).rgb;
    if (usarAO > 0.5) c *= TEX(texAO, fragTexCoord).r;
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
    vec4 p = matModel * pl;
    posMundo = p.xyz;
    mat3 mn = mat3(matModel) * mat3(S);
    normalMundo = normalize(mn * vertexNormal);
    cor = vertexColor;
    extra = vertexTexCoord;
    posRep = vertexPosition;
    nRep = vertexNormal;
    mc0 = mn[0];
    mc1 = mn[1];
    mc2 = mn[2];
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

// ---------- oclusao de ambiente em tela (SSAO) ----------
const char* FS_SSAO = R"(
uniform sampler2D texture0;   // profundidade da cena
uniform vec2 texel;           // 1 / resolucao da profundidade
uniform float escalaProj;     // pixels por metro a 1 m de distancia
uniform vec2 tanMeio;         // tan(fov/2) * (aspecto, 1)
float linear(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * 0.1 * 1000.0) / (1000.0 + 0.1 - z * (1000.0 - 0.1));
}
vec3 posVista(vec2 uv) {
    float z = linear(TEX(texture0, uv).r);
    return vec3((uv * 2.0 - 1.0) * tanMeio * z, -z);
}
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    vec2 uv = fragTexCoord;
    float d0 = TEX(texture0, uv).r;
    if (d0 >= 0.99999) { FRAG = vec4(1.0); return; }
    vec3 P = posVista(uv);
    // normal reconstruida pelos vizinhos (usa o lado com menor salto de profundidade)
    vec2 t3 = texel * 3.0;
    vec3 px1 = posVista(uv + vec2(t3.x, 0.0)) - P, px2 = P - posVista(uv - vec2(t3.x, 0.0));
    vec3 py1 = posVista(uv + vec2(0.0, t3.y)) - P, py2 = P - posVista(uv - vec2(0.0, t3.y));
    vec3 dx = abs(px1.z) < abs(px2.z) ? px1 : px2;
    vec3 dy = abs(py1.z) < abs(py2.z) ? py1 : py2;
    vec3 n = normalize(cross(dx, dy));
    if (n.z < 0.0) n = -n;
    float z0 = -P.z;
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
    float ao = clamp(1.0 - soma * (1.2 / 12.0), 0.0, 1.0);
    FRAG = vec4(vec3(ao * ao), 1.0);
}
)";
