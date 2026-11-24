#include "core/circuito.h"
#include <algorithm>
#include <cstdio>

const char* nome(Entalhe e) {
    switch (e) {
    case Entalhe::Espiral: return "Espiral";
    case Entalhe::Grade: return "Grade";
    case Entalhe::Onda: return "Onda";
    }
    return "?";
}

const char* nome(Pasta p) {
    switch (p) {
    case Pasta::PolvoraFina: return "Pólvora Fina";
    case Pasta::ResinaECera: return "Resina e Cera";
    case Pasta::EnxofreESal: return "Enxofre e Sal";
    case Pasta::PoDePrata: return "Pó de Prata";
    }
    return "?";
}

const char* nome(Doador d) {
    switch (d) {
    case Doador::Pena: return "Pena";
    case Doador::PedraPesada: return "Pedra Pesada";
    case Doador::CarvaoEmBrasa: return "Carvão em Brasa";
    }
    return "?";
}

const char* nome(Propriedade p) {
    switch (p) {
    case Propriedade::Velocidade: return "Velocidade";
    case Propriedade::Arrasto: return "Arrasto";
    case Propriedade::Peso: return "Peso";
    case Propriedade::Levante: return "Levante";
    case Propriedade::Calor: return "Calor";
    case Propriedade::Frio: return "Frio";
    }
    return "?";
}

Doador doadorDaReceita(Entalhe e) {
    switch (e) {
    case Entalhe::Espiral: return Doador::Pena;
    case Entalhe::Grade: return Doador::PedraPesada;
    case Entalhe::Onda: return Doador::CarvaoEmBrasa;
    }
    return Doador::Pena;
}

std::string nomeCombinacao(const Circuito& c) {
    return std::string(nome(c.entalhe)) + " + " + nome(c.pasta) + " + " + nome(c.doador);
}

// ---------------------------------------------------------------- efeito

EfeitoCircuito resolverEfeito(const Circuito& c, const ParametrosCircuito& p) {
    EfeitoCircuito e;
    const bool invertido = c.pasta == Pasta::PoDePrata;
    switch (c.entalhe) {
    case Entalhe::Espiral: e.propriedade = invertido ? Propriedade::Arrasto : Propriedade::Velocidade; break;
    case Entalhe::Grade: e.propriedade = invertido ? Propriedade::Levante : Propriedade::Peso; break;
    case Entalhe::Onda: e.propriedade = invertido ? Propriedade::Frio : Propriedade::Calor; break;
    }
    switch (c.pasta) {
    case Pasta::PolvoraFina: e.liberacao = Liberacao::Concentrada; break;
    case Pasta::ResinaECera: e.liberacao = Liberacao::Persistente; break;
    case Pasta::EnxofreESal: e.liberacao = Liberacao::Area; break;
    case Pasta::PoDePrata: e.liberacao = Liberacao::Invertida; break;
    }
    // voo
    e.velocidade = p.velProjetil;
    e.gravidadeMult = 1.f;
    if (e.propriedade == Propriedade::Velocidade) {
        e.velocidade = p.velEspiral;
        e.gravidadeMult = 0.f;
    } else if (e.propriedade == Propriedade::Arrasto) {
        e.velocidade = p.velArrasto;
        e.gravidadeMult = p.gravidadeArrasto;
    }
    e.ricochetes = e.liberacao == Liberacao::Persistente ? std::max(0, p.ricochetes) : 0;
    // impacto
    e.raio = e.liberacao == Liberacao::Concentrada ? p.raioConcentrado : (e.liberacao == Liberacao::Area ? p.raioEnxofre : p.raioPadrao);
    e.intensidade = e.liberacao == Liberacao::Concentrada ? p.intensidadeConcentrada : 1.f;
    const bool elemental = e.propriedade == Propriedade::Calor || e.propriedade == Propriedade::Frio;
    if (elemental && e.liberacao != Liberacao::Concentrada)
        e.duracaoZona = p.duracaoZona * (e.liberacao == Liberacao::Persistente ? p.multPersistente : 1.f);
    return e;
}

EfeitoCircuito efeitoChumbo(const ParametrosCircuito& p) {
    EfeitoCircuito e;
    e.velocidade = p.velProjetil;
    e.gravidadeMult = 1.f;
    e.raio = 0.f;
    e.intensidade = 0.f;
    return e;
}

