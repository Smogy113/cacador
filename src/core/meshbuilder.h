#pragma once
#include <cstddef>
#include <vector>
#include "core/math.h"

class Terreno;

// Tipo de material (vai para o shader): normal, agua, chama (sempre brilha), janela (brilha a noite)
constexpr float TIPO_NORMAL = 0.f, TIPO_AGUA = 1.f, TIPO_CHAMA = 2.f, TIPO_JANELA = 3.f;

struct Vert {
    Vec3f pos, normal;
    Rgb cor;
    float ao = 1.f;
    float tipo = TIPO_NORMAL;
};

// Monta geometria low-poly (faces chapadas, cores por vertice) e calcula oclusao de ambiente.
// Lista de triangulos sem indices: verts()[3k..3k+2] e um triangulo.
class MeshBuilder {
public:
    void caixa(Vec3f centro, Vec3f tamanho, float rotY, Rgb cor);
    void cilindro(Vec3f base, float raio, float altura, int lados, Rgb cor);
    void cone(Vec3f base, float raio, float altura, int lados, Rgb cor);
    void prisma(Vec3f centro, Vec3f tamanho, float rotY, Rgb cor);  // telhado de duas aguas, cumeeira no eixo x local
    void triangulo(Vec3f a, Vec3f b, Vec3f c, Rgb cor);
    void calcularAO(const Terreno* t, int amostras = 12);
    // AO barata: escurece de baixo (y0) para cima (y1). Usada em arvores.
    void aoVertical(float y0, float y1);
    void tipo(float t) { tipoAtual_ = t; }
    void anexar(const MeshBuilder& outro);
    void limpar() { v_.clear(); dono_.clear(); oclusores_.clear(); donoAtual_ = -1; }
    const std::vector<Vert>& verts() const { return v_; }
    std::size_t triangulos() const { return v_.size() / 3; }
    bool vazio() const { return v_.empty(); }

private:
    struct Caixa {
        Vec3f min, max;
    };
    void quadOrientado(Vec3f a, Vec3f b, Vec3f c, Vec3f d, Vec3f dir, Rgb cor, float tamMax);
    void triOrientado(Vec3f a, Vec3f b, Vec3f c, Vec3f dir, Rgb cor);
    void addOclusor(Vec3f mn, Vec3f mx);
    std::vector<Vert> v_;
    int donoAtual_ = -1;
    float tipoAtual_ = TIPO_NORMAL;
    std::vector<int> dono_;  // oclusor que gerou cada vertice (nao se auto-oclui)
    std::vector<Caixa> oclusores_;
};
