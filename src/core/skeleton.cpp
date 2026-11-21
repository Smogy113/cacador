#include "core/skeleton.h"

Pose poseRepouso(const Esqueleto& e) {
    Pose p;
    p.rot.assign(e.ossos.size(), Vec3f{});
    return p;
}

void calcularMatrizes(const Esqueleto& e, const Pose& p, std::vector<Mat4>& saida) {
    size_t n = e.ossos.size();
    static thread_local std::vector<Mat4> mundo;
    mundo.resize(n);
    saida.resize(n);
    for (size_t i = 0; i < n; i++) {
        const Osso& o = e.ossos[i];
        Vec3f r = i < p.rot.size() ? p.rot[i] : Vec3f{};
        Mat4 local = Mat4::rotY(r.y) * Mat4::rotX(r.x) * Mat4::rotZ(r.z);
        if (o.pai < 0)
            mundo[i] = Mat4::trans(o.inicio + p.raiz) * local;
        else
            mundo[i] = mundo[o.pai] * Mat4::trans(o.inicio - e.ossos[o.pai].inicio) * local;
        saida[i] = mundo[i] * Mat4::trans(Vec3f{} - o.inicio);
    }
}
