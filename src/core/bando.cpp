#include "core/bando.h"
#include <algorithm>
#include <cmath>
#include "core/noise.h"
#include "core/terrain.h"

float chanceAcerto(float dist, float vel) {
    float meiaLargura = std::atan(RAIO_CORPO / std::max(dist, 1.f));
    float desvio = 0.035f + 0.0005f * dist + 0.01f * vel;
    return std::min(0.75f, meiaLargura / desvio);
}

namespace {
float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vec3f posLivre(Vec3f p, const Terreno& t, const MundoColisao& col) {
    for (int k = 0; k < 4; k++) p = col.resolver(p, RAIO_PESSOA);
    p.y = t.altura(p.x, p.z);
    return p;
}
void encarar(Bandido& b, Vec3f alvo) { b.corpo.yaw = std::atan2(alvo.x - b.corpo.pos.x, alvo.z - b.corpo.pos.z); }
void mover(Bandido& b, Vec3f dir, float vel, float dt, const Terreno& t, const MundoColisao& col) {
    float n = std::hypot(dir.x, dir.z);
    if (n < 1e-4f) { b.corpo.vel = 0; return; }
    Vec3f d{dir.x / n, 0, dir.z / n};
    Vec3f p = b.corpo.pos + d * (vel * dt);
    if (t.mascaraRio(p.x, p.z) > 0.5f) { b.corpo.vel = 0; return; }  // nao entra no rio
    p = col.resolver(p, RAIO_PESSOA);
    p.y = t.altura(p.x, p.z);
    b.corpo.pos = p;
    b.corpo.vel = vel;
}
bool ve(const Bandido& b, Vec3f jog, const Terreno& t, const MundoColisao& col) {
    if (distXZ(b.corpo.pos, jog) > VISAO_BANDIDO) return false;
    return linhaLivre(b.corpo.pos + Vec3f{0, 1.6f, 0}, jog + Vec3f{0, 1.6f, 0}, t, col);
}
bool fora(EstadoBandido e) { return e == EstadoBandido::Morto || e == EstadoBandido::Sumiu; }
}  // namespace

Bando::Bando(Vec3f esc, Temperamento tp, int capangas, uint32_t semente, const Terreno& t, const MundoColisao& col) {
    uint32_t rng = hash32(semente * 977u + 3u);
    Bandido c;
    c.chefe = true;
    c.temperamento = tp;
    c.semente = hash32(rng);
    c.corpo.pos = posLivre(esc, t, col);
    b_.push_back(c);
    float giro = aleatorio01(rng) * 6.2831f;
    for (int k = 0; k < capangas; k++) {
        Bandido x;
        x.semente = hash32(rng + k);
        x.temperamento = Temperamento::Teimoso;
        for (int tent = 0; tent < 8; tent++) {
            float a = giro + (k + tent * 0.37f) * 6.2831f / capangas, r = 5.f + aleatorio01(rng) * 2.f;
            Vec3f p = posLivre(esc + Vec3f{std::sin(a) * r, 0, std::cos(a) * r}, t, col);
            x.corpo.pos = p;
            if (t.mascaraRio(p.x, p.z) < 0.3f) break;
        }
        x.corpo.yaw = aleatorio01(rng) * 6.2831f;
        b_.push_back(x);
    }
}

Bandido* Bando::chefe() {
    for (auto& x : b_)
        if (x.chefe) return &x;
    return nullptr;
}
const Bandido* Bando::chefe() const {
    for (auto& x : b_)
        if (x.chefe) return &x;
    return nullptr;
}
int Bando::atirando() const {
    int n = 0;
    for (auto& x : b_) n += x.rajada > 0 && x.estado == EstadoBandido::Combate;
    return n;
}
void Bando::alertarTodos() {
    for (auto& x : b_)
        if (x.estado == EstadoBandido::Vigiando) {
            x.estado = EstadoBandido::Alerta;
            x.reacao = REACAO_BANDIDO;
        }
}

bool Bando::levarTiro(int i, Parte parte) {
    if (i < 0 || i >= int(b_.size())) return false;
    Bandido& b = b_[i];
    if (fora(b.estado)) return false;
    bool indefeso = b.estado == EstadoBandido::Rendido || b.estado == EstadoBandido::Amarrado;
    b.vida.dano(indefeso || parte == Parte::Cabeca ? 1e9f : DANO_CORPO);
    b.tranco = 1;
    if (b.vida.morto()) {
        b.estado = EstadoBandido::Morto;
        b.tempoMorto = 0;
        b.rajada = 0;
        if (b.chefe) pendMorreu_ = true;
        return true;
    }
    if (b.estado == EstadoBandido::Vigiando || b.estado == EstadoBandido::Alerta) b.estado = EstadoBandido::Combate;
    alertarTodos();
    if (b.chefe && b.vida.fracao() < LIMIAR_RENDER && b.estado == EstadoBandido::Combate) {
        if (b.temperamento == Temperamento::Covarde) {
            b.estado = EstadoBandido::Rendido;
            b.rajada = 0;
            pendRendeu_ = true;
        } else if (b.temperamento == Temperamento::Fujao) {
            b.estado = EstadoBandido::Fugindo;
            b.rajada = 0;
        }
    }
    return false;
}

