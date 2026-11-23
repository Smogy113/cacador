#include "core/anim.h"
#include <algorithm>
#include <cmath>

static Pose vazia(const Esqueleto& e) { return poseRepouso(e); }

// pontos de apoio de um pe (calcanhar e ponta), no espaco de repouso
static void pontosDoPe(const Esqueleto& e, int pe, Vec3f out[2]) {
    const Osso& o = e.ossos[pe];
    out[0] = {o.inicio.x, 0.f, o.inicio.z - 0.045f};
    out[1] = {o.fim.x, 0.f, o.fim.z + 0.01f};
}

float alturaDosPes(const Esqueleto& e, const Pose& p) {
    static thread_local std::vector<Mat4> m;  // reaproveitado: sem alocar a cada quadro
    calcularMatrizes(e, p, m);
    float menor = 1e9f;
    for (int pe : {PE_E, PE_D}) {
        Vec3f pts[2];
        pontosDoPe(e, pe, pts);
        for (auto& q : pts) menor = std::min(menor, m[pe].ponto(q).y);
    }
    return menor;
}

void apoiarNoChao(const Esqueleto& e, Pose& p) { p.raiz.y -= alturaDosPes(e, p); }

Pose poseParado(const Esqueleto& e, float t) {
    Pose p = vazia(e);
    float resp = std::sin(t * 1.7f);
    p.rot[PEITO].x = resp * 0.025f;
    p.rot[COLUNA].x = -resp * 0.01f;
    p.rot[CABECA].y = std::sin(t * 0.31f) * 0.25f * std::max(0.f, std::sin(t * 0.13f));
    p.rot[CABECA].x = std::sin(t * 0.23f) * 0.05f;
    for (int lado = 0; lado < 2; lado++) {
        float s = lado ? -1.f : 1.f;
        int b = lado ? BRACO_D : BRACO_E, a = lado ? ANTEBRACO_D : ANTEBRACO_E;
        p.rot[b].x = std::sin(t * 0.9f + lado) * 0.03f;
        float junto = e.poseA ? .70f : .05f;
        p.rot[b].z = s * (-junto + resp * 0.01f);
        p.rot[a].x = -0.12f;
    }
    apoiarNoChao(e, p);
    return p;
}

