#include "core/quality.h"

Efeitos efeitosDe(Nivel n) {
    switch (n) {
        case Nivel::Alto: return {true, true, true, 2048, 1.f, 450.f};
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

Nivel lerConfig(const std::string& caminho, Nivel padrao) {
    std::ifstream f(caminho);
    std::string linha;
    while (std::getline(f, linha)) {
        if (linha.rfind("qualidade=", 0) != 0) continue;
        std::string v = minusculo(linha.substr(10));
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        if (v == "baixo") return Nivel::Baixo;
        if (v == "medio") return Nivel::Medio;
        if (v == "alto") return Nivel::Alto;
    }
    return padrao;
}

void salvarConfig(const std::string& caminho, Nivel n) {
    std::ofstream f(caminho);
    f << "qualidade=" << nomeNivel(n) << "\n";
}
