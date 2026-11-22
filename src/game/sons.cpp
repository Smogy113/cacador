#include "game/sons.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr int TAXA = 44100;
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

// Impulso de pressao, estalo de banda larga e reflexos do ambiente.
std::vector<float> somTiro(unsigned semente, bool interior, bool abafado) {
    Gerador g{semente};
    std::vector<float> a(TAXA * 3 / 2), direto(TAXA / 5);
    float medio = 0.f, grave = 0.f;
    for (size_t i = 0; i < direto.size(); ++i) {
        float t = float(i) / TAXA;
        float ruido = g.ruido();
        medio += 0.21f * (ruido - medio);
        grave += 0.025f * (ruido - grave);
        float frente = 1.f - std::exp(-t * 9000.f);
        float impulso = (std::exp(-t * 180.f) - std::exp(-t * 760.f)) * 0.8f;
        direto[i] = impulso + frente * ((ruido - medio) * 1.1f * std::exp(-t * 165.f)
                  + (medio - grave) * 0.95f * std::exp(-t * 39.f)
                  + grave * 2.2f * std::exp(-t * 26.f));
        a[i] = direto[i];
    }
    std::vector<float> reflexo = direto;
    abafar(reflexo, interior ? 2800.f : 950.f);
    const float sala[] = {0.014f, 0.023f, 0.036f, 0.054f, 0.079f, 0.109f, 0.153f, 0.207f};
    const float campo[] = {0.130f, 0.190f, 0.274f, 0.366f, 0.453f, 0.567f, 0.684f, 0.846f};
    const auto& atrasos = interior ? sala : campo;
    for (size_t k = 0; k < 8; ++k) {
        size_t atraso = size_t(TAXA * (atrasos[k] + 0.002f * g.ruido()));
        float ganho = (interior ? 0.49f : 0.23f) * std::exp(-float(k) * 0.35f);
        for (size_t i = 0; i < reflexo.size(); ++i) a[atraso + i] += reflexo[i] * ganho;
    }
    if (abafado) abafar(a, 510.f);
    return a;
}

