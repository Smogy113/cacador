#include "game/sons.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr int TAXA = 22050;
const float PI2 = 6.2831853f;

struct Gerador {
    unsigned s = 1;
    float ruido() {  // -1..1
        s = s * 1664525u + 1013904223u;
        return float((s >> 9) & 0x7FFF) / 16383.5f - 1.f;
    }
};

Sound paraSom(std::vector<float>& a) {
    float pico = 1e-6f;
    for (float v : a) pico = std::max(pico, std::fabs(v));
    std::vector<short> s(a.size());
    for (size_t i = 0; i < a.size(); i++) s[i] = short(a[i] / pico * 0.9f * 32767.f);
    Wave w{};
    w.frameCount = (unsigned)s.size();
    w.sampleRate = TAXA;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = s.data();
    return LoadSoundFromWave(w);  // copia os dados
}
// passa-baixa de um polo (abafa o ruido: terra, vento)
void abafar(std::vector<float>& a, float corte) {
    float k = 1.f - std::exp(-PI2 * corte / TAXA), y = 0;
    for (float& v : a) v = y += k * (v - y);
}

// Tiro de revolver: estalo de ruido curtissimo, estrondo grave (60-90 Hz) e eco nos morros.
std::vector<float> somTiro(unsigned semente) {
    Gerador g{semente};
    int n = int(TAXA * 1.6f);
    std::vector<float> a(n, 0.f), seco(int(TAXA * 0.45f));
    for (int i = 0; i < int(seco.size()); i++) {
        float t = float(i) / TAXA;
        float estalo = g.ruido() * std::exp(-t * 55.f);
        float corpo = g.ruido() * std::exp(-t * 9.f) * 0.5f;
        float grave = std::sin(PI2 * (85.f - 40.f * t) * t) * std::exp(-t * 7.f) * 0.9f;
        seco[i] = estalo + corpo + grave;
    }
    std::vector<float> corpoAbafado(seco);
    abafar(corpoAbafado, 1800.f);
    for (size_t i = 0; i < seco.size(); i++) seco[i] = 0.55f * seco[i] + 0.45f * corpoAbafado[i];
    // eco: copias atrasadas, mais abafadas e fracas
    std::vector<float> eco(seco);
    abafar(eco, 700.f);
    const float atrasos[] = {0.f, 0.21f, 0.47f, 0.8f}, ganhos[] = {1.f, 0.32f, 0.18f, 0.08f};
    for (int e = 0; e < 4; e++) {
        int o = int(atrasos[e] * TAXA);
        const std::vector<float>& src = e == 0 ? seco : eco;
        for (size_t i = 0; i < src.size() && o + int(i) < n; i++) a[o + i] += src[i] * ganhos[e];
    }
    return a;
}
// Passo na terra: baque grave curto + chiado de cascalho abafado.
std::vector<float> somPasso() {
    Gerador g{77};
    int n = int(TAXA * 0.16f);
    std::vector<float> a(n), r(n);
    for (int i = 0; i < n; i++) {
        float t = float(i) / TAXA;
        r[i] = g.ruido() * std::exp(-t * 30.f);
        a[i] = std::sin(PI2 * 70.f * t) * std::exp(-t * 45.f) * 0.7f;
    }
    abafar(r, 1200.f);
    for (int i = 0; i < n; i++) a[i] += r[i] * 1.4f;
    return a;
}
// Casco: batida seca dupla (pinca e talao) sobre terra batida.
std::vector<float> somCasco() {
    Gerador g{91};
    int n = int(TAXA * 0.22f);
    std::vector<float> a(n, 0.f);
    for (int b = 0; b < 2; b++) {
        int o = int(b * 0.045f * TAXA);
        for (int i = 0; o + i < n; i++) {
            float t = float(i) / TAXA;
            a[o + i] += (std::sin(PI2 * (160.f + 40.f * b) * t) * 0.8f + g.ruido() * 0.5f) * std::exp(-t * 38.f) * (b ? 0.7f : 1.f);
        }
    }
    abafar(a, 2200.f);
    return a;
}
// Recarga: tambor girando (catraca) e cartuchos entrando — cliques metalicos.
std::vector<float> somRecarga() {
    Gerador g{5};
    int n = int(TAXA * 1.2f);
    std::vector<float> a(n, 0.f);
    const float quando[] = {0.05f, 0.12f, 0.19f, 0.45f, 0.62f, 0.79f, 1.05f};
    for (float q : quando) {
        int o = int(q * TAXA);
        for (int i = 0; o + i < n && i < 1200; i++) {
            float t = float(i) / TAXA;
            a[o + i] += (std::sin(PI2 * 3100.f * t) * 0.6f + std::sin(PI2 * 5200.f * t) * 0.3f + g.ruido() * 0.4f) * std::exp(-t * 160.f);
        }
    }
    return a;
}
// Vento: ruido bem abafado com rajadas lentas (emendado para repetir sem corte).
std::vector<float> somVento() {
    Gerador g{3};
    int n = TAXA * 6;
    std::vector<float> a(n);
    for (float& v : a) v = g.ruido();
    abafar(a, 260.f);
    abafar(a, 400.f);
    for (int i = 0; i < n; i++) {
        float t = float(i) / n;
        a[i] *= 0.55f + 0.45f * std::sin(PI2 * t) * std::sin(PI2 * 2.f * t + 1.f);
        float borda = std::min(1.f, std::min(t, 1.f - t) * 20.f);  // sem estalo na volta
        a[i] *= borda;
    }
    return a;
}
// Passaro: tres assobios descendo (como um sabia ao longe).
std::vector<float> somPassaro() {
    int n = int(TAXA * 0.9f);
    std::vector<float> a(n, 0.f);
    float fase = 0;
    for (int k = 0; k < 3; k++) {
        int o = int(k * 0.27f * TAXA), d = int(0.18f * TAXA);
        for (int i = 0; i < d && o + i < n; i++) {
            float t = float(i) / d;
            float f = 3400.f - 900.f * t - 250.f * k + 180.f * std::sin(PI2 * 22.f * t);
            fase += PI2 * f / TAXA;
            a[o + i] += std::sin(fase) * std::sin(3.14159f * t);
        }
    }
    return a;
}
// Grilo: trinado de ~4,6 kHz em pulsos rapidos.
std::vector<float> somGrilo() {
    int n = int(TAXA * 0.6f);
    std::vector<float> a(n, 0.f);
    for (int i = 0; i < n; i++) {
        float t = float(i) / TAXA;
        float pulso = std::fmod(t, 0.05f) < 0.018f ? 1.f : 0.f;
        float env = std::sin(3.14159f * std::min(1.f, t / 0.6f));
        a[i] = std::sin(PI2 * 4600.f * t) * pulso * env;
    }
    return a;
}
}  // namespace

