#include "core/hair_physics.h"
#include <algorithm>
#include <cmath>

namespace {
Vec3f interpolar(Vec3f a, Vec3f b, float t) { return a + (b - a) * t; }

Vec3f limitarDesvio(Vec3f p, Vec3f alvo, float limite) {
    Vec3f delta = p - alvo;
    float d2 = dot(delta, delta);
    return d2 > limite * limite ? alvo + delta * (limite / std::sqrt(d2)) : p;
}

struct Capsula {
    Vec3f a, eixo;
    float invComprimento2, raio2, yMin, yMax;

    Capsula(Vec3f inicio, Vec3f fim, float raio)
        : a(inicio), eixo(fim - inicio), invComprimento2(1.f / std::max(1e-8f, dot(eixo, eixo))),
          raio2(raio * raio), yMin(std::min(inicio.y, fim.y) - raio), yMax(std::max(inicio.y, fim.y) + raio) {}

    Vec3f radial(Vec3f p) const {
        return p - (a + eixo * clamp01(dot(p - a, eixo) * invComprimento2));
    }

    Vec3f limitar(Vec3f p, Vec3f alvo) const {
        // Descarta mechas acima dos ombros ou abaixo do crânio antes da projeção.
        if (p.y >= yMax || p.y <= yMin) return p;
        Vec3f delta = radial(p);
        float d2 = dot(delta, delta);
        if (d2 >= raio2) return p;

        // Conserva a folga de repouso mesmo onde a cápsula é mais larga que a pele.
        Vec3f repouso = radial(alvo);
        float limite = std::min(raio2, dot(repouso, repouso));
        if (d2 >= limite) return p;
        Vec3f direcao = d2 > 1e-10f ? delta * (1.f / std::sqrt(d2)) : normalizar(repouso);
        return p - delta + direcao * std::sqrt(limite);
    }
};

Vec3f noEspacoDaCabeca(const Mat4& m, Vec3f v) {
    // A inversa de uma rotação é a transposta. O deslocamento não usa translação.
    return {dot(m.vetor({1, 0, 0}), v), dot(m.vetor({0, 1, 0}), v), dot(m.vetor({0, 0, 1}), v)};
}

// Ponto posado levado ao espaço de repouso do osso (os ossos são rígidos).
Vec3f paraRepouso(const Mat4& osso, Vec3f p) { return noEspacoDaCabeca(osso, p - osso.ponto({0, 0, 0})); }

Vec3f dividir(Vec3f a, Vec3f b) { return {a.x / b.x, a.y / b.y, a.z / b.z}; }

// Volume justo preso a um osso (orelha na cabeça, pescoço no pescoço), testado no espaço de repouso,
// onde ele é fixo. Regra das cápsulas: o ponto não fica mais fundo que no repouso, nem dentro do volume.
struct VolumeNaPose {
    const Mat4* osso;
    Vec3f a, b, raios;  // repouso; raios já com a folga

    // Escala no volume: 1 na casca, menor dentro (cápsula com raios por eixo; a == b: elipsoide).
    float escala(Vec3f repouso, Vec3f* eixo = nullptr) const {
        const Vec3f q = dividir(repouso - a, raios), d = dividir(b - a, raios);
        const float t = dot(d, d) > 1e-12f ? clamp01(dot(q, d) / dot(d, d)) : 0.f;
        if (eixo) *eixo = a + (b - a) * t;
        return len(q - d * t);
    }

    Vec3f limitar(Vec3f p, Vec3f repousoDoAlvo) const {
        const Vec3f r = paraRepouso(*osso, p);
        Vec3f eixo;
        const float s = escala(r, &eixo);
        const float limite = std::min(1.f, escala(repousoDoAlvo));
        if (s >= limite) return p;
        const Vec3f d = r - eixo;
        const Vec3f direcao = s > 1e-6f ? d * (1.f / s) : Vec3f{raios.x, 0, 0};
        return osso->ponto(eixo + direcao * limite);
    }
};

// Orelhas, pescoço e alto do tronco numa pose (só valem quando ColisoresCabelo::temVolumesJustos()).
std::array<VolumeNaPose, 4> volumesJustos(const ColisoresCabelo& c, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito) {
    const Vec3f folga{FOLGA_DOS_VOLUMES, FOLGA_DOS_VOLUMES, FOLGA_DOS_VOLUMES};
    const Vec3f orelhaE{-c.orelha.x, c.orelha.y, c.orelha.z};
    return {{{&cabeca, c.orelha, c.orelha, c.raiosOrelha + folga},
             {&cabeca, orelhaE, orelhaE, c.raiosOrelha + folga},
             {&pescoco, c.pescocoA, c.pescocoB, c.raiosPescocoJusto + folga},
             {&peito, c.troncoA, c.troncoB, c.raiosTronco + folga}}};
}

// Tudo em que os nós esbarram numa pose: crânio, pescoço e ombros (cápsulas) e os volumes justos.
struct CorpoNaPose {
    std::array<Capsula, 3> capsulas;
    std::array<VolumeNaPose, 4> volumes;
    const Mat4* cabeca;
    bool temVolumes;