// ---------------- andar e correr a partir de dados de marcha humana ----------------
// Como a marcha e montada (resumo para quem for mexer):
//  1. As pernas seguem curvas medias de laboratorio (angulo x % do ciclo). O ciclo comeca no toque do
//     calcanhar esquerdo e tem dois passos.
//  2. O pe que esta no chao (fase de apoio) fica travado no lugar: quadril e joelho saem de cinematica
//     inversa, como nas animacoes feitas com captura de movimento.
//  3. A altura da bacia segue a trajetoria suave medida em pessoas: andando ela sobe no meio do apoio e
//     desce na troca de pe; correndo desce no apoio e sobe no voo. (Antes ela vinha do pe mais baixo da
//     curva, o que dava um tranco de ~5 cm logo depois de cada pisada.)
//  4. A passada (metros por ciclo) e medida nas proprias curvas para o pe de apoio nao escorregar. Abaixo
//     de VEL_PASSADA_CHEIA os passos encurtam e a cadencia se mantem, como numa pessoa comecando a andar
//     ou parando.
// Curvas: andar (Perry; Winter), correr (Novacheck). Quadril: + = flexao (coxa a frente);
// joelho: + = dobrado; tornozelo: + = ponta para cima (dorsiflexao).
namespace {
struct K { float pct, graus; };
const K QUADRIL_ANDAR[] = {{0, 30}, {10, 30}, {30, 10}, {50, -10}, {60, -5}, {75, 20}, {87, 33}, {100, 30}};
const K JOELHO_ANDAR[] = {{0, 5}, {15, 18}, {40, 5}, {50, 10}, {62, 40}, {73, 62}, {87, 28}, {97, 5}, {100, 5}};
const K TORNOZ_ANDAR[] = {{0, 0}, {7, -7}, {30, 5}, {48, 10}, {62, -18}, {75, -3}, {90, 2}, {100, 0}};
const K QUADRIL_CORRER[] = {{0, 35}, {20, 17}, {38, -12}, {50, -8}, {65, 20}, {80, 45}, {92, 40}, {100, 35}};
const K JOELHO_CORRER[] = {{0, 20}, {15, 40}, {38, 18}, {55, 62}, {70, 95}, {82, 80}, {93, 32}, {100, 20}};
const K TORNOZ_CORRER[] = {{0, 5}, {15, 18}, {38, -25}, {50, -15}, {65, 5}, {85, 10}, {100, 5}};

const float PI = 3.14159265f, DOIS_PI = 6.2831853f, RAD = PI / 180.f;
// Fase (rad) em que o calcanhar esquerdo toca o chao; o direito toca meio ciclo depois.
const float FASE_PISADA_E = PI * 0.5f;

// Fracao do ciclo com o pe no chao nas curvas de laboratorio (onde a ponta sai do chao nelas).
constexpr float APOIO_CURVA_ANDAR = 0.60f, APOIO_CURVA_CORRER = 0.38f;
// Fracao usada no jogo. Andando e a mesma das curvas. Correndo a ~5 m/s o contato real dura ~0,2 s e o
// voo e longo: ~30% do ciclo em cada pe. As curvas de corrida sao esticadas no tempo para caber, o que
// alonga a passada e leva a cadencia para perto da medida (~200 passos/min, antes ~275).
constexpr float APOIO_ANDAR = 0.60f, APOIO_CORRER = 0.30f;
// As curvas de Perry sao de ~1,4 m/s e o nosso andar e de 1,6 m/s, com passos mais longos: o quadril
// abre um pouco mais em volta do ponto medio do movimento.
constexpr float GANHO_QUADRIL_ANDAR = 1.12f, MEIO_QUADRIL_ANDAR = 10.f;
// O angulo do quadril do laboratorio e medido contra a bacia, que nas pessoas fica inclinada para a frente
// ao andar (e mais ao correr). Nosso esqueleto nao inclina a bacia (ela carrega o tronco), entao a coxa
// recebe esse desconto (graus). Sem ele o pe pisava ~42 cm a frente do quadril (o real e ~27 cm) e a
// bacia precisava descer ~10 cm na pisada.
constexpr float BACIA_ANDAR = 7.f, BACIA_CORRER = 10.f;
// Velocidade (m/s) em que a passada chega ao tamanho cheio e menor fracao de passada usada (bem devagar a
// locomocao ja mistura a pose parada; o piso so evita passos de tamanho zero).
constexpr float VEL_PASSADA_CHEIA = 1.0f, PASSADA_MINIMA = 0.25f;
// Oscilacao vertical da bacia (metade do pico a pico, em metros): ~4 cm andando, ~6 cm correndo.
constexpr float OSCILA_ANDAR = 0.025f, OSCILA_CORRER = 0.030f;
// Dobra minima do joelho de apoio (graus): quase esticado andando, sempre flexionado correndo.
constexpr float JOELHO_MIN_ANDAR = 4.f, JOELHO_MIN_CORRER = 16.f;
// Janela (fracao do ciclo) logo apos a ponta sair do chao em que o joelho dobra um pouco mais para o pe
// passar limpo (nosso esqueleto tem pe sem dedos e proporcoes diferentes das do laboratorio).
constexpr float JANELA_SAIDA = 0.16f;
// Folga minima (m) do pe no meio do balanco; nas pessoas a ponta passa ~1,5 cm acima do chao.
constexpr float FOLGA_PE = 0.015f;
// Quanto (m) o pe de apoio pode ficar acima do chao sem o corpo descer para encosta-lo.
constexpr float TOLERA_FLUTUAR = 0.005f;

template <size_t N>
float curva(const K (&k)[N], float pct) {  // interpolacao suave (Catmull-Rom) e periodica
    pct = std::fmod(pct + 1000.f, 100.f);
    size_t i = 0;
    while (i + 2 < N && pct > k[i + 1].pct) i++;
    float t = (pct - k[i].pct) / std::max(1e-3f, k[i + 1].pct - k[i].pct);
    float p0 = k[i == 0 ? N - 2 : i - 1].graus, p1 = k[i].graus, p2 = k[i + 1].graus, p3 = k[i + 2 < N ? i + 2 : 1].graus;
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}
float corrida(float v) { return std::clamp((v - 2.2f) / 2.4f, 0.f, 1.f); }  // 0 = andando, 1 = correndo

// Parametros da marcha numa velocidade.
struct Marcha {
    float w;      // mistura das curvas: 0 = andar, 1 = correr
    float apoio;  // fracao do ciclo com o pe no chao
    float amp;    // PASSADA_MINIMA..1: amplitude dos movimentos (passos curtos abaixo de VEL_PASSADA_CHEIA)
};
Marcha marchaDe(float v) {
    float w = corrida(v);
    return {w, lerp(APOIO_ANDAR, APOIO_CORRER, w), std::clamp(v / VEL_PASSADA_CHEIA, PASSADA_MINIMA, 1.f)};
}
Marcha marchaCheia(float w) { return {w, lerp(APOIO_ANDAR, APOIO_CORRER, w), 1.f}; }

// fracao do ciclo (0..1) de cada perna para a fase f
float cicloDaPerna(float f, int lado) {
    float u = (f - FASE_PISADA_E) / DOIS_PI + (lado ? 0.5f : 0.f);
    return u - std::floor(u);
}

// Leva a fracao u do ciclo do jogo para o % das curvas: o apoio do jogo ocupa o apoio da curva.
float pctNaCurva(float u, float apoio, float apoioCurva) {
    float c = u < apoio ? u / apoio * apoioCurva : apoioCurva + (u - apoio) / (1.f - apoio) * (1.f - apoioCurva);
    return c * 100.f;
}

// Angulos (rad, convencao do esqueleto) de uma perna na fracao u do ciclo, com amplitude cheia.
void anguloPerna(float u, const Marcha& g, float& coxa, float& joelho, float& pe) {
    float pa = pctNaCurva(u, g.apoio, APOIO_CURVA_ANDAR), pc = pctNaCurva(u, g.apoio, APOIO_CURVA_CORRER);
    float qa = MEIO_QUADRIL_ANDAR + (curva(QUADRIL_ANDAR, pa) - MEIO_QUADRIL_ANDAR) * GANHO_QUADRIL_ANDAR;
    float q = lerp(qa - BACIA_ANDAR, curva(QUADRIL_CORRER, pc) - BACIA_CORRER, g.w);
    float j = lerp(curva(JOELHO_ANDAR, pa), curva(JOELHO_CORRER, pc), g.w);
    float t = lerp(curva(TORNOZ_ANDAR, pa), curva(TORNOZ_CORRER, pc), g.w);
    coxa = -q * RAD;   // rot.x < 0 leva a coxa para a frente
    joelho = j * RAD;  // rot.x > 0 dobra o joelho
    pe = -t * RAD;     // rot.x > 0 abaixa a ponta
    // no comeco do balanco a ponta precisa sair limpa do chao
    if (u >= g.apoio && u <= g.apoio + JANELA_SAIDA) {
        float b = std::sin(PI * (u - g.apoio) / JANELA_SAIDA);
        joelho += lerp(0.24f, 0.08f, g.w) * b;
        pe -= 0.25f * b;
    }
}

// Pernas pelas curvas, ja na amplitude da marcha (antes de travar o pe de apoio).
void pernasNaCurva(Pose& p, float f, const Marcha& g) {
    for (int lado = 0; lado < 2; lado++) {
        int cx = lado ? COXA_D : COXA_E, ca = lado ? CANELA_D : CANELA_E, pp = lado ? PE_D : PE_E;
        float c, j, t;
        anguloPerna(cicloDaPerna(f, lado), g, c, j, t);
        p.rot[cx].x = c * g.amp;
        p.rot[ca].x = j * g.amp;
        p.rot[pp].x = t * g.amp;
    }
}

// Bacia, tronco, cabeca e bracos no ritmo da passada.
void corpoNaMarcha(const Esqueleto& e, Pose& p, float f, const Marcha& g) {
    const float w = g.w, a = g.amp;
    float s = std::sin(f), c = std::cos(f);
    // bacia: gira com a perna que vai a frente e cai para o lado da perna no ar (~4 graus)
    p.rot[QUADRIL].y = s * (0.08f + 0.06f * w) * a;
    p.rot[QUADRIL].z = c * (0.07f - 0.03f * w) * a;
    p.raiz.x = -c * lerp(0.022f, 0.009f, w) * a;  // o peso passa de um pe para o outro
    p.rot[PESCOCO].z = -c * 0.018f * a;
    p.rot[COLUNA].z = -c * 0.04f * a;
    // tronco e ombros giram contra a bacia; a cabeca continua olhando para a frente
    p.rot[COLUNA].y = -s * 0.07f * a;
    p.rot[PEITO].y = -s * (0.08f + 0.1f * w) * a;
    p.rot[CABECA].y = s * (0.1f + 0.08f * w) * a;
    // inclinacao para a frente (maior correndo) e um leve aceno do tronco a cada pisada, quando o peso
    // cai sobre a perna da frente; a cabeca compensa para o olhar ficar firme
    float aceno = 0.012f * (1.f - w) * std::cos(2.f * (f - FASE_PISADA_E) - 0.9f) * a;
    p.rot[COLUNA].x = (0.03f + 0.13f * w) * a + aceno;
    p.rot[PEITO].x = 0.03f * w;
    p.rot[CABECA].x = -(0.03f + 0.12f * w) * a - aceno * 0.8f;
    // bracos: balancam contra a perna do mesmo lado, mais para tras que para a frente (como nas pessoas:
    // ~24 graus atras, ~14 a frente andando); o cotovelo dobra mais quando o braco vai a frente
    for (int lado = 0; lado < 2; lado++) {
        float faseBraco = f + (lado ? PI : 0.f);
        float balanco = std::sin(faseBraco - 0.12f);  // atrasa um pouco em relacao a perna
        int b = lado ? BRACO_D : BRACO_E, ab = lado ? ANTEBRACO_D : ANTEBRACO_E, mo = lado ? MAO_D : MAO_E;
        float sx = lado ? 1.f : -1.f;
        p.rot[b].x = (lerp(0.33f, 0.78f, w) * balanco + lerp(0.08f, 0.03f, w)) * a;
        p.rot[b].z = sx * ((e.poseA ? .70f : .07f) + 0.025f * std::cos(faseBraco));
        float avanco = 0.5f - 0.5f * std::sin(faseBraco - 0.36f);
        p.rot[ab].x = -(lerp(0.26f, 0.95f, w) + lerp(0.42f, 0.35f, w) * avanco) * a;
        p.rot[mo].x = (-0.08f + 0.07f * std::cos(faseBraco - 0.38f)) * a;
    }
}

// Forma (-1..1) da subida e descida da bacia: dois picos por ciclo, um por passo (u do pe esquerdo).
// Andando: mais alta no meio do apoio (~24% e 74%), quando a perna de apoio passa sob o corpo quase
// esticada; mais baixa na troca de pe, com as duas pernas abertas. A fase foi escolhida para os pontos
// baixos caírem onde nosso esqueleto mais precisa abaixar (pisada e fim do apoio), o que deixa a bacia
// mais alta no resto do ciclo e o joelho de apoio perto dos ~20 graus medidos, em vez de ~40.
// Correndo: mais baixa no meio do apoio, mais alta no meio do voo.
float formaOscilacao(float u, const Marcha& g) {
    float andar = std::cos(2.f * DOIS_PI * (u - 0.24f));
    float meioVoo = (g.apoio + 0.5f) * 0.5f;
    float correr = std::cos(2.f * DOIS_PI * (u - meioVoo));
    return lerp(andar, correr, g.w);
}

// Ponto de apoio de um pe (calcanhar ou ponta) no espaco de repouso.
Vec3f pontoDoPe(const Osso& o, bool ponta) {
    return ponta ? Vec3f{o.fim.x, 0.f, o.fim.z + 0.01f} : Vec3f{o.inicio.x, 0.f, o.inicio.z - 0.045f};
}

// Onde a perna de apoio quer o tornozelo (e o quadril dessa perna) numa pose ja montada.
struct AlvoDoPe {
    Vec3f quadril;     // junta do quadril na pose
    float z, y;        // tornozelo desejado
    float absPe;       // inclinacao do pe em relacao ao chao (rad; + = ponta para baixo)
};
// Inclinacao do pe em relacao ao chao dada pelas curvas (soma de quadril, joelho e tornozelo).
float inclinacaoDoPe(const Pose& p, int lado) {
    int cx = lado ? COXA_D : COXA_E, ca = lado ? CANELA_D : CANELA_E, pp = lado ? PE_D : PE_E;
    return p.rot[cx].x + p.rot[ca].x + p.rot[pp].x;
}
// O pe gira em volta do calcanhar (na pisada) ou da ponta (saindo do chao), que ficam parados no chao;
// o tornozelo anda para tras o mesmo que o corpo anda para a frente. absPeCurva vem de inclinacaoDoPe
// com a perna ainda nas curvas.
AlvoDoPe alvoDoPe(const Esqueleto& e, const std::vector<Mat4>& m, int lado, float u, float absPeCurva,
                  const Marcha& g, float zPisou, float ciclo) {
    int cx = lado ? COXA_D : COXA_E, ca = lado ? CANELA_D : CANELA_E, pp = lado ? PE_D : PE_E;
    AlvoDoPe a;
    a.quadril = m[cx].ponto(e.ossos[cx].inicio);
    // Andando, o pe fica chato no chao do fim da pisada ate o calcanhar comecar a subir. As curvas deixam
    // nosso pe ~3 graus de ponta para baixo nessa fase (outras proporcoes), o que erguia o calcanhar ~1 cm
    // e obrigava o joelho a dobrar mais.
    float chato = (1.f - g.w) * smoothstep(0.03f, 0.12f, u) * (1.f - smoothstep(0.30f, 0.45f, u));
    a.absPe = absPeCurva * (1.f - chato);
    const Osso& op = e.ossos[pp];
    Vec3f tornoz0 = e.ossos[ca].fim;
    Vec3f piv = Vec3f{0, -tornoz0.y, pontoDoPe(op, a.absPe >= 0).z - tornoz0.z};
    float cs = std::cos(a.absPe), sn = std::sin(a.absPe);
    Vec3f pivRod{0, piv.y * cs - piv.z * sn, piv.y * sn + piv.z * cs};
    float zTravado = zPisou - u * ciclo;  // tornozelo com o pe chato
    a.z = zTravado + piv.z - pivRod.z;
    a.y = -pivRod.y;
    return a;
}

// Coxa e canela vistas de lado (plano y-z, onde a perna dobra): comprimento e inclinacao em repouso
// (radianos a partir da vertical para baixo, + = para a frente). Os ossos do esqueleto nao sao retos
// (o joelho fica um pouco a frente), e a cinematica inversa precisa disso para o pe cair no lugar certo.
struct OssoDeLado { float L, ang; };
OssoDeLado deLado(const Osso& o) {
    Vec3f d = o.fim - o.inicio;
    return {std::sqrt(d.y * d.y + d.z * d.z), std::atan2(d.z, -d.y)};
}
void comprimentosPerna(const Esqueleto& e, float& L1, float& L2) {
    L1 = deLado(e.ossos[COXA_E]).L;
    L2 = deLado(e.ossos[CANELA_E]).L;
}

// Cinematica inversa de dois ossos no plano da perna: leva o tornozelo ao alvo mantendo o pe inclinado
// como na curva. Angulos de direcao medidos da vertical para baixo (+ = para a frente); rot.x positivo
// gira o osso para tras, entao rot = inclinacao em repouso - direcao desejada.
void resolverPerna(const Esqueleto& e, Pose& p, int lado, const AlvoDoPe& a) {
    int cx = lado ? COXA_D : COXA_E, ca = lado ? CANELA_D : CANELA_E, pp = lado ? PE_D : PE_E;
    OssoDeLado coxa = deLado(e.ossos[cx]), canela = deLado(e.ossos[ca]);
    float L1 = coxa.L, L2 = canela.L;
    float dz = a.z - a.quadril.z, dy = a.y - a.quadril.y;
    float D = std::clamp(std::sqrt(dz * dz + dy * dy), 0.05f, (L1 + L2) * 0.999f);
    float ang = std::atan2(dz, -dy);  // direcao quadril -> tornozelo
    float a1 = std::acos(std::clamp((L1 * L1 + D * D - L2 * L2) / (2 * L1 * D), -1.f, 1.f));
    float joelho = PI - std::acos(std::clamp((L1 * L1 + L2 * L2 - D * D) / (2 * L1 * L2), -1.f, 1.f));
    float dirCoxa = ang + a1, dirCanela = dirCoxa - joelho;
    p.rot[cx].x = coxa.ang - dirCoxa;
    p.rot[ca].x = canela.ang - dirCanela - p.rot[cx].x;
    p.rot[pp].x = a.absPe - p.rot[cx].x - p.rot[ca].x;
}

// Sobe o alvo do tornozelo do pe no ar ate calcanhar e ponta ficarem pelo menos 'folga' acima do chao.
void garantirFolga(const Esqueleto& e, int lado, AlvoDoPe& a, float folga) {
    int ca = lado ? CANELA_D : CANELA_E, pp = lado ? PE_D : PE_E;
    Vec3f t0 = e.ossos[ca].fim;
    float cs = std::cos(a.absPe), sn = std::sin(a.absPe);
    float menor = 1e9f;  // ponto mais baixo do pe em relacao ao tornozelo
    for (bool ponta : {false, true}) {
        Vec3f r = pontoDoPe(e.ossos[pp], ponta) - t0;
        menor = std::min(menor, r.y * cs - r.z * sn);
    }
    a.y = std::max(a.y, folga - menor);
}

// ---- medidas que so dependem da mistura andar/correr (esqueleto padrao, 11 amostras de w) ----
// z de um ponto do pe esquerdo com a perna na curva e o resto do corpo em repouso
float zDoPe(const Esqueleto& e, float u, const Marcha& g, bool ponta) {
    Pose p = poseRepouso(e);
    anguloPerna(u, g, p.rot[COXA_E].x, p.rot[CANELA_E].x, p.rot[PE_E].x);
    std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    return m[PE_E].ponto(pontoDoPe(e.ossos[PE_E], ponta)).z;
}
// tornozelo (pe chato) quando o calcanhar pisa
float zPisadaDe(const Esqueleto& e, const Marcha& g) {
    return zDoPe(e, 0.f, g, false) - (pontoDoPe(e.ossos[PE_E], false).z - e.ossos[CANELA_E].fim.z);
}
// tamanho do ciclo: o calcanhar pisa, o pe fica parado e a ponta sai exatamente onde a curva a solta
float cicloDe(const Esqueleto& e, const Marcha& g) {
    float pontaRel = pontoDoPe(e.ossos[PE_E], true).z - e.ossos[CANELA_E].fim.z;
    return std::max(0.4f, (zPisadaDe(e, g) + pontaRel - zDoPe(e, g.apoio, g, true)) / g.apoio);
}
// Altura media da raiz: a mais alta em que as pernas de apoio alcancam o chao com o joelho na dobra
// minima em todo o ciclo, ja contando a oscilacao (assim o pe de apoio nunca flutua).
float alturaBaseDe(const Esqueleto& e, const Marcha& g, float ciclo, float zPisou) {
    float L1, L2;
    comprimentosPerna(e, L1, L2);
    float kmin = lerp(JOELHO_MIN_ANDAR, JOELHO_MIN_CORRER, g.w) * RAD;
    float alcance = std::sqrt(L1 * L1 + L2 * L2 + 2 * L1 * L2 * std::cos(kmin));
    float oscila = lerp(OSCILA_ANDAR, OSCILA_CORRER, g.w);
    float base = 0.f;  // nunca acima do corpo em pe
    std::vector<Mat4> m;
    // basta o apoio do pe esquerdo (o direito e simetrico e a oscilacao se repete a cada passo); as
    // pontas (pisada e saida) entram na amostragem porque e nelas que a perna mais se estica
    const int AMOSTRAS = 96;
    for (int i = 0; i <= AMOSTRAS; i++) {
        float u = std::min(g.apoio * i / AMOSTRAS, g.apoio - 1e-4f);
        float f = FASE_PISADA_E + DOIS_PI * u;
        Pose p = poseRepouso(e);
        pernasNaCurva(p, f, g);
        corpoNaMarcha(e, p, f, g);
        calcularMatrizes(e, p, m);
        AlvoDoPe a = alvoDoPe(e, m, 0, u, inclinacaoDoPe(p, 0), g, zPisou, ciclo);
        float dz = a.z - a.quadril.z;
        if (std::abs(dz) >= alcance) continue;  // inalcancavel em qualquer altura
        float hMax = a.y - a.quadril.y + std::sqrt(alcance * alcance - dz * dz);
        base = std::min(base, hMax - oscila * formaOscilacao(u, g));
    }
    return base;
}

// Tornozelo de uma perna numa pose ja calculada.
Vec3f tornozelo(const Esqueleto& e, const std::vector<Mat4>& m, int lado) {
    int ca = lado ? CANELA_D : CANELA_E;
    return m[ca].ponto(e.ossos[ca].fim);
}
// Diferenca (z, y) entre o tornozelo travado no chao e o tornozelo da curva (com a raiz em 0) no instante u.
// O pe no ar segue a curva mais uma correcao que vai da diferenca na saida (u = apoio) ate a diferenca na
// pisada (u = 0 do ciclo seguinte): assim ele sai e pousa exatamente onde o pe de apoio esta, sem salto.
void emendaDoPe(const Esqueleto& e, const Marcha& g, float u, float ciclo, float zPisou, float& dz, float& dy) {
    float f = FASE_PISADA_E + DOIS_PI * u;
    Pose p = poseRepouso(e);
    pernasNaCurva(p, f, g);
    corpoNaMarcha(e, p, f, g);
    std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    AlvoDoPe a = alvoDoPe(e, m, 0, u, inclinacaoDoPe(p, 0), g, zPisou, ciclo);
    Vec3f t = tornozelo(e, m, 0);
    dz = a.z - t.z;
    dy = a.y - t.y;
}

struct TabelaMarcha {
    static constexpr int N = 11;
    float ciclo[N], pisada[N], base[N];
    float saidaZ[N], saidaY[N], pousoZ[N], pousoY[N];  // emendas do pe no ar (ver emendaDoPe)
    static float ler(const float (&t)[N], float w) {
        float x = std::clamp(w, 0.f, 1.f) * (N - 1);
        int k = std::min(N - 2, int(x));
        return lerp(t[k], t[k + 1], x - k);
    }
};
const TabelaMarcha& tabelaMarcha() {
    static const TabelaMarcha tab = [] {
        TabelaMarcha t{};
        Esqueleto e = esqueletoHumano({});
        for (int k = 0; k < TabelaMarcha::N; k++) {
            Marcha g = marchaCheia(float(k) / (TabelaMarcha::N - 1));
            t.ciclo[k] = cicloDe(e, g);
            t.pisada[k] = zPisadaDe(e, g);
            t.base[k] = alturaBaseDe(e, g, t.ciclo[k], t.pisada[k]);
            emendaDoPe(e, g, g.apoio, t.ciclo[k], t.pisada[k], t.saidaZ[k], t.saidaY[k]);
            emendaDoPe(e, g, 0.f, t.ciclo[k], t.pisada[k], t.pousoZ[k], t.pousoY[k]);
        }
        return t;
    }();
    return tab;
}
}  // namespace

