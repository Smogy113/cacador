// Cabelo longo da base humana: risca levemente fora do centro, mechas que deitam sobre o crânio e
// caem até os ombros, passando por cima das orelhas.
//
// Cada mecha tem duas partes:
//   1. arco: da raiz até o ponto onde o cabelo se solta da cabeça, deitado sobre o crânio
//      (flexibilidade quase zero: acompanha a cabeça sem balançar);
//   2. parte solta: pendurada a partir desse ponto. A forma em repouso sai de um "caimento" feito
//      na montagem: a corrente de pontos desce com a gravidade, sem esticar, e é empurrada para fora
//      do corpo (orelhas, pescoço, ombros). Assim a ponta assenta no ombro em vez de atravessá-lo.
// Medidas no espaço de 1,80 m da base humana (ver cabelo_cranio.h).
#include <algorithm>
#include <cmath>
#include "core/cabelo_cranio.h"

namespace loft {
using namespace cranio;

namespace {

// A parte solta começa um pouco acima deste limiar: o arco deitado fica preso à cabeça e só o
// que pende entra na física.
constexpr float LIMIAR_FIXACAO_LONGO = .09f;
constexpr float FLEX_FIM_DO_ARCO = .08f;
constexpr float FOLGA_DO_CORPO = .003f;  // m, entre a pele e o eixo das mechas
constexpr float PASSO_DA_CORRENTE = .02f;  // m, entre pontos da parte solta

// Uma mecha antes do caimento.
struct Mecha {
    Vec3f raiz, soltura;     // direções a partir do CENTRO: começo e fim do arco sobre o crânio
    float camada = 0.f;      // altura extra sobre o crânio (camadas de cima cobrem as de baixo)
    float yPonta = 1.45f;    // altura aproximada da ponta depois de cair (m, espaço de 1,80 m)
    float largura = .017f;
    float fase = 0.f;        // variação própria (ondas, tom)
};

Vec3f misturar(Vec3f a, Vec3f b, float t) { return a + (b - a) * t; }

// Corpo em que o caimento esbarra, convertido para o espaço de 1,80 m.
struct CorpoEmRepouso {
    const CorpoDoCabelo& corpo;
    float escala;
    Vec3f paraFora(Vec3f p, float folga) const { return corpo.paraFora(p * escala, folga) * (1.f / escala); }
};

std::vector<PontoCabelo> arcoSobreOCranio(const Cranio& cranio, const Mecha& m) {
    std::vector<PontoCabelo> pontos;
    constexpr int AMOSTRAS = 10;
    for (int i = 0; i <= AMOSTRAS; ++i) {
        const float t = float(i) / AMOSTRAS;
        // O arco sobe um pouco no meio para contornar a curva do crânio em vez de cortá-la.
        const Vec3f d = normalizar(m.raiz + (m.soltura - m.raiz) * t + Vec3f{0, .30f * std::sin(PI_F * t), 0});
        const float volume = volumeCoroa(d) + m.camada * smoothstep(0.f, .35f, t);
        pontos.push_back({cranio.superficie(d, volume), FLEX_FIM_DO_ARCO * t});
    }
    return pontos;
}

// Parte solta: corrente inextensível que cai a partir do fim do arco e assenta no corpo.
void cairAteAssentar(std::vector<PontoCabelo>& pontos, const Mecha& m, const CorpoEmRepouso& corpo) {
    const Vec3f soltura = pontos.back().p;
    const float comprimento = std::max(.04f, (soltura.y - m.yPonta) * 1.12f);
    const int elos = std::max(4, int(std::ceil(comprimento / PASSO_DA_CORRENTE)));
    const float elo = comprimento / elos;

    // Começa caindo para fora e para baixo, com uma onda leve que dá personalidade a cada mecha.
    const Vec3f fora = normalizar(Vec3f{soltura.x - CENTRO.x, 0, soltura.z - CENTRO.z});
    const Vec3f lado = cross(fora, {0, 1, 0});
    const size_t primeiro = pontos.size();
    for (int i = 1; i <= elos; ++i) {
        const float t = float(i) / elos;
        const Vec3f onda = lado * (.006f * std::sin(t * 7.f + m.fase) * t);
        pontos.push_back({soltura + (fora * .35f + Vec3f{0, -1, -.35f}) * (elo * i) + onda, lerp(.10f, .92f, t)});
    }

    // Gravidade inclinada para trás: nas laterais, o cabelo assenta atrás dos ombros, não na frente
    // deles (na frente, ao balançar, a mecha escorregaria para o canto entre o ombro e o pescoço).
    const Vec3f gravidade = normalizar({0, -1, -.55f}) * (elo * .25f);
    auto esticarAPartirDaRaiz = [&] {
        for (size_t i = primeiro; i < pontos.size(); ++i) {
            const Vec3f anterior = pontos[i - 1].p;
            pontos[i].p = anterior + normalizar(pontos[i].p - anterior) * elo;
        }
    };
    constexpr int ITERACOES = 60;
    for (int it = 0; it < ITERACOES; ++it) {
        for (size_t i = primeiro; i < pontos.size(); ++i) pontos[i].p = pontos[i].p + gravidade;
        // Rigidez de flexão: cada ponto puxa um pouco para a média dos vizinhos (mecha sem quinas).
        for (size_t i = primeiro; i + 1 < pontos.size(); ++i)
            pontos[i].p = misturar(pontos[i].p, (pontos[i - 1].p + pontos[i + 1].p) * .5f, .25f);
        for (size_t i = primeiro; i < pontos.size(); ++i) pontos[i].p = corpo.paraFora(pontos[i].p, FOLGA_DO_CORPO);
        esticarAPartirDaRaiz();
    }
    // Por último, o corpo vence o comprimento: um milímetro esticado não aparece, um ponto dentro sim.
    for (size_t i = primeiro; i < pontos.size(); ++i) pontos[i].p = corpo.paraFora(pontos[i].p, FOLGA_DO_CORPO);
}

}  // namespace

void cabeloLongo(MalhaPele& m, float escala, Rgb cor) {
    const Cranio cranio(m, escala);
    // O corpo é lido antes de o penteado entrar na malha (só a pele: cabeça, orelhas, pescoço, ombros).
    const CorpoDoCabelo corpo = corpoDaBaseHumana(m, escala);
    const CorpoEmRepouso corpoEmRepouso{corpo, escala};
    couroCabeludo(m, cranio, corpo, escala, cor);
    ConstrutorCabelo cabelo(m, CENTRO, escala, 1.f, LIMIAR_FIXACAO_LONGO);
    cabelo.respeitarCorpo(corpo, FOLGA_DO_CORPO);

    auto adicionar = [&](const Mecha& mecha, float tom, bool comFios) {
        std::vector<PontoCabelo> pontos = arcoSobreOCranio(cranio, mecha);
        cairAteAssentar(pontos, mecha, corpoEmRepouso);
        cabelo.fita(pontos, mecha.largura, cor * tom, .9f);
        if (!comFios) return;
        // Fios soltos por cima da parte solta: quebram a borda lisa das fitas largas.
        std::vector<PontoCabelo> fios;
        const size_t inicio = pontos.size() / 2;
        const Vec3f lado = normalizar(cross(pontos.back().p - pontos[inicio].p, pontos[inicio].p - CENTRO));
        for (size_t i = inicio - 2; i < pontos.size(); ++i) {
            const float t = float(i - inicio + 2) / float(pontos.size() - inicio + 1);
            fios.push_back({pontos[i].p + lado * (mecha.largura * .45f * std::sin(mecha.fase + t * 2.f)), pontos[i].flex});
        }
        cabelo.fita(fios, .0012f, cor * (tom + .12f), .95f, true);
    };

    const float risca = .012f;  // a risca fica um pouco à direita do personagem (+x)
    for (float lado : {-1.f, 1.f}) {
        // Camada de baixo, na nuca: preenche o vão sob as mechas de cima.
        for (int k = 0; k < 6; ++k) {
            const float u = (k + .5f) / 6.f, giro = lerp(112.f, 176.f, u);
            Mecha mecha;
            mecha.raiz = direcao(lado * giro, linhaDoCabelo(giro) + 14.f);
            mecha.soltura = direcao(lado * giro, linhaDoCabelo(giro) + 2.f);
            mecha.yPonta = lerp(1.44f, 1.41f, u);
            mecha.largura = .019f;
            mecha.fase = 1.7f * k + lado;
            adicionar(mecha, .78f + .06f * std::cos(mecha.fase), false);
        }
        // Têmporas: descem por cima das orelhas, sob a camada de cima.
        for (int k = 0; k < 4; ++k) {
            const float u = (k + .5f) / 4.f, giro = lerp(64.f, 104.f, u);
            Mecha mecha;
            mecha.raiz = direcao(lado * giro, 36.f);
            mecha.soltura = direcao(lado * (giro + 6.f), 4.f);
            mecha.camada = .002f;
            mecha.yPonta = lerp(1.50f, 1.47f, u);
            mecha.largura = .018f;
            mecha.fase = 2.3f * k + lado * .4f;
            adicionar(mecha, .84f + .06f * std::cos(mecha.fase), false);
        }
        // Camada de cima: nasce na risca, deita sobre o crânio e cai pelos lados e pelas costas.
        constexpr int MECHAS_DE_CIMA = 14;
        for (int k = 0; k < MECHAS_DE_CIMA; ++k) {
            const float u = (k + .5f) / MECHAS_DE_CIMA;
            const float fase = 2.37f * k + lado * .73f;
            // Ângulo na risca, da testa para o cocuruto. A primeira mecha nasce atrás da linha do cabelo
            // e desce íngreme até a têmpora: emoldura o rosto sem deixar uma borda reta na testa.
            const float raiz = lerp(47.f, 138.f, u);
            const float giro = lerp(66.f, 177.f, u) + 3.f * std::sin(fase);
            Mecha mecha;
            mecha.raiz = normalizar({risca + lado * .045f, std::sin(raiz * RAD), std::cos(raiz * RAD)});
            mecha.soltura = direcao(lado * giro, lerp(10.f, -18.f, u));
            mecha.camada = .005f;
            mecha.yPonta = lerp(1.50f, 1.40f, smoothstep(.05f, .85f, u)) + .012f * std::cos(fase);
            mecha.largura = .018f;
            mecha.fase = fase;
            adicionar(mecha, .92f + .09f * std::cos(fase), k % 2 == 0);
        }
    }
    cabelo.finalizar();
}

}  // namespace loft
