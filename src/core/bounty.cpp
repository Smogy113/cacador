#include "core/bounty.h"
#include <algorithm>
#include <set>
#include "core/noise.h"

namespace {
const char* NOMES[] = {"Aldric", "Bento", "Cassio", "Dorvan", "Elric", "Fausto", "Gaspar", "Hilda", "Ivo", "Joana",
                       "Leonel", "Mirela", "Nuno", "Otavio", "Petra", "Quirino", "Rodrigo", "Sancha", "Tomas", "Ursula",
                       "Vasco", "Bartolo", "Ines", "Gomes", "Tereza", "Ademar", "Brites", "Lourenco"};
const char* SOBRENOMES[] = {"Ferraz", "do Vale", "Corvo", "Pedreira", "Malhado", "Sombrio", "Torres", "Lobato",
                            "da Brenha", "Ruivo", "Carrasco", "Moura", "Valente", "Serra", "Cardoso", "Negrao"};
const char* APELIDOS[] = {"o Manco", "Mao de Ferro", "a Raposa", "o Silencioso", "Olho Torto", "o Carniceiro",
                          "Dedo Leve", "a Viuva", "o Vermelho", "Dente de Ouro", "o Coveiro", "Cara de Pedra",
                          "o Corvo", "a Sombra", "Faca Cega", "o Fantasma"};
struct Crime {
    const char* texto;
    int base;
};
const Crime CRIMES[] = {
    {"Roubo de cavalos", 40},       {"Assalto a carruagem", 70}, {"Contrabando de sal", 35},
    {"Incendio de celeiro", 55},    {"Fuga da cadeia", 50},      {"Assassinato de guarda", 150},
    {"Saque ao templo", 90},        {"Falsificar moedas", 80},   {"Chefe de bando", 220},
    {"Emboscada na estrada", 110},  {"Roubo de gado", 45},       {"Sequestro de mercador", 160},
};
template <class T, size_t N>
constexpr size_t tam(T (&)[N]) { return N; }
}  // namespace

std::string nomeLegivel(const std::string& n) {
    std::string r = n;
    for (auto& c : r)
        if (c == '_') c = ' ';
    return r;
}

std::vector<Cartaz> gerarCartazes(uint32_t semente, int quantidade, const Mapa& mapa) {
    std::vector<Cartaz> res;
    std::set<std::string> usados;
    uint32_t rng = hash32(semente * 31u + 7u);
    std::vector<std::string> lugares;
    for (auto& l : mapa.lugares)
        if (l.tipo != TipoLugar::Cidade) lugares.push_back(nomeLegivel(l.nome));  // procurados se escondem fora da cidade
    if (lugares.empty())
        for (auto& l : mapa.lugares) lugares.push_back(nomeLegivel(l.nome));
    if (lugares.empty()) lugares.push_back("Paradeiro desconhecido");
    int tentativas = 0;
    while (int(res.size()) < quantidade && tentativas++ < 10000) {
        Cartaz c;
        std::string nome = std::string(NOMES[size_t(aleatorio01(rng) * tam(NOMES))]) + " " + SOBRENOMES[size_t(aleatorio01(rng) * tam(SOBRENOMES))];
        if (!usados.insert(nome).second) continue;
        c.nome = nome;
        c.apelido = APELIDOS[size_t(aleatorio01(rng) * tam(APELIDOS))];
        const Crime& cr = CRIMES[size_t(aleatorio01(rng) * tam(CRIMES))];
        c.crime = cr.texto;
        int valor = int(cr.base * (0.8f + aleatorio01(rng) * 0.6f));
        c.recompensaVivo = (valor / 5) * 5 + 10;
        c.recompensaMorto = std::max(5, (int(c.recompensaVivo * 0.6f) / 5) * 5);
        c.ultimoVisto = lugares[size_t(aleatorio01(rng) * lugares.size()) % lugares.size()];
        c.rosto = hash32(rng);
        res.push_back(c);
    }
    return res;
}
