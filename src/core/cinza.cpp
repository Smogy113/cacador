#include "core/cinza.h"
#include <algorithm>
#include <cmath>
#include "core/noise.h"

namespace {
float distancia(Vec3f a, Vec3f b) { return len(a - b); }
}  // namespace

float NuvemFumaca::alfaVida() const {
    float entrada = smoothstep(0.f, 0.4f, idade);
    float saida = 1.f - smoothstep(vida * 0.67f, vida, idade);
    return entrada * saida;
}

bool ZonaUmida::contem(Vec3f p) const {
    float dx = p.x - centro.x, dz = p.z - centro.z;
    return dx * dx + dz * dz <= raio * raio && p.y >= centro.y - 0.5f && p.y <= centro.y + altura;
}

float danoNaDistancia(const GolpeDetonacao& g, float dist) {
    if (g.raio <= 0.f || dist >= g.raio) return 0.f;
    return g.dano * (1.f - dist / g.raio);
}

float empurraoNaDistancia(const GolpeDetonacao& g, float dist) {
    if (g.raio <= 0.f || dist >= g.raio) return 0.f;
    return g.empurrao * (1.f - dist / g.raio);
}

const char* nomeEstado(EstadoConjuracao e) {
    switch (e) {
    case EstadoConjuracao::Pronta: return "Pronta";
    case EstadoConjuracao::Canalizando: return "Canalizando";
    case EstadoConjuracao::Bloqueada: return "Bloqueada pela umidade";
    case EstadoConjuracao::SemSangue: return "Sangue insuficiente";
    case EstadoConjuracao::Indisponivel: return "Indisponivel";
    }
    return "?";
}

// ---------------------------------------------------------------- Custo de Sangue

void CustoSangue::pagar(Vida& v, float vida, float desgasteExtra, float minima) {
    if (v.morto()) return;
    desgaste = std::clamp(desgaste + std::max(0.f, desgasteExtra), 0.f, std::max(0.f, vidaBase - minima));
    v.max = vidaMaxima();
    // Conjurar sangra, mas nunca derruba: a vida para em 1 (ou onde já estava, se estava abaixo).
    float piso = std::min(v.atual, 1.f);
    v.atual = std::min(v.max, std::max(piso, v.atual - std::max(0.f, vida)));
    if (vida > 0.f) v.semDano = 0;  // sangrando, a regeneração natural espera como depois de um tiro
}

void CustoSangue::recuperar(Vida& v, float pontos) {
    desgaste = std::max(0.f, desgaste - std::max(0.f, pontos));
    v.max = vidaMaxima();
}

// ---------------------------------------------------------------- fumaça

void CorrenteCinza::emitir(Vec3f pos, float raio, Vec3f vel) {
    NuvemFumaca n;
    n.pos = pos;
    n.vel = vel;
    n.raio = n.raioInicial = raio;
    n.vida = p.vidaNuvem;
    n.semente = hash32(rng_++);
    nuvens_.push_back(n);
    limitarNuvens();
}

void CorrenteCinza::emitirDisparo(Vec3f boca, Vec3f dir) {
    Vec3f d = normalizar(dir);
    emitir(boca + d * p.distNuvemDisparo, p.raioNuvemDisparo, d * 0.4f + Vec3f{0, p.subidaNuvemDisparo, 0});
}

int CorrenteCinza::adicionarFonte(Vec3f pos) {
    fontes_.push_back({pos, p.intervaloFogueira, true});  // a primeira nuvem sai ja no proximo quadro
    return int(fontes_.size()) - 1;
}

bool CorrenteCinza::umido(Vec3f q) const {
    for (const ZonaUmida& z : zonas_)
        if (z.contem(q)) return true;
    return testeAgua_ && testeAgua_(q);
}

int CorrenteCinza::capturadas() const {
    int n = 0;
    for (const NuvemFumaca& x : nuvens_) n += x.capturada;
    return n;
}

void CorrenteCinza::limitarNuvens() {
    while (int(nuvens_.size()) > std::max(1, p.maxNuvens)) {
        // sai a nuvem livre mais velha; se todas estiverem capturadas, a primeira
        size_t sai = 0;
        float maisVelha = -1.f;
        for (size_t i = 0; i < nuvens_.size(); i++)
            if (!nuvens_[i].capturada && nuvens_[i].idade > maisVelha) {
                maisVelha = nuvens_[i].idade;
                sai = i;
            }
        nuvens_.erase(nuvens_.begin() + long(sai));
    }
}

// ---------------------------------------------------------------- faísca, escudo e cadeia

void CorrenteCinza::soltarCaptura() {
    for (NuvemFumaca& n : nuvens_) n.capturada = false;
}

