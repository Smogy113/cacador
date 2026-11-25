#pragma once
#include <string>

// Penteado do protagonista (base humana). O nome curto é o que vai no config.txt e no --cabelo.
// Para criar um penteado novo: acrescente aqui, em penteado.cpp e no switch de corpoHumanoBase.
enum class Penteado { Sem, Ondulado, Longo };
inline constexpr Penteado PENTEADOS[] = {Penteado::Sem, Penteado::Ondulado, Penteado::Longo};
inline constexpr int N_PENTEADOS = 3;

const char* nomePenteado(Penteado p);    // "sem", "ondulado", "longo"
const char* rotuloPenteado(Penteado p);  // "Sem cabelo", "Ondulado", "Longo" (menu)
// Lê o nome curto (sem diferenciar maiúsculas). Devolve false e não mexe em `p` se o nome não existe.
bool lerPenteado(const std::string& nome, Penteado& p);
