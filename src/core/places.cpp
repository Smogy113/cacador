#include "core/places.h"
#include <cmath>
#include <fstream>
#include <sstream>

namespace {
constexpr float LIMITE = 700.f;  // area jogavel

bool tipoDe(const std::string& s, TipoLugar& t) {
    static const struct { const char* n; TipoLugar t; } TAB[] = {
        {"cidade", TipoLugar::Cidade},       {"vilarejo", TipoLugar::Vilarejo},
        {"fazenda", TipoLugar::Fazenda},     {"castelo", TipoLugar::Castelo},
        {"acampamento", TipoLugar::Acampamento}, {"mina", TipoLugar::Mina},
        {"cemiterio", TipoLugar::Cemiterio}, {"ponte", TipoLugar::Ponte},
        {"floresta", TipoLugar::Floresta},
    };
    for (auto& e : TAB)
        if (s == e.n) { t = e.t; return true; }
    return false;
}
}  // namespace

const Lugar* Mapa::achar(const std::string& nome) const {
    for (auto& l : lugares)
        if (l.nome == nome) return &l;
    return nullptr;
}

Mapa lerMapa(const std::string& texto) {
    Mapa m;
    std::istringstream in(texto);
    std::string linha;
    int n = 0;
    std::vector<std::pair<int, Estrada>> pendentes;
    while (std::getline(in, linha)) {
        n++;
        auto hash = linha.find('#');
        if (hash != std::string::npos) linha.resize(hash);
        std::istringstream ls(linha);
        std::string cmd;
        if (!(ls >> cmd)) continue;
        if (cmd == "lugar") {
            std::string tipo, nome;
            Lugar l{};
            if (!(ls >> tipo >> nome >> l.x >> l.z >> l.rot >> l.raio)) {
                m.erros.push_back({n, "esperado: lugar <tipo> <nome> <x> <z> <rot> <raio>"});
                continue;
            }
            if (!tipoDe(tipo, l.tipo)) {
                m.erros.push_back({n, "tipo de lugar desconhecido: " + tipo});
                continue;
            }
            if (std::abs(l.x) > LIMITE || std::abs(l.z) > LIMITE) {
                m.erros.push_back({n, "lugar fora do mundo: " + nome});
                continue;
            }
            if (l.raio <= 0) {
                m.erros.push_back({n, "raio precisa ser positivo: " + nome});
                continue;
            }
            l.nome = nome;
            m.lugares.push_back(l);
        } else if (cmd == "estrada") {
            Estrada e;
            if (!(ls >> e.de >> e.para)) {
                m.erros.push_back({n, "esperado: estrada <lugarA> <lugarB>"});
                continue;
            }
            pendentes.push_back({n, e});
        } else {
            m.erros.push_back({n, "comando desconhecido: " + cmd});
        }
    }
    for (auto& [linhaN, e] : pendentes) {
        if (!m.achar(e.de) || !m.achar(e.para))
            m.erros.push_back({linhaN, "estrada liga lugar inexistente: " + e.de + " -> " + e.para});
        else
            m.estradas.push_back(e);
    }
    return m;
}

Mapa carregarMapa(const std::string& caminho) {
    std::ifstream f(caminho);
    if (!f) {
        Mapa m;
        m.erros.push_back({0, "arquivo não encontrado: " + caminho});
        return m;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return lerMapa(ss.str());
}
