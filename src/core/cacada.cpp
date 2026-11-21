#include "core/cacada.h"
#include <cmath>
#include "core/noise.h"
#include "core/terrain.h"

namespace {
float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

Vec3f escolherEsconderijo(const Lugar& l, uint32_t semente, const Terreno& t, const MundoColisao& col) {
    uint32_t rng = hash32(semente + 101u);
    float giro = aleatorio01(rng) * 6.2831f;
    auto bom = [&](Vec3f p) {
        return t.dentroDoJogavel(p.x, p.z) && t.mascaraRio(p.x, p.z) < 0.2f && t.normal(p.x, p.z).y > 0.85f &&
               distXZ(col.resolver(p, 1.5f), p) < 0.01f;
    };
    for (float r = 0; r <= l.raio + 120.f; r += 3.f) {  // se o lugar estiver todo ocupado, procura em volta
        int n = r < 1 ? 1 : int(6.2831f * r / 3.f);
        for (int k = 0; k < n; k++) {
            float a = giro + k * 6.2831f / n;
            Vec3f p{l.x + std::sin(a) * r, 0, l.z + std::cos(a) * r};
            p.y = t.altura(p.x, p.z);
            if (bom(p)) return p;
        }
    }
    Vec3f p = col.resolver({l.x, 0, l.z}, 1.5f);
    p.y = t.altura(p.x, p.z);
    return p;
}

void Cacada::aceitar(const Cartaz& c, int quadro, int indice, const Mapa& mapa, const Terreno& t,
                     const MundoColisao& col) {
    if (ativa() && quadro == quadro_ && indice == indice_) return;  // ja e a recompensa ativa
    cartaz_ = c;
    quadro_ = quadro;
    indice_ = indice;
    semente_ = hash32(c.rosto + 7u + uint32_t(indice) * 13u);
    const Lugar* lugar = nullptr;
    for (auto& l : mapa.lugares)
        if (nomeLegivel(l.nome) == c.ultimoVisto) lugar = &l;
    Lugar qualquer{TipoLugar::Floresta, "", 0, 0, 0, 30};
    esconderijo_ = escolherEsconderijo(lugar ? *lugar : qualquer, semente_, t, col);
    bando_ = Bando();
    lutou_ = false;
    carga_ = Carga::Nenhuma;
    fase_ = FaseCacada::Indo;
}

void Cacada::cancelar() {
    fase_ = FaseCacada::Nenhuma;
    bando_ = Bando();
    carga_ = Carga::Nenhuma;
}

bool Cacada::vivo() const {
    const Bandido* c = bando_.chefe();
    return !c || c->estado != EstadoBandido::Morto;
}

EventosBando Cacada::atualizar(float dt, Vec3f jog, float vel, bool montado, bool tiroOuvido, const Terreno& t,
                               const MundoColisao& col) {
    if (!ativa()) return {};
    float d = distXZ(jog, esconderijo_);
    if (bando_.vazio() && fase_ == FaseCacada::Indo && d <= DIST_CRIA_BANDO)
        bando_ = Bando(esconderijo_, cartaz_.temperamento, cartaz_.recompensaVivo >= 100 ? 3 : 2, semente_, t, col);
    if (!bando_.vazio() && !lutou_ && d > DIST_REMOVE_BANDO) bando_ = Bando();
    if (bando_.vazio()) return {};
    EventosBando ev = bando_.atualizar(dt, jog, vel, montado, tiroOuvido, t, col);
    for (auto& b : bando_.bandidos()) lutou_ |= b.estado != EstadoBandido::Vigiando;
    if (lutou_ && fase_ == FaseCacada::Indo) fase_ = FaseCacada::Combate;
    Bandido* c = bando_.chefe();
    if (c && c->estado == EstadoBandido::Morto && fase_ == FaseCacada::Combate) fase_ = FaseCacada::Coletar;
    // o preso acompanha o jogador
    if (c && carga_ == Carga::Garupa) {
        c->corpo.pos = jog;
    } else if (c && carga_ == Carga::Seguindo) {
        Vec3f para = jog - c->corpo.pos;
        float dd = std::hypot(para.x, para.z);
        if (dd > 1.5f) {
            float passo = std::min(dd - 1.5f, 4.f * dt);
            Vec3f p = col.resolver(c->corpo.pos + Vec3f{para.x / dd, 0, para.z / dd} * passo, RAIO_PESSOA);
            p.y = t.altura(p.x, p.z);
            c->corpo.pos = p;
            c->corpo.vel = passo / std::max(dt, 1e-4f);
            c->corpo.yaw = std::atan2(para.x, para.z);
        } else {
            c->corpo.vel = 0;
        }
    }
    return ev;
}

bool Cacada::podeAmarrar(Vec3f jog) const {
    const Bandido* c = bando_.chefe();
    return ativa() && c && c->estado == EstadoBandido::Rendido && distXZ(c->corpo.pos, jog) <= DIST_AMARRAR;
}
void Cacada::amarrar() {
    Bandido* c = bando_.chefe();
    if (!c || c->estado != EstadoBandido::Rendido) return;
    c->estado = EstadoBandido::Amarrado;
    fase_ = FaseCacada::Coletar;
}
bool Cacada::podeLevar(Vec3f jog) const {
    const Bandido* c = bando_.chefe();
    return ativa() && carga_ == Carga::Nenhuma && c &&
           (c->estado == EstadoBandido::Amarrado || c->estado == EstadoBandido::Morto) &&
           distXZ(c->corpo.pos, jog) <= DIST_LEVAR;
}
bool Cacada::levar(Carga k) {
    const Bandido* c = bando_.chefe();
    if (!c || k == Carga::Nenhuma) return false;
    if (c->estado == EstadoBandido::Morto && k != Carga::Garupa) return false;
    if (c->estado != EstadoBandido::Morto && c->estado != EstadoBandido::Amarrado) return false;
    carga_ = k;
    fase_ = FaseCacada::Levando;
    return true;
}
bool Cacada::levarAoMontar(Vec3f posCavalo) {
    const Bandido* c = bando_.chefe();
    if (!ativa() || !c || (c->estado != EstadoBandido::Amarrado && c->estado != EstadoBandido::Morto)) return false;
    if (carga_ != Carga::Seguindo && (carga_ != Carga::Nenhuma || distXZ(c->corpo.pos, posCavalo) > DIST_LEVAR)) return false;
    carga_ = Carga::Garupa;
    fase_ = FaseCacada::Levando;
    return true;
}

void Cacada::soltarCarga() {
    if (carga_ == Carga::Nenhuma) return;
    carga_ = Carga::Nenhuma;
    fase_ = FaseCacada::Coletar;
}
int Cacada::entregar(Vec3f jog, Vec3f xerife) {
    if (fase_ != FaseCacada::Levando || carga_ == Carga::Nenhuma || distXZ(jog, xerife) > DIST_ENTREGAR) return 0;
    int valor = vivo() ? cartaz_.recompensaVivo : cartaz_.recompensaMorto;
    if (Bandido* c = bando_.chefe()) c->estado = EstadoBandido::Sumiu;
    carga_ = Carga::Nenhuma;
    fase_ = FaseCacada::Entregue;
    return valor;
}
void Cacada::morrerJogador() {
    if (!ativa()) return;
    carga_ = Carga::Nenhuma;
    bando_.curarTodos();
    const Bandido* c = bando_.chefe();
    if (c && (c->estado == EstadoBandido::Amarrado || c->estado == EstadoBandido::Morto)) fase_ = FaseCacada::Coletar;
    else if (fase_ != FaseCacada::Indo) fase_ = FaseCacada::Combate;
}
Vec3f Cacada::objetivo(Vec3f jog, const std::vector<Vec3f>& xerifes) const {
    const Bandido* c = bando_.chefe();
    if (fase_ == FaseCacada::Levando) {
        Vec3f melhor = esconderijo_;
        float md = 1e18f;
        for (auto& x : xerifes)
            if (distXZ(x, jog) < md) { md = distXZ(x, jog); melhor = x; }
        return melhor;
    }
    if (c && (fase_ == FaseCacada::Combate || fase_ == FaseCacada::Coletar)) return c->corpo.pos;
    return esconderijo_;
}
