#include "core/projetil_encantado.h"
#include <algorithm>
#include <cmath>
#include "core/terrain.h"

// ---------------------------------------------------------------- Ferro Frio

bool ZonaFerroFrio::contem(Vec3f q) const {
    return q.x >= caixa.min.x && q.x <= caixa.max.x && q.y >= caixa.min.y && q.y <= caixa.max.y && q.z >= caixa.min.z &&
           q.z <= caixa.max.z;
}

bool ZonaFerroFrio::cruza(Vec3f a, Vec3f b, Vec3f* entrada) const {
    // teste de lajes: o intervalo [t0, t1] do segmento dentro da caixa em cada eixo
    float t0 = 0.f, t1 = 1.f;
    const float o[3] = {a.x, a.y, a.z}, d[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
    const float mn[3] = {caixa.min.x, caixa.min.y, caixa.min.z}, mx[3] = {caixa.max.x, caixa.max.y, caixa.max.z};
    for (int i = 0; i < 3; i++) {
        if (std::abs(d[i]) < 1e-9f) {
            if (o[i] < mn[i] || o[i] > mx[i]) return false;
            continue;
        }
        float ta = (mn[i] - o[i]) / d[i], tb = (mx[i] - o[i]) / d[i];
        if (ta > tb) std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
        if (t0 > t1) return false;
    }
    if (entrada) *entrada = a + (b - a) * t0;
    return true;
}

// ---------------------------------------------------------------- golpes

bool ZonaElemental::contem(Vec3f pes) const {
    return std::hypot(pes.x - centro.x, pes.z - centro.z) <= raio && std::abs(pes.y - centro.y) < 2.f;
}

GolpeAlvo golpeDoImpacto(const Impacto& im, Vec3f centroAlvo, bool direto, const ParametrosCircuito& p) {
    GolpeAlvo g;
    if (!im.encantado) return g;  // chumbo: só o dano da bala, que o jogo aplica
    const EfeitoCircuito& e = im.efeito;
    Vec3f rel = centroAlvo - im.ponto;
    float dist = len(rel);
    float queda = direto ? 1.f : (dist >= e.raio ? 0.f : 1.f - dist / e.raio);
    if (queda <= 0.f) return g;
    const float k = queda * e.intensidade;
    float h = std::hypot(rel.x, rel.z);
    Vec3f fora = h > 1e-3f ? Vec3f{rel.x / h, 0, rel.z / h} : normalizar(Vec3f{im.dir.x, 0, im.dir.z});
    switch (e.propriedade) {
    case Propriedade::Velocidade:
    case Propriedade::Arrasto:
        g.dano = p.danoVelocidade * k;
        break;
    case Propriedade::Peso:  // onda de pressão: esmaga, empurra para fora e derruba
        g.dano = p.danoPeso * k;
        g.empurrao = fora * (p.empurraoPeso * k);
        g.derrubar = p.tempoDerrubado * std::min(1.f, k);
        break;
    case Propriedade::Levante:  // o peso invertido: arremessa para cima, com um leve afastamento
        g.levante = p.levante * (0.5f + 0.5f * queda) * e.intensidade;
        g.empurrao = fora * (1.5f * k);
        break;
    case Propriedade::Calor:
        if (e.duracaoZona > 0.f) {
            g.queimar = 0.5f;  // a zona queima depois; o impacto só acende
        } else {
            g.dano = p.danoQueimaInstantanea * k;
            g.queimar = 2.f;
        }
        break;
    case Propriedade::Frio:
        if (e.duracaoZona <= 0.f) g.congelar = p.tempoCongelamento * k;  // com zona, quem congela é ela
        break;
    }
    return g;
}

GolpeAlvo golpeDaZona(const ZonaElemental& z, Vec3f pes, float dt, const ParametrosCircuito& p) {
    GolpeAlvo g;
    if (z.restante <= 0.f || !z.contem(pes)) return g;
    if (z.frio) {
        g.congelar = 0.4f;  // renovado a cada quadro dentro; solta 0,4 s depois de sair
    } else {
        g.dano = p.danoCalorPorSegundo * z.intensidade * dt;
        g.queimar = 0.5f;
    }
    return g;
}

// ---------------------------------------------------------------- superfície

Vec3f normalSuperficie(Vec3f ponto, Vec3f dir, const Terreno& t, const MundoColisao& col) {
    if (ponto.y <= t.altura(ponto.x, ponto.z) + 0.08f) return t.normal(ponto.x, ponto.z);
    // Sonda em 10 direções a partir de um ponto um pouco antes do contato: as que batem em sólido
    // apontam para dentro da parede; a normal é o oposto da soma delas.
    const Vec3f q = ponto - dir * 0.05f;
    static const Vec3f sondas[10] = {{1, 0, 0},           {-1, 0, 0},          {0, 0, 1},           {0, 0, -1},
                                     {0.7071f, 0, 0.7071f}, {-0.7071f, 0, 0.7071f}, {0.7071f, 0, -0.7071f}, {-0.7071f, 0, -0.7071f},
                                     {0, 1, 0},           {0, -1, 0}};
    Vec3f soma{};
    for (const Vec3f& s : sondas)
        if (!col.raioLivre(q, q + s * 0.3f)) soma = soma + s;
    if (len(soma) < 1e-3f) return normalizar(dir * -1.f);
    return normalizar(soma * -1.f);
}

// ---------------------------------------------------------------- mundo

void CircuitosNoMundo::disparar(Vec3f boca, Vec3f dir, const Circuito& c) {
    ProjetilEncantado pr;
    pr.circuito = c;
    pr.efeito = resolverEfeito(c, p);
    pr.pos = boca;
    pr.vel = normalizar(dir) * pr.efeito.velocidade;
    pr.ricochetesRestantes = pr.efeito.ricochetes;
    pr.rastro[0] = boca;
    pr.nRastro = 1;
    projeteis_.push_back(pr);
}

bool CircuitosNoMundo::noFerro(Vec3f q) const {
    for (const ZonaFerroFrio& z : ferro_)
        if (z.contem(q)) return true;
    return false;
}

EventosCircuito CircuitosNoMundo::atualizar(float dt, const Terreno& t, const MundoColisao& col, const std::vector<Alvo>& alvos) {
    EventosCircuito ev;
    auto impactar = [&](const ProjetilEncantado& pr, Vec3f ponto, Vec3f normal, Vec3f dir, int alvo, Parte parte) {
        Impacto im;
        im.ponto = ponto;
        im.normal = normal;
        im.dir = dir;
        im.circuito = pr.circuito;
        im.efeito = pr.efeito;
        im.encantado = pr.encantado;
        im.alvo = alvo;
        im.parte = parte;
        ev.impactos.push_back(im);
        if (pr.encantado && pr.efeito.duracaoZona > 0.f) {  // a Onda que não é Pólvora deixa zona
            ZonaElemental z;
            z.centro = {ponto.x, t.altura(ponto.x, ponto.z), ponto.z};
            z.raio = pr.efeito.raio;
            z.restante = z.duracao = pr.efeito.duracaoZona;
            z.intensidade = pr.efeito.intensidade;
            z.frio = pr.efeito.propriedade == Propriedade::Frio;
            zonas_.push_back(z);
        }
    };

    for (size_t i = 0; i < projeteis_.size();) {
        ProjetilEncantado& pr = projeteis_[i];
        pr.idade += dt;
        if (pr.idade > p.vidaProjetil) {
            ev.sumidos.push_back(pr.pos);
            projeteis_.erase(projeteis_.begin() + long(i));
            continue;
        }
        pr.vel.y -= p.gravidade * pr.efeito.gravidadeMult * dt;
        const Vec3f a = pr.pos, b = a + pr.vel * dt;
        // Ferro Frio: o circuito apaga ao tocar o metal; a bala segue como chumbo comum.
        if (pr.encantado) {
            for (const ZonaFerroFrio& z : ferro_) {
                Vec3f entrada;
                if (z.cruza(a, b, &entrada)) {
                    pr.encantado = false;
                    pr.efeito = efeitoChumbo(p);
                    pr.ricochetesRestantes = 0;
                    ev.desencantos.push_back(entrada);
                    break;
                }
            }
        }
        const float passo = len(b - a);
        const Vec3f d = normalizar(b - a);
        // lancarTiro devolve dist = alcance quando nada para o raio: a folga além do passo separa
        // "não acertou" de "acertou no fim do passo"
        Acerto h = lancarTiro(a, d, passo + 0.3f, alvos, t, col);
        if (h.dist <= passo) {
            if (h.indice >= 0) {  // alvo: para nele
                impactar(pr, h.ponto, d * -1.f, d, h.indice, h.parte);
                projeteis_.erase(projeteis_.begin() + long(i));
                continue;
            }
            Vec3f n = normalSuperficie(h.ponto, d, t, col);
            if (pr.ricochetesRestantes > 0 && dot(pr.vel, n) < 0.f) {
                // Resina e Cera: quica mantendo a propriedade (e a falta de gravidade da Espiral)
                pr.ricochetesRestantes--;
                pr.vel = (pr.vel - n * (2.f * dot(pr.vel, n))) * p.restituicao;
                pr.pos = h.ponto - d * 0.02f + n * 0.06f;
                ev.ricochetes.push_back(h.ponto);
            } else {
                impactar(pr, h.ponto, n, d, -1, Parte::Nada);
                projeteis_.erase(projeteis_.begin() + long(i));
                continue;
            }
        } else {
            pr.pos = b;
        }
        for (int k = ProjetilEncantado::RASTRO - 1; k > 0; k--) pr.rastro[size_t(k)] = pr.rastro[size_t(k - 1)];
        pr.rastro[0] = pr.pos;
        pr.nRastro = std::min(ProjetilEncantado::RASTRO, pr.nRastro + 1);
        i++;
    }

    for (ZonaElemental& z : zonas_) z.restante -= dt;
    zonas_.erase(std::remove_if(zonas_.begin(), zonas_.end(), [](const ZonaElemental& z) { return z.restante <= 0.f; }), zonas_.end());
    for (CapsulaVestigio& c : capsulas_) c.idade += dt;
    return ev;
}

// ---------------------------------------------------------------- vestígios

void CircuitosNoMundo::soltarCapsula(Vec3f chao, float yaw, const Circuito& c) {
    capsulas_.push_back({chao, yaw, 0.f, c});
    while (int(capsulas_.size()) > std::max(1, p.maxCapsulas)) capsulas_.erase(capsulas_.begin());
}

int CircuitosNoMundo::capsulaPerto(Vec3f q, float raio) const {
    int melhor = -1;
    float md = raio;
    for (size_t i = 0; i < capsulas_.size(); i++) {
        float d = std::hypot(capsulas_[i].pos.x - q.x, capsulas_[i].pos.z - q.z);
        if (d <= md) {
            md = d;
            melhor = int(i);
        }
    }
    return melhor;
}

int CircuitosNoMundo::recolher(Vec3f q, float raio) {
    size_t antes = capsulas_.size();
    capsulas_.erase(std::remove_if(capsulas_.begin(), capsulas_.end(),
                                   [&](const CapsulaVestigio& c) { return std::hypot(c.pos.x - q.x, c.pos.z - q.z) <= raio; }),
                    capsulas_.end());
    return int(antes - capsulas_.size());
}
