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
        p.rot[b].z = s * (-0.05f + resp * 0.01f);  // bracos mais junto ao corpo
        p.rot[a].x = -0.12f;
    }
    apoiarNoChao(e, p);
    return p;
}

// ---------------- andar e correr a partir de dados de marcha humana ----------------
// Curvas medias de laboratorio (graus x % do ciclo, comecando no toque do calcanhar):
// andar (Perry; Winter), correr (Novacheck). Quadril: + = flexao (coxa a frente);
// joelho: + = dobrado; tornozelo: + = ponta para cima (dorsiflexao).
namespace {
struct K { float pct, graus; };
const K QUADRIL_ANDAR[] = {{0, 30}, {10, 30}, {30, 10}, {50, -10}, {60, -5}, {75, 20}, {87, 33}, {100, 30}};
const K JOELHO_ANDAR[] = {{0, 5}, {15, 18}, {40, 5}, {50, 10}, {62, 40}, {73, 62}, {87, 28}, {97, 5}, {100, 5}};
const K TORNOZ_ANDAR[] = {{0, 0}, {7, -7}, {30, 5}, {48, 10}, {62, -18}, {75, -3}, {90, 2}, {100, 0}};
const K QUADRIL_CORRER[] = {{0, 35}, {20, 17}, {38, -12}, {50, -8}, {65, 20}, {80, 45}, {92, 40}, {100, 35}};
const K JOELHO_CORRER[] = {{0, 20}, {15, 40}, {38, 18}, {55, 62}, {70, 95}, {82, 80}, {93, 32}, {100, 20}};
const K TORNOZ_CORRER[] = {{0, 5}, {15, 18}, {38, -25}, {50, -15}, {65, 5}, {85, 10}, {100, 5}};
constexpr float APOIO_ANDAR = 0.6f, APOIO_CORRER = 0.38f;

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
const float RAD = 3.14159265f / 180.f;

// angulos (rad, convencao do esqueleto) de uma perna num ponto do ciclo
void anguloPerna(float pct, float w, float& coxa, float& joelho, float& pe) {
    float q = lerp(curva(QUADRIL_ANDAR, pct), curva(QUADRIL_CORRER, pct * APOIO_CORRER / APOIO_ANDAR < 100.f ? pct : pct), w);
    float j = lerp(curva(JOELHO_ANDAR, pct), curva(JOELHO_CORRER, pct), w);
    float t = lerp(curva(TORNOZ_ANDAR, pct), curva(TORNOZ_CORRER, pct), w);
    coxa = -q * RAD;   // rot.x < 0 leva a coxa para a frente
    joelho = j * RAD;  // rot.x > 0 dobra o joelho
    pe = -t * RAD;     // rot.x > 0 abaixa a ponta
}
}  // namespace

// tamanho do ciclo (2 passos) medido nas proprias curvas: o pe de apoio anda para tras o mesmo
// que o corpo anda para a frente -> sem escorregar
// ponto do pe (calcanhar ou ponta) na pose da curva, com o corpo em repouso
static float zDoPe(const Esqueleto& e, float pct, float w, bool ponta) {
    Pose p = poseRepouso(e);
    anguloPerna(pct, w, p.rot[COXA_E].x, p.rot[CANELA_E].x, p.rot[PE_E].x);
    std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    const Osso& o = e.ossos[PE_E];
    Vec3f pt = ponta ? Vec3f{o.fim.x, 0.f, o.fim.z + 0.01f} : Vec3f{o.inicio.x, 0.f, o.inicio.z - 0.045f};
    return m[PE_E].ponto(pt).z;
}
// tornozelo (pe chato) quando o calcanhar pisa
static float zPisada(const Esqueleto& e, float w) {
    const Osso& o = e.ossos[PE_E];
    return zDoPe(e, 0.f, w, false) - (o.inicio.z - 0.045f - e.ossos[CANELA_E].fim.z);
}
// tamanho do ciclo: o calcanhar pisa, o pe fica parado e a ponta sai exatamente onde a curva a solta
static float cicloDe(const Esqueleto& e, float w) {
    float apoio = lerp(APOIO_ANDAR, APOIO_CORRER, w);
    const Osso& o = e.ossos[PE_E];
    float pontaRel = o.fim.z + 0.01f - e.ossos[CANELA_E].fim.z;
    return std::max(0.4f, (zPisada(e, w) + pontaRel - zDoPe(e, apoio * 100.f, w, true)) / apoio);
}
float fasePorMetro(float v) {
    static const Esqueleto e = esqueletoHumano({});
    static float tab[11];
    static bool feito = false;
    if (!feito) {
        for (int k = 0; k <= 10; k++) tab[k] = cicloDe(e, k / 10.f);
        feito = true;
    }
    float w = corrida(std::max(v, 0.3f)) * 10.f;
    int k = std::min(9, int(w));
    return 6.2831853f / lerp(tab[k], tab[k + 1], w - k);
}

