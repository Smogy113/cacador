#include "core/anim.h"
#include "core/horse.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr float PI = 3.14159265359f;
constexpr float CICLO = 2.f * PI;

float faseCircular(float radianos) {
    float ciclos = radianos / CICLO;
    return ciclos - std::floor(ciclos);
}

struct PernaGalope {
    int casco;
    float apoio;      // fração do ciclo com a sola no chão
    float elevacao;   // altura máxima buscada na fase de recolhimento
    float centro;     // ajuste longitudinal da passada em metros
    bool dianteira() const { return casco == C_CASCO_DE || casco == C_CASCO_DD; }
};

constexpr std::array<PernaGalope, 4> PERNAS{{
    {C_CASCO_TD, .27f, .37f, -.02f},
    {C_CASCO_TE, .26f, .37f, -.02f},
    // A dianteira direita recebe o peso primeiro. A esquerda alcança mais
    // à frente e termina o apoio antes da suspensão.
    {C_CASCO_DD, .265f, .37f, -.125f},
    {C_CASCO_DE, .24f, .46f, .015f},
}};

struct AlvoCasco {
    Vec3f boleto;
    float giro;
};

AlvoCasco alvoGalope(const Osso& casco, const PernaGalope& perna, float fase) {
    constexpr float PASSADA = CICLO / FASE_GALOPE_POR_METRO;
    float z = casco.fim.z + perna.centro;
    float y = 0.f, giro = 0.f;
    if (fase <= perna.apoio) {
        // Recua exatamente quanto o corpo avança. A sola fica fixa no mundo.
        z += PASSADA * (.5f * perna.apoio - fase);
    } else {
        float t = (fase - perna.apoio) / (1.f - perna.apoio);
        float avanco = t * t * t * (10.f + t * (-15.f + 6.f * t));
        float borda = 1.f - 2.f * t;
        float borda2 = borda * borda;
        // A tangente acompanha a velocidade do apoio nas duas pontas.
        float retorno = t * (1.f - t) * borda * borda2 * borda2 * borda2;
        z += PASSADA * (perna.apoio * (avanco - .5f) - (1.f - perna.apoio) * retorno);
        float arco = std::sin(PI * t);
        float recolhimento = arco * arco;
        y = recolhimento * perna.elevacao * (1.10f - .22f * t);
        giro = recolhimento * (perna.dianteira() ? .68f : -.58f);
    }
    // O alvo da IK é o boleto. O giro do casco acontece ao redor da sola.
    Vec3f boleto = Vec3f{casco.inicio.x, y, z} + Mat4::rotX(giro).vetor(casco.inicio - casco.fim);
    return {boleto, giro};
}

float anguloSagital(Vec3f v) { return std::atan2(-v.z, -v.y); }

void flexionarPerna(Pose& p, int casco, Vec3f acima, Vec3f abaixo, float a, float b,
                    Vec3f ateBoleto, float giroCasco, bool dianteira) {
    float dist2 = ateBoleto.y * ateBoleto.y + ateBoleto.z * ateBoleto.z;
    float cosseno = (dist2 - a * a - b * b) / (2.f * a * b);
    float dobra = std::acos(std::clamp(cosseno, -1.f, 1.f)) * (dianteira ? 1.f : -1.f);
    float alto = anguloSagital(ateBoleto) - std::atan2(b * std::sin(dobra), a + b * std::cos(dobra));
    float altoRepouso = anguloSagital(acima);
    p.rot[casco - 2].x = alto - altoRepouso;
    p.rot[casco - 1].x = dobra + altoRepouso - anguloSagital(abaixo);
    p.rot[casco].x = giroCasco - p.rot[C_CORPO].x - p.rot[casco - 2].x - p.rot[casco - 1].x;
}