float Sons::rnd() {
    semente_ = semente_ * 1664525u + 1013904223u;
    return float((semente_ >> 8) & 0xFFFF) / 65535.f;
}

void Sons::carregar() {
    ok_ = IsAudioDeviceReady();
    if (!ok_) return;
    for (int k = 0; k < 4; k++) {
        auto t = somTiro(11u + unsigned(k) * 977u);
        tiro_[k] = paraSom(t);
    }
    auto p = somPasso(), c = somCasco(), r = somRecarga(), v = somVento(), b = somPassaro(), g = somGrilo();
    passo_ = paraSom(p);
    casco_ = paraSom(c);
    recarga_ = paraSom(r);
    vento_ = paraSom(v);
    passaro_ = paraSom(b);
    grilo_ = paraSom(g);
}

void Sons::descarregar() {
    if (!ok_) return;
    for (auto& s : tiro_) UnloadSound(s);
    for (Sound* s : {&passo_, &casco_, &recarga_, &vento_, &passaro_, &grilo_}) UnloadSound(*s);
    ok_ = false;
}

void Sons::tiro(float vol, float pan) {
    if (!ok_ || vol < 0.01f) return;
    Sound& s = tiro_[prox_++ % 4];  // varios ao mesmo tempo
    SetSoundVolume(s, vol);
    SetSoundPan(s, pan);
    SetSoundPitch(s, 0.92f + rnd() * 0.16f);
    PlaySound(s);
}
void Sons::passo(float vol) {
    if (!ok_) return;
    SetSoundVolume(passo_, vol * (0.7f + rnd() * 0.3f));
    SetSoundPitch(passo_, 0.85f + rnd() * 0.3f);
    PlaySound(passo_);
}
void Sons::casco(float vol) {
    if (!ok_) return;
    SetSoundVolume(casco_, vol * (0.75f + rnd() * 0.25f));
    SetSoundPitch(casco_, 0.9f + rnd() * 0.2f);
    PlaySound(casco_);
}
void Sons::recarga() {
    if (!ok_) return;
    SetSoundVolume(recarga_, 0.5f);
    PlaySound(recarga_);
}
void Sons::ambiente(float dt, float noite) {
    if (!ok_) return;
    if (!IsSoundPlaying(vento_)) {
        SetSoundVolume(vento_, 0.18f);
        SetSoundPitch(vento_, 0.9f + rnd() * 0.2f);
        PlaySound(vento_);
    }
    tPassaro_ -= dt;
    if (tPassaro_ <= 0) {
        tPassaro_ = 4.f + rnd() * 9.f;
        if (noite < 0.3f) {
            SetSoundVolume(passaro_, 0.06f + rnd() * 0.1f);
            SetSoundPan(passaro_, rnd());
            SetSoundPitch(passaro_, 0.85f + rnd() * 0.35f);
            PlaySound(passaro_);
        }
    }
    tGrilo_ -= dt;
    if (tGrilo_ <= 0) {
        tGrilo_ = 0.7f + rnd() * 1.2f;
        if (noite > 0.5f) {
            SetSoundVolume(grilo_, 0.05f + rnd() * 0.05f);
            SetSoundPan(grilo_, rnd());
            SetSoundPitch(grilo_, 0.95f + rnd() * 0.1f);
            PlaySound(grilo_);
        }
    }
}
