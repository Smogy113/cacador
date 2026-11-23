#include "core/body.h"
#include <algorithm>
#include <cmath>
#include <functional>

#include "core/loft.h"
using namespace loft;


Esqueleto esqueletoHumano(const Proporcoes& p) {
    float s = p.altura / 1.8f, o = p.ombros * s, q = p.quadril * s;
    auto P = [&](float x, float y, float z) { return Vec3f{x, y * s, z * s}; };
    Esqueleto e;
    e.ossos.resize(N_OSSOS_HUMANO);
    e.ossos[QUADRIL] = {-1, P(0, 0.95f, 0), P(0, 1.05f, 0)};
    e.ossos[COLUNA] = {QUADRIL, P(0, 1.05f, 0), P(0, 1.25f, 0)};
    e.ossos[PEITO] = {COLUNA, P(0, 1.25f, 0), P(0, 1.47f, 0)};
    e.ossos[PESCOCO] = {PEITO, P(0, 1.50f, 0), P(0, 1.60f, 0)};
    e.ossos[CABECA] = {PESCOCO, P(0, 1.60f, 0), P(0, 1.80f, 0)};
    for (int lado = 0; lado < 2; lado++) {
        float sx = lado == 0 ? 1.f : -1.f;  // esquerda do personagem = +x
        int b = lado == 0 ? BRACO_E : BRACO_D, ab = lado == 0 ? ANTEBRACO_E : ANTEBRACO_D, m = lado == 0 ? MAO_E : MAO_D;
        e.ossos[b] = {PEITO, P(sx * 0.19f * o, 1.44f, 0), P(sx * 0.25f * o, 1.16f, 0)};
        e.ossos[ab] = {b, P(sx * 0.25f * o, 1.16f, 0), P(sx * 0.285f * o, 0.90f, 0.01f)};
        e.ossos[m] = {ab, P(sx * 0.285f * o, 0.90f, 0.01f), P(sx * 0.295f * o, 0.74f, 0.015f)};
        int c = lado == 0 ? COXA_E : COXA_D, ca = lado == 0 ? CANELA_E : CANELA_D, pe = lado == 0 ? PE_E : PE_D;
        e.ossos[c] = {QUADRIL, P(sx * 0.095f * q, 0.93f, 0), P(sx * 0.105f * q, 0.50f, 0.005f)};
        e.ossos[ca] = {c, P(sx * 0.105f * q, 0.50f, 0.005f), P(sx * 0.11f * q, 0.09f, -0.01f)};
        e.ossos[pe] = {ca, P(sx * 0.11f * q, 0.09f, -0.01f), P(sx * 0.115f * q, 0.02f, 0.16f)};
    }
    // o osso guarda x ja escalado pelos ombros/quadril; corrige o x de pescoco/cabeca/coluna (0)
    return e;
}

Esqueleto esqueletoHumanoBase(const Proporcoes& p) {
    Esqueleto e = esqueletoHumano(p);
    float s = p.altura / 1.8f;
    auto P = [&](float x, float y, float z) {
        float alto = smoothstep(1.16f, 1.45f, y);
        float pelve = 1.f - smoothstep(.91f, 1.22f, y);
        float largura = p.massa * (1.f + (p.ombros - 1.f) * alto + (p.quadril - 1.f) * pelve);
        return Vec3f{x * largura * s, y * s, z * p.massa * s};
    };
    e.poseA = true;
    for (int lado = 0; lado < 2; ++lado) {
        float sx = lado == 0 ? 1.f : -1.f;
        int b = lado == 0 ? BRACO_E : BRACO_D, ab = lado == 0 ? ANTEBRACO_E : ANTEBRACO_D;
        int m = lado == 0 ? MAO_E : MAO_D;
        e.ossos[b] = {PEITO, P(sx*.194f, 1.441f, .020f), P(sx*.366f, 1.244f, .020f)};
        e.ossos[ab] = {b, P(sx*.366f, 1.244f, .020f), P(sx*.535f, 1.075f, .050f)};
        e.ossos[m] = {ab, P(sx*.535f, 1.075f, .050f), P(sx*.660f, .950f, .050f)};
    }
    return e;
}