Pose poseGalope(const Esqueleto& e, float fase, float tempo) {
    Pose p = poseRepouso(e);
    float f = faseCircular(fase) * CICLO;
    float onda = std::sin(f - 1.95f);
    float inclinacao = .115f * onda;
    p.rot[C_CORPO].x = inclinacao;
    p.rot[C_PESCOCO].x = .16f - .17f * std::sin(f - 1.68f);
    p.rot[C_CABECA].x = -.055f - .07f * std::sin(f - 2.08f);
    p.rot[C_CAUDA].x = -.43f + .13f * std::sin(f - 3.15f);
    p.rot[C_CAUDA].z = .11f * std::sin(tempo * 1.3f) + .06f * std::sin(f - 1.f);

    Vec3f piv = e.ossos[C_CORPO].inicio;
    Mat4 corpo = Mat4::rotX(inclinacao);
    Mat4 inversa = Mat4::rotX(-inclinacao);
    // Traz o eixo da garupa para o centro do tronco, sob a sela.
    Vec3f ateSela{0, 0, assentoCavalo().z - piv.z};
    p.raiz = ateSela - corpo.vetor(ateSela);
    // O primeiro harmônico eleva o corpo na suspensão. O segundo distribui
    // a compressão entre a saída das patas traseiras e o apoio dianteiro.
    float altura = -.127f + .052f * std::cos(f - .43f) - .021f * std::cos(2.f * (f - 4.92f));
    altura += .050f * smoothstep(.78f, .99f, std::cos(f - 4.43f)); // extensão no apoio dianteiro
    altura += .052f * smoothstep(.65f, .99f, std::cos(f - 6.10f)); // elevação na suspensão

    struct PernaPosada { AlvoCasco alvo; Vec3f quadril, acima, abaixo; float a, b; };
    std::array<PernaPosada, 4> pernas;
    for (size_t i = 0; i < PERNAS.size(); ++i) {
        int casco = PERNAS[i].casco;
        const Vec3f quadril = e.ossos[casco - 2].inicio;
        const Vec3f junta = e.ossos[casco - 1].inicio;
        const Osso& osso = e.ossos[casco];
        Vec3f acima = junta - quadril, abaixo = osso.inicio - junta;
        float a = std::hypot(acima.y, acima.z), b = std::hypot(abaixo.y, abaixo.z);
        AlvoCasco alvo = alvoGalope(osso, PERNAS[i], faseCircular(f - TOQUES_GALOPE[i]));
        Vec3f quadrilPosado = piv + p.raiz + corpo.vetor(quadril - piv);
        pernas[i] = {alvo, quadrilPosado, acima, abaixo, a, b};

        // Mantém os alvos ao alcance, com uma pequena folga na extensão total.
        float z = alvo.boleto.z - quadrilPosado.z;
        float alcance = a + b - .008f;
        float limite = alvo.boleto.y - quadrilPosado.y + std::sqrt(std::max(0.f, alcance * alcance - z * z));
        float folga = std::max(.04f - std::abs(altura - limite), 0.f);
        altura = std::min(altura, limite) - folga * folga / .16f;
    }
    p.raiz.y += altura;

    for (size_t i = 0; i < PERNAS.size(); ++i) {
        const PernaPosada& perna = pernas[i];
        Vec3f ateBoleto = perna.alvo.boleto - perna.quadril - Vec3f{0, altura, 0};
        flexionarPerna(p, PERNAS[i].casco, perna.acima, perna.abaixo, perna.a, perna.b,
                       inversa.vetor(ateBoleto), perna.alvo.giro, PERNAS[i].dianteira());
    }
    return p;
}

