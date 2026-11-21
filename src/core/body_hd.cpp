// Corpo humano em alta definicao (usado perto da camera).
// Mesmo esqueleto e mesmas proporcoes da versao leve, com:
//  - aneis suavizados (Catmull-Rom) e mais lados -> silhueta redonda
//  - rosto esculpido por relevos (nariz, orbitas, macas, labios, queixo, mandibula) e orelhas
//  - dedos, gola, lapelas, cinto com fivela, botas com sola, chapeu com copa amassada
//  - equipamento opcional: bandoleira com virotes, besta nas costas, laco na cintura
#include <algorithm>
#include <cmath>
#include "core/body.h"
#include "core/loft.h"

using namespace loft;

namespace {
float g(float x, float c, float s) {
    float t = (x - c) / s;
    return std::exp(-t * t);
}

// perfil do tronco (mesma tabela da versao leve)
struct T {
    float y, rx, rz, z;
    int a, b;
    float p;
};
const T TRONCO[] = {
    {0.84f, .130f, .100f, -.005f, QUADRIL, QUADRIL, 1},  {0.90f, .165f, .115f, -.010f, QUADRIL, QUADRIL, 1},
    {0.97f, .175f, .120f, -.010f, QUADRIL, QUADRIL, 1},  {1.05f, .158f, .108f, .000f, QUADRIL, COLUNA, .5f},
    {1.13f, .150f, .102f, .005f, COLUNA, COLUNA, 1},     {1.22f, .158f, .108f, .010f, COLUNA, PEITO, .5f},
    {1.31f, .172f, .118f, .015f, PEITO, PEITO, 1},       {1.39f, .185f, .122f, .010f, PEITO, PEITO, 1},
    {1.44f, .192f, .112f, .002f, PEITO, PEITO, 1},       {1.475f, .168f, .098f, .000f, PEITO, PEITO, 1},
    {1.505f, .120f, .080f, .000f, PEITO, PESCOCO, .6f},
    {1.54f, .068f, .066f, .005f, PESCOCO, PESCOCO, 1},   {1.59f, .064f, .064f, .010f, PESCOCO, CABECA, .6f},
    {1.625f, .066f, .078f, .020f, CABECA, CABECA, 1},    {1.655f, .075f, .092f, .015f, CABECA, CABECA, 1},
    {1.69f, .083f, .100f, .010f, CABECA, CABECA, 1},     {1.72f, .088f, .103f, .000f, CABECA, CABECA, 1},
    {1.75f, .087f, .100f, -.005f, CABECA, CABECA, 1},    {1.775f, .075f, .088f, -.010f, CABECA, CABECA, 1},
    {1.795f, .045f, .055f, -.010f, CABECA, CABECA, 1},
};
constexpr int NT = sizeof(TRONCO) / sizeof(TRONCO[0]);

// raio do tronco numa altura (para por coisas por cima da roupa)
void perfil(float y, float& rx, float& rz, float& z) {
    if (y <= TRONCO[0].y) { rx = TRONCO[0].rx; rz = TRONCO[0].rz; z = TRONCO[0].z; return; }
    for (int i = 0; i + 1 < NT; i++)
        if (y <= TRONCO[i + 1].y) {
            float t = (y - TRONCO[i].y) / (TRONCO[i + 1].y - TRONCO[i].y);
            rx = lerp(TRONCO[i].rx, TRONCO[i + 1].rx, t);
            rz = lerp(TRONCO[i].rz, TRONCO[i + 1].rz, t);
            z = lerp(TRONCO[i].z, TRONCO[i + 1].z, t);
            return;
        }
    rx = TRONCO[NT - 1].rx; rz = TRONCO[NT - 1].rz; z = TRONCO[NT - 1].z;
}

Pintor cor(Rgb c) {
    return [c](int, float, Vec3f&, Vec3f, Vec3f, float&) -> Rgb { return c; };
}
}  // namespace

