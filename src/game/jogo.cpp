#include "game/jogo.h"
#include <cmath>
#include "game/ui.h"

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
    rig_.yaw = yaw + op.girarCamera * DEG2RAD;
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
    // moradores: roupas variadas
    {
        const Rgb cas[] = {{0.45f, 0.35f, 0.22f}, {0.30f, 0.36f, 0.45f}, {0.55f, 0.22f, 0.18f}, {0.35f, 0.42f, 0.28f}, {0.62f, 0.56f, 0.44f}, {0.25f, 0.22f, 0.25f}};
        const Rgb pel[] = {{0.85f, 0.66f, 0.52f}, {0.62f, 0.44f, 0.32f}, {0.45f, 0.31f, 0.22f}, {0.78f, 0.58f, 0.45f}};
        for (int k = 0; k < 6; k++) {
            Roupa rp;
            rp.casaco = cas[k];
            rp.calca = cas[(k + 3) % 6] * 0.6f;
            rp.camisa = {0.82f, 0.78f, 0.68f};
            rp.chapeu = k % 2 ? Rgb{0.35f, 0.25f, 0.15f} : Rgb{0.28f, 0.2f, 0.12f};
            rp.pele = pel[k % 4];
            rp.lenco = cas[(k + 1) % 6];
            rp.chapeuAba = k % 3 != 0;
            rp.casacoLongo = k % 2 == 0;
            roupasNpc_.emplace_back();
            roupasNpc_.back().carregar(rp);
        }
        populacao_ = Populacao(cenario_.rotasNPC, 1337, 60);
    }
    for (size_t q = 0; q < cenario_.quadros.size(); q++) cartazes_.push_back(gerarCartazes(77 + uint32_t(q) * 13, 6, mapa_));
    if (op.abrirQuadro && !cenario_.quadros.empty()) {
        const Quadro& q = cenario_.quadros[0];
        jogador_.pos = q.pos;
        jogador_.pos.y = terreno_->altura(q.pos.x, q.pos.z);
        quadroAberto_ = 0;
    }
    // cavalos nos estabulos
    const Rgb pelos[] = {{0.40f, 0.25f, 0.16f}, {0.16f, 0.13f, 0.12f}, {0.78f, 0.74f, 0.68f}, {0.6f, 0.42f, 0.24f}};
    const Rgb crinas[] = {{0.15f, 0.1f, 0.07f}, {0.08f, 0.07f, 0.07f}, {0.55f, 0.5f, 0.45f}, {0.85f, 0.8f, 0.65f}};
    std::vector<Vec3f> posCav = cenario_.cavalos;
    if (op.comecarMontado) posCav.insert(posCav.begin(), jogador_.pos);
    for (size_t i = 0; i < posCav.size(); i++) {
        Cavalo c;
        c.corpo.pos = posCav[i];
        c.corpo.pos.y = terreno_->altura(c.corpo.pos.x, c.corpo.pos.z);
        c.corpo.yaw = i == 0 && op.comecarMontado ? yaw : float(i) * 1.7f;
        cavalos_.push_back(c);
        modelosCavalo_.emplace_back();
        modelosCavalo_.back().carregar(pelos[i % 4], crinas[i % 4], true);
    }
    if (op.comecarMontado && !cavalos_.empty()) {
        cavalos_[0].montado = true;
        montadoEm_ = 0;
        rig_.distAlvo = 6.f;
    }
    sombra_.carregar();
    for (int i = 0; i < 40; i++) rig_.atualizar(montadoEm_ >= 0 ? jogador_.pos + Vec3f{0, 0.9f, 0} : jogador_.pos, 0, 0, *terreno_, col_, 0.05f);
    atualizarCamera(0, Entrada{});
    terrenoR_->atualizar(jogador_.pos, luz_.efeitos().distanciaVisao, 0, 100000);
    cenario_.atualizar(jogador_.pos, 0, 100000);
    for (int i = 0; i < 300; i++) populacao_.atualizar(0.1f, relogio_, *terreno_, col_, {1e4f, 0, 1e4f});
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
    Vec3f alvo = montadoEm_ >= 0 ? cavalos_[montadoEm_].corpo.pos + Vec3f{0, 0.9f, 0} : jogador_.pos;
    rig_.atualizar(alvo, e.mouseDx, e.mouseDy, *terreno_, col_, dt);
    Vec3f p = rig_.posicao(), o = rig_.olhar();
    cam_.position = {p.x, p.y, p.z};
    cam_.target = {o.x, o.y, o.z};
}

void Jogo::desenharInterface() {
    desenharAvisos(mapa_.erros, tempoTotal_);
    if (quadroAberto_ >= 0) {
        desenharQuadro(cartazes_[quadroAberto_], cartazSel_, semAcentos(nomeLegivel(cenario_.quadros[quadroAberto_].lugar)));
        return;
    }
    if (!pausado_) {
        if (quadroPerto_ >= 0) desenharDica("E  -  Ver recompensas");
        else if (cavaloPerto_ >= 0) desenharDica("E  -  Montar");
        return;
    }
    int w = GetScreenWidth(), h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, {0, 0, 0, 140});
    const char* t1 = "PAUSADO";
    const char* t2 = "Clique para continuar   -   Q para sair";
    DrawText(t1, w / 2 - MeasureText(t1, 48) / 2, h / 2 - 50, 48, {235, 220, 190, 255});
    DrawText(t2, w / 2 - MeasureText(t2, 22) / 2, h / 2 + 10, 22, {220, 210, 190, 255});
}

