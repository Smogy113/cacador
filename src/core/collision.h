#pragma once
#include <unordered_map>
#include <vector>
#include "core/math.h"

struct AABB {
    Vec3f min, max;
};
// Retangulo girado no plano xz (rotacao como rotY) com faixa de altura [y0, y1]: paredes de casas giradas.
struct CaixaGirada {
    Vec3f centro;  // y ignorado (a altura e y0..y1)
    float meiaX, meiaZ, rot, y0, y1;
};

// Caixas solidas do mundo (construcoes, troncos, cercas), com grade espacial para busca rapida.
class MundoColisao {
public:
    static constexpr float LIMITE = 700.f;
    void adicionar(AABB c);
    void adicionarGirada(const CaixaGirada& c);
    // caixas que mudam a cada quadro (folhas de porta): substitui a lista inteira
    void definirDinamicas(std::vector<CaixaGirada> v) { dinamicas_ = std::move(v); }
    // Empurra um circulo (raio, no plano xz) para fora das caixas e para dentro do mapa.
    Vec3f resolver(Vec3f pos, float raio) const;
    // Segmento a->b nao atravessa nenhuma caixa?
    bool raioLivre(Vec3f a, Vec3f b) const;
    size_t tamanho() const { return caixas_.size(); }
    // Caixas que encostam no quadrado de lado 2r em volta de p (plano xz).
    std::vector<AABB> caixasPerto(Vec3f p, float r) const;
    // Pisos andaveis (tabuleiro de ponte): o topo da caixa vira chao.
    void adicionarPiso(AABB p);
    void adicionarPisoGirado(const CaixaGirada& p);  // o topo (y1) vira chao
    // Topo do piso mais alto em (x,z) que esteja no maximo 0.7 m acima de yAtual; senao -1e9.
    float alturaPiso(float x, float z, float yAtual) const;

private:
    static constexpr float CEL = 32.f;
    long chave(int i, int j) const { return (long)i * 100003L + j; }
    std::vector<AABB> caixas_, pisos_;
    std::vector<CaixaGirada> giradas_, pisosGirados_, dinamicas_;
    std::unordered_map<long, std::vector<int>> grade_, gradeGirada_;
};
