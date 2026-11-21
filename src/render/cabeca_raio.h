#pragma once
#include <vector>
#include "core/head_volume.h"
#include "raylib.h"
#include "render/skinned.h"

// Cabeca do heroi desenhada por raymarching: atlas do volume (distancias em meia precisao e cor)
// e a caixa presa aos ossos da cabeca/pescoco. O shader e o FS_LIT com RAYMARCH (Iluminacao).
class CabecaRaio {
public:
    void carregar(const VolumeCabeca& v, float escalaXZ, float escalaY);
    void descarregar();
    bool ok() const { return ok_; }
    // ossos = matrizes de pele da pose (as mesmas do corpo); modelo = matriz do personagem
    void desenhar(Material& mat, const std::vector<Mat4>& ossos, Matrix modelo) const;

private:
    Texture2D dist_{}, cor_{};
    MalhaEsqueleto caixa_;
    Vec3f lo_{}, hi_{}, n_{}, nc_{}, escala_{};
    float info_[4]{}, tam_[4]{};
    bool ok_ = false;
};