Jogo::~Jogo() {
    for (auto& m : roupasNpc_) m.descarregar();
    for (auto& m : modelosCavalo_) m.descarregar();
    cenario_.descarregar();
    modeloJogador_.descarregar();
    sombra_.descarregar();
    terrenoR_.reset();
    luz_.descarregar();
}

void Jogo::atualizar(float dt) {
    dt = limitarDt(dt);
    tempoTotal_ += dt;
    if (quadroAberto_ >= 0) {
        int n = int(cartazes_[quadroAberto_].size());
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) cartazSel_ = (cartazSel_ + 1) % n;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) cartazSel_ = (cartazSel_ + n - 1) % n;
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ESCAPE)) quadroAberto_ = -1;
        if (!op_.congelarHora) relogio_.avancar(dt);
        return;
    }
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
    cavaloPerto_ = -1;
    if (montadoEm_ >= 0) {
        Cavalo& c = cavalos_[montadoEm_];
        if (e.interagir) {
            desmontar(c, jogador_, col_);
            montadoEm_ = -1;
            rig_.distAlvo = 4.f;
        } else {
            moverCavalo(c, e, dt, *terreno_, col_);
            jogador_.pos = c.corpo.pos;
            jogador_.yaw = c.corpo.yaw;
            jogador_.vel = 0;
        }
    } else {
        moverAPe(jogador_, e, rig_.yaw, dt, *terreno_, col_);
        passada_ += dt * jogador_.vel * 3.2f;
        for (size_t i = 0; i < cavalos_.size(); i++)
            if (std::hypot(cavalos_[i].corpo.pos.x - jogador_.pos.x, cavalos_[i].corpo.pos.z - jogador_.pos.z) < DIST_MONTAR) cavaloPerto_ = int(i);
        quadroPerto_ = -1;
        for (size_t q = 0; q < cenario_.quadros.size(); q++)
            if (std::hypot(cenario_.quadros[q].pos.x - jogador_.pos.x, cenario_.quadros[q].pos.z - jogador_.pos.z) < 2.2f) quadroPerto_ = int(q);
        if (e.interagir && quadroPerto_ >= 0) {
            quadroAberto_ = quadroPerto_;
            cartazSel_ = 0;
            e.interagir = false;
        }
        if (e.interagir && cavaloPerto_ >= 0 && tentarMontar(cavalos_[cavaloPerto_], jogador_)) {
            montadoEm_ = cavaloPerto_;
            rig_.distAlvo = 6.f;
            cavaloPerto_ = -1;
        }
    }
    populacao_.atualizar(dt, relogio_, *terreno_, col_, jogador_.pos);
    for (size_t i = 0; i < cavalos_.size(); i++)
        if (int(i) != montadoEm_) moverCavalo(cavalos_[i], Entrada{}, dt, *terreno_, col_);
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
    for (auto& n : populacao_.npcs()) {
        if (!n.ativo) continue;
        float dx = n.corpo.pos.x - cam_.position.x, dz = n.corpo.pos.z - cam_.position.z;
        float d2 = dx * dx + dz * dz;
        if (d2 > 160.f * 160.f) continue;
        const ModeloPessoa& m = roupasNpc_[n.semente % roupasNpc_.size()];
        if (d2 < 35.f * 35.f) {
            m.desenhar(luz_.material(), n.corpo.pos, n.corpo.yaw, n.passada, n.corpo.vel > 0.1f ? 0.45f : 0.f);
            sombra_.desenhar(n.corpo.pos, terreno_->normal(n.corpo.pos.x, n.corpo.pos.z), 0.42f);
        } else
            m.desenharSimples(luz_.material(), n.corpo.pos, n.corpo.yaw);
    }
    for (size_t i = 0; i < cavalos_.size(); i++) {
        const Cavalo& c = cavalos_[i];
        Vector3 d{c.corpo.pos.x - cam_.position.x, 0, c.corpo.pos.z - cam_.position.z};
        if (d.x * d.x + d.z * d.z > dv * dv) continue;
        modelosCavalo_[i].desenhar(luz_.material(), c.corpo.pos, c.corpo.yaw, c.passada, int(c.andadura));
        if (int(i) == montadoEm_) modeloJogador_.desenharSentado(luz_.material(), ModeloCavalo::assento(c.corpo.pos, c.corpo.yaw), c.corpo.yaw);
    }
    if (montadoEm_ < 0) {
        float amp = std::min(1.f, jogador_.vel / VEL_CORRER + (jogador_.vel > 0 ? 0.35f : 0.f));
        modeloJogador_.desenhar(luz_.material(), jogador_.pos, jogador_.yaw, passada_, amp);
        sombra_.desenhar(jogador_.pos, terreno_->normal(jogador_.pos.x, jogador_.pos.z), 0.45f);
    }
    for (auto& c : cavalos_) {
        Vec3f f = frenteDe(c.corpo.yaw);
        for (float s : {0.6f, -0.6f}) {
            Vec3f q = c.corpo.pos + f * s;
            sombra_.desenhar({q.x, terreno_->altura(q.x, q.z), q.z}, terreno_->normal(q.x, q.z), 0.6f);
        }
    }
    EndMode3D();
    tris_ = terrenoR_->triangulosDesenhados() + cenario_.triangulos();
    chamadas_ = terrenoR_->chamadasDesenho() + cenario_.chamadas();
}
