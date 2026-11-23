#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "raylib.h"

enum class PisoSonoro { Terra, Capim, Pedra, Madeira, Agua, Total };
enum class AcusticaTiro { Externa, Interior, Distante, Obstruida, Total };

// Efeitos mono para posicionar no estéreo e ambientes estéreo em streaming.
class Sons {
public:
    void carregar();  // chamado depois de InitAudioDevice
    void descarregar();
    bool ok() const { return ok_; }

    // Pan da raylib: 0 à direita, 1 à esquerda.
    void tiro(AcusticaTiro acustica, float volume, float pan);
    void passo(PisoSonoro piso, float volume, float pan);
    void casco(PisoSonoro piso, float volume, float pan);
    void recarga();
    void moverPorta(float volume, float pan);
    void fecharPorta(float volume, float pan);
    // Atualiza os buffers também nos menus abertos durante o jogo.
    void ambiente(float dt, float noite, bool interior, bool pausado);

private:
    struct Banco {
        std::vector<Sound> amostras;
        size_t ultima = 0;
    };
    struct Faixa {
        Music musica{};
        float volume = 0;
        bool tocando = false;
    };

    bool ok_ = false;
    std::array<Banco, size_t(AcusticaTiro::Total)> tiros_;
    std::array<Banco, size_t(PisoSonoro::Total)> passos_;
    Banco cascoMacio_, cascoDuro_, cascoAgua_;
    Banco recargas_, rangidos_, batentes_, passaros_;
    Faixa vento_, insetos_;
    float atePassaro_ = 2.5f;
    uint32_t semente_ = 12345;

    float aleatorio();
    void tocar(Banco& banco, float volume, float pan, float variarTom);
    static void carregarBanco(Banco& banco, const char* nome, int quantas);
    static void descarregarBanco(Banco& banco);
    static Faixa carregarFaixa(const char* nome);
    static void atualizarFaixa(Faixa& faixa, float alvo, float transicao);
    static void descarregarFaixa(Faixa& faixa);
};
