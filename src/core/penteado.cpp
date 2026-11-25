#include "core/penteado.h"
#include <cctype>

const char* nomePenteado(Penteado p) {
    switch (p) {
    case Penteado::Ondulado: return "ondulado";
    case Penteado::Longo: return "longo";
    case Penteado::Sem: break;
    }
    return "sem";
}

const char* rotuloPenteado(Penteado p) {
    switch (p) {
    case Penteado::Ondulado: return "Ondulado";
    case Penteado::Longo: return "Longo";
    case Penteado::Sem: break;
    }
    return "Sem cabelo";
}

bool lerPenteado(const std::string& nome, Penteado& p) {
    std::string minusculo;
    for (char c : nome) minusculo += char(std::tolower((unsigned char)c));
    for (Penteado candidato : PENTEADOS)
        if (minusculo == nomePenteado(candidato)) {
            p = candidato;
            return true;
        }
    return false;
}