void CorrenteCinza::detonar(size_t i, bool primeira, EventosCinza& ev) {
    const Vec3f centro = nuvens_[i].pos;
    ev.detonacoes.push_back({centro, p.raioDetonacao, p.danoDetonacao, p.empurraoDetonacao, primeira});
    explosoes_.push_back({centro, p.raioDetonacao, 0.f, p.duracaoExplosao});
    nuvens_.erase(nuvens_.begin() + long(i));
    // A chama salta para as nuvens livres por perto; cada elo espera atrasoPropagacao.
    for (NuvemFumaca& o : nuvens_)
        if (!o.capturada && o.pavio < 0.f && distancia(o.pos, centro) <= p.raioPropagacao) o.pavio = p.atrasoPropagacao;
}

void CorrenteCinza::soltarFaisca(const EntradaCinza& e, Vida& vida, EventosCinza& ev) {
    const int total = capturadas();
    if (total == 0) {
        ev.faiscaVazia = true;
        return;
    }
    ev.faiscaSolta = true;
    sangue_.pagar(vida, p.custoVidaFaisca + p.custoVidaPorNuvem * float(total),
                  p.desgasteFaisca + p.desgastePorNuvem * float(total), p.vidaMaximaMinima);

    // 1) Junto ao corpo: a fumaça condensa no escudo.
    int noEscudo = 0;
    for (size_t i = 0; i < nuvens_.size();) {
        if (nuvens_[i].capturada && distancia(nuvens_[i].pos, e.peito) < p.distEscudo) {
            nuvens_.erase(nuvens_.begin() + long(i));
            noEscudo++;
        } else {
            i++;
        }
    }
    if (noEscudo > 0) {
        float cheio = p.nuvensEscudoCheio > 1 ? clamp01(float(noEscudo - 1) / float(p.nuvensEscudoCheio - 1)) : 1.f;
        float absorcao = lerp(p.absorcaoEscudoMin, p.absorcaoEscudoMax, cheio);
        if (escudo_.ativo()) absorcao = std::max(absorcao, escudo_.absorcao);  // reforçar nunca enfraquece
        Vec3f f = normalizar(Vec3f{e.frente.x, 0, e.frente.z});
        escudo_.centro = e.pes + f * p.afastamentoEscudo + Vec3f{0, p.alturaEscudo * 0.5f, 0};
        escudo_.yaw = std::atan2(f.x, f.z);
        escudo_.restante = escudo_.duracao = p.duracaoEscudo;
        escudo_.absorcao = absorcao;
        escudo_.nuvens = noEscudo;
        ev.escudoErguido = true;
    }
    // 2) Longe do corpo: a faísca acende o que sobrou; as vizinhas pegam fogo em cadeia depois.
    for (size_t i = 0; i < nuvens_.size();) {
        if (nuvens_[i].capturada) detonar(i, true, ev);  // detonar() remove a nuvem i
        else i++;
    }
}

float CorrenteCinza::fatorDano(Vec3f origem, Vec3f alvo) const {
    if (!escudo_.ativo()) return 1.f;
    const Vec3f n{std::sin(escudo_.yaw), 0, std::cos(escudo_.yaw)};
    const Vec3f c = escudo_.centro;
    float s0 = dot(origem - c, n), s1 = dot(alvo - c, n);
    if (s0 <= 0.f || s1 >= 0.f) return 1.f;  // só barra o que vem pela frente e passa pelo plano
    float t = s0 / (s0 - s1);
    Vec3f q = origem + (alvo - origem) * t;
    Vec3f lado{n.z, 0, -n.x};
    if (std::abs(dot(q - c, lado)) > p.larguraEscudo * 0.5f || std::abs(q.y - c.y) > p.alturaEscudo * 0.5f) return 1.f;
    return 1.f - escudo_.absorcao;
}

// ---------------------------------------------------------------- quadro

