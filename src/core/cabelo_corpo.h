#pragma once
#include <array>
#include "core/superficie.h"

// O que o cabelo contorna ao ser montado: a pele do personagem em repouso e as orelhas como
// volumes sólidos.
//
// A pele sozinha não basta na orelha: ela é uma aba fina, e uma mecha empurrada para a superfície
// mais próxima pode ficar com um ponto na frente da aba e o seguinte atrás dela, entre a orelha e o
// crânio. Com o volume, a mecha passa por fora da orelha inteira, como o cabelo de verdade.
// A física usa o mesmo volume (ColisoresCabelo::orelha) para as pontas não entrarem na orelha.

struct Elipsoide {
    Vec3f centro, raios;
    Elipsoide espelhadoEmX() const { return {{-centro.x, centro.y, centro.z}, raios}; }
    Elipsoide escalado(float s) const { return {centro * s, raios * s}; }
    // Distância aproximada (exata numa esfera): negativa dentro.
    float distancia(Vec3f p) const;
    // Leva p para a casca do elipsoide inflado por `folga`, na direção a partir do centro.
    Vec3f paraFora(Vec3f p, float folga) const;
    bool atravessa(Vec3f a, Vec3f b) const;
};

// Orelha direita (+x) da base humana no espaço de 1,80 m. Contém toda a aba da orelha da malha;
// tests/test_cabelo_corpo.cpp confere isso. A esquerda é o espelho em x.
inline constexpr Elipsoide ORELHA_DIREITA_BASE{{.077f, 1.679f, .044f}, {.018f, .045f, .029f}};

// Pescoço da base humana no espaço de 1,80 m: cápsula vertical de seção elíptica (6,2 x 4,5 cm),
// do alto dos ombros à base do crânio, justa à pele (sem o queixo) e inclinada para trás embaixo,
// onde a nuca vira trapézio. Presa ao osso PESCOCO.
// Com as orelhas, fecha os cantos côncavos (orelha + mandíbula, ombro + pescoço) que o plano de
// apoio de cada vértice sozinho não segura.
inline constexpr Vec3f PESCOCO_BASE_A{0, 1.49f, .002f}, PESCOCO_BASE_B{0, 1.63f, .024f};
inline constexpr Vec3f PESCOCO_BASE_RAIOS{.062f, .045f, .045f};

// Alto do tronco (ombros, omoplatas e peito) da base humana no espaço de 1,80 m: cápsula deitada em x,
// justa às costas, onde o cabelo longo assenta. Presa ao osso PEITO.
inline constexpr Vec3f TRONCO_BASE_A{-.06f, 1.372f, .040f}, TRONCO_BASE_B{.06f, 1.372f, .040f};
inline constexpr Vec3f TRONCO_BASE_RAIOS{.161f, .157f, .134f};

class CorpoDoCabelo {
public:
    // A pele vem dos triângulos de pele da malha acima de yMinimo. Tudo nas coordenadas finais da malha.
    CorpoDoCabelo(const MalhaPele& malha, float yMinimo, Elipsoide orelhaDireita);

    // Leva p para fora da pele e das orelhas, a pelo menos `folga` metros.
    Vec3f paraFora(Vec3f p, float folga) const;
    // Menor distância com sinal até a pele ou uma orelha (negativa dentro).
    float distancia(Vec3f p) const;
    // O segmento ab cruza a pele ou entra numa orelha?
    bool atravessa(Vec3f a, Vec3f b) const;

    const Superficie& pele() const { return pele_; }
    const Elipsoide& orelhaDireita() const { return orelhas_[0]; }

private:
    Superficie pele_;
    std::array<Elipsoide, 2> orelhas_;
};

// Corpo da base humana (corpoHumanoBase) já na escala do personagem (altura / 1,80 m).
CorpoDoCabelo corpoDaBaseHumana(const MalhaPele& malha, float escala);
