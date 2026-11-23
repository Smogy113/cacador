#pragma once
#include <future>
#include "raylib.h"
#include "core/body.h"
#include "core/math.h"
#include "render/cabeca_raio.h"
#include "render/skinned.h"

enum class VistaArma { Guardada, NaMao, PrimeiraPessoa };

// Pessoa anatomica com esqueleto (malha unica deformada na placa de video).
class ModeloHumano {
public:
    void carregar(const Proporcoes& p, const Visual& v);
    // o heroi: tambem a malha sem a pele da cabeca (corpo mais denso) e a cabeca por raymarching
    void carregarHeroi(const Proporcoes& p, const Visual& v);
    void esperarCabeca() const { if (volumePendente_) volume_.wait(); }  // capturas de tela: deterministico
    void descarregar();
    // hd = modelo de alta definicao (perto da camera); matCabeca != null: cabeca por raymarching
    void desenhar(Material& mat, Vec3f pes, float yaw, const Pose& pose, bool hd = false, Material* matCabeca = nullptr,
                  VistaArma vista = VistaArma::Guardada) const;

    bool temCabelo() const { return malha_.temCabelo() || hd_.temCabelo() || hdSemPele_.temCabelo(); }
    void atualizarCabelo(float dt, Vec3f pos, float yaw, const Pose& pose, Vec3f vento, Vec3f camera);
    void reiniciarCabelo();
    void empunharArma(bool empunhada);

    const Esqueleto& esqueleto() const { return esq_; }
    int triangulos(bool hd = false, VistaArma vista = VistaArma::Guardada) const;

private:
    Esqueleto esq_;
    MalhaEsqueleto malha_, hd_, hdSemPele_;
    MalhaEsqueleto revolver_, bracoArmado_;
    std::vector<float> maoPosicoes_, maoNormais_;
    size_t maoInicio_ = 0;
    bool armaEmpunhada_ = false;
    mutable CabecaRaio cabeca_;
    mutable std::future<VolumeCabeca> volume_;  // gerado em segundo plano (nao atrasa o carregamento)
    mutable bool volumePendente_ = false;
    float escXZ_ = 1, escY_ = 1;
    int trisSemPele_ = 0;
    bool baseHumana_ = false;
    mutable std::vector<Mat4> ossos_;
};

// Cavalo anatomico com esqueleto.
class ModeloCavaloAnat {
public:
    void carregar(const VisualCavalo& v);
    void descarregar();
    void desenhar(Material& mat, Vec3f pos, float yaw, const Pose& pose, bool perto) const;
    // onde fica o quadril do cavaleiro, no mundo (acompanha o balanco do corpo)
    Vec3f assento(Vec3f pos, float yaw, const Pose& pose) const;
    const Esqueleto& esqueleto() const { return esq_; }
    int triangulos(bool perto) const { return (perto ? detalhada_ : leve_).triangulos(); }

private:
    Esqueleto esq_;
    MalhaEsqueleto detalhada_, leve_;
    mutable std::vector<Mat4> ossos_;
};

// Sombra simples (disco escuro) usada no nivel Baixo.
class SombraBolha {
public:
    void carregar();
    void descarregar();
    void desenhar(Vec3f pos, Vec3f normalChao, float raio) const;

private:
    Model disco_{};
};