float fasePorMetro(float v) {
    Marcha g = marchaDe(v);
    float ciclo = TabelaMarcha::ler(tabelaMarcha().ciclo, g.w) * g.amp;
    return DOIS_PI / ciclo;
}

Pose poseAndar(const Esqueleto& e, float f, float v) {
    const TabelaMarcha& tab = tabelaMarcha();
    Marcha g = marchaDe(v);
    Pose p = vazia(e);
    pernasNaCurva(p, f, g);
    corpoNaMarcha(e, p, f, g);
    static thread_local std::vector<Mat4> m;
    // caminho do pe no ar: o da curva com a bacia na altura de repouso (altura do pe sobre o chao igual a
    // medida no laboratorio), guardado antes de a bacia subir e descer
    calcularMatrizes(e, p, m);
    Vec3f peNoAr[2] = {tornozelo(e, m, 0), tornozelo(e, m, 1)};
    const float absPeCurva[2] = {inclinacaoDoPe(p, 0), inclinacaoDoPe(p, 1)};
    // altura da bacia: trajetoria suave (ver formaOscilacao); passos curtos ficam mais perto de em pe
    float oscila = lerp(OSCILA_ANDAR, OSCILA_CORRER, g.w);
    p.raiz.y = (TabelaMarcha::ler(tab.base, g.w) + oscila * formaOscilacao(cicloDaPerna(f, 0), g)) * g.amp;
    // as duas pernas por cinematica inversa: a de apoio com o pe travado no chao, a do ar seguindo a curva
    float ciclo = DOIS_PI / fasePorMetro(v);
    float zPisou = TabelaMarcha::ler(tab.pisada, g.w) * g.amp;
    bool apoio[2];
    auto ajustarPernas = [&] {
        calcularMatrizes(e, p, m);
        AlvoDoPe alvos[2];
        for (int lado = 0; lado < 2; lado++) {
            float u = cicloDaPerna(f, lado);
            apoio[lado] = u < g.apoio;
            if (apoio[lado]) {
                alvos[lado] = alvoDoPe(e, m, lado, u, absPeCurva[lado], g, zPisou, ciclo);
                continue;
            }
            int cx = lado ? COXA_D : COXA_E;
            float s = (u - g.apoio) / (1.f - g.apoio);  // 0 = ponta saindo do chao, 1 = calcanhar pousando
            float meio = std::sin(PI * s);                // 0 nas pontas do balanco, 1 no meio
            AlvoDoPe& a = alvos[lado];
            a.quadril = m[cx].ponto(e.ossos[cx].inicio);
            a.absPe = absPeCurva[lado];
            // no meio do balanco o pe acompanha o sobe e desce da bacia (joelho com a dobra da curva)
            a.z = peNoAr[lado].z + lerp(TabelaMarcha::ler(tab.saidaZ, g.w), TabelaMarcha::ler(tab.pousoZ, g.w), s) * g.amp;
            a.y = peNoAr[lado].y + lerp(TabelaMarcha::ler(tab.saidaY, g.w), TabelaMarcha::ler(tab.pousoY, g.w), s) * g.amp +
                  p.raiz.y * meio;
            garantirFolga(e, lado, a, FOLGA_PE * meio * g.amp);
        }
        for (int lado = 0; lado < 2; lado++) resolverPerna(e, p, lado, alvos[lado]);
    };
    ajustarPernas();
    // Seguranca para esqueletos de outras proporcoes: se uma perna de apoio nao alcancou o chao, o corpo
    // desce ate ela pisar e as pernas sao ajustadas de novo. Ate TOLERA_FLUTUAR fica como esta: a
    // cinematica inversa e feita de lado e a bacia inclinada encurta a perna em uns 3 mm no fim do apoio;
    // corrigir isso daria um degrau na bacia bem na hora em que a ponta sai do chao.
    calcularMatrizes(e, p, m);
    float flutua = 0.f;
    for (int lado = 0; lado < 2; lado++) {
        if (!apoio[lado]) continue;
        int o = lado ? PE_D : PE_E;
        float y = std::min(m[o].ponto(pontoDoPe(e.ossos[o], false)).y, m[o].ponto(pontoDoPe(e.ossos[o], true)).y);
        flutua = std::max(flutua, y);
    }
    if (flutua > TOLERA_FLUTUAR) {
        p.raiz.y -= flutua;
        ajustarPernas();
    }
    return p;
}

