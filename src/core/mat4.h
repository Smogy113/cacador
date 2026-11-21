#pragma once
#include <cmath>
#include "core/math.h"

// Matriz 4x4 linha-maior, vetores-coluna: p' = M * p. A*B = aplica B e depois A.
struct Mat4 {
    float a[4][4];
    static Mat4 id() {
        Mat4 r{};
        for (int i = 0; i < 4; i++) r.a[i][i] = 1;
        return r;
    }
    static Mat4 trans(Vec3f t) {
        Mat4 r = id();
        r.a[0][3] = t.x; r.a[1][3] = t.y; r.a[2][3] = t.z;
        return r;
    }
    static Mat4 rotX(float t) {
        Mat4 r = id();
        float c = std::cos(t), s = std::sin(t);
        r.a[1][1] = c; r.a[1][2] = -s; r.a[2][1] = s; r.a[2][2] = c;
        return r;
    }
    static Mat4 rotY(float t) {
        Mat4 r = id();
        float c = std::cos(t), s = std::sin(t);
        r.a[0][0] = c; r.a[0][2] = s; r.a[2][0] = -s; r.a[2][2] = c;
        return r;
    }
    static Mat4 rotZ(float t) {
        Mat4 r = id();
        float c = std::cos(t), s = std::sin(t);
        r.a[0][0] = c; r.a[0][1] = -s; r.a[1][0] = s; r.a[1][1] = c;
        return r;
    }
    Vec3f ponto(Vec3f p) const {
        return {a[0][0] * p.x + a[0][1] * p.y + a[0][2] * p.z + a[0][3], a[1][0] * p.x + a[1][1] * p.y + a[1][2] * p.z + a[1][3],
                a[2][0] * p.x + a[2][1] * p.y + a[2][2] * p.z + a[2][3]};
    }
    Vec3f vetor(Vec3f p) const {
        return {a[0][0] * p.x + a[0][1] * p.y + a[0][2] * p.z, a[1][0] * p.x + a[1][1] * p.y + a[1][2] * p.z,
                a[2][0] * p.x + a[2][1] * p.y + a[2][2] * p.z};
    }
};

inline Mat4 operator*(const Mat4& x, const Mat4& y) {
    Mat4 r{};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++) r.a[i][j] += x.a[i][k] * y.a[k][j];
    return r;
}