// Revolver na mao direita, seguindo o eixo da mao na pose de repouso.
Vec3f bocaDoRevolver(const Esqueleto& esq, float escala) {
    const Osso& mao = esq.ossos[MAO_D];
    Vec3f eixo = normalizar(mao.fim - mao.inicio);
    return mao.inicio + (mao.fim - mao.inicio) * 0.55f + Vec3f{0, 0, 0.028f * escala} + eixo * (0.16f * escala);
}

void loft::revolverNaMao(Construtor& k, const Esqueleto& esq, float s) {
    const Osso& om = esq.ossos[MAO_D];
    Vec3f A = normalizar(om.fim - om.inicio);          // ao longo da mao (cano)
    Vec3f F{0, 0, 1};                                  // frente da mao (fica para cima ao mirar)
    Vec3f H = om.inicio + (om.fim - om.inicio) * 0.55f;
    auto anel = [&](Vec3f c, float r) { return Anel{c, r * s, r * s, uint8_t(MAO_D), uint8_t(MAO_D), 1}; };
    auto cor = [](Rgb c) { return [c](int, float, Vec3f&, Vec3f, Vec3f, float&) -> Rgb { return c; }; };
    float mat = k.material;
    Rgb aco{0.16f, 0.16f, 0.17f}, madeira{0.3f, 0.18f, 0.1f};
    Vec3f base = H + F * (0.028f * s);
    k.material = MAT_METAL;
    k.tubo({anel(base - A * (0.01f * s), 0.008f), anel(bocaDoRevolver(esq, s), 0.007f)}, 8, F, true, true, cor(aco));  // cano
    k.tubo({anel(base, 0.017f), anel(base + A * (0.04f * s), 0.017f)}, 8, F, true, true, cor(aco * 1.2f));           // tambor
    k.material = MAT_COURO;
    k.tubo({anel(base - A * (0.015f * s), 0.011f), anel(H - F * (0.012f * s) - A * (0.04f * s), 0.012f)}, 6, A, true, true,
           cor(madeira));  // cabo
    k.material = mat;
}

