#include "core/horse.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include "core/terrain.h"

bool tentarMontar(Cavalo& c, const Corpo& j) {
    float d = std::hypot(c.corpo.pos.x - j.pos.x, c.corpo.pos.z - j.pos.z);
    if (d > DIST_MONTAR) return false;
    c.montado = true;
    return true;
}

void desmontar(Cavalo& c, Corpo& j, const MundoColisao& col) {
    float y = c.corpo.yaw;
    Vec3f esquerda{std::cos(y), 0, -std::sin(y)}, frente = frenteDe(y);
    const Vec3f opcoes[] = {esquerda * 1.4f, esquerda * -1.4f, frente * -2.2f, frente * 2.2f, esquerda * 2.2f, esquerda * -2.2f};
    Vec3f escolhido = c.corpo.pos + esquerda * 1.4f;
    for (auto o : opcoes) {
        Vec3f p = c.corpo.pos + o;
        if (len(col.resolver(p, RAIO_PESSOA) - p) < 1e-4f) { escolhido = p; break; }
    }
    j.pos = col.resolver(escolhido, RAIO_PESSOA);
    j.pos.y = c.corpo.pos.y;
    j.yaw = y;
    j.vel = 0;
    c.montado = false;
}

bool inclinacaoPermitida(const Terreno& t, Vec3f de, Vec3f para) {
    float dh = t.altura(para.x, para.z) - t.altura(de.x, de.z);
    float d = std::hypot(para.x - de.x, para.z - de.z);
    if (d < 1e-4f || dh <= 0) return true;
    return dh / d <= INCLINACAO_MAX;
}

Vec3f resolverCavalo(Vec3f pes, float yaw, const MundoColisao& col, bool montado) {
    // As seções acompanham o comprimento da malha. A cabeça fica acima do chão
    // e a seção do dorso inclui a altura do cavaleiro quando equipado.
    struct Secao { float z, base, raio, altura; };
    const Secao secoes[] = {
        {-.68f, 0.f, .41f, 1.66f}, {-.02f, 0.f, .43f, montado ? 2.62f : 1.77f},
        {.62f, 0.f, .38f, 1.78f}, {1.24f, 1.23f, .28f, 1.01f},
    };
    Vec3f frente = frenteDe(yaw);
    for (int it = 0; it < 2; ++it) {
        Vec3f antes = pes;
        for (const auto& s : secoes) {
            Vec3f centro = pes + frente * s.z + Vec3f{0, s.base, 0};
            pes = pes + (col.resolver(centro, s.raio, s.altura) - centro);
        }
        if (len(pes - antes) < 1e-4f) break;
    }
    return pes;
}

static Andadura classificar(float v) {
    if (v < 0.3f) return Andadura::Parado;
    if (v < 3.5f) return Andadura::Passo;
    if (v < INICIO_GALOPE) return Andadura::Trote;
    return Andadura::Galope;
}