    CorpoNaPose(const ColisoresCabelo& c, const std::array<Vec3f, 5>& p, const Mat4& cabeca, const Mat4& pescoco,
                const Mat4& peito)
        : capsulas{{{p[0], p[1], c.raioCranio}, {p[1], p[2], c.raioPescoco}, {p[3], p[4], c.raioOmbros}}},
          volumes(volumesJustos(c, cabeca, pescoco, peito)), cabeca(&cabeca), temVolumes(c.temVolumesJustos()) {}

    // alvoNaCabeca: o repouso do nó, no espaço da cabeça (o mesmo que o modelo em repouso).
    Vec3f limitar(Vec3f p, Vec3f alvo, Vec3f alvoNaCabeca) const {
        if (temVolumes)
            for (const auto& volume : volumes) p = volume.limitar(p, alvoNaCabeca);
        for (const auto& capsula : capsulas) p = capsula.limitar(p, alvo);
        return p;
    }
};

// Amortecimento (1/s). O interno freia o balanço da mecha em relação à cabeça (o cabelo para de
// sacudir); o do ar freia a velocidade em relação ao mundo e é o que deita o cabelo para trás numa
// corrida: a 5 m/s, uma mecha nos ombros recua ~15 cm, uma curta ~3 cm. Antes havia só um termo de
// 10/s contra o mundo, e a corrida deixava qualquer cabelo esticado na horizontal.
constexpr float AMORTECIMENTO_INTERNO = 8.5f, ARRASTO_DO_AR = 1.5f;

// Mechas compridas (acima de ~20 cm de arco livre) balançam como pêndulos mais lentos e vão mais
// longe: 0 até 20 cm, 1 a partir de 40 cm.
float fatorComprido(float arco) { return smoothstep(.20f, .40f, arco); }
}  // namespace

Vec3f deslocamentoDaFita(const PerfilCabelo& perfil, const DeformacaoCabelo& deformacao, Vec3f repouso,
                         uint8_t guiaA, uint8_t guiaB, float pesoB, float t) {
    if (t <= 0.f) return {};
    Vec3f meio = deformacao.meio[guiaA], ponta = deformacao.ponta[guiaA];
    if (pesoB > 0.f) {
        meio = interpolar(meio, deformacao.meio[guiaB], pesoB);
        ponta = interpolar(ponta, deformacao.ponta[guiaB], pesoB);
    }
    // Curva cúbica com raiz fixa e tangente contínua no ponto de fixação.
    Vec3f desloc = (meio * (8.f * (1.f - t)) + ponta * (2.f * t - 1.f)) * (t * t);

    // Perto da raiz, a fita desliza mantendo a folga em repouso; a ponta respeita o elipsoide da cabeça.
    const auto& c = perfil.colisores;
    const Vec3f raios{std::max(c.raiosCabeca.x, 1e-4f), std::max(c.raiosCabeca.y, 1e-4f), std::max(c.raiosCabeca.z, 1e-4f)};
    auto naEscala = [&](Vec3f v) { return Vec3f{v.x / raios.x, v.y / raios.y, v.z / raios.z}; };
    const Vec3f q0 = naEscala(repouso - c.centroCabeca);
    const Vec3f q = q0 + naEscala(desloc);
    const float noRepouso = dot(q0, q0), d = dot(q, q);
    const float alvo = lerp(noRepouso, std::max(std::min(1.f, noRepouso), d), smoothstep(.12f, .58f, t));
    if (std::abs(d - alvo) > 1e-6f) {
        const Vec3f direcao = d > 1e-8f ? q * (1.f / std::sqrt(d)) : normalizar(q0);
        const Vec3f r = direcao * std::sqrt(alvo) - q0;
        desloc = {r.x * raios.x, r.y * raios.y, r.z * raios.z};
    }
    return desloc;
}

