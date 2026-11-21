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

Pose poseAndar(const Esqueleto& e, float f, float v) {
    Pose p = vazia(e);
    float amp = std::clamp(v / 5.f, 0.f, 1.2f);
    float andar = std::min(1.f, v / 1.6f);
    float perna = (0.42f + 0.38f * amp) * andar;
    float s = std::sin(f), c = std::cos(f);
    // pernas: esquerda vai a frente com sin > 0
    p.rot[COXA_E].x = -s * perna;
    p.rot[COXA_D].x = s * perna;
    // joelho dobra quando a perna passa para a frente (fase de voo)
    p.rot[CANELA_E].x = (0.08f + (0.55f + 0.7f * amp) * std::max(0.f, std::cos(f - 0.6f))) * andar;
    p.rot[CANELA_D].x = (0.08f + (0.55f + 0.7f * amp) * std::max(0.f, -std::cos(f - 0.6f))) * andar;
    p.rot[PE_E].x = -0.25f * s * andar;
    p.rot[PE_D].x = 0.25f * s * andar;
    // bracos opostos as pernas
    float braco = (0.32f + 0.45f * amp) * andar;
    p.rot[BRACO_E].x = s * braco;
    p.rot[BRACO_D].x = -s * braco;
    p.rot[BRACO_E].z = -0.06f;
    p.rot[BRACO_D].z = 0.06f;
    p.rot[ANTEBRACO_E].x = -(0.2f + 0.9f * amp) - std::max(0.f, -s) * 0.3f * amp;
    p.rot[ANTEBRACO_D].x = -(0.2f + 0.9f * amp) - std::max(0.f, s) * 0.3f * amp;
    // tronco: gira contra o quadril e inclina ao correr
    p.rot[QUADRIL].y = s * 0.1f * andar;
    p.rot[COLUNA].y = -s * 0.12f * andar;
    p.rot[COLUNA].x = 0.08f * amp;
    p.rot[PEITO].x = 0.04f * amp;
    p.rot[CABECA].x = -0.1f * amp;
    p.rot[QUADRIL].z = c * 0.04f * andar;
    apoiarNoChao(e, p);
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