MalhaPele corpoHumano(const Proporcoes& pr, const Visual& vis) {
    if (vis.corpoBase) return corpoHumanoBase(pr, vis);
    MalhaPele m;
    Construtor k{m};
    float s = pr.altura / 1.8f, ms = pr.massa;
    float lo = pr.ombros, lq = pr.quadril;
    Esqueleto esq = esqueletoHumano(pr);
    auto A = [&](float x, float y, float z, float rx, float rz, int a, int b, float peso) {
        return Anel{{x, y * s, z * s}, rx * s * ms, rz * s * ms, uint8_t(a), uint8_t(b), peso};
    };
    Rgb tronco = vis.sobretudo ? vis.casaco : vis.camisa;
    Rgb mangas = vis.sobretudo ? vis.casaco : vis.camisa;
    Rgb cinto{0.2f, 0.13f, 0.08f};

    // ---------------- tronco, pescoco e cabeca ----------------
    struct T { float y, rx, rz, z; int a, b; float p; };
    const T tr[] = {
        {0.84f, .130f, .100f, -.005f, QUADRIL, QUADRIL, 1},  {0.90f, .165f, .115f, -.010f, QUADRIL, QUADRIL, 1},
        {0.97f, .175f, .120f, -.010f, QUADRIL, QUADRIL, 1},  {1.05f, .158f, .108f, .000f, QUADRIL, COLUNA, .5f},
        {1.13f, .150f, .102f, .005f, COLUNA, COLUNA, 1},     {1.22f, .158f, .108f, .010f, COLUNA, PEITO, .5f},
        {1.31f, .172f, .118f, .015f, PEITO, PEITO, 1},       {1.39f, .185f, .122f, .010f, PEITO, PEITO, 1},
        {1.45f, .195f, .108f, .000f, PEITO, PEITO, 1},       {1.50f, .125f, .080f, .000f, PEITO, PESCOCO, .6f},
        {1.54f, .068f, .066f, .005f, PESCOCO, PESCOCO, 1},   {1.59f, .064f, .064f, .010f, PESCOCO, CABECA, .6f},
        {1.625f, .066f, .078f, .020f, CABECA, CABECA, 1},    {1.655f, .075f, .092f, .015f, CABECA, CABECA, 1},
        {1.69f, .083f, .100f, .010f, CABECA, CABECA, 1},     {1.72f, .088f, .103f, .000f, CABECA, CABECA, 1},
        {1.75f, .087f, .100f, -.005f, CABECA, CABECA, 1},    {1.775f, .075f, .088f, -.010f, CABECA, CABECA, 1},
        {1.795f, .045f, .055f, -.010f, CABECA, CABECA, 1},
    };
    std::vector<Anel> an;
    for (auto& t : tr) {
        float largura = t.y < 1.0f ? lq : (t.y > 1.25f && t.y < 1.52f ? lo : 1.f);
        Anel a = A(0, t.y, t.z, t.rx * largura, t.rz, t.a, t.b, t.p);
        an.push_back(a);
    }
    const int NT = 14;
    k.tubo(an, NT, {0, 0, 1}, true, true,
           [&](int i, float ang, Vec3f& p, Vec3f la, Vec3f fr, float& ao) -> Rgb {
               float y = tr[i].y;
               float f = ang < 0 ? 0.f : std::sin(ang);  // 1 = frente
               float l = ang < 0 ? 0.f : std::cos(ang);  // +1 = lado esquerdo
               (void)la;
               if (ang < 0) {  // tampas
                   if (y < 1.f) { ao = 0.7f; return vis.calca; }
                   return vis.chapeuAba ? vis.chapeu : vis.cabelo;
               }
               // rosto
               if (y >= 1.6f) {
                   if (y == 1.69f && f > 0.95f) p = p + fr * (0.026f * s);                      // nariz
                   if (y == 1.72f && f > 0.6f) p = p + fr * (0.008f * s);                       // sobrancelha
                   if (y == 1.625f && f > 0.9f) p = p + fr * (0.012f * s);                      // queixo
                   if ((y == 1.69f || y == 1.72f) && std::abs(l) > 0.95f) p = p + la * (l * 0.014f * s);  // orelhas
                   if (y == 1.625f) ao = 0.8f;
                   bool olho = y == 1.72f && f > 0.6f && f < 0.95f;
                   if (olho) return Rgb{0.12f, 0.09f, 0.07f};
                   if (vis.barba && y <= 1.655f && f > 0.15f) return vis.cabelo;
                   if (y >= 1.745f && f < 0.45f) return vis.cabelo;
                   if (y >= 1.655f && f < -0.35f) return vis.cabelo;  // nuca
                   if (y >= 1.775f) return vis.cabelo;
                   return vis.pele;
               }
               if (y >= 1.53f && y < 1.56f) return vis.lenco;
               if (y > 1.56f) return vis.pele;
               if (y >= 1.0f && y <= 1.06f) return cinto;
               if (y < 1.0f) {
                   if (y <= 0.9f) ao = 0.82f;
                   return vis.calca;
               }
               if (vis.sobretudo && f > 0.97f && y > 1.1f && y < 1.4f) return vis.casaco * 0.8f;  // fecho do casaco
               if (y >= 1.45f && f < -0.2f) ao = 0.92f;
               return tronco;
           },
           0.03f * s, 0.012f * s);

    // ---------------- bracos ----------------
    for (int lado = 0; lado < 2; lado++) {
        int b = lado == 0 ? BRACO_E : BRACO_D, ab = lado == 0 ? ANTEBRACO_E : ANTEBRACO_D, mo = lado == 0 ? MAO_E : MAO_D;
        const Osso &ob = esq.ossos[b], &oa = esq.ossos[ab], &om = esq.ossos[mo];
        auto L = [](Vec3f a, Vec3f c, float t) { return a + (c - a) * t; };
        auto Ar = [&](Vec3f c, float rx, float rz, int oa_, int ob_, float p) {
            return Anel{c, rx * s * ms, rz * s * ms, uint8_t(oa_), uint8_t(ob_), p};
        };
        std::vector<Anel> br = {
            Ar(L(ob.inicio, ob.fim, -0.12f), .068f, .070f, b, b, 1), Ar(L(ob.inicio, ob.fim, 0.3f), .058f, .060f, b, b, 1),
            Ar(L(ob.inicio, ob.fim, 0.72f), .050f, .051f, b, b, 1),  Ar(L(ob.inicio, ob.fim, 0.9f), .048f, .049f, b, ab, .78f),
            Ar(oa.inicio, .049f, .051f, b, ab, .5f), Ar(L(oa.inicio, oa.fim, 0.1f), .047f, .048f, ab, b, .78f),
            Ar(L(oa.inicio, oa.fim, 0.3f), .047f, .048f, ab, ab, 1), Ar(L(oa.inicio, oa.fim, 0.72f), .038f, .038f, ab, ab, 1),
            Ar(om.inicio, .028f, .03f, ab, mo, .5f),                 Ar(L(om.inicio, om.fim, 0.4f), .018f, .046f, mo, mo, 1),
            Ar(L(om.inicio, om.fim, 0.85f), .016f, .04f, mo, mo, 1),
        };
        k.tubo(br, 8, {0, 0, 1}, true, true,
               [&](int i, float ang, Vec3f&, Vec3f, Vec3f, float& ao) -> Rgb {
                   if (i == 0) ao = 0.78f;
                   if (i >= 9 || (ang < 0 && i > 5)) return vis.pele;
                   if (i == 8) return vis.pele * 0.95f;
                   return mangas;
               },
               0.02f * s, 0.022f * s);
    }

    // ---------------- pernas e pes ----------------
    for (int lado = 0; lado < 2; lado++) {
        int c = lado == 0 ? COXA_E : COXA_D, ca = lado == 0 ? CANELA_E : CANELA_D, pe = lado == 0 ? PE_E : PE_D;
        const Osso &oc = esq.ossos[c], &oca = esq.ossos[ca];
        float sx = lado == 0 ? 1.f : -1.f;
        auto Ar = [&](Vec3f cc, float rx, float rz, int a_, int b_, float p) {
            return Anel{cc, rx * s * ms * lq, rz * s * ms, uint8_t(a_), uint8_t(b_), p};
        };
        auto L = [](Vec3f a, Vec3f b2, float t) { return a + (b2 - a) * t; };
        std::vector<Anel> pr2 = {
            Ar(L(oc.inicio, oc.fim, -0.05f), .090f, .095f, c, QUADRIL, .6f), Ar(L(oc.inicio, oc.fim, 0.4f), .078f, .082f, c, c, 1),
            Ar(L(oc.inicio, oc.fim, 0.85f), .060f, .062f, c, ca, .85f), Ar(L(oc.inicio, oc.fim, 0.93f), .057f, .06f, c, ca, .75f),
            Ar(oca.inicio, .058f, .062f, c, ca, .5f), Ar(L(oca.inicio, oca.fim, 0.08f), .056f, .06f, ca, c, .75f),
            Ar(L(oca.inicio, oca.fim, 0.3f) + Vec3f{0, 0, -0.01f * s}, .056f, .062f, ca, ca, 1),
            Ar(L(oca.inicio, oca.fim, 0.7f), .044f, .046f, ca, ca, 1), Ar(oca.fim + Vec3f{0, 0.01f * s, 0}, .036f, .04f, ca, pe, .6f),
        };
        k.tubo(pr2, 8, {0, 0, 1}, true, false,
               [&](int i, float ang, Vec3f& p, Vec3f, Vec3f, float& ao) -> Rgb {
                   float lx = (p.x - pr2[i].c.x) * sx;  // < 0 = lado de dentro
                   if (i <= 1 && lx < 0) ao = 0.82f;
                   (void)ang;
                   return i >= 7 ? vis.botas : vis.calca;
               });
        // pe: tubo do calcanhar ate a ponta, "frente" do anel = para cima
        float px = oca.fim.x;
        std::vector<Anel> pes = {
            Anel{{px, 0.05f * s, -0.055f * s}, .035f * s * lq, .040f * s, uint8_t(pe), uint8_t(ca), .4f},
            Anel{{px, 0.05f * s, -0.02f * s}, .042f * s * lq, .050f * s, uint8_t(pe), uint8_t(pe), 1},
            Anel{{px + sx * 0.005f, 0.040f * s, 0.06f * s}, .046f * s * lq, .038f * s, uint8_t(pe), uint8_t(pe), 1},
            Anel{{px + sx * 0.008f, 0.030f * s, 0.15f * s}, .042f * s * lq, .028f * s, uint8_t(pe), uint8_t(pe), 1},
        };
        k.tubo(pes, 8, {0, 1, 0}, true, true, [&](int, float, Vec3f&, Vec3f, Vec3f, float&) -> Rgb { return vis.botas; }, 0.01f * s,
               0.02f * s);
    }

    // ---------------- abas do sobretudo ----------------
    if (vis.sobretudo) {
        const struct { float y, rx, rz, pc; } sb[] = {{1.02f, .185f, .128f, 0.f}, {0.86f, .195f, .142f, .45f}, {0.70f, .21f, .155f, .65f},
                                                    {0.54f, .222f, .165f, .8f}};
        // um tubo por metade (esquerda/direita) para cada lado seguir a sua coxa
        for (int lado = 0; lado < 2; lado++) {
            std::vector<Anel> an2;
            int coxa = lado == 0 ? COXA_E : COXA_D;
            for (auto& q : sb) an2.push_back(Anel{{0, q.y * s, -0.01f * s}, q.rx * s * ms * lq, q.rz * s * ms, uint8_t(coxa), uint8_t(QUADRIL), q.pc});
            size_t ini = m.v.size(), idxIni = m.idx.size();
            k.tubo(an2, 14, {0, 0, 1}, false, false, [&](int, float, Vec3f&, Vec3f, Vec3f, float&) -> Rgb { return vis.casaco * 0.95f; });
            // fica so com a metade do lado certo (x>0 esquerda); e abre na frente
            std::vector<uint16_t> novos;
            for (size_t t = idxIni; t < m.idx.size(); t += 3) {
                Vec3f c3 = (m.v[m.idx[t]].pos + m.v[m.idx[t + 1]].pos + m.v[m.idx[t + 2]].pos) * (1.f / 3.f);
                bool meu = lado == 0 ? c3.x >= -0.001f : c3.x <= 0.001f;
                bool frente = c3.z > 0.1f * s && std::abs(c3.x) < 0.06f * s;
                if (meu && !frente) novos.insert(novos.end(), {m.idx[t], m.idx[t + 1], m.idx[t + 2]});
            }
            m.idx.resize(idxIni);
            m.idx.insert(m.idx.end(), novos.begin(), novos.end());
            // no anel de cima o casaco acompanha o quadril
            for (size_t vi = ini; vi < m.v.size(); vi++)
                if (m.v[vi].pos.y > 1.0f * s) { m.v[vi].osso[0] = QUADRIL; m.v[vi].peso = 1; }
        }
    }

    // ---------------- chapeu de aba ----------------
    if (vis.chapeuAba) {
        float y0 = 1.765f * s;
        std::vector<Anel> aba = {Anel{{0, y0, -0.01f * s}, .19f * s, .20f * s, CABECA, CABECA, 1},
                                 Anel{{0, y0 + 0.014f * s, -0.01f * s}, .19f * s, .20f * s, CABECA, CABECA, 1}};
        k.tubo(aba, 14, {0, 0, 1}, true, true, [&](int, float, Vec3f&, Vec3f, Vec3f, float&) -> Rgb { return vis.chapeu; });
        std::vector<Anel> copa = {Anel{{0, y0 + 0.01f * s, -0.01f * s}, .098f * s, .108f * s, CABECA, CABECA, 1},
                                  Anel{{0, y0 + 0.07f * s, -0.01f * s}, .094f * s, .104f * s, CABECA, CABECA, 1},
                                  Anel{{0, y0 + 0.11f * s, -0.01f * s}, .08f * s, .09f * s, CABECA, CABECA, 1}};
        k.tubo(copa, 10, {0, 0, 1}, false, true,
               [&](int i, float, Vec3f&, Vec3f, Vec3f, float&) -> Rgb { return i == 0 ? Rgb{0.12f, 0.08f, 0.05f} : vis.chapeu * 1.06f; },
               0.f, 0.008f * s);
    }

    if (vis.cabeloLongo) cabeloComprido(m, s, ms, vis.cabelo, true);
    if (vis.revolver) revolverNaMao(k, esq, s);

    // ---------------- normais suaves ----------------
    std::vector<Vec3f> acc(m.v.size());
    for (size_t t = 0; t < m.idx.size(); t += 3) {
        Vec3f a = m.v[m.idx[t]].pos, b = m.v[m.idx[t + 1]].pos, c = m.v[m.idx[t + 2]].pos;
        Vec3f n = cross(b - a, c - a);
        for (int j = 0; j < 3; j++) acc[m.idx[t + j]] = acc[m.idx[t + j]] + n;
    }
    for (size_t i = 0; i < m.v.size(); i++) m.v[i].normal = normalizar(acc[i]);
    return m;
}