std::vector<Alvo> Bando::alvos() const {
    std::vector<Alvo> r;
    for (auto& x : b_) {
        Alvo a{x.corpo.pos, 1.8f};
        if (fora(x.estado)) a.pes.y = -1e6f;
        r.push_back(a);
    }
    return r;
}

void Bando::curarTodos() {
    for (auto& x : b_) {
        if (fora(x.estado) || x.estado == EstadoBandido::Rendido || x.estado == EstadoBandido::Amarrado) continue;
        x.vida.curar();
        x.estado = EstadoBandido::Vigiando;
        x.rajada = 0;
        x.temCobertura = false;
    }
}

EventosBando Bando::atualizar(float dt, Vec3f jog, float velJog, bool montado, bool tiroOuvido, const Terreno& t,
                              const MundoColisao& col) {
    EventosBando ev;
    ev.chefeRendeu = pendRendeu_;
    ev.chefeMorreu = pendMorreu_;
    pendRendeu_ = pendMorreu_ = false;
    (void)montado;  // a pe ou montado, chegar perto rende o fujao
    const Bandido* ch = chefe();
    bool chefeCaiu = ch && (ch->estado == EstadoBandido::Rendido || ch->estado == EstadoBandido::Amarrado ||
                            ch->estado == EstadoBandido::Morto);
    bool alguemLutando = false;
    for (auto& x : b_) alguemLutando |= x.estado == EstadoBandido::Combate || x.estado == EstadoBandido::Alerta;

    for (auto& b : b_) {
        b.tranco = std::max(0.f, b.tranco - dt * 4.f);
        b.arma.atualizar(dt);
        b.tempoEstado += dt;
        float d = distXZ(b.corpo.pos, jog);
        uint32_t& rng = b.semente;
        if (!b.chefe && chefeCaiu && !fora(b.estado) && b.estado != EstadoBandido::Fugindo) {
            b.estado = EstadoBandido::Fugindo;
            b.rajada = 0;
        }
        switch (b.estado) {
        case EstadoBandido::Morto: b.tempoMorto += dt; break;
        case EstadoBandido::Sumiu:
        case EstadoBandido::Amarrado: b.corpo.vel = 0; break;
        case EstadoBandido::Rendido:
            b.corpo.vel = 0;
            encarar(b, jog);
            break;
        case EstadoBandido::Vigiando:
            b.corpo.vel = 0;
            if (ve(b, jog, t, col) || (tiroOuvido && d <= AUDICAO_BANDIDO) || (alguemLutando && d <= AUDICAO_BANDIDO)) {
                b.estado = EstadoBandido::Alerta;
                b.reacao = REACAO_BANDIDO;
            }
            break;
        case EstadoBandido::Alerta:
            b.corpo.vel = 0;
            encarar(b, jog);
            b.reacao -= dt;
            if (b.reacao <= 0) b.estado = EstadoBandido::Combate;
            break;
        case EstadoBandido::Fugindo: {
            Vec3f longe = b.corpo.pos - jog;
            mover(b, longe, b.chefe ? VEL_FUJAO : VEL_CAPANGA_FUGA, dt, t, col);
            b.corpo.yaw = std::atan2(longe.x, longe.z);
            d = distXZ(b.corpo.pos, jog);
            if (b.chefe && d <= DIST_RENDE_FUJAO) {
                b.estado = EstadoBandido::Rendido;
                pendRendeu_ = true;
            }
            if (!b.chefe && d > DIST_SOME) b.estado = EstadoBandido::Sumiu;
            break;
        }
        case EstadoBandido::Combate: {
            // cobertura: atras da caixa mais proxima, do lado oposto ao jogador (revista a cada 2 s)
            b.tempoCobertura -= dt;
            bool linhaAgora = linhaLivre(b.corpo.pos + Vec3f{0, 1.5f, 0}, jog + Vec3f{0, 1.3f, 0}, t, col);
            b.semLinha = linhaAgora ? 0.f : b.semLinha + dt;
            if (b.semLinha > 2.f && b.temCobertura) {  // cobertura sem tiro: sai e contorna
                b.temCobertura = false;
                b.tempoCobertura = 6.f;
            }
            if (b.tempoCobertura <= 0) {
                b.tempoCobertura = 2.f;
                b.temCobertura = false;
                float melhor = 1e9f;
                for (const AABB& c : col.caixasPerto(b.corpo.pos, 15.f)) {
                    Vec3f centro{(c.min.x + c.max.x) * 0.5f, 0, (c.min.z + c.max.z) * 0.5f};
                    Vec3f fuga = centro - jog;
                    float n = std::hypot(fuga.x, fuga.z);
                    if (n < 1e-3f) continue;
                    float meia = std::max(c.max.x - c.min.x, c.max.z - c.min.z) * 0.5f + 0.8f;
                    Vec3f p = posLivre(centro + Vec3f{fuga.x / n * meia, 0, fuga.z / n * meia}, t, col);
                    float dd = distXZ(p, b.corpo.pos);
                    bool ocupada = false;  // cada um na sua cobertura
                    for (auto& o : b_)
                        if (&o != &b && !fora(o.estado) && ((o.temCobertura && distXZ(o.cobertura, p) < 2.f) || distXZ(o.corpo.pos, p) < 1.5f))
                            ocupada = true;
                    if (!ocupada && dd < melhor && distXZ(p, jog) > 6.f) { melhor = dd; b.cobertura = p; b.temCobertura = true; }
                }
            }
            if (b.temCobertura && distXZ(b.cobertura, b.corpo.pos) > 0.6f) {
                mover(b, b.cobertura - b.corpo.pos, VEL_COMBATE, dt, t, col);
            } else if (!b.temCobertura) {
                Vec3f para = jog - b.corpo.pos;
                if (b.semLinha > 1.f) {  // sem linha de visao: avanca contornando (pelo lado)
                    b.tempoLado -= dt;
                    if (b.tempoLado <= 0) { b.tempoLado = 3.f; b.lado = -b.lado; }
                    float n = std::max(1e-3f, std::hypot(para.x, para.z));
                    Vec3f f{para.x / n, 0, para.z / n}, l{f.z * b.lado, 0, -f.x * b.lado};
                    mover(b, f * 0.6f + l, VEL_COMBATE, dt, t, col);
                } else if (d > DIST_COMBATE_MAX) mover(b, para, VEL_COMBATE, dt, t, col);
                else if (d < DIST_COMBATE_MIN) mover(b, para * -1.f, VEL_COMBATE, dt, t, col);
                else {
                    b.tempoLado -= dt;
                    if (b.tempoLado <= 0) { b.tempoLado = 2.f + aleatorio01(rng) * 1.5f; b.lado = -b.lado; }
                    mover(b, Vec3f{para.z * b.lado, 0, -para.x * b.lado}, 1.5f, dt, t, col);
                }
            } else {
                b.corpo.vel = 0;
            }
            encarar(b, jog);
            // tiros em rajadas de 1 a 3, no maximo MAX_ATIRANDO ao mesmo tempo
            b.proxTiro -= dt;
            bool linha = linhaAgora;  // nem caixas nem morro no caminho
            if (!linha) b.rajada = 0;
            if (b.rajada == 0 && b.proxTiro <= 0 && linha && atirando() < MAX_ATIRANDO && d < 60.f)
                b.rajada = 1 + int(aleatorio01(rng) * 3) % 3;
            if (b.rajada > 0 && b.proxTiro <= 0) {
                if (b.arma.podeAtirar()) {
                    b.arma.atirar();
                    ev.claroes.push_back(b.corpo.pos + Vec3f{0, 1.4f, 0} + frenteDe(b.corpo.yaw) * 0.55f);
                    if (aleatorio01(rng) < chanceAcerto(d, velJog)) ev.tirosNoJogador++;
                    b.rajada--;
                    b.proxTiro = b.rajada > 0 ? 0.45f : 1.2f + aleatorio01(rng) * 1.3f;
                } else if (b.arma.balas == 0) {
                    b.arma.recarregar();
                    b.rajada = 0;
                    b.proxTiro = Revolver::TEMPO_RECARGA;
                }
            }
            break;
        }
        }
    }
    // ninguem dentro de ninguem: afasta pares muito juntos
    for (size_t i = 0; i < b_.size(); i++)
        for (size_t j = i + 1; j < b_.size(); j++) {
            Bandido &a = b_[i], &c = b_[j];
            if (fora(a.estado) || fora(c.estado) || a.estado == EstadoBandido::Amarrado || c.estado == EstadoBandido::Amarrado) continue;
            Vec3f d = c.corpo.pos - a.corpo.pos;
            float n = std::hypot(d.x, d.z);
            if (n >= 1.3f) continue;
            Vec3f u = n > 1e-3f ? Vec3f{d.x / n, 0, d.z / n} : Vec3f{1, 0, 0};
            float m = (1.3f - n) * 0.5f;
            a.corpo.pos = posLivre(a.corpo.pos - u * m, t, col);
            c.corpo.pos = posLivre(c.corpo.pos + u * m, t, col);
        }
    ev.chefeRendeu |= pendRendeu_;
    pendRendeu_ = false;
    return ev;
}