std::string descreverEfeito(const Circuito& c, const ParametrosCircuito& p) {
    if (!c.valido()) return std::string("Receita inválida: ") + nome(c.entalhe) + " exige " + nome(doadorDaReceita(c.entalhe));
    const EfeitoCircuito e = resolverEfeito(c, p);
    std::string s;
    switch (e.propriedade) {
    case Propriedade::Velocidade: s = "Voa reto e rápido, sem cair; dano extra no impacto"; break;
    case Propriedade::Arrasto: s = "Voo lento em arco curto; dano extra no impacto"; break;
    case Propriedade::Peso: s = "Onda de pressão que empurra e derruba"; break;
    case Propriedade::Levante: s = "Impulso que arremessa os alvos para cima"; break;
    case Propriedade::Calor: s = e.duracaoZona > 0 ? "Zona de fogo no chão" : "Labareda no ponto do impacto"; break;
    case Propriedade::Frio: s = e.duracaoZona > 0 ? "Zona congelante no chão" : "Congela o alvo atingido"; break;
    }
    char buf[96];
    std::snprintf(buf, sizeof buf, ", raio de %.1f m", e.raio);
    s += buf;
    if (e.duracaoZona > 0) {
        std::snprintf(buf, sizeof buf, ", por %.0f s", e.duracaoZona);
        s += buf;
    }
    if (e.ricochetes > 0) {
        std::snprintf(buf, sizeof buf, "; ricocheteia até %d vezes", e.ricochetes);
        s += buf;
    }
    return s;
}

// ---------------------------------------------------------------- pavio

const char* nome(EstadoPavio e) {
    switch (e) {
    case EstadoPavio::Seco: return "Seco";
    case EstadoPavio::Umido: return "Úmido";
    case EstadoPavio::Molhado: return "Molhado";
    case EstadoPavio::BloqueadoFerro: return "Bloqueado por Ferro Frio";
    }
    return "?";
}

void Pavio::atualizar(float dt, bool naAgua, bool noFerro, float tempoSecagem) {
    umidade = naAgua ? tempoSecagem : std::max(0.f, umidade - dt);
    encharcado = naAgua;
    ferro = noFerro;
}

EstadoPavio Pavio::estado() const {
    if (ferro) return EstadoPavio::BloqueadoFerro;
    if (encharcado) return EstadoPavio::Molhado;
    if (umidade > 0.f) return EstadoPavio::Umido;
    return EstadoPavio::Seco;
}

// ---------------------------------------------------------------- arsenal

const char* nome(Preparo p) {
    switch (p) {
    case Preparo::Cuidadoso: return "Cuidadoso";
    case Preparo::Rapido: return "Rápido / Imperfeito";
    }
    return "?";
}

const char* nome(ResultadoPreparo r) {
    switch (r) {
    case ResultadoPreparo::Ok: return "Bala gravada";
    case ResultadoPreparo::SlotInvalido: return "Slot inválido";
    case ResultadoPreparo::ReceitaInvalida: return "Receita inválida";
    case ResultadoPreparo::SemDoador: return "Sem objeto doador no estoque";
    case ResultadoPreparo::EntalheDesconhecido: return "Entalhe desconhecido: inspecione pistas para aprender";
    case ResultadoPreparo::PastaDesconhecida: return "Pasta desconhecida: inspecione pistas para aprender";
    }
    return "?";
}

Arsenal::Arsenal(const ParametrosCircuito& p) : slots_(size_t(std::max(1, p.slotsPreparados))), estoqueInicial_(p.estoqueInicialDoador) {
    reporEstoque();
}

void Arsenal::reporEstoque() {
    for (int& e : estoque_) e = estoqueInicial_;
}

ResultadoPreparo Arsenal::preparar(int i, const Circuito& c, Preparo preparo) {
    if (i < 0 || i >= slots()) return ResultadoPreparo::SlotInvalido;
    if (!c.valido()) return ResultadoPreparo::ReceitaInvalida;
    SlotMunicao& s = slots_[size_t(i)];
    const bool devolveIgual = s.cheio && s.circuito.doador == c.doador;
    if (estoque_[int(c.doador)] + (devolveIgual ? 1 : 0) <= 0) return ResultadoPreparo::SemDoador;
    if (s.cheio) estoque_[int(s.circuito.doador)]++;  // regravar devolve o doador antigo
    estoque_[int(c.doador)]--;
    s.cheio = true;
    s.circuito = c;
    s.preparo = preparo;
    return ResultadoPreparo::Ok;
}

void Arsenal::esvaziar(int i) {
    if (i < 0 || i >= slots() || !slots_[size_t(i)].cheio) return;
    estoque_[int(slots_[size_t(i)].circuito.doador)]++;
    slots_[size_t(i)].cheio = false;
}

void Arsenal::selecionar(int i) {
    if (i >= -1 && i < slots()) sel_ = i;
}

void Arsenal::proximo() {
    for (int i = sel_ + 1; i < slots(); i++)
        if (slots_[size_t(i)].cheio) {
            sel_ = i;
            return;
        }
    sel_ = -1;
}

Circuito Arsenal::consumirNaVez() {
    Circuito c = slots_[size_t(std::max(0, sel_))].circuito;
    if (preparadaNaVez()) slots_[size_t(sel_)].cheio = false;
    sel_ = -1;
    return c;
}
