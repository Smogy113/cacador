#pragma once
#include "core/body.h"
#include "core/hair_profile.h"
#include "core/mat4.h"

// Física das mechas: cada guia principal é uma corrente de dois nós (dobra e ponta) presa à raiz,
// com mola até a forma de repouso, inércia, gravidade relativa à cabeça, vento e colisão com o corpo
// (crânio, pescoço, ombros e orelhas). Os guias secundários têm só a ponta e herdam a dobra dos
// principais mais próximos. A placa de vídeo curva cada fita a partir desses deslocamentos.

// Deslocamentos no espaço de repouso da cabeça, em metros.
struct DeformacaoCabelo {
    std::array<Vec3f, MAX_GUIAS_CABELO> meio{}, ponta{};
};

// Deslocamento de um vértice de fita, no espaço de repouso da cabeça. É a mesma conta do vertex
// shader (render/shaders.cpp, deslocarMecha): mude as duas juntas. Os testes usam esta versão para
// conferir que o cabelo em movimento não entra no corpo.
//   repouso: posição do vértice na malha; guiaA, guiaB e pesoB: os guias da fita (pesoB 0..1);
//   progresso: 0 onde a fita se solta da cabeça, 1 na ponta.
Vec3f deslocamentoDaFita(const PerfilCabelo& perfil, const DeformacaoCabelo& deformacao, Vec3f repouso,
                         uint8_t guiaA, uint8_t guiaB, float pesoB, float progresso);

// O vértice já curvado não atravessa o plano da pele mais próxima (VertPele::apoioNormal), levado
// pelo osso dessa pele: a nuca acompanha o pescoço, os ombros o peito, o rosto e as orelhas a cabeça.
// Pode afundar até a folga de repouso, menos FOLGA_MINIMA_DO_APOIO.
inline constexpr float FOLGA_MINIMA_DO_APOIO = .0015f;
Vec3f apoiarNoCorpo(Vec3f posado, const VertPele& v, const Mat4* ossos);

// Por último, nenhum ponto entra nas orelhas, no pescoço nem no alto do tronco (ColisoresCabelo:
// volumes justos), nem fica mais fundo neles do que estava em repouso. `repouso` é o ponto na malha
// em repouso. Os volumes são inflados por FOLGA_DOS_VOLUMES (a mesma folga da montagem das fitas).
inline constexpr float FOLGA_DOS_VOLUMES = .003f;
Vec3f afastarDosVolumesJustos(Vec3f posado, Vec3f repouso, const ColisoresCabelo& colisores, const Mat4* ossos);

// Os volumes justos como a placa de vídeo os recebe (uniform cabeloVolumes, 8 vec4): orelha direita
// (centro, raios), pescoço (a, b, raios) e tronco (a, b, raios), raios já inflados pela folga.
// cabeloVolumes[0].w = 1 quando existem. Mude junto com afastarDosVolumesJustos no shader.
std::array<float, 32> volumesParaShader(const ColisoresCabelo& colisores);

// Posição final de um vértice de fita no espaço do modelo posado: preso à cabeça, curvado pela
// física e apoiado no corpo. É o vertex shader inteiro (VS_PELE) em C++; `ossos` são as mesmas
// matrizes do uniform `ossos` do shader.
Vec3f posicaoDaFita(const PerfilCabelo& perfil, const DeformacaoCabelo& deformacao, const VertPele& v, const Mat4* ossos);

class FisicaCabelo {
public:
    explicit FisicaCabelo(const PerfilCabelo& perfil);
    void reiniciar() { iniciada_ = detalhesAtivos_ = false; deformacao_ = {}; }
    void atualizar(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f vento, float detalhe = 1.f);
    bool iniciada() const { return iniciada_; }
    const PerfilCabelo& perfil() const { return perfil_; }
    const DeformacaoCabelo& deformacao() const { return deformacao_; }

private:
    struct No { Vec3f pos, vel; };
    struct EstadoGuia {
        std::array<No, 2> moveis{};
        std::array<Vec3f, 3> alvosAnteriores{};
        std::array<float, 2> comprimentos{}, rigidez{}, limites{};
    };
    struct EstadoSecundario {
        No ponta{};
        Vec3f repousoAnterior{};  // ponta em repouso no quadro anterior (velocidade da cabeça ali)
        float comprimento = 0, rigidez = 0, limite = 0;
        float escalaMeio = 1, escalaPonta = 1;
    };
    PerfilCabelo perfil_;
    std::array<EstadoGuia, MAX_GUIAS_PRINCIPAIS> guias_{};
    std::array<EstadoSecundario, MAX_GUIAS_CABELO> secundarios_{};
    std::array<Vec3f, 5> colisoresAnteriores_{};
    bool iniciada_ = false, detalhesAtivos_ = false;
    DeformacaoCabelo deformacao_{};

    void atualizarSecundarios(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f forca, float detalhe,
                              const std::array<Vec3f, 5>& colisores);
};