Pose poseMontado(const Esqueleto& e, float f, int andadura) {
    Pose p = vazia(e);
    float amp = andadura == 0 ? 0.f : (andadura == 1 ? 0.3f : (andadura == 2 ? 0.6f : 1.f));
    for (int lado = 0; lado < 2; lado++) {
        float sx = lado ? -1.f : 1.f;
        int coxa = lado ? COXA_D : COXA_E, canela = lado ? CANELA_D : CANELA_E, pe = lado ? PE_D : PE_E;
        int b = lado ? BRACO_D : BRACO_E, a = lado ? ANTEBRACO_D : ANTEBRACO_E;
        p.rot[coxa].x = -1.12f;
        p.rot[coxa].z = sx * 0.62f;  // abre as pernas em volta do cavalo
        p.rot[canela].x = 1.05f;
        p.rot[pe].x = -0.15f;
        p.rot[b].x = -0.55f;
        p.rot[b].z = -sx * (e.poseA ? .70f : .10f);
        p.rot[a].x = -0.85f;
        p.rot[a].z = sx * 0.2f;
    }
    // balanco no ritmo do cavalo
    p.rot[COLUNA].x = 0.1f + std::sin(f * 2.f) * 0.06f * amp + 0.12f * amp;
    p.rot[CABECA].x = -0.1f - 0.08f * amp;
    p.raiz.y = std::sin(f * 2.f) * 0.035f * amp;
    return p;
}

