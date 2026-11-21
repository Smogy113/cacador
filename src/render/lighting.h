#pragma once
#include <vector>
#include "raylib.h"
#include "core/daytime.h"
#include "core/quality.h"

struct LuzPontual {
    Vec3f pos;
    Rgb cor;
    float raio;
    float tremor;  // 0 = luz firme, 1 = fogo tremulando
};

class Iluminacao {
public:
    static constexpr int MAX_LUZES = 8;
    void carregar(Nivel nivel);
    void descarregar();
    // Prepara as luzes do frame: escolhe as 8 mais proximas de "foco".
    void comecarCena(const Camera3D& cam, const DayClock& relogio, const std::vector<LuzPontual>& todas, Vec3f foco, float tempo);
    void desenharCeu(const Camera3D& cam);
    void modoHorizonte(float descartarPerto);  // 0 = normal
    Shader shaderLit() const { return lit_; }
    Material& material() { return mat_; }
    const Efeitos& efeitos() const { return ef_; }
    Nivel nivel() const { return nivel_; }

private:
    void uniformesComuns(Shader s, const Paleta& p, Vec3f sol, float noite);
    Shader lit_{}, ceu_{};
    Material mat_{};
    Model esfera_{};
    Efeitos ef_{};
    Nivel nivel_ = Nivel::Baixo;
    Paleta paleta_{};
    Vec3f sol_{}, lua_{};
    float noite_ = 0;
    bool carregado_ = false;
};
