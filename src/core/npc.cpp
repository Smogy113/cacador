#include "core/npc.h"
#include <algorithm>
#include <cmath>
#include "core/noise.h"
#include "core/terrain.h"

Populacao::Populacao(const std::vector<std::vector<Vec3f>>& grupos, uint32_t semente, int quantidade) {
    size_t total = 0;
    for (auto& g : grupos) total += g.size();
    if (total == 0 || quantidade <= 0) return;
    uint32_t rng = semente * 2654435761u + 1u;
    for (int k = 0; k < quantidade; k++) {
        // escolhe um grupo proporcional ao numero de pontos
        size_t sorteio = size_t(aleatorio01(rng) * total), gi = 0;
        while (sorteio >= grupos[gi].size()) { sorteio -= grupos[gi].size(); gi++; }
        const auto& g = grupos[gi];
        Npc n;
        n.semente = hash32(semente + k * 977u);
        n.noturno = aleatorio01(rng) < 0.2f;
        // rota: os pontos do grupo, comecando num ponto sorteado
        size_t ini = (g.size() == 1) ? 0 : size_t(aleatorio01(rng) * g.size()) % g.size();
        if (quantidade == 1) ini = 0;
        for (size_t q = 0; q < g.size(); q++) n.rota.push_back(g[(ini + q) % g.size()]);
        n.corpo.pos = n.rota[0];
        n.alvo = n.rota.size() > 1 ? 1 : 0;
        npcs_.push_back(n);
    }
}

int Populacao::ativos() const {
    int a = 0;
    for (auto& n : npcs_) a += n.ativo;
    return a;
}

static bool hora_de_rua(const DayClock& c) { return c.fase() != Fase::Noite && c.fase() != Fase::Madrugada; }

void Populacao::passo(Npc& n, float dt, bool deveFicar, const Terreno& t, const MundoColisao& col, Vec3f jog) {
    Vec3f alvo = deveFicar ? n.rota[n.alvo] : n.rota[0];  // fora de hora: volta para casa
    if (n.espera > 0 && deveFicar) {
        n.espera -= dt;
        n.corpo.vel = 0;
        return;
    }
    Vec3f d = alvo - n.corpo.pos;
    d.y = 0;
    float dist = len(d);
    if (dist < 0.6f) {
        n.corpo.vel = 0;
        if (!deveFicar) { n.ativo = false; return; }
        uint32_t s = n.semente + uint32_t(n.alvo) * 31u + uint32_t(n.passada * 10);
        n.espera = 2.f + aleatorio01(s) * 4.f;
        n.alvo = (n.alvo + 1) % int(n.rota.size());
        n.parado = 0;
        return;
    }
    Vec3f dir = d * (1.f / dist);
    // desvia do jogador
    Vec3f pj = n.corpo.pos - jog;
    pj.y = 0;
    float dj = len(pj);
    if (dj < 1.5f && dj > 1e-3f) dir = normalizar(dir + pj * (1.f / dj) * 1.2f);
    float total = std::min(dist, VEL_NPC * dt);
    Vec3f antes = n.corpo.pos;
    int passos = std::max(1, int(std::ceil(total / 0.25f)));
    for (int i = 0; i < passos; i++) {
        Vec3f p = col.resolver(n.corpo.pos + dir * (total / passos), RAIO_PESSOA);
        p.y = std::max(t.altura(p.x, p.z), col.alturaPiso(p.x, p.z, n.corpo.pos.y));
        n.corpo.pos = p;
    }
    float andou = len(n.corpo.pos - antes);
    n.corpo.vel = andou / std::max(dt, 1e-4f);
    n.passada += andou * 4.2f;  // mesma passada das animacoes (anim.h: FASE_POR_METRO)
    n.corpo.yaw = std::atan2(dir.x, dir.z);
    // preso numa parede: desiste deste ponto
    n.parado = andou < total * 0.3f ? n.parado + dt : 0;
    if (n.parado > 4.f) {
        n.parado = 0;
        if (deveFicar) n.alvo = (n.alvo + 1) % int(n.rota.size());
        else n.ativo = false;
    }
}

void Populacao::atualizar(float dt, const DayClock& c, const Terreno& t, const MundoColisao& col, Vec3f jog) {
    bool rua = hora_de_rua(c);
    for (auto& n : npcs_) {
        bool deveFicar = rua || n.noturno;
        float dj = std::hypot(n.corpo.pos.x - jog.x, n.corpo.pos.z - jog.z);
        if (!n.ativo) {
            if (deveFicar) {  // volta a aparecer em casa
                n.ativo = true;
                n.corpo.pos = n.rota[0];
                n.corpo.pos.y = t.altura(n.corpo.pos.x, n.corpo.pos.z);
                n.alvo = n.rota.size() > 1 ? 1 : 0;
                n.espera = 0;
            }
            continue;
        }
        if (!deveFicar && dj > DIST_FORA_DE_VISTA) {  // ninguem ve: ja foi para casa
            n.ativo = false;
            continue;
        }
        n.acumulado += dt;
        if (dj > DIST_ATUALIZA_SEMPRE && n.acumulado < 1.f) continue;  // longe: 1 vez por segundo
        float passoDt = n.acumulado;
        n.acumulado = 0;
        passo(n, passoDt, deveFicar, t, col, jog);
    }
}
