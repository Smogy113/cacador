#pragma once
#include "core/math.h"

enum class Fase { Madrugada, Amanhecer, Dia, PorDoSol, Noite };

struct Paleta {
    Rgb ceuTopo, ceuHorizonte, neblina, sol, ambienteCeu, ambienteChao;
    float densidadeNeblina = 0, exposicao = 1;
    Rgb grading;
};

class DayClock {
public:
    static constexpr float DURACAO_DIA = 1200.f;
    explicit DayClock(float horaInicial = 8.f);
    void avancar(float dt);
    float hora() const;
    Fase fase() const;
    Vec3f direcaoSol() const;
    Vec3f direcaoLua() const;
    float intensidadeTochas() const;
    Paleta paleta() const;

private:
    float hora_;
};
