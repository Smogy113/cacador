#include "core/loft.h"
#include <map>

namespace loft {

static Vec3f catmull(Vec3f p0, Vec3f p1, Vec3f p2, Vec3f p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return (p1 * 2.f + (p2 - p0) * t + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 + (p3 - p0 + (p1 - p2) * 3.f) * t3) * 0.5f;
}

static float cat1(float a, float b, float c, float d, float t) { return catmull({a, 0, 0}, {b, 0, 0}, {c, 0, 0}, {d, 0, 0}, t).x; }

// mistura as influencias de dois aneis e fica com os dois ossos mais fortes
static void misturarOssos(const Anel& A, const Anel& B, float t, Anel& out) {
    std::map<int, float> inf;
    inf[A.a] += A.peso * (1 - t);
    inf[A.b] += (1 - A.peso) * (1 - t);
    inf[B.a] += B.peso * t;
    inf[B.b] += (1 - B.peso) * t;
    int o1 = A.a, o2 = A.a;
    float w1 = -1, w2 = -1;
    for (auto& [o, w] : inf) {
        if (w > w1) { o2 = o1; w2 = w1; o1 = o; w1 = w; }
        else if (w > w2) { o2 = o; w2 = w; }
    }
    if (w2 < 0) { o2 = o1; w2 = 0; }
    out.a = uint8_t(o1);
    out.b = uint8_t(o2);
    out.peso = (w1 + w2) > 0 ? w1 / (w1 + w2) : 1.f;
}

std::vector<Anel> suavizar(const std::vector<Anel>& an, int sub) {
    if (an.size() < 2 || sub <= 1) return an;
    std::vector<Anel> r;
    for (size_t i = 0; i + 1 < an.size(); i++) {
        const Anel& p0 = an[i ? i - 1 : 0];
        const Anel& p1 = an[i];
        const Anel& p2 = an[i + 1];
        const Anel& p3 = an[std::min(i + 2, an.size() - 1)];
        for (int k = 0; k < sub; k++) {
            float t = k / float(sub);
            Anel a;
            a.c = catmull(p0.c, p1.c, p2.c, p3.c, t);
            a.rx = std::max(0.002f, cat1(p0.rx, p1.rx, p2.rx, p3.rx, t));
            a.rz = std::max(0.002f, cat1(p0.rz, p1.rz, p2.rz, p3.rz, t));
            misturarOssos(p1, p2, t, a);
            r.push_back(a);
        }
    }
    r.push_back(an.back());
    return r;
}

void normaisSuaves(MalhaPele& m) {
    std::vector<Vec3f> acc(m.v.size());
    for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
        Vec3f a = m.v[m.idx[t]].pos, b = m.v[m.idx[t + 1]].pos, c = m.v[m.idx[t + 2]].pos;
        Vec3f n = cross(b - a, c - a);
        for (int j = 0; j < 3; j++) acc[m.idx[t + j]] = acc[m.idx[t + j]] + n;
    }
    for (size_t i = 0; i < m.v.size(); i++) m.v[i].normal = normalizar(acc[i]);
}

}  // namespace loft
