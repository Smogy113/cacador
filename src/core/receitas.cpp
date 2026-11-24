#include "core/receitas.h"
#include <cmath>

Descoberta Descoberta::de(Entalhe e) {
    Descoberta d;
    d.tipo = Tipo::Entalhe;
    d.entalhe = e;
    return d;
}

Descoberta Descoberta::de(Pasta p) {
    Descoberta d;
    d.tipo = Tipo::Pasta;
    d.pasta = p;
    return d;
}

std::string nome(const Descoberta& d) {
    return d.tipo == Descoberta::Tipo::Entalhe ? std::string("entalhe ") + nome(d.entalhe) : std::string("pasta ") + nome(d.pasta);
}

bool ReceitasConhecidas::conhece(const Descoberta& d) const {
    return d.tipo == Descoberta::Tipo::Entalhe ? conhece(d.entalhe) : conhece(d.pasta);
}

bool ReceitasConhecidas::aprender(const Descoberta& d) {
    if (conhece(d)) return false;
    if (d.tipo == Descoberta::Tipo::Entalhe) entalhes_ = uint8_t(entalhes_ | (1u << int(d.entalhe)));
    else pastas_ = uint8_t(pastas_ | (1u << int(d.pasta)));
    return true;
}

void ReceitasConhecidas::aprenderTudo() {
    entalhes_ = uint8_t((1u << N_ENTALHES) - 1u);
    pastas_ = uint8_t((1u << N_PASTAS) - 1u);
}

int ReceitasConhecidas::entalhesConhecidos() const {
    int n = 0;
    for (int i = 0; i < N_ENTALHES; i++) n += conhece(Entalhe(i));
    return n;
}

int ReceitasConhecidas::pastasConhecidas() const {
    int n = 0;
    for (int i = 0; i < N_PASTAS; i++) n += conhece(Pasta(i));
    return n;
}

int pistaPerto(const std::vector<PistaEntalhada>& pistas, Vec3f q, float raio) {
    int melhor = -1;
    float md = raio;
    for (size_t i = 0; i < pistas.size(); i++) {
        if (pistas[i].inspecionada) continue;
        float d = std::hypot(pistas[i].pos.x - q.x, pistas[i].pos.z - q.z);
        if (d <= md) {
            md = d;
            melhor = int(i);
        }
    }
    return melhor;
}

bool inspecionar(PistaEntalhada& pista, ReceitasConhecidas& receitas) {
    if (pista.inspecionada) return false;
    pista.inspecionada = true;
    return receitas.aprender(pista.ensina);
}

ResultadoPreparo gravar(Arsenal& arsenal, const ReceitasConhecidas& receitas, int slot, const Circuito& c, Preparo preparo) {
    if (!receitas.conhece(c.entalhe)) return ResultadoPreparo::EntalheDesconhecido;
    if (!receitas.conhece(c.pasta)) return ResultadoPreparo::PastaDesconhecida;
    return arsenal.preparar(slot, c, preparo);
}