Pose poseLenta(const Esqueleto& e, float f, int andadura, float t) {
    Pose p = poseRepouso(e);
    float amp = andadura == 0 ? 0.f : (andadura == 1 ? .32f : .5f);
    const float phPasso[] = {0.f, PI, PI / 2.f, PI * 1.5f};
    const float phTrote[] = {0.f, PI, PI, 0.f};
    const float* ph = andadura == 2 ? phTrote : phPasso;
    const int sup[] = {C_BRACO_DE, C_BRACO_DD, C_COXA_TE, C_COXA_TD};
    for (int k = 0; k < 4; ++k) {
        float a = f + ph[k];
        p.rot[sup[k]].x = -std::sin(a) * amp;
        float dobra = std::max(0.f, std::cos(a - .5f)) * (amp > 0 ? .3f + 1.4f * amp : 0.f);
        p.rot[sup[k] + 1].x = k < 2 ? dobra : dobra * .6f;
        p.rot[sup[k] + 2].x = -dobra * .4f;
    }
    p.rot[C_PESCOCO].x = std::sin(f * 2.f) * .06f * amp;
    if (andadura == 0) {
        p.rot[C_PESCOCO].x = .05f + std::sin(t * .4f) * .06f;
        p.rot[C_PESCOCO].y = std::sin(t * .23f) * .12f;
        p.rot[C_CORPO].x = std::sin(t * 1.2f) * .008f;
    }
    p.rot[C_CAUDA].z = std::sin(t * 1.3f) * .18f;
    p.rot[C_CAUDA].x = -.15f * amp;
    p.raiz.y -= alturaDosCascos(e, p);
    return p;
}

}  // namespace

float alturaDosCascos(const Esqueleto& e, const Pose& p) {
    static thread_local std::vector<Mat4> m;
    calcularMatrizes(e, p, m);
    float menor = 1e9f;
    for (int c : {C_CASCO_DE, C_CASCO_DD, C_CASCO_TE, C_CASCO_TD}) {
        const Osso& o = e.ossos[c];
        for (float dz : {-MEIO_COMPRIMENTO_CASCO, MEIO_COMPRIMENTO_CASCO})
            menor = std::min(menor, m[c].ponto({o.fim.x, 0.f, o.fim.z + dz}).y);
    }
    return menor;
}

Pose poseCavalo(const Esqueleto& e, float fase, int andadura, float tempo) {
    return andadura == int(Andadura::Galope) ? poseGalope(e, fase, tempo) : poseLenta(e, fase, andadura, tempo);
}

Pose poseCavalo(const Esqueleto& e, const Cavalo& cavalo, float tempo) {
    float mistura = pesoGalopeCavalo(cavalo.corpo.vel);
    if (mistura == 0.f) return poseLenta(e, cavalo.passada, int(cavalo.andadura), tempo);
    Pose galope = poseGalope(e, cavalo.passada, tempo);
    if (mistura == 1.f) return galope;

    Pose p = poseLenta(e, cavalo.passada, int(Andadura::Trote), tempo);
    p.raiz = p.raiz * (1.f - mistura) + galope.raiz * mistura;
    for (size_t i = 0; i < p.rot.size(); ++i) p.rot[i] = p.rot[i] * (1.f - mistura) + galope.rot[i] * mistura;
    p.raiz.y -= std::min(0.f, alturaDosCascos(e, p));
    return p;
}

Pose poseMontado(const Esqueleto& e, const Cavalo& cavalo, const Pose& poseDaMontaria) {
    float f = faseCircular(cavalo.passada) * CICLO;
    float peso = pesoGalopeCavalo(cavalo.corpo.vel);
    int andadura = std::min(int(cavalo.andadura), int(Andadura::Trote));
    Pose p = poseMontado(e, f, andadura);
    float giro = poseDaMontaria.rot[C_CORPO].x;
    float absorver = std::sin(f - 2.4f);
    // O quadril e as pernas acompanham os estribos. O tronco absorve o impacto.
    p.rot[QUADRIL].x = giro;
    p.raiz.y *= 1.f - peso;
    p.rot[COLUNA].x = lerp(p.rot[COLUNA].x, .22f + .11f * absorver - .35f * giro, peso);
    p.rot[CABECA].x = lerp(p.rot[CABECA].x, -.14f - .07f * absorver - .35f * giro, peso);
    for (int braco : {BRACO_E, BRACO_D}) {
        p.rot[braco].x += .10f * absorver * peso;
        p.rot[braco + 1].x -= .16f * absorver * peso;
    }
    return p;
}
