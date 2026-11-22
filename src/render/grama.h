#pragma once
#include <map>
#include <utility>
#include "core/collision.h"
#include "core/math.h"
#include "core/predio.h"
#include "raylib.h"

class Terreno;
struct RegioesCapim;

// Gramado denso em volta do jogador: milhares de folhas finas em blocos de 8 m, montados quando
// o jogador chega perto e descartados quando ele se afasta. A ponta balanca com o vento no shader
// (tipo 4); a borda do campo some aos poucos (raioGrama). So nos niveis Medio e Alto.
class Grama {
public:
    static constexpr float BLOCO = 8.f;
    void iniciar(const Terreno& t, const MundoColisao& col, const std::vector<InteriorMundo>& interiores,
                 const RegioesCapim& regioes) {
        t_ = &t;
        col_ = &col;
        interiores_ = &interiores;
        regioes_ = &regioes;
    }
    // raio 0 = sem grama; densidade em folhas por m2
    void configurar(float raio, float densidade);
    void atualizar(Vec3f foco, int maxNovos);
    void desenhar(const Camera3D& cam, Material& mat);
    void descarregar();
    float raio() const { return raio_; }
    size_t triangulos() const { return tris_; }
    int chamadas() const { return chamadas_; }

private:
    Mesh montar(int i, int j) const;
    const Terreno* t_ = nullptr;
    const MundoColisao* col_ = nullptr;
    const std::vector<InteriorMundo>* interiores_ = nullptr;
    const RegioesCapim* regioes_ = nullptr;
    float raio_ = 0, densidade_ = 0;
    std::map<std::pair<int, int>, Mesh> blocos_;
    size_t tris_ = 0;
    int chamadas_ = 0;
};
