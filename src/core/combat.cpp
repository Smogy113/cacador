#include "core/combat.h"
#include <algorithm>
#include <cmath>
#include "core/terrain.h"

void Revolver::atirar() {
    if (!podeAtirar()) return;
    balas--;
    espera = CADENCIA;
}
void Revolver::recarregar() {
    if (balas < CAPACIDADE && recarga <= 0) recarga = TEMPO_RECARGA;
}
void Revolver::atualizar(float dt) {
    espera = std::max(0.f, espera - dt);
    if (recarga > 0) {
        recarga -= dt;
        if (recarga <= 0) {
            recarga = 0;
            balas = CAPACIDADE;
        }
    }
}

void Vida::dano(float v) {
    atual = std::max(0.f, atual - v);
    semDano = 0;
}
void Vida::atualizar(float dt, bool regenera) {
    if (morto()) return;
    float antes = semDano;
    semDano += dt;
    if (!regenera || semDano < ATRASO_REGEN) return;
    float tempoRegen = std::min(dt, semDano - std::max(antes, ATRASO_REGEN));
    atual = std::min(max, atual + REGEN * tempoRegen);
}

namespace {
// distancia ao longo do raio ate entrar no cilindro vertical (x,z) de raio r entre y0 e y1; < 0 se nao entra
float raioCilindro(Vec3f o, Vec3f d, Vec3f base, float r, float y0, float y1) {
    float ox = o.x - base.x, oz = o.z - base.z;
    float a = d.x * d.x + d.z * d.z, b = 2 * (ox * d.x + oz * d.z), c = ox * ox + oz * oz - r * r;
    if (a < 1e-9f) return -1;
    float disc = b * b - 4 * a * c;
    if (disc < 0) return -1;
    float s = std::sqrt(disc);
    for (float t : {(-b - s) / (2 * a), (-b + s) / (2 * a)}) {
        if (t < 0) continue;
        float y = o.y + d.y * t;
        if (y >= y0 && y <= y1) return t;
    }
    return -1;
}
float raioEsfera(Vec3f o, Vec3f d, Vec3f c, float r) {
    Vec3f oc = o - c;
    float b = dot(oc, d), cc = dot(oc, oc) - r * r, disc = b * b - cc;
    if (disc < 0) return -1;
    float t = -b - std::sqrt(disc);
    return t >= 0 ? t : -1;
}
}  // namespace

Acerto lancarTiro(Vec3f origem, Vec3f dir, float alcance, const std::vector<Alvo>& alvos, const Terreno& t,
                  const MundoColisao& col) {
    // 1) onde o raio para no mundo (terreno ou caixa)
    float fim = alcance;
    const float PASSO = 0.25f;
    Vec3f ant = origem;
    for (float s = PASSO; s <= alcance + 1e-4f; s += PASSO) {
        Vec3f p = origem + dir * s;
        bool chao = p.y < t.altura(p.x, p.z);
        bool caixa = !col.raioLivre(ant, p);
        if (chao || caixa) {
            float a = s - PASSO, b = s;  // bissecao
            for (int i = 0; i < 12; i++) {
                float m = (a + b) * 0.5f;
                Vec3f q = origem + dir * m;
                bool bate = (chao && q.y < t.altura(q.x, q.z)) || (caixa && !col.raioLivre(ant, q));
                (bate ? b : a) = m;
            }
            fim = b;
            break;
        }
        ant = p;
    }
    Acerto r;
    r.dist = fim;
    r.ponto = origem + dir * fim;
    // 2) o alvo mais proximo antes disso
    for (size_t i = 0; i < alvos.size(); i++) {
        const Alvo& a = alvos[i];
        Vec3f cab = a.pes + Vec3f{0, a.altura * 0.92f, 0};
        float tc = raioEsfera(origem, dir, cab, RAIO_CABECA);
        float tb = raioCilindro(origem, dir, a.pes, RAIO_CORPO, a.pes.y, a.pes.y + a.altura * 0.82f);
        Parte parte = Parte::Nada;
        float tt = 1e9f;
        if (tc >= 0 && tc < tt) { tt = tc; parte = Parte::Cabeca; }
        if (tb >= 0 && tb < tt) { tt = tb; parte = Parte::Corpo; }
        if (parte != Parte::Nada && tt < r.dist) {
            r.indice = int(i);
            r.parte = parte;
            r.dist = tt;
            r.ponto = origem + dir * tt;
        }
    }
    return r;
}

bool linhaLivre(Vec3f a, Vec3f b, const Terreno& t, const MundoColisao& col) {
    if (!col.raioLivre(a, b)) return false;
    float d = len(b - a);
    int n = std::max(2, int(d / 1.f));
    for (int i = 1; i < n; i++) {  // amostra o relevo a cada ~1 m
        Vec3f p = a + (b - a) * (float(i) / n);
        if (p.y < t.altura(p.x, p.z)) return false;
    }
    return true;
}

float espalhamento(bool mirando, float vel) {
    float base = mirando ? 0.004f : 0.035f;
    return base + 0.006f * std::min(vel, 6.f);
}
