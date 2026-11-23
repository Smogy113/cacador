#include "core/quality.h"

Efeitos efeitosDe(Nivel n) {
    switch (n) {
        case Nivel::Alto: return {true, true, true, 2048, 1.f, 450.f, true};
        case Nivel::Medio: return {true, false, false, 1024, 1.f, 350.f};
        default: return {false, false, false, 0, 1.f, 250.f};
    }
}

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

static std::string minusculo(std::string s) {
    for (auto& c : s) c = char(std::tolower((unsigned char)c));
    return s;
}

Nivel sugerirNivel(const std::string& r) {
    std::string s = minusculo(r);
    auto tem = [&](const char* p) { return s.find(p) != std::string::npos; };
    // AMD integrada (APU de notebook): nao e placa dedicada
    bool radeonR = false;  // "Radeon R2..R9" (integradas), mas nao "Radeon RX"
    for (const char* p : {"radeon r", "radeon(tm) r"}) {
        size_t i = s.find(p);
        if (i != std::string::npos && i + std::strlen(p) < s.size() && std::isdigit((unsigned char)s[i + std::strlen(p)])) radeonR = true;
    }
    if (tem("radeon(tm) graphics") || tem("vega") || radeonR) return Nivel::Medio;
    if (tem("nvidia") || tem("geforce") || tem("radeon") || tem("amd") || tem("arc(tm)") || tem(" arc ")) return Nivel::Alto;
    if (tem("intel") || tem("llvmpipe") || tem("softpipe") || tem("swiftshader") || tem("mali") || tem("adreno") || tem("powervr") ||
        tem("mesa"))
        return Nivel::Baixo;
    return Nivel::Medio;
}

const char* nomeNivel(Nivel n) { return n == Nivel::Alto ? "alto" : (n == Nivel::Medio ? "medio" : "baixo"); }

bool temConfig(const std::string& caminho) {
    std::ifstream f(caminho);
    return bool(f);
}

ConfigJogo lerConfig(const std::string& caminho, ConfigJogo config) {
    std::ifstream f(caminho);
    std::string linha;
    while (std::getline(f, linha)) {
        size_t separador = linha.find('=');
        if (separador == std::string::npos) continue;
        std::string chave = linha.substr(0, separador);
        std::string valor = minusculo(linha.substr(separador + 1));
        while (!valor.empty() && std::isspace((unsigned char)valor.back())) valor.pop_back();
        if (chave == "qualidade") {
            if (valor == "baixo") config.nivel = Nivel::Baixo;
            if (valor == "medio") config.nivel = Nivel::Medio;
            if (valor == "alto") config.nivel = Nivel::Alto;
        } else if (chave == "cabelo") {
            if (valor == "sem") config.cabeloOndulado = false;
            if (valor == "ondulado") config.cabeloOndulado = true;
        }
    }
    return config;
}

void salvarConfig(const std::string& caminho, ConfigJogo config) {
    std::ofstream f(caminho);
    f << "qualidade=" << nomeNivel(config.nivel) << "\n"
      << "cabelo=" << (config.cabeloOndulado ? "ondulado" : "sem") << "\n";
}
