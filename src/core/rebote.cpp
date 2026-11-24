#include "core/rebote.h"
#include <algorithm>
#include <cmath>

const char* nome(Gatilho g) {
    switch (g) {
    case Gatilho::Comum: return "Comum";
    case Gatilho::Acende: return "Acende";
    case Gatilho::Falha: return "Falha";
    case Gatilho::Rebote: return "Rebote";
    case Gatilho::ApagaNoFerro: return "Apaga no Ferro Frio";
    }
    return "?";
}

float chanceRebote(const SlotMunicao& bala, const Pavio& pavio, const ParametrosCircuito& p) {
    if (!bala.cheio) return 0.f;
    const float imperfeita = bala.imperfeita() ? p.chanceReboteRapido : 0.f;
    float umido = 0.f;
    if (pavio.estado() == EstadoPavio::Umido) {
        const float f = std::clamp(pavio.fracaoUmidade(p.tempoSecagem), 0.f, 1.f);
        umido = p.chanceReboteUmidoMin + (p.chanceReboteUmidoMax - p.chanceReboteUmidoMin) * f;
    }
    // dois riscos independentes: falha se qualquer um deles falhar
    return std::clamp(1.f - (1.f - imperfeita) * (1.f - umido), 0.f, 1.f);
}

DecisaoGatilho decidirGatilho(const SlotMunicao* bala, const Pavio& pavio, bool forcando, float sorteio01, const ParametrosCircuito& p) {
    DecisaoGatilho d;
    if (!bala || !bala->cheio) return d;  // Comum
    switch (pavio.estado()) {
    case EstadoPavio::BloqueadoFerro: d.gatilho = Gatilho::ApagaNoFerro; return d;
    case EstadoPavio::Molhado: d.gatilho = Gatilho::Falha; return d;
    case EstadoPavio::Umido:
        if (!forcando) {
            d.gatilho = Gatilho::Falha;
            d.pedeForcar = true;
            d.chanceRebote = chanceRebote(*bala, pavio, p);  // o aviso mostra o risco de forçar
            return d;
        }
        break;
    case EstadoPavio::Seco: break;
    }
    d.chanceRebote = chanceRebote(*bala, pavio, p);
    d.gatilho = sorteio01 < d.chanceRebote ? Gatilho::Rebote : Gatilho::Acende;
    return d;
}

void TremorMira::disparar(float dur, float rec, float amp) {
    duracao = restante = std::max(0.01f, dur);
    recuo = rec;
    amplitude = amp;
}

void TremorMira::atualizar(float dt) { restante = std::max(0.f, restante - dt); }

float TremorMira::forca() const { return duracao > 0.f ? std::clamp(restante / duracao, 0.f, 1.f) : 0.f; }

void TremorMira::deslocamento(float& dYaw, float& dPitch) const {
    const float k = forca(), envelope = k * k;  // cai rápido no começo e assenta devagar
    const float t = duracao - restante;
    dPitch = envelope * (recuo + amplitude * std::sin(t * 23.f));
    dYaw = envelope * amplitude * std::sin(t * 17.f + 1.3f);
}
