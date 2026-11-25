#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "core/body.h"

// Conjunto de triângulos de uma MalhaPele com consultas rápidas de proximidade (grade uniforme).
//
// Uso principal: impedir que o cabelo atravesse o corpo. O penteado é montado com a pele do
// personagem em repouso (cabeça, orelhas, pescoço e ombros) e cada ponto das mechas é levado para
// fora dela com uma folga; os testes usam a mesma classe para conferir que nenhuma fita cruza a pele.
// As coordenadas são as da malha (metros, espaço de repouso do modelo, já na escala do personagem).
class Superficie {
public:
    // Escolhe os triângulos pelos três vértices. A grade é montada uma vez; as consultas são const.
    using Filtro = std::function<bool(const VertPele&, const VertPele&, const VertPele&)>;
    Superficie(const MalhaPele& malha, const Filtro& incluir, float celula = .015f);

    struct Contato {
        Vec3f ponto;      // ponto mais próximo na superfície
        Vec3f normal;     // normal suave (interpolada dos vértices) nesse ponto, para fora
        float distancia;  // com sinal: negativa quando a consulta está do lado de dentro
        uint8_t osso = 0; // osso que mais move o triângulo tocado (a pele acompanha esse osso)
    };
    // Ponto mais próximo de p. Sem triângulo a menos de `alcance`, devolve distancia = alcance
    // (o ponto é tratado como fora).
    Contato maisProximo(Vec3f p, float alcance = .08f) const;
    // Leva p para fora da superfície, a pelo menos `folga` metros. Pontos já fora ficam onde estão.
    Vec3f paraFora(Vec3f p, float folga) const;
    // O segmento ab cruza algum triângulo?
    bool atravessa(Vec3f a, Vec3f b) const;
    size_t triangulos() const { return tris_.size(); }

private:
    struct Tri {
        Vec3f a, b, c;
        Vec3f na, nb, nc;
        uint8_t osso;
    };
    std::vector<Tri> tris_;
    // Grade em formato compacto: os triângulos da célula k estão em lista_[inicio_[k] .. inicio_[k+1]).
    Vec3f origem_{};
    float celula_;
    int n_[3] = {0, 0, 0};
    std::vector<uint32_t> inicio_, lista_;

    int indice(int x, int y, int z) const { return (z * n_[1] + y) * n_[0] + x; }
    void celulaDe(Vec3f p, int c[3]) const;
};

// Pele do corpo que o cabelo precisa respeitar: triângulos de pele acima de `yMinimo`
// (orelhas, rosto, pescoço, ombros e alto das costas, sem olhos nem o próprio cabelo).
Superficie peleParaCabelo(const MalhaPele& malha, float yMinimo);