Pose poseAndar(const Esqueleto& e, float f, float v) {
    Pose p = vazia(e);
    float w = corrida(v);                     // mistura andar -> correr
    float andar = std::min(1.f, v / 1.0f);     // comecando a andar: movimentos menores
    float apoio = lerp(APOIO_ANDAR, APOIO_CORRER, w);
    // fase: f = pi/2 e o toque do calcanhar esquerdo (mesma convencao dos bracos)
    float pctE = std::fmod((f - 1.5707963f) / 6.2831853f * 100.f + 1000.f, 100.f), pctD = std::fmod(pctE + 50.f, 100.f);
    float cE, jE, pE, cD, jD, pD;
    anguloPerna(pctE, w, cE, jE, pE);
    anguloPerna(pctD, w, cD, jD, pD);
    // no comeco do balanco a ponta precisa sair limpa do chao (nosso esqueleto tem outras proporcoes)
    auto saida = [&](float pct, float& j, float& pe) {
        float u = pct / 100.f;
        if (u < apoio || u > apoio + 0.16f) return;
        float b = std::sin(3.14159f * (u - apoio) / 0.16f);
        j += lerp(0.24f, 0.08f, w) * b;
        pe -= 0.25f * b;
    };
    saida(pctE, jE, pE);
    saida(pctD, jD, pD);
    p.rot[COXA_E].x = cE * andar;
    p.rot[CANELA_E].x = jE * andar;
    p.rot[PE_E].x = pE * andar;
    p.rot[COXA_D].x = cD * andar;
    p.rot[CANELA_D].x = jD * andar;
    p.rot[PE_D].x = pD * andar;
    // bacia: gira com a perna que vai a frente e cai para o lado da perna no ar (~5 graus)
    float s = std::sin(f), c = std::cos(f);
    p.rot[QUADRIL].y = s * (0.08f + 0.06f * w) * andar;
    p.rot[QUADRIL].z = c * (0.07f - 0.03f * w) * andar;
    p.rot[COLUNA].z = -c * 0.04f * andar;
    // tronco e ombros giram contra a bacia; a cabeca olha para a frente
    p.rot[COLUNA].y = -s * 0.07f * andar;
    p.rot[PEITO].y = -s * (0.08f + 0.1f * w) * andar;
    p.rot[CABECA].y = s * (0.1f + 0.08f * w) * andar;
    p.rot[COLUNA].x = (0.03f + 0.13f * w) * andar;  // correndo inclina para a frente
    p.rot[PEITO].x = 0.03f * w;
    p.rot[CABECA].x = -(0.03f + 0.12f * w) * andar;
    // bracos: balancam contra a perna do mesmo lado; cotovelo dobra mais quando o braco vai a frente
    for (int lado = 0; lado < 2; lado++) {
        float qPerna = (lado == 0 ? cE : cD);                  // < 0 = perna do mesmo lado a frente
        float ombro = qPerna * lerp(0.55f, 0.9f, w) - 0.05f;   // perna a frente -> braco atras
        int b = lado ? BRACO_D : BRACO_E, a = lado ? ANTEBRACO_D : ANTEBRACO_E, mo = lado ? MAO_D : MAO_E;
        float sx = lado ? 1.f : -1.f;
        p.rot[b].x = -ombro * andar;
        p.rot[b].z = sx * (0.07f + 0.05f * w);
        float frente = std::max(0.f, ombro);
        p.rot[a].x = -(lerp(0.3f, 1.25f, w) + frente * lerp(0.5f, 0.25f, w)) * std::max(andar, 0.4f);
        p.rot[mo].x = -0.12f;
    }
    // pe de apoio travado no chao (como nas animacoes com captura de movimento): o tornozelo anda para
    // tras exatamente o que o corpo anda para a frente; quadril e joelho por cinematica inversa
    apoiarNoChao(e, p);
    if (andar > 0.99f) {
        float ciclo = 6.2831853f / fasePorMetro(v);
        const Osso &cx0 = e.ossos[COXA_E], &ca0 = e.ossos[CANELA_E];
        float L1 = len(cx0.fim - cx0.inicio), L2 = len(ca0.fim - ca0.inicio);
        std::vector<Mat4> m;
        for (int lado = 0; lado < 2; lado++) {
            float pct = lado == 0 ? pctE : pctD;
            float u = pct / 100.f;
            if (u >= apoio) continue;
            int cx = lado ? COXA_D : COXA_E, ca = lado ? CANELA_D : CANELA_E, pp = lado ? PE_D : PE_E;
            // onde o tornozelo pisou (curva no toque do calcanhar) e quanto ja andou para tras
            float zPisou = zPisada(e, w);
            // altura do tornozelo pela curva (calcanhar/ponta sobem no fim do apoio)
            calcularMatrizes(e, p, m);
            Vec3f quad = m[cx].ponto(e.ossos[cx].inicio);
            // o pe gira em volta do calcanhar (na pisada) ou da ponta (ao sair), que ficam parados no chao
            float absPeCurva = p.rot[cx].x + p.rot[ca].x + p.rot[pp].x;
            const Osso& op = e.ossos[pp];
            Vec3f tornoz0 = e.ossos[ca].fim;
            Vec3f piv = absPeCurva < 0 ? Vec3f{0, -tornoz0.y, op.inicio.z - 0.045f - tornoz0.z}   // calcanhar
                                       : Vec3f{0, -tornoz0.y, op.fim.z + 0.01f - tornoz0.z};      // ponta
            float ca_ = std::cos(absPeCurva), sa_ = std::sin(absPeCurva);
            Vec3f pivRod{0, piv.y * ca_ - piv.z * sa_, piv.y * sa_ + piv.z * ca_};
            float zTravado = zPisou - u * ciclo;                  // tornozelo com o pe chato
            float zAlvo = zTravado + piv.z - pivRod.z, yAlvo = -pivRod.y;
            float dz = zAlvo - quad.z, dy = yAlvo - quad.y;
            float D = std::clamp(std::sqrt(dz * dz + dy * dy), 0.05f, (L1 + L2) * 0.999f);
            float ang = std::atan2(dz, -dy);
            float a1 = std::acos(std::clamp((L1 * L1 + D * D - L2 * L2) / (2 * L1 * D), -1.f, 1.f));
            float joelho = 3.14159265f - std::acos(std::clamp((L1 * L1 + L2 * L2 - D * D) / (2 * L1 * L2), -1.f, 1.f));
            float absPe = p.rot[cx].x + p.rot[ca].x + p.rot[pp].x;  // mantem a inclinacao do pe da curva
            p.rot[cx].x = -(ang + a1);
            p.rot[ca].x = joelho;
            p.rot[pp].x = absPe - p.rot[cx].x - p.rot[ca].x;
        }
    }
    // altura: apoia o corpo so no(s) pe(s) em fase de apoio (o pe que esta saindo nao "puxa" o corpo)
    {
        std::vector<Mat4> m;
        calcularMatrizes(e, p, m);
        float menor = 1e9f;
        for (int lado = 0; lado < 2; lado++) {
            float u = (lado == 0 ? pctE : pctD) / 100.f;
            if (u >= apoio) continue;
            int o = lado ? PE_D : PE_E;
            Vec3f pts[2];
            pontosDoPe(e, o, pts);
            for (auto& q : pts) menor = std::min(menor, m[o].ponto(q).y);
        }
        if (menor < 1e8f) p.raiz.y -= menor;
        else apoiarNoChao(e, p);
    }
    if (w > 0.01f) {
        float uE = pctE / 100.f, uD = pctD / 100.f;
        auto voo = [&](float u) { return u > apoio && u < 0.5f ? std::sin(3.14159f * (u - apoio) / (0.5f - apoio)) : 0.f; };
        p.raiz.y += 0.05f * w * std::max(voo(uE), voo(uD));
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
        p.rot[b].z = -sx * 0.1f;
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
    p.rot[BRACO_D] = {-1.5708f - ang, 0.3f, 0.f};
    p.rot[ANTEBRACO_D] = {-0.05f - 0.55f * recuo, 0, 0};
    p.rot[MAO_D] = {-0.1f * recuo, 0, 0};
    p.rot[COLUNA].y = 0.12f;
    p.rot[PEITO].y = 0.1f;
    p.rot[CABECA].x = -ang * 0.6f;
    p.rot[CABECA].y = -0.15f;
    p.rot[BRACO_E] = {-0.25f, 0, -0.05f};
    p.rot[ANTEBRACO_E] = {-0.5f, 0, 0};
    apoiarNoChao(e, p);
    return p;
}

Pose poseRecarregar(const Esqueleto& e, float t) {
    Pose p = poseParado(e, t);
    float w = std::sin(t * 6.2831f * 2.f) * 0.08f;
    p.rot[BRACO_D] = {-0.6f, 0.2f, 0};
    p.rot[ANTEBRACO_D] = {-1.35f + w, 0, 0};
    p.rot[BRACO_E] = {-0.55f, -0.25f, 0};
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