void moverCavalo(Cavalo& c, const Entrada& e, float dt, const Terreno& t, const MundoColisao& col) {
    Corpo& k = c.corpo;
    const float yawAntes = k.yaw;
    float chao = std::max(t.altura(k.pos.x, k.pos.z), col.alturaPiso(k.pos.x, k.pos.z, std::max(k.pos.y, t.altura(k.pos.x, k.pos.z))));
    float alvo = 0, taxa = 8.f;
    if (c.montado) {
        if (e.frente > 0.1f) { alvo = e.correr ? VEL_GALOPE : VEL_TROTE; taxa = ACEL_CAVALO; }
        else if (e.frente < -0.1f) taxa = 14.f;
        if (chao < t.nivelAgua() - 0.6f) alvo = std::min(alvo, VEL_PASSO);  // agua funda
        float giro = 2.4f * (1.f - 0.55f * k.vel / VEL_GALOPE);
        k.yaw -= e.lado * giro * dt;
    }
    if (k.vel < alvo) k.vel = std::min(alvo, k.vel + taxa * dt);
    else k.vel = std::max(alvo, k.vel - (alvo > 0 ? ACEL_CAVALO : taxa) * dt);

    float total = k.vel * dt;
    if (total == 0.f && k.yaw != yawAntes && len(resolverCavalo(k.pos, k.yaw, col, c.montado) - k.pos) > .04f)
        k.yaw = yawAntes;
    int passos = std::max(1, int(std::ceil(total / 0.25f)));
    Vec3f f = frenteDe(k.yaw);
    for (int i = 0; i < passos && total > 0; i++) {
        Vec3f prox = k.pos + f * (total / passos);
        // sobre um piso (tabuleiro de ponte) o relevo de baixo nao importa
        bool noPiso = col.alturaPiso(prox.x, prox.z, k.pos.y) > t.altura(prox.x, prox.z);
        if (!noPiso && !inclinacaoPermitida(t, k.pos, prox)) { k.vel = 0; break; }
        prox = resolverCavalo(prox, k.yaw, col, c.montado);
        prox.y = std::max(t.altura(prox.x, prox.z), col.alturaPiso(prox.x, prox.z, k.pos.y));
        k.pos = prox;
    }
    k.pos.y = std::max(t.altura(k.pos.x, k.pos.z), col.alturaPiso(k.pos.x, k.pos.z, k.pos.y));
    c.andadura = classificar(k.vel);
    // fase da passada: um ciclo a cada ~1,6 m (passo), 2,5 m (trote), 3,5 m (galope)
    float porMetro = k.vel < 3.5f ? 3.9f : lerp(2.5f, FASE_GALOPE_POR_METRO, pesoGalopeCavalo(k.vel));
    c.passada += dt * k.vel * porMetro;
}

ContatosCavalo contatosCavalo(Andadura andadura, float faseAnterior, float faseAtual) {
    ContatosCavalo contatos;
    if (andadura == Andadura::Parado || faseAtual <= faseAnterior) return contatos;

    struct Apoio { float fase; BatidaCasco batida; };
    // Passo tem quatro apoios espaçados. No trote as diagonais tocam juntas.
    // No galope os quatro impactos se agrupam antes da fase de suspensão.
    static constexpr Apoio passo[] = {
        {1.13f, {.82f, 1.f}}, {2.70f, {1.f, -1.f}},
        {4.27f, {.82f, -1.f}}, {5.84f, {1.f, 1.f}},
    };
    static constexpr Apoio trote[] = {{2.57f, {1.f, -.2f}}, {5.71f, {1.f, .2f}}};
    static constexpr Apoio galope[] = {
        {TOQUES_GALOPE[0], {.69f, 1.f}}, {TOQUES_GALOPE[1], {.86f, -1.f}},
        {TOQUES_GALOPE[2], {1.f, 1.f}}, {TOQUES_GALOPE[3], {.73f, -1.f}},
    };
    const Apoio* apoios = andadura == Andadura::Passo ? passo : (andadura == Andadura::Trote ? trote : galope);
    const int n = andadura == Andadura::Trote ? 2 : 4;
    constexpr double CICLO = 6.283185307179586;
    // Procura o último cruzamento de cada apoio. Em um salto de quadro, evita
    // tocar a mesma batida várias vezes ao mesmo tempo.
    std::array<std::pair<double, BatidaCasco>, 4> cruzados{};
    for (int i = 0; i < n; ++i) {
        double fase = apoios[i].fase;
        double ultima = fase + CICLO * std::floor((faseAtual - fase) / CICLO);
        if (ultima > faseAnterior) cruzados[contatos.total++] = {ultima, apoios[i].batida};
    }
    std::sort(cruzados.begin(), cruzados.begin() + contatos.total,
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (int i = 0; i < contatos.total; ++i) contatos.batidas[i] = cruzados[i].second;
    return contatos;
}