MalhaPele corpoHumanoHD(const Proporcoes& pr, const Visual& vis) {
    MalhaPele m;
    Construtor k{m};
    const float s = pr.altura / 1.8f, ms = pr.massa, lo = pr.ombros, lq = pr.quadril;
    Esqueleto esq = esqueletoHumano(pr);
    Rgb tronco = vis.sobretudo ? vis.casaco : vis.camisa;
    Rgb mangas = vis.sobretudo ? vis.casaco : vis.camisa;
    const Rgb cinto{0.2f, 0.13f, 0.08f}, metal{0.55f, 0.5f, 0.42f}, madeira{0.4f, 0.27f, 0.15f};
    auto An = [&](Vec3f c, float rx, float rz, int a, int b, float p) { return Anel{c, rx * s * ms, rz * s * ms, uint8_t(a), uint8_t(b), p}; };
    auto L = [](Vec3f a, Vec3f b, float t) { return a + (b - a) * t; };

    // ================= tronco, pescoco e cabeca =================
    {
        std::vector<Anel> an;
        for (auto& t : TRONCO) {
            float largura = t.y < 1.0f ? lq : (t.y > 1.25f && t.y < 1.52f ? lo : 1.f);
            an.push_back(Anel{{0, t.y * s, t.z * s}, t.rx * largura * s * ms, t.rz * s * ms, uint8_t(t.a), uint8_t(t.b), t.p});
        }
        auto sv = suavizar(an, 3);
        k.material = MAT_PELE;
        k.tubo(sv, 36, {0, 0, 1}, true, true,
               [&](int, float ang, Vec3f& p, Vec3f la, Vec3f fr, float& ao) -> Rgb {
                   float y = p.y / s;
                   if (ang < 0) {
                       if (y < 1.f) { ao = 0.7f; k.materialVert = MAT_TECIDO; return vis.calca; }
                       k.materialVert = MAT_CABELO;
                       return vis.cabelo;
                   }
                   float f = std::sin(ang), l = std::cos(ang), al = std::abs(l);
                   float frente = f > 0 ? 1.f : 0.f;
                   if (y < 1.56f) k.materialVert = (y >= 1.0f && y <= 1.06f) ? MAT_COURO : MAT_TECIDO;
                   if (y > 1.6f) {
                       // ---- rosto esculpido ----
                       float d = 0;
                       float meio = g(l, 0, 0.13f) * frente;
                       d += meio * (0.022f * g(y, 1.683f, 0.011f) + 0.011f * g(y, 1.70f, 0.012f) + 0.005f * g(y, 1.716f, 0.007f));  // nariz
                       float olho = g(y, 1.712f, 0.009f) * g(al, 0.38f, 0.12f) * frente;
                       d -= 0.013f * olho;                                                              // orbitas (abrigam o globo ocular)
                       d += 0.0045f * g(y, 1.7215f, 0.0035f) * g(al, 0.38f, 0.1f) * frente;            // palpebra de cima
                       d += 0.0025f * g(y, 1.7025f, 0.003f) * g(al, 0.38f, 0.09f) * frente;            // palpebra de baixo
                       d -= 0.0035f * g(y, 1.672f, 0.014f) * g(al, 0.27f + (1.69f - y) * 1.6f, 0.05f) * frente;  // sulco nariz-boca
                       d += 0.007f * g(y, 1.729f, 0.007f) * g(al, 0.33f, 0.25f) * (f > 0.3f ? 1.f : 0.f);  // sobrancelha
                       d += 0.007f * g(y, 1.69f, 0.012f) * g(al, 0.62f, 0.12f) * frente;                  // macas do rosto
                       d += 0.006f * g(y, 1.656f, 0.006f) * g(l, 0, 0.22f) * frente;                      // labios
                       d -= 0.004f * g(y, 1.648f, 0.003f) * g(l, 0, 0.2f) * frente;                       // boca
                       d += 0.011f * g(y, 1.625f, 0.01f) * g(l, 0, 0.3f) * frente;                        // queixo
                       d += 0.006f * g(y, 1.635f, 0.012f) * g(al, 0.75f, 0.15f);                          // mandibula
                       d += 0.006f * g(y, 1.742f, 0.03f) * (f < -0.3f ? 1.f : 0.f);                       // nuca
                       p = p + (la * l + fr * f) * (d * s);
                       if (vis.chapeuAba && y > 1.66f) ao = 0.82f;  // sombra da aba
                       if (olho > 0.15f) ao *= 0.85f;
                       if (g(y, 1.656f, 0.005f) * g(l, 0, 0.16f) * frente > 0.55f)
                           return Rgb{vis.pele.r * 0.92f, vis.pele.g * 0.7f, vis.pele.b * 0.66f};  // labios
                       // barba e cabelo com bordas suaves (parece barba por fazer)
                       float wb = 0.f;
                       if (vis.barba) {
                           float queixo = smoothstep(1.648f, 1.632f, y) * smoothstep(0.3f, 0.65f, f);
                           float bigode = g(y, 1.665f, 0.0045f) * smoothstep(0.78f, 0.95f, f);
                           float sombraBarba = smoothstep(1.668f, 1.64f, y) * smoothstep(-0.1f, 0.3f, f) * 0.35f;
                           wb = std::max(std::max(queixo, bigode), sombraBarba);
                       }
                       float wc = std::max(smoothstep(1.758f, 1.772f, y + 0.03f * f), (y >= 1.66f ? smoothstep(-0.25f, -0.45f, f) : 0.f));
                       float w = std::max(wb, wc);
                       if (w > 0.5f) k.materialVert = MAT_CABELO;
                       return lerp(vis.pele, vis.cabelo, std::min(1.f, w * 1.1f));

                   }
                   // ---- corpo ----
                   if (y > 1.3f && y < 1.42f) {  // peitoral e escapulas por baixo da roupa
                       float d = 0.008f * g(y, 1.36f, 0.04f) * g(al, 0.4f, 0.2f) * frente + 0.006f * g(y, 1.38f, 0.04f) * g(al, 0.45f, 0.2f) * (f < 0 ? 1.f : 0.f);
                       p = p + (la * l + fr * f) * (d * s);
                   }
                   if (y > 1.56f) return vis.pele;
                   if (y >= 1.0f && y <= 1.06f) return cinto;
                   if (y < 1.0f) {
                       if (y <= 0.9f) ao = 0.82f;
                       return vis.calca;
                   }
                   if (y >= 1.45f && f < -0.2f) ao = 0.92f;
                   return tronco;
               },
               0.03f * s, 0.012f * s);
    }
    // globos oculares (esclera, iris e pupila), dentro das orbitas
    {
        float rx, rz, z;
        perfil(1.712f, rx, rz, z);
        float r = 0.0125f * s;
        for (float sx : {1.f, -1.f}) {
            Vec3f c{sx * 0.36f * rx * s * ms, 1.712f * s, z * s + 0.93f * rz * s * ms - 0.0105f * s};
            std::vector<Anel> olho;
            const int NR = 9;
            for (int i = 0; i <= NR; i++) {
                float th = 3.14159f * (1.f - i / float(NR)) * 0.96f + 0.02f;  // de tras para a frente
                olho.push_back(Anel{c + Vec3f{0, 0, std::cos(th) * r}, std::sin(th) * r + 0.0005f, std::sin(th) * r + 0.0005f, CABECA, CABECA, 1});
            }
            k.material = MAT_OLHO;
            Rgb iris = vis.cabelo.r > 0.4f ? Rgb{0.25f, 0.38f, 0.5f} : Rgb{0.32f, 0.2f, 0.1f};
            k.tubo(olho, 16, {0, 1, 0}, true, true, [&](int i, float ang, Vec3f&, Vec3f, Vec3f, float&) -> Rgb {
                if (ang < 0 && i > 0) return Rgb{0.02f, 0.02f, 0.02f};  // pupila
                if (i >= NR - 2) return iris;
                return Rgb{0.85f, 0.83f, 0.8f};
            }, 0.f, 0.0008f * s);
        }
    }

    // orelhas
    for (float sx : {1.f, -1.f}) {
        float x = sx * 0.088f * s * ms;
        std::vector<Anel> o = {An({x, 1.676f * s, -0.004f * s}, .005f, .011f, CABECA, CABECA, 1), An({x + sx * 0.006f * s, 1.70f * s, -0.008f * s}, .008f, .02f, CABECA, CABECA, 1),
                               An({x + sx * 0.005f * s, 1.726f * s, -0.01f * s}, .006f, .015f, CABECA, CABECA, 1)};
        k.material = MAT_PELE;
        k.tubo(o, 8, {0, 0, 1}, true, true, cor(vis.pele * 0.96f), 0.004f * s, 0.006f * s);
    }
    // lenco com no
    {
        std::vector<Anel> l = {An({0, 1.525f * s, 0.006f * s}, .082f, .078f, PESCOCO, PEITO, .5f), An({0, 1.56f * s, 0.008f * s}, .074f, .072f, PESCOCO, PESCOCO, 1)};
        k.material = MAT_TECIDO;
        k.tubo(l, 28, {0, 0, 1}, false, false, cor(vis.lenco));
        std::vector<Anel> no = {An({0.02f * s, 1.535f * s, 0.085f * s}, .016f, .012f, PESCOCO, PEITO, .5f), An({0.03f * s, 1.5f * s, 0.095f * s}, .022f, .01f, PEITO, PEITO, 1),
                                An({0.035f * s, 1.45f * s, 0.105f * s}, .03f, .006f, PEITO, PEITO, 1)};
        k.material = MAT_TECIDO;
        k.tubo(no, 8, {0, 0, 1}, true, true, cor(vis.lenco * 0.9f), 0.005f * s, 0.004f * s);
    }

    // ================= bracos, maos e dedos =================
    for (int lado = 0; lado < 2; lado++) {
        float sx = lado == 0 ? 1.f : -1.f;
        int b = lado == 0 ? BRACO_E : BRACO_D, ab = lado == 0 ? ANTEBRACO_E : ANTEBRACO_D, mo = lado == 0 ? MAO_E : MAO_D;
        const Osso &ob = esq.ossos[b], &oa = esq.ossos[ab], &om = esq.ossos[mo];
        std::vector<Anel> br = {
            An(L(ob.inicio, ob.fim, -0.03f), .062f, .066f, b, b, 1), An(L(ob.inicio, ob.fim, 0.3f), .058f, .060f, b, b, 1),
            An(L(ob.inicio, ob.fim, 0.72f), .050f, .051f, b, b, 1), An(L(ob.inicio, ob.fim, 0.9f), .048f, .049f, b, ab, .78f),
            An(oa.inicio, .049f, .051f, b, ab, .5f), An(L(oa.inicio, oa.fim, 0.1f), .047f, .048f, ab, b, .78f),
            An(L(oa.inicio, oa.fim, 0.3f), .047f, .048f, ab, ab, 1), An(L(oa.inicio, oa.fim, 0.8f), .040f, .040f, ab, ab, 1),
            An(L(oa.inicio, oa.fim, 0.93f), .043f, .043f, ab, ab, 1),  // punho do casaco
            An(om.inicio, .028f, .03f, ab, mo, .5f), An(L(om.inicio, om.fim, 0.35f), .016f, .045f, mo, mo, 1),
            An(L(om.inicio, om.fim, 0.7f), .016f, .043f, mo, mo, 1),
        };
        size_t nb = br.size();
        auto sv = suavizar(br, 3);
        size_t pulso = (nb - 3) * 3;  // indice (suavizado) do anel do pulso
        k.material = MAT_TECIDO;
        k.tubo(sv, 16, {0, 0, 1}, true, true,
               [&, pulso](int i, float ang, Vec3f& p, Vec3f la, Vec3f fr, float& ao) -> Rgb {
                   if (i < 3) ao = 0.78f;
                   if (i >= 8 && i <= 16) {  // rugas do cotovelo
                       float r = std::sin(ang * 4.f + float(i) * 1.7f);
                       p = p + (la * std::cos(ang) + fr * std::sin(ang)) * (0.003f * s * r);
                       ao *= 0.9f + 0.1f * r;
                   }
                   if (size_t(i) >= pulso) { k.materialVert = MAT_PELE; return vis.pele; }
                   if (size_t(i) >= pulso - 3) return mangas * 0.85f;  // punho
                   return mangas;
               },
               0.02f * s, 0.008f * s);
        // dedos: saem da ponta da palma, curvados para dentro (palma virada para a coxa)
        Vec3f ponta = L(om.inicio, om.fim, 0.72f);
        const float zs[4] = {-0.027f, -0.009f, 0.009f, 0.027f}, comp[4] = {0.062f, 0.074f, 0.07f, 0.056f};
        for (int d = 0; d < 4; d++) {
            Vec3f base = ponta + Vec3f{-sx * 0.002f * s, 0, zs[d] * s};
            float c = comp[d] * s;
            std::vector<Anel> dd = {Anel{base, .0085f * s, .0085f * s, uint8_t(mo), uint8_t(mo), 1},
                                    Anel{base + Vec3f{-sx * 0.006f * s, -c * 0.45f, 0}, .0078f * s, .0078f * s, uint8_t(mo), uint8_t(mo), 1},
                                    Anel{base + Vec3f{-sx * 0.016f * s, -c * 0.8f, 0}, .0068f * s, .0068f * s, uint8_t(mo), uint8_t(mo), 1},
                                    Anel{base + Vec3f{-sx * 0.026f * s, -c, 0}, .006f * s, .006f * s, uint8_t(mo), uint8_t(mo), 1}};
            k.material = MAT_PELE;
            k.tubo(dd, 6, {0, 0, 1}, false, true, cor(vis.pele), 0, 0.005f * s);
        }
        // polegar
        Vec3f pb = L(om.inicio, om.fim, 0.25f) + Vec3f{-sx * 0.006f * s, 0, 0.038f * s};
        std::vector<Anel> po = {Anel{pb, .011f * s, .011f * s, uint8_t(mo), uint8_t(mo), 1},
                                Anel{pb + Vec3f{-sx * 0.008f * s, -0.03f * s, 0.018f * s}, .009f * s, .009f * s, uint8_t(mo), uint8_t(mo), 1},
                                Anel{pb + Vec3f{-sx * 0.016f * s, -0.055f * s, 0.02f * s}, .007f * s, .007f * s, uint8_t(mo), uint8_t(mo), 1}};
        k.material = MAT_PELE;
        k.tubo(po, 6, {0, 0, 1}, false, true, cor(vis.pele), 0, 0.006f * s);
    }

    // ================= pernas, botas e pes =================
    for (int lado = 0; lado < 2; lado++) {
        int c = lado == 0 ? COXA_E : COXA_D, ca = lado == 0 ? CANELA_E : CANELA_D, pe = lado == 0 ? PE_E : PE_D;
        const Osso &oc = esq.ossos[c], &oca = esq.ossos[ca];
        float sx = lado == 0 ? 1.f : -1.f;
        auto Ap = [&](Vec3f cc, float rx, float rz, int a_, int b_, float p) { return Anel{cc, rx * s * ms * lq, rz * s * ms, uint8_t(a_), uint8_t(b_), p}; };
        std::vector<Anel> pr2 = {
            Ap(L(oc.inicio, oc.fim, -0.05f), .090f, .095f, c, QUADRIL, .6f), Ap(L(oc.inicio, oc.fim, 0.4f), .078f, .082f, c, c, 1),
            Ap(L(oc.inicio, oc.fim, 0.85f), .060f, .062f, c, ca, .85f), Ap(L(oc.inicio, oc.fim, 0.93f), .057f, .06f, c, ca, .75f),
            Ap(oca.inicio, .058f, .062f, c, ca, .5f), Ap(L(oca.inicio, oca.fim, 0.08f), .056f, .06f, ca, c, .75f),
            Ap(L(oca.inicio, oca.fim, 0.3f) + Vec3f{0, 0, -0.01f * s}, .056f, .062f, ca, ca, 1),
            Ap(L(oca.inicio, oca.fim, 0.52f), .056f, .058f, ca, ca, 1),  // borda da bota
            Ap(L(oca.inicio, oca.fim, 0.58f), .050f, .052f, ca, ca, 1),
            Ap(L(oca.inicio, oca.fim, 0.8f), .044f, .046f, ca, ca, 1), Ap(oca.fim + Vec3f{0, 0.01f * s, 0}, .038f, .042f, ca, pe, .6f),
        };
        float yBota = L(oca.inicio, oca.fim, 0.54f).y;
        k.material = MAT_TECIDO;
        k.tubo(suavizar(pr2, 3), 16, {0, 0, 1}, true, false, [&](int, float, Vec3f& p, Vec3f, Vec3f, float& ao) -> Rgb {
            if (p.y > 0.7f * s && (p.x * sx) < 0.09f * s * lq) ao = 0.85f;
            if (p.y <= yBota) { k.materialVert = MAT_COURO; return vis.botas; }
            return vis.calca;
        });
        float px = oca.fim.x;
        std::vector<Anel> pes = {
            Anel{{px, 0.055f * s, -0.055f * s}, .037f * s * lq, .042f * s, uint8_t(pe), uint8_t(ca), .4f},
            Anel{{px, 0.055f * s, -0.02f * s}, .044f * s * lq, .045f * s, uint8_t(pe), uint8_t(pe), 1},
            Anel{{px + sx * 0.005f, 0.045f * s, 0.06f * s}, .048f * s * lq, .034f * s, uint8_t(pe), uint8_t(pe), 1},
            Anel{{px + sx * 0.008f, 0.036f * s, 0.15f * s}, .044f * s * lq, .024f * s, uint8_t(pe), uint8_t(pe), 1},
        };
        k.material = MAT_COURO;
        k.tubo(suavizar(pes, 3), 16, {0, 1, 0}, true, true, cor(vis.botas), 0.008f * s, 0.018f * s);
        // sola e salto
        std::vector<Anel> sola = {Anel{{px, 0.012f * s, -0.06f * s}, .04f * s * lq, .012f * s, uint8_t(pe), uint8_t(pe), 1},
                                  Anel{{px, 0.012f * s, 0.0f}, .046f * s * lq, .012f * s, uint8_t(pe), uint8_t(pe), 1},
                                  Anel{{px + sx * 0.008f, 0.012f * s, 0.16f * s}, .045f * s * lq, .012f * s, uint8_t(pe), uint8_t(pe), 1}};
        k.material = MAT_COURO;
        k.tubo(sola, 10, {0, 1, 0}, true, true, cor(Rgb{0.08f, 0.06f, 0.05f}), 0.004f * s, 0.01f * s);
    }

    // ================= sobretudo: abas, gola e lapelas =================
    if (vis.sobretudo) {
        const struct { float y, rx, rz, pc; } sb[] = {{1.02f, .185f, .128f, 0.f}, {0.86f, .195f, .142f, .45f}, {0.70f, .21f, .155f, .65f}, {0.54f, .222f, .165f, .8f}};
        for (int lado = 0; lado < 2; lado++) {
            std::vector<Anel> an2;
            int coxa = lado == 0 ? COXA_E : COXA_D;
            for (auto& q : sb) an2.push_back(Anel{{0, q.y * s, -0.01f * s}, q.rx * s * ms * lq, q.rz * s * ms, uint8_t(coxa), uint8_t(QUADRIL), q.pc});
            size_t ini = m.v.size(), idxIni = m.idx.size();
            k.material = MAT_TECIDO;
            k.tubo(suavizar(an2, 3), 32, {0, 0, 1}, false, false, [&](int, float ang, Vec3f& p, Vec3f la, Vec3f fr, float& ao) -> Rgb {
                float baixo = clamp01((1.0f * s - p.y) / (0.45f * s));
                float dobra = std::sin(ang * 9.f + 1.3f) * 0.6f + std::sin(ang * 5.f + 0.4f) * 0.4f;
                p = p + (la * std::cos(ang) + fr * std::sin(ang)) * (0.008f * s * dobra * baixo);
                ao = 0.88f + 0.12f * (dobra * 0.5f + 0.5f);
                return vis.casaco * (p.y < 0.57f * s ? 0.85f : 0.95f);  // barra mais escura
            });
            std::vector<uint16_t> novos;
            for (size_t t = idxIni; t < m.idx.size(); t += 3) {
                Vec3f c3 = (m.v[m.idx[t]].pos + m.v[m.idx[t + 1]].pos + m.v[m.idx[t + 2]].pos) * (1.f / 3.f);
                bool meu = lado == 0 ? c3.x >= -0.001f : c3.x <= 0.001f;
                bool frente = c3.z > 0.1f * s && std::abs(c3.x) < 0.05f * s;
                if (meu && !frente) novos.insert(novos.end(), {m.idx[t], m.idx[t + 1], m.idx[t + 2]});
            }
            m.idx.resize(idxIni);
            m.idx.insert(m.idx.end(), novos.begin(), novos.end());
            for (size_t vi = ini; vi < m.v.size(); vi++)
                if (m.v[vi].pos.y > 1.0f * s) { m.v[vi].osso[0] = QUADRIL; m.v[vi].peso = 1; }
        }
        // gola levantada
        std::vector<Anel> gola = {An({0, 1.47f * s, 0}, .145f * lo, .1f, PEITO, PEITO, 1), An({0, 1.53f * s, -0.005f * s}, .1f, .088f, PEITO, PESCOCO, .6f),
                                  An({0, 1.585f * s, -0.01f * s}, .088f, .082f, PESCOCO, PESCOCO, 1)};
        size_t gi = m.idx.size();
        k.material = MAT_TECIDO;
        k.tubo(gola, 28, {0, 0, 1}, false, false, cor(vis.casaco * 0.9f));
        std::vector<uint16_t> ok;
        for (size_t t = gi; t < m.idx.size(); t += 3) {
            Vec3f c3 = (m.v[m.idx[t]].pos + m.v[m.idx[t + 1]].pos + m.v[m.idx[t + 2]].pos) * (1.f / 3.f);
            if (!(c3.z > 0.04f * s && std::abs(c3.x) < 0.045f * s)) ok.insert(ok.end(), {m.idx[t], m.idx[t + 1], m.idx[t + 2]});
        }
        m.idx.resize(gi);
        m.idx.insert(m.idx.end(), ok.begin(), ok.end());
        // lapelas (por cima do peito)
        for (float sx : {1.f, -1.f}) {
            std::vector<Anel> lp;
            for (float y : {1.47f, 1.4f, 1.32f, 1.24f}) {
                float rx, rz, z;
                perfil(y, rx, rz, z);
                float x = sx * lerp(0.05f, 0.025f, (1.47f - y) / 0.23f);
                lp.push_back(An({x * s, y * s, (z + rz + 0.006f) * s}, lerp(.035f, .012f, (1.47f - y) / 0.23f), .005f, PEITO, y < 1.25f ? COLUNA : PEITO, y < 1.25f ? .5f : 1.f));
            }
            k.material = MAT_TECIDO;
            k.tubo(lp, 6, {0, 0, 1}, true, true, cor(vis.casaco * 0.8f), 0.002f, 0.002f);
        }
    }
    // cinto com fivela
    {
        float rx, rz, z;
        perfil(1.03f, rx, rz, z);
        std::vector<Anel> c = {An({0, 1.005f * s, z * s}, rx * lq + .006f, rz + .006f, QUADRIL, QUADRIL, 1), An({0, 1.055f * s, z * s}, rx + .006f, rz + .006f, QUADRIL, COLUNA, .5f)};
        k.material = MAT_COURO;
        k.tubo(c, 28, {0, 0, 1}, false, false, cor(cinto));
        float zf = (z + rz + 0.008f);
        std::vector<Anel> f = {An({0.03f * s, 1.03f * s, zf * s}, .028f, .004f, QUADRIL, QUADRIL, 1), An({0.03f * s, 1.03f * s, (zf + 0.006f) * s}, .028f, .004f, QUADRIL, QUADRIL, 1)};
        // fivela: placa quadrada (tubo curto de 4 lados)
        std::vector<Anel> fv = {An({0.03f * s, 1.012f * s, zf * s}, .024f, .004f, QUADRIL, QUADRIL, 1), An({0.03f * s, 1.05f * s, zf * s}, .024f, .004f, QUADRIL, QUADRIL, 1)};
        k.material = MAT_METAL;
        k.tubo(fv, 4, {0, 0, 1}, true, true, cor(metal));
        (void)f;
    }

    // ================= chapeu =================
    if (vis.chapeuAba) {
        float y0 = 1.765f * s;
        // aba com as laterais levantadas
        std::vector<Anel> aba = {Anel{{0, y0, -0.01f * s}, .195f * s, .205f * s, CABECA, CABECA, 1}, Anel{{0, y0 + 0.012f * s, -0.01f * s}, .195f * s, .205f * s, CABECA, CABECA, 1}};
        k.material = MAT_FELTRO;
        k.tubo(aba, 32, {0, 0, 1}, true, true, [&](int, float ang, Vec3f& p, Vec3f, Vec3f, float&) -> Rgb {
            if (ang >= 0) {
                float l = std::cos(ang);
                p.y += 0.028f * s * l * l;  // curva da aba
            }
            return vis.chapeu;
        });
        // copa amassada no meio (estilo faroeste)
        std::vector<Anel> copa = {Anel{{0, y0 + 0.01f * s, -0.01f * s}, .1f * s, .11f * s, CABECA, CABECA, 1},
                                  Anel{{0, y0 + 0.03f * s, -0.01f * s}, .101f * s, .111f * s, CABECA, CABECA, 1},
                                  Anel{{0, y0 + 0.075f * s, -0.01f * s}, .096f * s, .106f * s, CABECA, CABECA, 1},
                                  Anel{{0, y0 + 0.11f * s, -0.012f * s}, .085f * s, .094f * s, CABECA, CABECA, 1},
                                  Anel{{0, y0 + 0.12f * s, -0.012f * s}, .06f * s, .075f * s, CABECA, CABECA, 1}};
        k.material = MAT_FELTRO;
        k.tubo(copa, 20, {0, 0, 1}, false, true,
               [&](int i, float ang, Vec3f& p, Vec3f, Vec3f, float&) -> Rgb {
                   if (ang >= 0 && i >= 3) {  // vinco na frente e no meio
                       float f = std::sin(ang), l = std::cos(ang);
                       p.y -= 0.012f * s * g(l, 0, 0.35f) * (f > 0 ? 1.f : 0.6f);
                   }
                   if (i <= 1) { k.materialVert = MAT_COURO; return Rgb{0.12f, 0.08f, 0.05f}; }  // fita na base da copa
                   return vis.chapeu * 1.05f;
               },
               0.f, -0.01f * s);
    }

    // ================= equipamento do cacador =================
    if (vis.equipamento) {
        // bandoleira: do ombro esquerdo ao quadril direito
        std::vector<Anel> band;
        const int N = 40;
        for (int i = 0; i <= N; i++) {
            float th = 2 * PI_F * i / N;
            float y = 1.24f + 0.2f * std::cos(th);
            float rx, rz, z;
            perfil(y, rx, rz, z);
            float lx = y > 1.25f && y < 1.52f ? lo : (y < 1.0f ? lq : 1.f);
            Vec3f c{(rx * lx + 0.012f) * std::cos(th) * s * ms, y * s, (z + (rz + 0.012f) * std::sin(th)) * s * ms};
            int o = y > 1.25f ? PEITO : COLUNA;
            band.push_back(Anel{c, 0.006f * s, 0.026f * s, uint8_t(o), uint8_t(o), 1});
        }
        k.material = MAT_COURO;
        k.tubo(band, 4, {0, 1, 0}, false, false, cor(Rgb{0.32f, 0.2f, 0.12f}));
        // virotes presos na bandoleira (frente)
        for (int i = 0; i < 6; i++) {
            float th = 0.55f + i * 0.17f;
            float y = 1.24f + 0.2f * std::cos(th), rx, rz, z;
            perfil(y, rx, rz, z);
            Vec3f c{(rx * lo + 0.02f) * std::cos(th) * s, y * s, (z + rz + 0.02f) * std::sin(th) * s};
            std::vector<Anel> vr = {An(c + Vec3f{0, -0.04f * s, 0}, .006f, .006f, PEITO, PEITO, 1), An(c + Vec3f{0, 0.05f * s, 0}, .006f, .006f, PEITO, PEITO, 1),
                                    An(c + Vec3f{0, 0.07f * s, 0}, .009f, .004f, PEITO, PEITO, 1)};
            k.material = MAT_COURO;
            k.tubo(vr, 5, {0, 0, 1}, true, true, [&](int j, float, Vec3f&, Vec3f, Vec3f, float&) -> Rgb { if (j == 2) { k.materialVert = MAT_METAL; return Rgb{0.7f, 0.65f, 0.55f}; } return madeira; }, 0, 0.012f * s);
        }
        // besta nas costas (coronha na diagonal + arco atravessado)
        float zc = -0.165f;
        std::vector<Anel> coronha = {An({0.06f * s, 1.02f * s, zc * s}, .022f, .016f, COLUNA, COLUNA, 1), An({-0.01f * s, 1.22f * s, zc * s}, .02f, .015f, PEITO, COLUNA, .5f),
                                     An({-0.08f * s, 1.42f * s, (zc + 0.005f) * s}, .018f, .014f, PEITO, PEITO, 1), An({-0.11f * s, 1.5f * s, (zc + 0.005f) * s}, .016f, .012f, PEITO, PEITO, 1)};
        k.material = MAT_COURO;
        k.tubo(coronha, 8, {0, 0, 1}, true, true, cor(madeira), 0.01f * s, 0.01f * s);
        Vec3f topo{-0.1f * s, 1.47f * s, (zc - 0.01f) * s};
        std::vector<Anel> arco;
        for (int i = -4; i <= 4; i++) {
            float t = i / 4.f;
            Vec3f d = normalizar(Vec3f{0.72f, 0.69f, 0});  // perpendicular a coronha, no plano das costas
            Vec3f c = topo + Vec3f{d.x * t * 0.26f * s, -d.y * t * 0.26f * s * -1.f, -0.04f * s * (1 - t * t)};
            arco.push_back(An(c, .009f, .012f, PEITO, PEITO, 1));
        }
        k.material = MAT_COURO;
        k.tubo(arco, 6, {0, 0, 1}, true, true, cor(Rgb{0.3f, 0.22f, 0.15f}), 0.005f * s, 0.005f * s);
        // corda do arco
        std::vector<Anel> corda = {Anel{arco.front().c + Vec3f{0, 0, -0.006f * s}, .002f * s, .002f * s, PEITO, PEITO, 1},
                                   Anel{arco.back().c + Vec3f{0, 0, -0.006f * s}, .002f * s, .002f * s, PEITO, PEITO, 1}};
        k.material = MAT_TECIDO;
        k.tubo(corda, 4, {0, 0, 1}, false, false, cor(Rgb{0.75f, 0.72f, 0.62f}));
        // laco enrolado no quadril direito
        for (int volta = 0; volta < 2; volta++) {
            std::vector<Anel> laco;
            Vec3f cc{-0.205f * s * lq, 0.93f * s, 0.03f * s};
            float R = (0.085f - volta * 0.012f) * s;
            for (int i = 0; i <= 18; i++) {
                float th = 2 * PI_F * i / 18;
                laco.push_back(Anel{cc + Vec3f{-volta * 0.012f * s, std::cos(th) * R, std::sin(th) * R}, 0.011f * s, 0.011f * s, uint8_t(QUADRIL), uint8_t(QUADRIL), 1});
            }
            k.material = MAT_TECIDO;
            k.tubo(laco, 6, {1, 0, 0}, false, false, cor(Rgb{0.68f, 0.58f, 0.38f}));
        }
    }

    normaisSuaves(m);
    return m;
}
