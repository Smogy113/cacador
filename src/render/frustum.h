#pragma once

#include <array>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

class Frustum {
public:
    // Cobre tanto a camera principal quanto a projecao ortografica das sombras.
    static Frustum doPasse() {
        Matrix m = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
        Frustum f;
        f.planos_ = {{
            {m.m3 + m.m0, m.m7 + m.m4, m.m11 + m.m8,  m.m15 + m.m12},
            {m.m3 - m.m0, m.m7 - m.m4, m.m11 - m.m8,  m.m15 - m.m12},
            {m.m3 + m.m1, m.m7 + m.m5, m.m11 + m.m9,  m.m15 + m.m13},
            {m.m3 - m.m1, m.m7 - m.m5, m.m11 - m.m9,  m.m15 - m.m13},
            {m.m3 + m.m2, m.m7 + m.m6, m.m11 + m.m10, m.m15 + m.m14},
            {m.m3 - m.m2, m.m7 - m.m6, m.m11 - m.m10, m.m15 - m.m14}
        }};
        return f;
    }

    bool contemParte(const BoundingBox& caixa) const {
        for (const Vector4& p : planos_) {
            // O canto mais distante deste plano precisa ficar fora para descartar a malha inteira.
            float x = p.x >= 0 ? caixa.max.x : caixa.min.x;
            float y = p.y >= 0 ? caixa.max.y : caixa.min.y;
            float z = p.z >= 0 ? caixa.max.z : caixa.min.z;
            if (p.x * x + p.y * y + p.z * z + p.w < -0.01f) return false;
        }
        return true;
    }

private:
    std::array<Vector4, 6> planos_;
};