FisicaCabelo::FisicaCabelo(const PerfilCabelo& perfil) : perfil_(perfil) {
    perfil_.quantidade = std::min(perfil_.quantidade, MAX_GUIAS_CABELO);
    perfil_.principais = std::min({perfil_.principais, perfil_.quantidade, MAX_GUIAS_PRINCIPAIS});
    for (size_t g = 0; g < perfil_.quantidade; ++g) {
        const auto& guia = perfil_.guias[g];
        const auto& p = guia.pontos;
        float l0 = len(p[1] - p[0]), l1 = len(p[2] - p[1]);
        float comprimento = l0 + l1;
        float flex = clamp01(guia.flexibilidade);
        float arco = std::max(comprimento, guia.comprimentoLivre);
        float solto = smoothstep(.032f, .20f, arco) * lerp(.3f, 1.f, flex);
        // Pêndulo: a frequência cai com o comprimento (ω² ~ g/L). Um cabelo nos ombros balança
        // mais devagar e vai mais longe que uma mecha curta, sem perder a forma de repouso.
        float comprido = fatorComprido(arco) * flex;
        float rigidezPonta = lerp(290.f, 105.f, solto) * lerp(1.f, .5f, comprido);
        float desvio = comprimento * lerp(.26f, .60f, flex);
        float limitePonta = std::min(lerp(.16f, .30f, comprido), desvio);
        if (g < perfil_.principais) {
            auto& estado = guias_[g];
            estado.comprimentos = {l0, l1};
            estado.rigidez = {lerp(495.f, 200.f, solto) * lerp(1.f, .6f, comprido), rigidezPonta};
            estado.limites = {std::min(lerp(.085f, .15f, comprido), desvio * .60f), limitePonta};
        } else {
            const auto& v = guia.pais;
            const auto& a = guias_[v.a].comprimentos;
            const auto& b = guias_[v.b].comprimentos;
            auto escala = [](float proprio, float herdado) {
                return std::clamp(proprio / std::max(.003f, herdado), .45f, 1.65f);
            };
            auto& estado = secundarios_[g];
            estado.comprimento = l1;
            estado.rigidez = rigidezPonta;
            estado.limite = limitePonta;
            estado.escalaMeio = escala(l0, lerp(a[0], b[0], v.pesoB));
            estado.escalaPonta = escala(comprimento, lerp(a[0] + a[1], b[0] + b[1], v.pesoB));
        }
    }
}