// =====================================================================
// cavalo
// =====================================================================

float alturaDosCascos(const Esqueleto& e, const Pose& p) {
    static thread_local std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    float menor = 1e9f;
    for (int c : {C_CASCO_DE, C_CASCO_DD, C_CASCO_TE, C_CASCO_TD}) {
        const Osso& o = e.ossos[c];
        for (float dz : {-0.05f, 0.05f}) menor = std::min(menor, m[c].ponto({o.fim.x, 0.f, o.fim.z + dz}).y);
    }
    return menor;
}

Pose poseCavalo(const Esqueleto& e, float f, int and_, float t) {
    Pose p = poseRepouso(e);
    float amp = and_ == 0 ? 0.f : (and_ == 1 ? 0.32f : (and_ == 2 ? 0.5f : 0.8f));
    float ph[4];  // DE, DD, TE, TD
    if (and_ == 3) { ph[0] = 0.f; ph[1] = 0.45f; ph[2] = 3.14f; ph[3] = 3.6f; }
    else if (and_ == 2) { ph[0] = 0.f; ph[1] = 3.14159f; ph[2] = 3.14159f; ph[3] = 0.f; }
    else { ph[0] = 0.f; ph[1] = 3.14159f; ph[2] = 1.5708f; ph[3] = 4.7124f; }
    const int sup[4] = {C_BRACO_DE, C_BRACO_DD, C_COXA_TE, C_COXA_TD};
    for (int k = 0; k < 4; k++) {
        float a = f + ph[k];
        p.rot[sup[k]].x = -std::sin(a) * amp;
        float dobra = std::max(0.f, std::cos(a - 0.5f)) * (0.3f + 1.4f * amp) * (amp > 0 ? 1.f : 0.f);
        p.rot[sup[k] + 1].x = k < 2 ? dobra : dobra * 0.6f;
        p.rot[sup[k] + 2].x = -dobra * 0.4f;
    }
    float galope = and_ == 3 ? 1.f : 0.f;
    p.rot[C_CORPO].x = std::sin(f) * 0.05f * galope;
    p.rot[C_PESCOCO].x = std::sin(f * (and_ == 3 ? 1.f : 2.f)) * 0.06f * amp + 0.22f * galope;
    p.rot[C_CABECA].x = -0.1f * galope;
    // parado: respira, mexe a cabeca e o rabo
    if (and_ == 0) {
        p.rot[C_PESCOCO].x = 0.05f + std::sin(t * 0.4f) * 0.06f;
        p.rot[C_PESCOCO].y = std::sin(t * 0.23f) * 0.12f;
        p.rot[C_CORPO].x = std::sin(t * 1.2f) * 0.008f;
    }
    p.rot[C_CAUDA].z = std::sin(t * 1.3f) * 0.18f;
    p.rot[C_CAUDA].x = -0.45f * galope - 0.15f * amp;
    p.raiz.y -= alturaDosCascos(e, p);
    return p;
}

