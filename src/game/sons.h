#pragma once
#include "raylib.h"

// Sons gerados por codigo (sem arquivos), imitando gravacoes reais:
// tiro com estalo + estrondo grave + eco, passos na terra, cascos, cliques de recarga,
// vento, passaros de dia e grilos a noite.
class Sons {
public:
    void carregar();  // precisa de InitAudioDevice antes; sem placa de som, fica mudo
    void descarregar();
    bool ok() const { return ok_; }
    // vol 0..1, pan 0 (direita) .. 1 (esquerda), tom ~1
    void tiro(float vol, float pan);
    void passo(float vol);
    void casco(float vol);
    void recarga();
    void porta(bool abrindo);  // rangido ao abrir, batida ao fechar
    // chamado todo quadro: noite 0..1, dt
    void ambiente(float dt, float noite);

private:
    bool ok_ = false;
    Sound tiro_[4]{}, passo_{}, casco_{}, recarga_{}, vento_{}, passaro_{}, grilo_{}, rangido_{}, batida_{};
    int prox_ = 0;
    float tPassaro_ = 3, tGrilo_ = 1;
    unsigned semente_ = 12345;
    float rnd();
};