void FisicaCabelo::atualizar(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f vento, float detalhe) {
    if (dt <= 0.f || perfil_.principais == 0) return;
    const auto& c = perfil_.colisores;
    const std::array<Vec3f, 5> colisores{
        cabeca.ponto(c.cranio), cabeca.ponto(c.nuca), pescoco.ponto(c.pescoco),
        peito.ponto(c.ombroE), peito.ponto(c.ombroD),
    };
    std::array<std::array<Vec3f, 3>, MAX_GUIAS_PRINCIPAIS> alvos;
    for (size_t g = 0; g < perfil_.principais; ++g)
        for (size_t i = 0; i < 3; ++i)
            alvos[g][i] = cabeca.ponto(perfil_.guias[g].pontos[i]);

    Vec3f salto = colisores[0] - colisoresAnteriores_[0];
    if (!iniciada_ || dot(salto, salto) > .60f * .60f || dt > .12f) {
        for (size_t g = 0; g < perfil_.principais; ++g)
            for (size_t i = 0; i < 2; ++i)
                guias_[g].moveis[i] = {alvos[g][i + 1], {}};
        iniciada_ = true;
        detalhesAtivos_ = false;
        deformacao_ = {};
    } else {
        int passos = std::max(1, int(std::ceil(dt * 120.f)));
        const float h = dt / passos;
        const float amortecimento = std::exp(-AMORTECIMENTO_INTERNO * h), arrasto = std::exp(-ARRASTO_DO_AR * h);
        // O repouso já inclui o caimento do corte. Uma inclinação muda a direção
        // da gravidade relativa a esse caimento, sem achatar a silhueta parada.
        const Vec3f gravidade{0, -9.81f, 0};
        Vec3f forca = vento + gravidade - cabeca.vetor(gravidade);
        for (int passo = 1; passo <= passos; ++passo) {
            float t = float(passo) / passos;
            std::array<Vec3f, 5> p;
            for (size_t i = 0; i < p.size(); ++i) p[i] = interpolar(colisoresAnteriores_[i], colisores[i], t);
            const CorpoNaPose corpo(c, p, cabeca, pescoco, peito);
            for (size_t g = 0; g < perfil_.principais; ++g) {
                auto& guia = guias_[g];
                Vec3f raiz = interpolar(guia.alvosAnteriores[0], alvos[g][0], t);
                std::array<Vec3f, 2> alvosMoveis, antes;
                for (size_t i = 0; i < guia.moveis.size(); ++i) {
                    auto& no = guia.moveis[i];
                    alvosMoveis[i] = interpolar(guia.alvosAnteriores[i + 1], alvos[g][i + 1], t);
                    const Vec3f alvoAntes = interpolar(guia.alvosAnteriores[i + 1], alvos[g][i + 1], t - 1.f / passos);
                    const Vec3f velAlvo = (alvosMoveis[i] - alvoAntes) * (1.f / h);
                    antes[i] = no.pos;
                    // O amortecimento interno freia só o balanço em relação à cabeça; o ar freia pouco.
                    no.vel = (velAlvo + (no.vel - velAlvo) * amortecimento) * arrasto +
                        ((alvosMoveis[i] - no.pos) * guia.rigidez[i] + forca * (i == 0 ? .50f : 1.f)) * h;
                    no.pos = no.pos + no.vel * h;
                }
                auto& meio = guia.moveis[0].pos;
                auto& ponta = guia.moveis[1].pos;
                for (int it = 0; it < 3; ++it) {
                    meio = raiz + normalizar(meio - raiz) * guia.comprimentos[0];
                    Vec3f delta = ponta - meio;
                    Vec3f excesso = delta - normalizar(delta) * guia.comprimentos[1];
                    meio = meio + excesso * .30f;
                    ponta = ponta - excesso * .70f;

                    for (size_t i = 0; i < guia.moveis.size(); ++i) {
                        auto& pos = guia.moveis[i].pos;
                        pos = limitarDesvio(pos, alvosMoveis[i], guia.limites[i]);
                        pos = corpo.limitar(pos, alvosMoveis[i], perfil_.guias[g].pontos[i + 1]);
                    }
                }
                for (size_t i = 0; i < guia.moveis.size(); ++i)
                    guia.moveis[i].vel = (guia.moveis[i].pos - antes[i]) * (1.f / h);
            }
        }
        for (size_t g = 0; g < perfil_.principais; ++g) {
            deformacao_.meio[g] = noEspacoDaCabeca(cabeca, guias_[g].moveis[0].pos - alvos[g][1]);
            deformacao_.ponta[g] = noEspacoDaCabeca(cabeca, guias_[g].moveis[1].pos - alvos[g][2]);
        }
        if (perfil_.quantidade > perfil_.principais)
            atualizarSecundarios(dt, cabeca, pescoco, peito, forca, clamp01(detalhe), colisores);
    }
    for (size_t g = 0; g < perfil_.principais; ++g) guias_[g].alvosAnteriores = alvos[g];
    colisoresAnteriores_ = colisores;
}

