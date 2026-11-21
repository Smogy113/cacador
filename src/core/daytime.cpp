#include "core/daytime.h"
#include <cmath>

namespace {
constexpr float PI = 3.14159265f;
constexpr float NASCER = 6.f, POR = 19.f, INCLINACAO = 0.35f;

float wrap24(float h) {
    h = std::fmod(h, 24.f);
    return h < 0 ? h + 24.f : h;
}

struct Chave {
    float hora;
    Paleta p;
};

Rgb c(float r, float g, float b) { return {r, g, b}; }

// Paleta-chave ao longo do dia. Cores lineares (antes do tone mapping).
const Chave CHAVES[] = {
    // hora, ceuTopo, ceuHorizonte, neblina, sol, ambienteCeu, ambienteChao, densidade, exposicao, grading
    {0.0f, {c(.010f, .014f, .035f), c(.030f, .040f, .080f), c(.025f, .033f, .065f), c(.18f, .22f, .35f), c(.030f, .040f, .075f), c(.012f, .012f, .018f), .0060f, 1.6f, c(.85f, .92f, 1.15f)}},
    {5.0f, {c(.015f, .020f, .050f), c(.060f, .060f, .110f), c(.050f, .052f, .095f), c(.20f, .22f, .35f), c(.035f, .042f, .080f), c(.015f, .014f, .020f), .0060f, 1.5f, c(.90f, .92f, 1.10f)}},
    {6.5f, {c(.180f, .260f, .480f), c(.950f, .550f, .300f), c(.850f, .560f, .380f), c(1.6f, .90f, .45f), c(.280f, .260f, .300f), c(.090f, .070f, .050f), .0045f, 1.1f, c(1.08f, .98f, .90f)}},
    {9.0f, {c(.200f, .420f, .800f), c(.620f, .740f, .880f), c(.640f, .720f, .800f), c(2.6f, 2.3f, 1.9f), c(.360f, .440f, .560f), c(.140f, .120f, .085f), .0030f, 1.0f, c(1.02f, 1.00f, .98f)}},
    {12.f, {c(.160f, .380f, .820f), c(.580f, .720f, .900f), c(.620f, .720f, .840f), c(3.0f, 2.85f, 2.6f), c(.380f, .460f, .600f), c(.150f, .130f, .090f), .0025f, .95f, c(1.00f, 1.00f, 1.00f)}},
    {17.f, {c(.180f, .360f, .720f), c(.780f, .700f, .600f), c(.760f, .680f, .580f), c(2.5f, 1.9f, 1.3f), c(.360f, .400f, .480f), c(.150f, .120f, .080f), .0032f, 1.0f, c(1.05f, 1.00f, .94f)}},
    {18.5f, {c(.220f, .200f, .420f), c(1.30f, .550f, .220f), c(1.000f, .560f, .280f), c(2.2f, 1.0f, .40f), c(.300f, .240f, .260f), c(.120f, .070f, .040f), .0045f, 1.1f, c(1.15f, .97f, .82f)}},
    {19.5f, {c(.040f, .050f, .140f), c(.300f, .160f, .180f), c(.280f, .170f, .180f), c(.30f, .25f, .40f), c(.080f, .075f, .120f), c(.030f, .022f, .025f), .0055f, 1.4f, c(.95f, .92f, 1.08f)}},
    {21.f, {c(.012f, .016f, .040f), c(.035f, .045f, .085f), c(.028f, .036f, .070f), c(.18f, .22f, .35f), c(.032f, .042f, .078f), c(.012f, .012f, .018f), .0060f, 1.6f, c(.85f, .92f, 1.15f)}},
};
constexpr int N = sizeof(CHAVES) / sizeof(CHAVES[0]);

Paleta misturar(const Paleta& a, const Paleta& b, float t) {
    Paleta r;
    r.ceuTopo = lerp(a.ceuTopo, b.ceuTopo, t);
    r.ceuHorizonte = lerp(a.ceuHorizonte, b.ceuHorizonte, t);
    r.neblina = lerp(a.neblina, b.neblina, t);
    r.sol = lerp(a.sol, b.sol, t);
    r.ambienteCeu = lerp(a.ambienteCeu, b.ambienteCeu, t);
    r.ambienteChao = lerp(a.ambienteChao, b.ambienteChao, t);
    r.densidadeNeblina = lerp(a.densidadeNeblina, b.densidadeNeblina, t);
    r.exposicao = lerp(a.exposicao, b.exposicao, t);
    r.grading = lerp(a.grading, b.grading, t);
    return r;
}
}  // namespace

DayClock::DayClock(float horaInicial) : hora_(wrap24(horaInicial)) {}

void DayClock::avancar(float dt) { hora_ = wrap24(hora_ + dt * 24.f / DURACAO_DIA); }

float DayClock::hora() const { return hora_; }

Fase DayClock::fase() const {
    float h = hora_;
    if (h < 5.f) return Fase::Madrugada;
    if (h < 7.f) return Fase::Amanhecer;
    if (h < 18.f) return Fase::Dia;
    if (h < 19.5f) return Fase::PorDoSol;
    return Fase::Noite;
}

static Vec3f direcaoPorAngulo(float ang) {
    // ang 0 = nascente (leste, +x), PI/2 = zenite, PI = poente
    float s = std::sin(ang);
    return normalizar({std::cos(ang), s * std::cos(INCLINACAO), s * std::sin(INCLINACAO)});
}

Vec3f DayClock::direcaoSol() const {
    float h = hora_;
    float ang;
    if (h >= NASCER && h <= POR)
        ang = (h - NASCER) / (POR - NASCER) * PI;
    else {
        float noite = (h > POR) ? h - POR : h + 24.f - POR;  // 0..11
        ang = PI + noite / (24.f - (POR - NASCER)) * PI;
    }
    return direcaoPorAngulo(ang);
}

Vec3f DayClock::direcaoLua() const {
    Vec3f s = direcaoSol();
    return normalizar({-s.x, -s.y, s.z + 0.2f});
}

float DayClock::intensidadeTochas() const {
    float h = hora_;
    if (h >= 18.f) return smoothstep(18.f, 19.5f, h);
    if (h <= 5.5f) return 1.f;
    return 1.f - smoothstep(5.5f, 7.f, h);
}

Paleta DayClock::paleta() const {
    float h = hora_;
    for (int i = 0; i < N; i++) {
        const Chave& a = CHAVES[i];
        const Chave& b = CHAVES[(i + 1) % N];
        float hb = (i + 1 < N) ? b.hora : b.hora + 24.f;
        if (h >= a.hora && h < hb) return misturar(a.p, b.p, smoothstep(a.hora, hb, h));
    }
    return CHAVES[0].p;
}