// ---------------- tiroteio e captura ----------------
namespace {
// apoia pelo ponto mais baixo de qualquer junta (corpo deitado/ajoelhado), com folga para a espessura do corpo
void apoiarJuntas(const Esqueleto& e, Pose& p, float folga) {
    std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    float menor = 1e9f;
    for (size_t o = 0; o < e.ossos.size(); o++)
        menor = std::min({menor, m[o].ponto(e.ossos[o].inicio).y, m[o].ponto(e.ossos[o].fim).y});
    float pes = alturaDosPes(e, p);
    p.raiz.y -= std::min(pes, menor - folga);
}
float suave(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
}  // namespace

Pose poseMirar(const Esqueleto& e, float ang, float recuo) {
    Pose p = poseParado(e, 0.f);
    p.rot[BRACO_D] = {-1.5708f - ang, 0.3f, e.poseA ? .70f : 0.f};
    p.rot[ANTEBRACO_D] = {-0.05f - 0.55f * recuo, 0, 0};
    p.rot[MAO_D] = {-0.1f * recuo, 0, 0};
    p.rot[COLUNA].y = 0.12f;
    p.rot[PEITO].y = 0.1f;
    p.rot[CABECA].x = -ang * 0.6f;
    p.rot[CABECA].y = -0.15f;
    p.rot[BRACO_E] = {-0.25f, 0, e.poseA ? -.70f : -.05f};
    p.rot[ANTEBRACO_E] = {-0.5f, 0, 0};
    apoiarNoChao(e, p);
    return p;
}

Pose poseRecarregar(const Esqueleto& e, float t) {
    Pose p = poseParado(e, t);
    float w = std::sin(t * 6.2831f * 2.f) * 0.08f;
    p.rot[BRACO_D] = {-0.6f, 0.2f, e.poseA ? .70f : 0.f};
    p.rot[ANTEBRACO_D] = {-1.35f + w, 0, 0};
    p.rot[BRACO_E] = {-0.55f, -0.25f, e.poseA ? -.70f : 0.f};
    p.rot[ANTEBRACO_E] = {-1.45f - w, 0, 0};
    p.rot[CABECA].x = 0.3f;
    apoiarNoChao(e, p);
    return p;
}

void aplicarTranco(Pose& p, float t) {
    t = std::clamp(t, 0.f, 1.f);
    p.rot[COLUNA].x -= 0.3f * t;
    p.rot[PEITO].x -= 0.15f * t;
    p.rot[CABECA].x -= 0.2f * t;
}

Pose poseCair(const Esqueleto& e, float t) {
    float s = suave(t), meio = std::sin(std::clamp(t, 0.f, 1.f) * 3.1416f);
    Pose p = poseRepouso(e);
    p.rot[QUADRIL].x = -1.5708f * s;                       // tomba de costas
    for (int c : {COXA_E, COXA_D}) p.rot[c].x = -0.5f * meio;   // joelhos dobram na queda
    for (int c : {CANELA_E, CANELA_D}) p.rot[c].x = 0.8f * meio;
    p.rot[BRACO_E] = {-0.4f * s, 0, -1.f * s};             // bracos abertos
    p.rot[BRACO_D] = {-0.4f * s, 0, 1.f * s};
    p.rot[CABECA].x = 0.25f * s;
    apoiarJuntas(e, p, 0.09f * s);
    return p;
}

Pose poseRendido(const Esqueleto& e, float tempo) {
    Pose p = poseRepouso(e);
    float r = std::sin(tempo * 1.7f) * 0.03f;
    for (int c : {CANELA_E, CANELA_D}) p.rot[c].x = 1.5708f;  // canelas deitadas para tras
    for (int pe : {PE_E, PE_D}) p.rot[pe].x = 0.6f;
    p.rot[BRACO_E] = {-2.85f + r, 0, -0.25f};
    p.rot[BRACO_D] = {-2.85f + r, 0, 0.25f};
    p.rot[ANTEBRACO_E] = {-0.35f, 0, 0};
    p.rot[ANTEBRACO_D] = {-0.35f, 0, 0};
    p.rot[CABECA].x = 0.15f;
    p.rot[PEITO].x = r;
    apoiarJuntas(e, p, 0.04f);
    return p;
}

Pose poseAmarrado(const Esqueleto& e, float tempo) {
    Pose p = poseRepouso(e);
    float r = std::sin(tempo * 1.5f) * 0.03f;
    for (int c : {COXA_E, COXA_D}) p.rot[c].x = -2.0f;     // coxas para a frente e para cima (sentado no chao)
    for (int c : {CANELA_E, CANELA_D}) p.rot[c].x = 2.1f;  // joelhos para cima, pes no chao
    p.rot[COLUNA].x = 0.15f + r;
    p.rot[BRACO_E] = {0.55f, 0, 0.25f};                     // bracos para tras, maos juntas nas costas
    p.rot[BRACO_D] = {0.55f, 0, -0.25f};
    p.rot[ANTEBRACO_E] = {-0.9f, 0.4f, 0};
    p.rot[ANTEBRACO_D] = {-0.9f, -0.4f, 0};
    p.rot[CABECA].x = 0.35f;
    apoiarJuntas(e, p, 0.07f);
    return p;
}

Pose poseGarupa(const Esqueleto& e) {
    Pose p = poseRepouso(e);
    p.rot[QUADRIL].x = 0.75f;                              // pernas para tras e para baixo
    p.rot[COLUNA].x = 1.4f;                                // tronco dobrado, pendurado do outro lado
    p.rot[PEITO].x = 0.4f;
    for (int c : {COXA_E, COXA_D}) p.rot[c].x = 0.6f;
    for (int b : {BRACO_E, BRACO_D}) p.rot[b].x = -0.3f;
    p.rot[CABECA].x = 0.3f;
    return p;
}
