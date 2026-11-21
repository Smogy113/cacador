#include "core/pescoco.h"

Mat4 inversaRigida(const Mat4& m) {
    Mat4 r = Mat4::id();
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r.a[i][j] = m.a[j][i];
    for (int i = 0; i < 3; i++) r.a[i][3] = -(r.a[i][0] * m.a[0][3] + r.a[i][1] * m.a[1][3] + r.a[i][2] * m.a[2][3]);
    return r;
}

float pesoCabeca(float y) { return y < 1.585f ? smoothstep(1.53f, 1.585f, y) : 1.f; }

namespace {
float derivPeso(float y) {  // d(pesoCabeca)/dy
    if (y <= 1.53f || y >= 1.585f) return 0.f;
    float t = (y - 1.53f) / 0.055f;
    return 6.f * t * (1.f - t) / 0.055f;
}
}  // namespace

Vec3f paraRepouso(Vec3f p, const Mat4& ic, const Mat4& ip, float escalaY) {
    // Newton: F(r) = w(r.y) C r + (1 - w) N r = p. O jacobiano e a mistura das rotacoes (A) mais o
    // termo do peso que muda com a altura (u v^T, u = C r - N r, v = (0, w', 0)); invertido por
    // Sherman-Morrison com A^-1 ~ mistura das rotacoes inversas. Parte das duas inversas e fica a melhor.
    Mat4 c = inversaRigida(ic), n = inversaRigida(ip);
    Vec3f melhor{};
    float menor = 1e9f;
    for (int ini = 0; ini < 2; ini++) {
        Vec3f r = ini ? ip.ponto(p) : ic.ponto(p);
        float erro = 0;
        for (int it = 0; it < 5; it++) {
            float y = r.y / escalaY, w = pesoCabeca(y), dw = derivPeso(y) / escalaY;
            Vec3f cr = c.ponto(r), nr = n.ponto(r);
            Vec3f e = p - (cr * w + nr * (1.f - w));
            erro = len(e);
            if (erro < 1e-7f) break;
            auto Ainv = [&](Vec3f q) { return ic.vetor(q) * w + ip.vetor(q) * (1.f - w); };
            Vec3f ae = Ainv(e), au = Ainv(cr - nr);
            float den = 1.f + dw * au.y;
            r = r + ae - au * (dw * ae.y / (std::abs(den) > 0.05f ? den : 0.05f));
        }
        if (erro < menor) menor = erro, melhor = r;
    }
    return melhor;
}