void FisicaCabelo::atualizarSecundarios(float dt, const Mat4& cabeca, const Mat4& pescoco, const Mat4& peito, Vec3f forca,
                                        float detalhe, const std::array<Vec3f, 5>& colisores) {
    const bool simular = detalhe > 0.f;
    const bool inicializar = simular && !detalhesAtivos_;
    const CorpoNaPose corpo(perfil_.colisores, colisores, cabeca, pescoco, peito);
    for (size_t g = perfil_.principais; g < perfil_.quantidade; ++g) {
        const auto& guia = perfil_.guias[g];
        const auto& v = guia.pais;
        auto& estado = secundarios_[g];
        Vec3f meio = interpolar(deformacao_.meio[v.a], deformacao_.meio[v.b], v.pesoB) * estado.escalaMeio;
        Vec3f herdada = interpolar(deformacao_.ponta[v.a], deformacao_.ponta[v.b], v.pesoB) * estado.escalaPonta;
        deformacao_.meio[g] = meio;
        deformacao_.ponta[g] = herdada;
        if (!simular) continue;

        Vec3f repouso = cabeca.ponto(guia.pontos[2]);
        auto& no = estado.ponta;
        if (inicializar) {
            no = {repouso + cabeca.vetor(herdada), {}};
            estado.repousoAnterior = repouso;
            continue;
        }

        // A dobra acompanha o movimento dos guias principais. Uma única ponta
        // em espaço de mundo mantém a inércia própria da mecha quando a cabeça gira.
        Vec3f dobra = cabeca.ponto(guia.pontos[1] + meio);
        Vec3f alvo = repouso + cabeca.vetor(meio) * .8f;
        Vec3f anterior = no.pos;
        const Vec3f velAlvo = (repouso - estado.repousoAnterior) * (1.f / dt);
        estado.repousoAnterior = repouso;
        // Euler implícito para a mola e os dois amortecimentos (interno, relativo à cabeça, e do ar),
        // em um passo por quadro: v' (1 + (c_i + c_ar) dt + k dt²) = v + (k (alvo - p) + f + c_i v_alvo) dt.
        const float amortecimento = 1.f / (1.f + (AMORTECIMENTO_INTERNO + ARRASTO_DO_AR) * dt + estado.rigidez * dt * dt);
        no.vel = (no.vel + ((alvo - no.pos) * estado.rigidez + forca + velAlvo * AMORTECIMENTO_INTERNO) * dt) * amortecimento;
        no.pos = dobra + normalizar(no.pos + no.vel * dt - dobra) * estado.comprimento;
        no.pos = limitarDesvio(no.pos, repouso, estado.limite);
        no.pos = corpo.limitar(no.pos, repouso, guia.pontos[2]);
        no.vel = (no.pos - anterior) * (1.f / dt);
        Vec3f propria = noEspacoDaCabeca(cabeca, no.pos - repouso);
        deformacao_.ponta[g] = interpolar(herdada, propria, detalhe);
    }
    detalhesAtivos_ = simular;
}

Vec3f apoiarNoCorpo(Vec3f p, const VertPele& v, const Mat4* ossos) {
    if (v.apoioFolga <= 0.f) return p;
    const Mat4& osso = ossos[v.apoioOsso];
    const Vec3f normal = normalizar(osso.vetor(v.apoioNormal));
    const Vec3f apoio = osso.ponto(v.pos - v.apoioNormal * v.apoioFolga);
    const float folga = dot(p - apoio, normal);
    const float minimo = std::min(v.apoioFolga, FOLGA_MINIMA_DO_APOIO);
    return folga < minimo ? p + normal * (minimo - folga) : p;
}


Vec3f afastarDosVolumesJustos(Vec3f p, Vec3f repouso, const ColisoresCabelo& c, const Mat4* ossos) {
    if (!c.temVolumesJustos()) return p;
    for (const VolumeNaPose& volume : volumesJustos(c, ossos[CABECA], ossos[PESCOCO], ossos[PEITO])) p = volume.limitar(p, repouso);
    return p;
}

Vec3f posicaoDaFita(const PerfilCabelo& perfil, const DeformacaoCabelo& deformacao, const VertPele& v, const Mat4* ossos) {
    // As fitas são rígidas no osso A (a cabeça); o progresso vem do comprimento da tangente.
    const float progresso = clamp01(len(v.tang) - .05f);
    const Vec3f desloc = deslocamentoDaFita(perfil, deformacao, v.pos, v.guiaCabelo[0], v.guiaCabelo[1],
                                            v.misturaGuias / 255.f, progresso);
    const Vec3f apoiado = apoiarNoCorpo(ossos[v.osso[0]].ponto(v.pos + desloc), v, ossos);
    return afastarDosVolumesJustos(apoiado, v.pos, perfil.colisores, ossos);
}

std::array<float, 32> volumesParaShader(const ColisoresCabelo& c) {
    std::array<float, 32> v{};
    if (!c.temVolumesJustos()) return v;
    const Vec3f folga{FOLGA_DOS_VOLUMES, FOLGA_DOS_VOLUMES, FOLGA_DOS_VOLUMES};
    const Vec3f vetores[8] = {c.orelha, c.raiosOrelha + folga, c.pescocoA, c.pescocoB, c.raiosPescocoJusto + folga,
                              c.troncoA, c.troncoB, c.raiosTronco + folga};
    for (int i = 0; i < 8; ++i) {
        v[i * 4] = vetores[i].x;
        v[i * 4 + 1] = vetores[i].y;
        v[i * 4 + 2] = vetores[i].z;
    }
    v[3] = 1.f;  // há volumes
    return v;
}
