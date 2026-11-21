#pragma once
#include <vector>
#include "core/bando.h"
#include "core/bounty.h"
#include "core/places.h"

class Terreno;

// Andamento da recompensa ativa.
enum class FaseCacada { Nenhuma, Indo, Combate, Coletar, Levando, Entregue };
enum class Carga { Nenhuma, Seguindo, Garupa };  // como o procurado vai com o jogador

constexpr float DIST_CRIA_BANDO = 150.f, DIST_REMOVE_BANDO = 400.f, DIST_AMARRAR = 2.f, DIST_LEVAR = 8.f,
                DIST_ENTREGAR = 4.f;

// Ponto livre (sem colisao, em terra, dentro do mapa) perto do centro do lugar.
Vec3f escolherEsconderijo(const Lugar& l, uint32_t semente, const Terreno& t, const MundoColisao& col);

class Cacada {
public:
    // Aceitar troca a recompensa ativa (a anterior e o bando dela somem).
    void aceitar(const Cartaz& c, int quadro, int indiceCartaz, const Mapa& mapa, const Terreno& t,
                 const MundoColisao& col);
    void cancelar();
    EventosBando atualizar(float dt, Vec3f jogador, float velJogador, bool montado, bool tiroOuvido, const Terreno& t,
                           const MundoColisao& col);
    bool podeAmarrar(Vec3f jogador) const;
    void amarrar();
    bool podeLevar(Vec3f jogador) const;
    bool levar(Carga c);
    // Ao montar com o cavalo em 'posCavalo': o preso que segue ou o alvo (vivo amarrado ou morto) a <= 8 m vai na garupa.
    bool levarAoMontar(Vec3f posCavalo);  // o corpo so vai na garupa; retorna false se nao deu
    Carga carga() const { return carga_; }
    void soltarCarga();
    int entregar(Vec3f jogador, Vec3f xerife);  // valor pago (0 se nao pode)
    void morrerJogador();
    Vec3f objetivo(Vec3f jogador, const std::vector<Vec3f>& xerifes) const;
    FaseCacada fase() const { return fase_; }
    bool ativa() const { return fase_ != FaseCacada::Nenhuma && fase_ != FaseCacada::Entregue; }
    const Cartaz& cartaz() const { return cartaz_; }
    int quadro() const { return quadro_; }
    int indiceCartaz() const { return indice_; }
    Vec3f esconderijo() const { return esconderijo_; }
    Bando& bando() { return bando_; }
    const Bando& bando() const { return bando_; }
    bool vivo() const;

private:
    FaseCacada fase_ = FaseCacada::Nenhuma;
    Carga carga_ = Carga::Nenhuma;
    Cartaz cartaz_;
    int quadro_ = -1, indice_ = -1;
    Vec3f esconderijo_{};
    Bando bando_;
    bool lutou_ = false;
    uint32_t semente_ = 1;
};