EventosCinza CorrenteCinza::atualizar(float dtReal, float dtMundo, const EntradaCinza& e, Vida& vida) {
    EventosCinza ev;
    vida.max = sangue_.vidaMaxima();
    if (vida.atual > vida.max) vida.atual = vida.max;

    // ---- estado da conjuração ----
    EstadoConjuracao livre = !e.podeConjurar ? EstadoConjuracao::Indisponivel
                             : e.jogadorUmido ? EstadoConjuracao::Bloqueada
                                              : EstadoConjuracao::Pronta;
    const bool temSangue = vida.atual > p.vidaMinimaConjurar;
    if (canalizando_) {
        if (livre != EstadoConjuracao::Pronta || !temSangue) {  // molhou, montou ou secou: sem faísca
            soltarCaptura();
            canalizando_ = false;
            ev.interrompida = true;
        } else if (!e.segurar) {
            soltarFaisca(e, vida, ev);
            canalizando_ = false;
        }
    } else if (livre == EstadoConjuracao::Pronta && e.segurar && temSangue) {
        canalizando_ = true;
        tempoCanalizando_ = 0;
        ev.comecouCanalizar = true;
    }
    if (canalizando_) estado_ = EstadoConjuracao::Canalizando;
    else if (livre != EstadoConjuracao::Pronta) estado_ = livre;
    else estado_ = vida.atual > p.vidaMinimaConjurar ? EstadoConjuracao::Pronta : EstadoConjuracao::SemSangue;

    // ---- custo contínuo e dilatação do tempo (relógio real) ----
    if (canalizando_) {
        tempoCanalizando_ += dtReal;
        sangue_.pagar(vida, p.custoVidaPorSegundo * dtReal, p.desgastePorSegundo * dtReal, p.vidaMaximaMinima);
    } else if (p.recuperacaoDesgaste > 0.f) {
        sangue_.recuperar(vida, p.recuperacaoDesgaste * dtReal);
    }
    {
        float alvo = canalizando_ ? p.fatorTempo : 1.f;
        float passo = std::abs(1.f - p.fatorTempo) * dtReal / std::max(1e-3f, p.rampaTempo);
        fatorAtual_ = fatorAtual_ < alvo ? std::min(alvo, fatorAtual_ + passo) : std::max(alvo, fatorAtual_ - passo);
    }

    // ---- fontes contínuas ----
    for (FonteFumaca& f : fontes_) {
        if (!f.ativa) continue;
        f.acumulado += dtMundo;
        while (f.acumulado >= p.intervaloFogueira) {
            f.acumulado -= p.intervaloFogueira;
            float jx = aleatorio01(rng_) - 0.5f, jz = aleatorio01(rng_) - 0.5f;
            emitir(f.pos + Vec3f{jx * 0.4f, 0.6f, jz * 0.4f}, p.raioNuvemFogueira, Vec3f{jx * 0.25f, p.subidaNuvemFogueira, jz * 0.25f});
        }
    }

    // ---- nuvens: captura, puxão, deriva e envelhecimento ----
    for (NuvemFumaca& n : nuvens_) {
        if (canalizando_ && !n.capturada && n.pavio < 0.f) {
            bool perto = distancia(n.pos, e.peito) <= p.raioAtracao ||
                         (e.moldeMirado && distancia(n.pos, e.pontoMolde) <= p.raioAtracao);
            n.capturada = perto;
        }
        if (n.capturada) {
            // Cada nuvem mira um ponto próprio em volta do destino (estável pela semente).
            float ang = float(n.semente % 628u) * 0.01f;
            float r = p.espalhamentoMolde * float((n.semente >> 10) % 100u) * 0.01f;
            Vec3f destino = e.pontoMolde + Vec3f{std::cos(ang) * r, 0.35f * std::sin(ang * 1.7f) * r, std::sin(ang) * r};
            Vec3f d = destino - n.pos;
            float dist = len(d);
            Vec3f desejada = dist > 1e-4f ? d * (std::min(p.velAtracao, dist * p.ganhoAtracao) / dist) : Vec3f{};
            n.vel = n.vel + (desejada - n.vel) * std::min(1.f, dtReal * 6.f);
            n.pos = n.pos + n.vel * dtReal;
            n.controle = std::min(1.f, n.controle + dtReal / 0.3f);
        } else {
            n.idade += dtMundo;
            float freio = std::exp(-1.5f * dtMundo);  // o empurrão horizontal morre; a subida fica
            n.vel.x *= freio;
            n.vel.z *= freio;
            n.pos = n.pos + n.vel * dtMundo;
            n.controle = std::max(0.f, n.controle - dtMundo / 0.5f);
            if (n.pavio >= 0.f) n.pavio = std::max(0.f, n.pavio - dtMundo);
        }
        n.raio = n.raioInicial * (1.f + p.crescimentoNuvem * clamp01(n.idade / std::max(1e-3f, n.vida)));
    }
    // cadeia: cada pavio que chegou a zero detona (e pode acender outros, que esperam o próprio pavio)
    for (size_t i = 0; i < nuvens_.size();) {
        if (nuvens_[i].pavio == 0.f) detonar(i, false, ev);
        else i++;
    }
    // dissipação natural e água
    for (size_t i = 0; i < nuvens_.size();) {
        const NuvemFumaca& n = nuvens_[i];
        bool molhou = umido(n.pos);
        if (molhou || n.idade >= n.vida) {
            ev.apagadasPelaAgua += molhou;
            nuvens_.erase(nuvens_.begin() + long(i));
        } else {
            i++;
        }
    }

    // ---- escudo preso ao corpo ----
    if (escudo_.ativo()) {
        Vec3f f = normalizar(Vec3f{e.frente.x, 0, e.frente.z});
        escudo_.centro = e.pes + f * p.afastamentoEscudo + Vec3f{0, p.alturaEscudo * 0.5f, 0};
        escudo_.yaw = std::atan2(f.x, f.z);
        escudo_.restante = std::max(0.f, escudo_.restante - dtMundo);
        if (e.jogadorUmido || umido(escudo_.centro)) escudo_.restante = 0.f;  // a fuligem se desfaz na água
    }

    // ---- explosões (visual) ----
    for (ExplosaoCinza& x : explosoes_) x.idade += dtMundo;
    explosoes_.erase(std::remove_if(explosoes_.begin(), explosoes_.end(), [](const ExplosaoCinza& x) { return x.idade >= x.duracao; }),
                     explosoes_.end());
    return ev;
}
