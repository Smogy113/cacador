#include "core/fisica_alvo.h"
#include <algorithm>
#include <cmath>
#include "core/terrain.h"

void FisicaAlvo::aplicar(const GolpeAlvo& g) {
    empurrao = empurrao + g.empurrao;
    if (g.levante > 0.f) vy = std::max(vy, g.levante);
    derrubado = std::max(derrubado, g.derrubar);
    atordoado = std::max(atordoado, g.atordoar);
    congelado = std::max(congelado, g.congelar);
    queimando = std::max(queimando, g.queimar);
    if (g.lentidao > 0.f) {  // duas lentidões juntas: vale a mais forte
        fatorLento = lento > 0.f ? std::min(fatorLento, g.fatorLento) : g.fatorLento;
        lento = std::max(lento, g.lentidao);
    }
}

float FisicaAlvo::atualizar(float dt, Vec3f& pos, const Terreno& t, const MundoColisao& col, float raio) {
    float danoQueda = 0.f;
    if (empurrao.x != 0.f || empurrao.z != 0.f) {
        // Desliza respeitando paredes; não entra no rio. No ar o atrito é bem menor.
        Vec3f q = col.resolver(pos + empurrao * dt, raio);
        if (t.mascaraRio(q.x, q.z) <= 0.5f) {
            pos.x = q.x;
            pos.z = q.z;
        }
        empurrao = empurrao * std::exp((noAr() ? -1.f : -6.f) * dt);
        if (std::hypot(empurrao.x, empurrao.z) < 0.05f) empurrao = {};
    }
    if (noAr()) {
        vy -= GRAVIDADE * dt;
        voo += vy * dt;
        if (voo <= 0.f) {  // aterrissou
            float impacto = -vy;
            voo = vy = 0.f;
            danoQueda = std::max(0.f, impacto - QUEDA_SEM_DANO) * DANO_POR_MS;
            if (impacto > 5.f) derrubado = std::max(derrubado, 1.f);
        }
    } else {
        derrubado = std::max(0.f, derrubado - dt);  // só levanta depois de chegar ao chão
    }
    congelado = std::max(0.f, congelado - dt);
    atordoado = std::max(0.f, atordoado - dt);
    queimando = std::max(0.f, queimando - dt);
    lento = std::max(0.f, lento - dt);
    if (lento <= 0.f) fatorLento = 1.f;
    pos.y = t.altura(pos.x, pos.z);
    return danoQueda;
}

bool golpear(AlvoTreino& a, const GolpeAlvo& g) {
    if (!a.ativo()) return false;
    a.fisica.aplicar(g);
    if (g.dano > 0.f) {
        a.vida.dano(g.dano);
        a.tranco = 1.f;
    }
    if (a.vida.morto()) {
        a.reaparecer = a.tempoReaparecer;
        return true;
    }
    return false;
}

void atualizar(AlvoTreino& a, float dt, const Terreno& t, const MundoColisao& col) {
    a.tranco = std::max(0.f, a.tranco - dt * 3.f);
    if (!a.ativo()) {
        a.reaparecer -= dt;
        if (a.reaparecer <= 0.f) {  // volta inteiro para a base
            a.pos = a.base;
            a.vida = Vida{};
            a.fisica = FisicaAlvo{};
            a.reaparecer = 0.f;
        }
        return;
    }
    float queda = a.fisica.atualizar(dt, a.pos, t, col, 0.3f);
    if (queda > 0.f) {
        a.vida.dano(queda);
        a.tranco = 1.f;
        if (a.vida.morto()) a.reaparecer = a.tempoReaparecer;
    }
    a.vida.atualizar(dt, true);
}
