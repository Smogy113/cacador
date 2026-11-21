#pragma once
#include <string>
#include <vector>

enum class TipoLugar { Cidade, Vilarejo, Fazenda, Castelo, Acampamento, Mina, Cemiterio, Ponte, Floresta };

struct Lugar {
    TipoLugar tipo;
    std::string nome;
    float x, z, rot, raio;
};
struct Estrada {
    std::string de, para;
};
struct ErroMapa {
    int linha;
    std::string msg;
};
struct Mapa {
    std::vector<Lugar> lugares;
    std::vector<Estrada> estradas;
    std::vector<ErroMapa> erros;
    const Lugar* achar(const std::string& nome) const;
};

Mapa lerMapa(const std::string& texto);
Mapa carregarMapa(const std::string& caminho);
