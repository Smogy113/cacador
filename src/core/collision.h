#pragma once
#include <unordered_map>
#include <vector>
#include "core/math.h"
#include "core/medidas_mundo.h"

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
    static constexpr float LIMITE = medidas::MEIO_AREA_JOGAVEL;
    void adicionar(AABB c);
    void adicionarGirada(const CaixaGirada& c);
    // caixas que mudam a cada quadro (folhas de porta): substitui a lista inteira
    void definirDinamicas(std::vector<CaixaGirada> v) { dinamicas_ = std::move(v); }
    // Empurra a seção circular de um corpo para fora de caixas na sua faixa de altura.
    Vec3f resolver(Vec3f pos, float raio, float altura = medidas::ALTURA_ADULTO) const;
    // Segmento a->b nao atravessa nenhuma caixa?
    bool raioLivre(Vec3f a, Vec3f b) const;
    // Primeiro contato do segmento a->b com uma caixa (normal ou girada): 't' recebe a fracao 0..1 do
    // caminho e 'normal' a normal exata da face atingida, apontando para fora da caixa. false = livre.
    // O ricochete usa esta normal (a sondagem em volta erra o angulo das paredes giradas).
    bool primeiroContato(Vec3f a, Vec3f b, float& t, Vec3f& normal) const;
    size_t tamanho() const { return caixas_.size() + giradas_.size(); }
    // Caixas que encostam no quadrado de lado 2r em volta de p (plano xz).
    std::vector<AABB> caixasPerto(Vec3f p, float r) const;
    // Pisos andaveis (tabuleiro de ponte): o topo da caixa vira chao.
    void adicionarPiso(AABB p);
    // y1 é a altura no centro; decliveZ é a subida por metro em +z local.
    void adicionarPisoGirado(const CaixaGirada& p, float decliveZ = 0.f);
    // Topo do piso mais alto em (x,z) que esteja no maximo 0.7 m acima de yAtual; senao -1e9.
    float alturaPiso(float x, float z, float yAtual) const;

private:
    static constexpr float CEL = 32.f;
    long chave(int i, int j) const { return (long)i * 100003L + j; }
    std::vector<AABB> caixas_, pisos_;
    struct PisoGirado { CaixaGirada caixa; float decliveZ; };
    std::vector<CaixaGirada> giradas_, dinamicas_;
    std::vector<PisoGirado> pisosGirados_;
    std::unordered_map<long, std::vector<int>> grade_, gradeGirada_;
};
