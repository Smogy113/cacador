#include "game/sons.h"
#include "core/math.h"
#include <cmath>
#include <cstdio>

namespace {
constexpr float VOLUME_MINIMO = 0.003f;
}

float Sons::aleatorio() {
    semente_ = semente_ * 1664525u + 1013904223u;
    return float(semente_ >> 8) / 16777216.f;
}

void Sons::carregarBanco(Banco& banco, const char* nome, int quantas) {
    banco.amostras.reserve(quantas);
    for (int i = 1; i <= quantas; ++i) {
        char caminho[96];
        std::snprintf(caminho, sizeof caminho, DATA_DIR "/sons/%s_%02d.ogg", nome, i);
        Sound amostra = LoadSound(caminho);
        if (IsSoundValid(amostra)) banco.amostras.push_back(amostra);
    }
    if (!banco.amostras.empty()) banco.ultima = banco.amostras.size() - 1;
}

void Sons::descarregarBanco(Banco& banco) {
    for (Sound amostra : banco.amostras) UnloadSound(amostra);
    banco = {};
}

Sons::Faixa Sons::carregarFaixa(const char* nome) {
    char caminho[96];
    std::snprintf(caminho, sizeof caminho, DATA_DIR "/sons/%s.ogg", nome);
    Music musica = LoadMusicStream(caminho);
    musica.looping = true;
    return {musica};
}

void Sons::atualizarFaixa(Faixa& faixa, float alvo, float transicao) {
    if (!IsMusicValid(faixa.musica)) return;
    faixa.volume = lerp(faixa.volume, alvo, transicao);
    if (faixa.volume < VOLUME_MINIMO && alvo < VOLUME_MINIMO) {
        if (faixa.tocando) StopMusicStream(faixa.musica);
        faixa.tocando = false;
        faixa.volume = 0;
        return;
    }
    SetMusicVolume(faixa.musica, faixa.volume);
    if (!faixa.tocando) {
        PlayMusicStream(faixa.musica);
        faixa.tocando = true;
    }
    UpdateMusicStream(faixa.musica);
}

void Sons::descarregarFaixa(Faixa& faixa) {
    if (IsMusicValid(faixa.musica)) UnloadMusicStream(faixa.musica);
    faixa = {};
}

void Sons::carregar() {
    if (ok_ || !IsAudioDeviceReady()) return;
    ok_ = true;
    carregarBanco(tiros_[size_t(AcusticaTiro::Externa)], "tiro_campo", 4);
    carregarBanco(tiros_[size_t(AcusticaTiro::Interior)], "tiro_sala", 4);
    carregarBanco(tiros_[size_t(AcusticaTiro::Distante)], "tiro_longe", 3);
    carregarBanco(tiros_[size_t(AcusticaTiro::Obstruida)], "tiro_abafado", 3);
    carregarBanco(passos_[size_t(PisoSonoro::Terra)], "passo_terra", 4);
    carregarBanco(passos_[size_t(PisoSonoro::Capim)], "passo_capim", 4);
    carregarBanco(passos_[size_t(PisoSonoro::Pedra)], "passo_pedra", 4);
    carregarBanco(passos_[size_t(PisoSonoro::Madeira)], "passo_madeira", 4);
    carregarBanco(passos_[size_t(PisoSonoro::Agua)], "passo_agua", 4);
    carregarBanco(cascoMacio_, "casco_terra", 5);
    carregarBanco(cascoDuro_, "casco_firme", 4);
    carregarBanco(cascoAgua_, "casco_agua", 4);
    carregarBanco(recargas_, "recarga", 2);
    carregarBanco(rangidos_, "porta_movimento", 3);
    carregarBanco(batentes_, "porta_batente", 3);
    carregarBanco(passaros_, "ave", 5);
    vento_ = carregarFaixa("vento");
    insetos_ = carregarFaixa("insetos");
    atePassaro_ = 2.5f;
}

void Sons::descarregar() {
    if (!ok_) return;
    for (auto& banco : tiros_) descarregarBanco(banco);
    for (auto& banco : passos_) descarregarBanco(banco);
    for (auto* banco : {&cascoMacio_, &cascoDuro_, &cascoAgua_, &recargas_, &rangidos_, &batentes_, &passaros_})
        descarregarBanco(*banco);
    descarregarFaixa(vento_);
    descarregarFaixa(insetos_);
    ok_ = false;
}

void Sons::tocar(Banco& banco, float volume, float pan, float variarTom) {
    const size_t n = banco.amostras.size();
    if (n == 0 || volume < VOLUME_MINIMO) return;

    // Começa por uma variante diferente. Prefere um buffer livre para preservar as caudas.
    size_t indice = (banco.ultima + 1 + size_t(aleatorio() * (n - 1))) % n;
    for (size_t vistos = 0; vistos + 1 < n && IsSoundPlaying(banco.amostras[indice]); ++vistos) {
        size_t proxima = (indice + 1) % n;
        if (proxima == banco.ultima) break;
        indice = proxima;
    }
    banco.ultima = indice;
    Sound som = banco.amostras[indice];
    SetSoundVolume(som, clamp01(volume) * lerp(.93f, 1.f, aleatorio()));
    SetSoundPan(som, pan);
    SetSoundPitch(som, 1.f + variarTom * (2.f * aleatorio() - 1.f));
    PlaySound(som);
}

void Sons::tiro(AcusticaTiro acustica, float volume, float pan) {
    tocar(tiros_[size_t(acustica)], volume * .74f, pan, .028f);
}

void Sons::passo(PisoSonoro piso, float volume, float pan) {
    tocar(passos_[size_t(piso)], volume, pan, .065f);
}

void Sons::casco(PisoSonoro piso, float volume, float pan) {
    const bool firme = piso == PisoSonoro::Pedra || piso == PisoSonoro::Madeira;
    Banco& banco = piso == PisoSonoro::Agua ? cascoAgua_ : (firme ? cascoDuro_ : cascoMacio_);
    tocar(banco, volume, pan, .055f);
}

void Sons::recarga() { tocar(recargas_, .55f, .46f, 0.f); }

void Sons::moverPorta(float volume, float pan) { tocar(rangidos_, .47f * volume, pan, .075f); }

void Sons::fecharPorta(float volume, float pan) { tocar(batentes_, .57f * volume, pan, .045f); }

void Sons::ambiente(float dt, float noite, bool interior, bool pausado) {
    if (!ok_) return;
    float abertura = interior ? .22f : 1.f;
    if (pausado) abertura *= .38f;
    float transicao = 1.f - std::exp(-dt * 3.6f);
    atualizarFaixa(vento_, abertura * .46f, transicao);
    atualizarFaixa(insetos_, abertura * .37f * smoothstep(.30f, .9f, noite), transicao);

    if (interior || pausado || noite > .48f) return;
    atePassaro_ -= dt;
    if (atePassaro_ <= 0.f) {
        atePassaro_ = lerp(4.5f, 11.5f, aleatorio());
        float volume = lerp(.17f, .28f, aleatorio()) * smoothstep(.48f, .12f, noite);
        tocar(passaros_, volume, lerp(.17f, .83f, aleatorio()), .035f);
    }
}
