#include "core/bussola.h"
#include <cmath>

float anguloRelativo(float yaw, float rumo) {
    float a = yaw - rumo;
    const float PI2 = 6.2831853f;
    a = std::fmod(a + 3.14159265f, PI2);
    if (a < 0) a += PI2;
    return a - 3.14159265f;
}
float posicaoNaBussola(float yaw, Vec3f de, Vec3f para, float campo) {
    float rumo = std::atan2(para.x - de.x, para.z - de.z);
    return 0.5f + anguloRelativo(yaw, rumo) / campo;
}
float rumoDoNorte(float yaw) { return anguloRelativo(yaw, 3.14159265f); }
