#include "game/jogo.h"
#include <cmath>

Jogo::Jogo(const OpcoesJogo& op) : op_(op), relogio_(op.hora) {
    mapa_ = carregarMapa(op.arquivoMapa);
    terreno_ = std::make_unique<Terreno>(mapa_, 1337);
    terrenoR_ = std::make_unique<TerrenoRender>(*terreno_);
    cenario_.construir(mapa_, *terreno_, col_, 1337);
    luz_.carregar(op.nivel);

    // comeca na entrada de Pedrabranca, olhando para a ponte
    float x = op.temPos ? op.x : -40.f, z = op.temPos ? op.z : 10.f, yaw = (op.temPos ? op.yaw : 160.f) * DEG2RAD;
    jogador_.pos = {x, terreno_->altura(x, z), z};
    jogador_.yaw = yaw;
    rig_.yaw = yaw;
    rig_.pitch = 0.22f;
    cam_.up = {0, 1, 0};
    cam_.fovy = 60.f;
    cam_.projection = CAMERA_PERSPECTIVE;
    Roupa r;
    r.casaco = {0.36f, 0.26f, 0.18f};
    r.calca = {0.22f, 0.2f, 0.2f};
    r.camisa = {0.7f, 0.65f, 0.55f};
    r.chapeu = {0.2f, 0.15f, 0.11f};
    r.pele = {0.78f, 0.58f, 0.45f};
    r.lenco = {0.55f, 0.12f, 0.1f};
    modeloJogador_.carregar(r);
    sombra_.carregar();
    for (int i = 0; i < 40; i++) rig_.atualizar(jogador_.pos, 0, 0, *terreno_, col_, 0.05f);
    atualizarCamera(0, Entrada{});
    terrenoR_->atualizar(jogador_.pos, luz_.efeitos().distanciaVisao, 0, 100000);
    cenario_.atualizar(jogador_.pos, 0, 100000);
    if (!op.semEntrada) DisableCursor();
}

Entrada Jogo::lerEntrada() {
    Entrada e;
    if (op_.semEntrada) return e;
    e.frente = (IsKeyDown(KEY_W) ? 1.f : 0.f) - (IsKeyDown(KEY_S) ? 1.f : 0.f);
    e.lado = (IsKeyDown(KEY_D) ? 1.f : 0.f) - (IsKeyDown(KEY_A) ? 1.f : 0.f);
    e.correr = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    e.interagir = IsKeyPressed(KEY_E);
    Vector2 md = GetMouseDelta();
    e.mouseDx = md.x;
    e.mouseDy = md.y;
    return e;
}

void Jogo::atualizarCamera(float dt, const Entrada& e) {
    rig_.atualizar(jogador_.pos, e.mouseDx, e.mouseDy, *terreno_, col_, dt);
    Vec3f p = rig_.posicao(), o = rig_.olhar();
    cam_.position = {p.x, p.y, p.z};
    cam_.target = {o.x, o.y, o.z};
}

void Jogo::desenharInterface() {
    if (!pausado_) return;
    int w = GetScreenWidth(), h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, {0, 0, 0, 140});
    const char* t1 = "PAUSADO";
    const char* t2 = "Clique para continuar   -   Q para sair";
    DrawText(t1, w / 2 - MeasureText(t1, 48) / 2, h / 2 - 50, 48, {235, 220, 190, 255});
    DrawText(t2, w / 2 - MeasureText(t2, 22) / 2, h / 2 + 10, 22, {220, 210, 190, 255});
}

Jogo::~Jogo() {
    cenario_.descarregar();
    modeloJogador_.descarregar();
    sombra_.descarregar();
    terrenoR_.reset();
    luz_.descarregar();
}

void Jogo::atualizar(float dt) {
    dt = limitarDt(dt);
    // pausa: Esc, ou a janela perdeu o foco
    if (!op_.semEntrada) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            pausado_ = !pausado_;
            if (pausado_) EnableCursor(); else DisableCursor();
        }
        if (!IsWindowFocused() && !pausado_) { pausado_ = true; EnableCursor(); }
        if (pausado_) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { pausado_ = false; DisableCursor(); }
            if (IsKeyPressed(KEY_Q)) sair_ = true;
            return;
        }
    }
    tempo_ += dt;
    if (!op_.congelarHora) relogio_.avancar(dt);
    Entrada e = lerEntrada();
    moverAPe(jogador_, e, rig_.yaw, dt, *terreno_, col_);
    passada_ += dt * jogador_.vel * 3.2f;
    atualizarCamera(dt, e);
    terrenoR_->atualizar(jogador_.pos, luz_.efeitos().distanciaVisao, tempo_);
    cenario_.atualizar(jogador_.pos, tempo_);

    luzes_.clear();
    float noite = relogio_.intensidadeTochas();
    if (noite > 0.01f)
        for (auto l : cenario_.luzes) {
            l.cor = l.cor * noite;
            luzes_.push_back(l);
        }
    // lanterna do jogador (acende com o anoitecer)
    if (noite > 0.01f) {
        Vec3f dir{-std::cos(jogador_.yaw), 0, std::sin(jogador_.yaw)};
        Vec3f p = jogador_.pos + Vec3f{0, 1.25f, 0} + dir * 0.35f + frenteDe(jogador_.yaw) * 0.2f;
        luzes_.push_back({p, Rgb{1.0f, 0.72f, 0.42f} * (2.4f * noite), 15.f, 0.3f});
    }
}

void Jogo::desenhar() {
    luz_.comecarCena(cam_, relogio_, luzes_, jogador_.pos, tempo_);
    ClearBackground(BLACK);
    BeginMode3D(cam_);
    luz_.desenharCeu(cam_);
    float dv = luz_.efeitos().distanciaVisao;
    luz_.modoHorizonte(dv - 40.f);
    terrenoR_->desenharHorizonte(luz_.material());
    luz_.modoHorizonte(0);
    terrenoR_->desenhar(cam_, dv, luz_.material());
    cenario_.desenhar(cam_, dv, luz_.material());
    float amp = std::min(1.f, jogador_.vel / VEL_CORRER + (jogador_.vel > 0 ? 0.35f : 0.f));
    modeloJogador_.desenhar(luz_.material(), jogador_.pos, jogador_.yaw, passada_, amp);
    sombra_.desenhar(jogador_.pos, terreno_->normal(jogador_.pos.x, jogador_.pos.z), 0.45f);
    EndMode3D();
    tris_ = terrenoR_->triangulosDesenhados() + cenario_.triangulos();
    chamadas_ = terrenoR_->chamadasDesenho() + cenario_.chamadas();
}
