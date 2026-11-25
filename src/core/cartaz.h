#pragma once
// Geometria do cartaz do menu inicial: uma folha de papel pregada à esquerda da tela, levemente
// torta e balançando devagar. O desenho (game/menu_inicial.cpp) e o mouse usam as mesmas contas,
// então um clique acerta a opção mesmo com o papel girado.
//
// Dentro do cartaz tudo é medido em unidades da folha: 0..CARTAZ_LARGURA por 0..CARTAZ_ALTURA,
// origem no canto superior esquerdo. escalaDoCartaz converte essas unidades em pixels.

inline constexpr float CARTAZ_LARGURA = 640.f, CARTAZ_ALTURA = 900.f;

struct Vec2 {
    float x, y;
};

struct Cartaz {
    float cx, cy;            // centro na tela (pixels)
    float largura, altura;   // tamanho na tela (pixels)
    float angulo;            // graus, sentido horário na tela
};

// Cartaz para uma tela desse tamanho no instante `tempo` (segundos; o balanço vem daí).
Cartaz cartazNaTela(float larguraTela, float alturaTela, float tempo);
float escalaDoCartaz(const Cartaz& c);              // pixels por unidade da folha
Vec2 noCartaz(const Cartaz& c, float u, float v);   // folha -> tela
Vec2 paraCartaz(const Cartaz& c, float x, float y); // tela -> folha (para o mouse)
