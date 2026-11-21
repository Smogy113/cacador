#include "game/jogo.h"
#include <cmath>

Jogo::Jogo(const OpcoesJogo& op) : op_(op), relogio_(op.hora) {
    mapa_ = carregarMapa(op.arquivoMapa);
    terreno_ = std::make_unique<Terreno>(mapa_, 1337);
    terrenoR_ = std::make_unique<TerrenoRender>(*terreno_);
    luz_.carregar(op.nivel);

    float x = op.temPos ? op.x : 20.f, z = op.temPos ? op.z : -30.f, yaw = op.temPos ? op.yaw : 200.f;
    float y = terreno_->altura(x, z) + 7.f;
    float r = yaw * DEG2RAD;
    cam_.position = {x, y, z};
    cam_.target = {x + std::sin(r) * 10.f, y - 1.2f, z + std::cos(r) * 10.f};
    cam_.up = {0, 1, 0};
    cam_.fovy = 60.f;
    cam_.projection = CAMERA_PERSPECTIVE;
    terrenoR_->atualizar({x, y, z}, luz_.efeitos().distanciaVisao, 0, 100000);
}

Jogo::~Jogo() {
    terrenoR_.reset();
    luz_.descarregar();
}

void Jogo::atualizar(float dt) {
    tempo_ += dt;
    if (!op_.congelarHora) relogio_.avancar(dt);
    Vec3f foco{cam_.position.x, cam_.position.y, cam_.position.z};
    terrenoR_->atualizar(foco, luz_.efeitos().distanciaVisao, tempo_);

    luzes_.clear();
    // lanterna do jogador (acende com o anoitecer)
    float noite = relogio_.intensidadeTochas();
    if (noite > 0.01f) {
        Vec3f p{cam_.position.x, terreno_->altura(cam_.position.x, cam_.position.z) + 1.6f, cam_.position.z};
        luzes_.push_back({p, Rgb{1.0f, 0.72f, 0.42f} * (2.2f * noite), 16.f, 0.3f});
    }
}

void Jogo::desenhar() {
    Vec3f foco{cam_.position.x, cam_.position.y, cam_.position.z};
    luz_.comecarCena(cam_, relogio_, luzes_, foco, tempo_);
    ClearBackground(BLACK);
    BeginMode3D(cam_);
    luz_.desenharCeu(cam_);
    float dv = luz_.efeitos().distanciaVisao;
    luz_.modoHorizonte(dv - 40.f);
    terrenoR_->desenharHorizonte(luz_.material());
    luz_.modoHorizonte(0);
    terrenoR_->desenhar(cam_, dv, luz_.material());
    EndMode3D();
    tris_ = terrenoR_->triangulosDesenhados();
    chamadas_ = terrenoR_->chamadasDesenho();
}