std::vector<float> somPassoMadeira(unsigned semente) {
    Gerador g{semente};
    std::vector<float> a(TAXA / 5);
    for (size_t i = 0; i < a.size(); ++i) {
        float t = float(i) / TAXA;
        float calcanhar = g.ruido() * std::exp(-t * 260.f);
        float tabua = (std::sin(PI2 * 124.f * t) + 0.35f * std::sin(PI2 * 281.f * t)) * std::exp(-t * 48.f);
        float sola = std::max(0.f, t - 0.035f);
        a[i] = calcanhar * 0.28f + tabua * 0.45f + g.ruido() * sola * 25.f * std::exp(-sola * 90.f);
    }
    abafar(a, 3100.f);
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
// Porta: rangido da dobradica (atrito: tom que sobe e desce com aspereza) e batida de madeira.
std::vector<float> somRangido() {
    Gerador g{4242u};
    int n = int(TAXA * 0.44f);
    std::vector<float> a(n);
    float fase = 0;
    for (int i = 0; i < n; i++) {
        float t = float(i) / TAXA, u = float(i) / n;
        float f = 310.f + 125.f * std::sin(u * 6.4f) + 56.f * std::sin(u * 28.f);
        fase += PI2 * f / TAXA;
        float atrito = 0.5f + 0.5f * std::sin(t * PI2 * (27.f + 12.f * u));  // "engasgos" do atrito
        float env = std::sin(PI2 * 0.5f * u) * (0.6f + 0.4f * atrito);
        a[i] = (std::sin(fase) * 0.6f + std::sin(fase * 2.03f) * 0.25f + g.ruido() * 0.15f) * env;
    }
    return a;
}
std::vector<float> somBatida() {
    Gerador g{777u};
    int n = int(TAXA * 0.45f);
    std::vector<float> a(n);
    for (int i = 0; i < n; i++) {
        float t = float(i) / TAXA;
        a[i] = (std::sin(PI2 * 95.f * t) * 0.8f + std::sin(PI2 * 180.f * t) * 0.3f) * std::exp(-t * 14.f) + g.ruido() * std::exp(-t * 60.f) * 0.6f;
    }
    abafar(a, 1800.f);
    for (size_t i = TAXA * 18 / 1000; i < a.size(); ++i) {
        float t = float(i - TAXA * 18 / 1000) / TAXA;
        a[i] += (0.19f * std::sin(PI2 * 1580.f * t) + g.ruido() * 0.31f) * std::exp(-t * 155.f);
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
    for (unsigned k = 0; k < 4; ++k) {
        auto aberto = somTiro(11u + k * 977u, false, false);
        auto fechado = somTiro(11u + k * 977u, true, false);
        auto abafado = somTiro(11u + k * 977u, false, true);
        tiro_[0][k] = paraSom(aberto);
        tiro_[1][k] = paraSom(fechado);
        tiro_[2][k] = paraSom(abafado);
        auto madeira = somPassoMadeira(53u + k * 691u);
        madeira_[k] = paraSom(madeira);
    }
    auto p = somPasso(), c = somCasco(), r = somRecarga(), v = somVento(), b = somPassaro(), g = somGrilo();
    passo_ = paraSom(p);
    casco_ = paraSom(c);
    recarga_ = paraSom(r);
    vento_ = paraSom(v);
    passaro_ = paraSom(b);
    grilo_ = paraSom(g);
    auto rg = somRangido(), bt = somBatida();
    rangido_ = paraSom(rg);
    batida_ = paraSom(bt);
}

void Sons::descarregar() {
    if (!ok_) return;
    for (auto& grupo : tiro_) for (auto& s : grupo) UnloadSound(s);
    for (auto& s : madeira_) UnloadSound(s);
    for (Sound* s : {&passo_, &casco_, &recarga_, &vento_, &passaro_, &grilo_, &rangido_, &batida_}) UnloadSound(*s);
    ok_ = false;
}

void Sons::tiro(float vol, float pan, bool interior, bool abafado) {
    if (!ok_ || vol < 0.01f) return;
    unsigned grupo = abafado ? 2 : interior ? 1 : 0;
    Sound& s = tiro_[grupo][proxTiro_[grupo]++ % 4];
    SetSoundVolume(s, vol);
    SetSoundPan(s, pan);
    SetSoundPitch(s, 0.97f + rnd() * 0.06f);
    PlaySound(s);
}
void Sons::passo(float vol, bool interior) {
    if (!ok_) return;
    Sound& s = interior ? madeira_[proxPasso_++ % 4] : passo_;
    SetSoundVolume(s, vol * (0.82f + rnd() * 0.18f));
    SetSoundPitch(s, 0.92f + rnd() * 0.16f);
    PlaySound(s);
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
void Sons::porta(bool abrindo) {
    if (!ok_) return;
    Sound& s = abrindo ? rangido_ : batida_;
    SetSoundVolume(s, abrindo ? 0.35f : 0.5f);
    SetSoundPitch(s, 0.9f + rnd() * 0.2f);
    PlaySound(s);
}
void Sons::ambiente(float dt, float noite, bool interior) {
    if (!ok_) return;
    if (!IsSoundPlaying(vento_)) {
        SetSoundPitch(vento_, 0.9f + rnd() * 0.2f);
        PlaySound(vento_);
    }
    SetSoundVolume(vento_, interior ? 0.035f : 0.18f);
    if (interior) {
        SetSoundVolume(passaro_, 0.02f);
        SetSoundVolume(grilo_, 0.02f);
    }
    tPassaro_ -= dt;
    if (tPassaro_ <= 0) {
        tPassaro_ = 4.f + rnd() * 9.f;
        if (!interior && noite < 0.3f) {
            SetSoundVolume(passaro_, 0.06f + rnd() * 0.1f);
            SetSoundPan(passaro_, rnd());
            SetSoundPitch(passaro_, 0.85f + rnd() * 0.35f);
            PlaySound(passaro_);
        }
    }
    tGrilo_ -= dt;
    if (tGrilo_ <= 0) {
        tGrilo_ = 0.7f + rnd() * 1.2f;
        if (!interior && noite > 0.5f) {
            SetSoundVolume(grilo_, 0.05f + rnd() * 0.05f);
            SetSoundPan(grilo_, rnd());
            SetSoundPitch(grilo_, 0.95f + rnd() * 0.1f);
            PlaySound(grilo_);
        }
    }
}
