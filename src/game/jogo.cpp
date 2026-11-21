#include "game/jogo.h"
#include <cmath>
#include "game/ui.h"
#include "rlgl.h"
#include "core/anim.h"
#include "core/bussola.h"

Jogo::Jogo(const OpcoesJogo& op) : op_(op), relogio_(op.hora) {
    mapa_ = carregarMapa(op.arquivoMapa);
    terreno_ = std::make_unique<Terreno>(mapa_, 1337);
    terrenoR_ = std::make_unique<TerrenoRender>(*terreno_);
    cenario_.construir(mapa_, *terreno_, col_, 1337);
    luz_.carregar(op.nivel);
    grama_.iniciar(*terreno_, col_);
    configurarGrama();

    // comeca na entrada de Pedrabranca, olhando para a ponte
    float x = op.temPos ? op.x : -40.f, z = op.temPos ? op.z : 10.f, yaw = (op.temPos ? op.yaw : 160.f) * DEG2RAD;
    jogador_.pos = {x, terreno_->altura(x, z), z};
    jogador_.yaw = yaw;
    rig_.yaw = yaw + op.girarCamera * DEG2RAD;
    rig_.pitch = 0.22f;
    cam_.up = {0, 1, 0};
    cam_.fovy = 60.f;
    cam_.projection = CAMERA_PERSPECTIVE;
    Visual r;
    r.casaco = {0.36f, 0.26f, 0.18f};
    r.calca = {0.24f, 0.21f, 0.2f};
    r.camisa = {0.72f, 0.66f, 0.55f};
    r.chapeu = {0.2f, 0.15f, 0.11f};
    r.pele = {0.78f, 0.58f, 0.45f};
    r.lenco = {0.55f, 0.12f, 0.1f};
    r.cabelo = {0.18f, 0.13f, 0.09f};
    r.barba = false;  // rosto limpo (pedido do usuario)
    r.equipamento = !op.estudio;
    r.heroi = true;
    r.revolver = !op.estudio;
    if (op.estudio) r.chapeuAba = false;
    modeloJogador_.carregar(Proporcoes{1.82f, 1.05f, 1.f, 1.f}, r);
    // moradores: roupas variadas
    {
        const Rgb cas[] = {{0.45f, 0.35f, 0.22f}, {0.30f, 0.36f, 0.45f}, {0.55f, 0.22f, 0.18f}, {0.35f, 0.42f, 0.28f}, {0.62f, 0.56f, 0.44f}, {0.25f, 0.22f, 0.25f}};
        const Rgb pel[] = {{0.85f, 0.66f, 0.52f}, {0.62f, 0.44f, 0.32f}, {0.45f, 0.31f, 0.22f}, {0.78f, 0.58f, 0.45f}};
        const Rgb cab[] = {{0.12f, 0.09f, 0.07f}, {0.35f, 0.22f, 0.12f}, {0.55f, 0.4f, 0.22f}, {0.5f, 0.48f, 0.45f}};
        for (int k = 0; k < 12; k++) {
            Visual rp;
            rp.casaco = cas[k % 6];
            rp.calca = cas[(k + 3) % 6] * 0.6f;
            rp.camisa = Rgb{0.82f, 0.78f, 0.68f} * (k % 3 == 1 ? 0.85f : 1.f);
            rp.chapeu = k % 2 ? Rgb{0.35f, 0.25f, 0.15f} : Rgb{0.28f, 0.2f, 0.12f};
            rp.pele = pel[k % 4];
            rp.lenco = cas[(k + 1) % 6];
            rp.cabelo = cab[(k * 7) % 4];
            rp.chapeuAba = k % 3 != 0;
            rp.sobretudo = k % 2 == 0;
            rp.barba = k % 4 == 1;
            Proporcoes pp{1.62f + 0.03f * float((k * 5) % 9), 0.9f + 0.03f * float(k % 5), 0.95f + 0.03f * float((k * 3) % 4), 0.92f + 0.04f * float(k % 4)};
            roupasNpc_.emplace_back();
            roupasNpc_.back().carregar(pp, rp);
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
        VisualCavalo vc;
        vc.pelo = pelos[i % 4];
        vc.crina = crinas[i % 4];
        vc.meias = i % 3 == 2;
        modelosCavalo_.back().carregar(vc);
    }
    if (op.comecarMontado && !cavalos_.empty()) {
        cavalos_[0].montado = true;
        montadoEm_ = 0;
        rig_.distAlvo = 6.f;
    }
    iniciarCombate();
    sons_.carregar();
    if (op.recompensa >= 0) aceitarCartaz(0, op.recompensa);
    sombra_.carregar();
    if (op.distCamera > 0) rig_.distAlvo = op.distCamera;
    for (int i = 0; i < 40; i++) rig_.atualizar(montadoEm_ >= 0 ? jogador_.pos + Vec3f{0, 0.9f, 0} : jogador_.pos, 0, 0, *terreno_, col_, 0.05f);
    atualizarCamera(0, Entrada{});
    terrenoR_->atualizar(jogador_.pos, luz_.efeitos().distanciaVisao, 0, 100000);
    cenario_.atualizar(jogador_.pos, 0, 100000);
    grama_.atualizar(jogador_.pos, 100000);
    for (int i = 0; i < 300; i++) populacao_.atualizar(0.1f, relogio_, *terreno_, col_, {1e4f, 0, 1e4f});
    if (op.tiroteio) prepararTiroteio();
    if (!op.semEntrada) DisableCursor();
}

Entrada Jogo::lerEntrada() {
    Entrada e;
    e.mirar = op_.mirar;
    if (op_.semEntrada) return e;
    e.frente = (IsKeyDown(KEY_W) ? 1.f : 0.f) - (IsKeyDown(KEY_S) ? 1.f : 0.f);
    e.lado = (IsKeyDown(KEY_D) ? 1.f : 0.f) - (IsKeyDown(KEY_A) ? 1.f : 0.f);
    e.correr = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    e.interagir = IsKeyPressed(KEY_E);
    e.segurarInteragir = IsKeyDown(KEY_E);
    e.mirar = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    e.atirar = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    e.recarregar = IsKeyPressed(KEY_R);
    Vector2 md = GetMouseDelta();
    e.mouseDx = md.x;
    e.mouseDy = md.y;
    return e;
}

void Jogo::atualizarCamera(float dt, const Entrada& e) {
    Vec3f alvo = montadoEm_ >= 0 ? cavalos_[montadoEm_].corpo.pos + Vec3f{0, 0.9f, 0} : jogador_.pos;
    rig_.atualizar(alvo, e.mouseDx, e.mouseDy, *terreno_, col_, dt);
    Vec3f p = rig_.posicao(), o = rig_.olhar();
    if (op_.retrato > 0) {
        float alt = modeloJogador_.esqueleto().ossos[CABECA].inicio.y + 0.06f;
        Vec3f f = frenteDe(jogador_.yaw + op_.girarCamera * DEG2RAD);
        o = jogador_.pos + Vec3f{0, alt - 0.1f * op_.retrato, 0};
        p = o + f * op_.retrato + Vec3f{0, 0.05f, 0};
    }
    cam_.position = {p.x, p.y, p.z};
    cam_.target = {o.x, o.y, o.z};
}

void Jogo::desenharInterface() {
    desenharAvisos(mapa_.erros, tempoTotal_);
    if (quadroAberto_ >= 0) {
        int aceito = cacada_.ativa() && cacada_.quadro() == quadroAberto_ ? cacada_.indiceCartaz() : -1;
        desenharQuadro(cartazes_[quadroAberto_], cartazSel_, semAcentos(nomeLegivel(cenario_.quadros[quadroAberto_].lugar)), aceito);
        return;
    }
    if (morteT_ >= 0) {
        desenharTelaMorte(morteT_);
        return;
    }
    if (!pausado_) {
        desenharBordaDano(vida_.fracao());
        if (cacada_.ativa()) {
            Vec3f obj = cacada_.objetivo(jogador_.pos, xerifes_);
            desenharBussola(rig_.yaw, true, posicaoNaBussola(rig_.yaw, jogador_.pos, obj, 1.6f),
                            std::hypot(obj.x - jogador_.pos.x, obj.z - jogador_.pos.z));
        } else {
            desenharBussola(rig_.yaw, false, 0, 0);
        }
        desenharHud(vida_.fracao(), arma_.balas, arma_.recarregando(), dinheiro_, mira_ > 0.5f, marcaAcerto_, marcaMatou_);
        desenharMensagem(mensagem_, mensagemT_);
        if (cacada_.podeAmarrar(jogador_.pos)) desenharDica(amarrarT_ > 0 ? "Amarrando..." : "Segure E  -  Amarrar");
        else if (xerifePerto_ >= 0 && cacada_.fase() == FaseCacada::Levando) desenharDica("E  -  Entregar ao xerife");
        else if (montadoEm_ < 0 && cavaloPerto_ < 0 && cacada_.podeLevar(jogador_.pos) && cacada_.bando().chefe()->estado == EstadoBandido::Amarrado) desenharDica("E  -  Puxar pela corda");
        else if (montadoEm_ < 0 && cavaloPerto_ < 0 && cacada_.podeLevar(jogador_.pos)) desenharDica("Traga um cavalo: monte perto do corpo para levar");
        else if (quadroPerto_ >= 0) desenharDica("E  -  Ver recompensas");
        else if (cavaloPerto_ >= 0) desenharDica("E  -  Montar");
        return;
    }
    int w = GetScreenWidth(), h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, {0, 0, 0, 140});
    const char* t1 = "PAUSADO";
    const char* t2 = "Clique para continuar   -   Q para sair";
    DrawText(t1, w / 2 - MeasureText(t1, 48) / 2, h / 2 - 50, 48, {235, 220, 190, 255});
    DrawText(t2, w / 2 - MeasureText(t2, 22) / 2, h / 2 + 10, 22, {220, 210, 190, 255});
    const char* nomes[] = {"1 - Baixo", "2 - Medio", "3 - Alto"};
    const char* t3 = "Qualidade grafica:";
    DrawText(t3, w / 2 - MeasureText(t3, 22) / 2, h / 2 + 70, 22, {220, 210, 190, 255});
    for (int k = 0; k < 3; k++) {
        bool atual = int(luz_.nivel()) == k;
        Color c = atual ? Color{255, 200, 110, 255} : Color{190, 180, 165, 255};
        int x = w / 2 - 210 + k * 150;
        if (atual) DrawRectangleLines(x - 8, h / 2 + 104, 136, 34, c);
        DrawText(nomes[k], x, h / 2 + 110, 22, c);
    }
}

Jogo::~Jogo() {
    descarregarCombate();
    grama_.descarregar();
    sons_.descarregar();
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
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) aceitarCartaz(quadroAberto_, cartazSel_);
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
            if (IsKeyPressed(KEY_ONE)) trocarNivel(Nivel::Baixo);
            if (IsKeyPressed(KEY_TWO)) trocarNivel(Nivel::Medio);
            if (IsKeyPressed(KEY_THREE)) trocarNivel(Nivel::Alto);
            return;
        }
    }
    tempo_ += dt;
    if (!op_.congelarHora) relogio_.avancar(dt);
    Entrada e = lerEntrada();
    if (morteT_ >= 0) {  // caido: sem controle
        Entrada nada;
        nada.mouseDx = e.mouseDx;
        nada.mouseDy = e.mouseDy;
        e = nada;
    }
    cavaloPerto_ = -1;
    if (montadoEm_ >= 0) {
        Cavalo& c = cavalos_[montadoEm_];
        if (e.interagir && entregarNoXerife()) {
            e.interagir = false;
        } else if (e.interagir) {
            desmontar(c, jogador_, col_);
            montadoEm_ = -1;
            rig_.distAlvo = 4.f;
            aoDesmontar();
        } else {
            moverCavalo(c, e, dt, *terreno_, col_);
            if (int(c.passada / 1.5708f) != cascoAntes_) {  // quatro patas por ciclo
                cascoAntes_ = int(c.passada / 1.5708f);
                if (c.corpo.vel > 0.3f) sons_.casco(0.25f + 0.04f * c.corpo.vel);
            }
            jogador_.pos = c.corpo.pos;
            jogador_.yaw = c.corpo.yaw;
            jogador_.vel = 0;
        }
    } else {
        cavaloPerto_ = -1;  // calculado antes da cacada: perto de cavalo, o E monta
        for (size_t i = 0; i < cavalos_.size(); i++)
            if (std::hypot(cavalos_[i].corpo.pos.x - jogador_.pos.x, cavalos_[i].corpo.pos.z - jogador_.pos.z) < DIST_MONTAR) cavaloPerto_ = int(i);
        interagirCacada(e);
        if (cacada_.carga() == Carga::Seguindo || mira_ > 0.5f) e.correr = false;
        moverAPe(jogador_, e, rig_.yaw, dt, *terreno_, col_);
        if (mira_ > 0.5f) jogador_.yaw = rig_.yaw;
        passada_ += dt * jogador_.vel * FASE_POR_METRO;
        if (int(passada_ / 3.14159f) != passoAntes_) {  // um som por passo
            passoAntes_ = int(passada_ / 3.14159f);
            if (jogador_.vel > 0.1f) sons_.passo(jogador_.vel > 3.f ? 0.55f : 0.35f);
        }
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
            rig_.ombroAlvo = 0.45f;
            cavaloPerto_ = -1;
            aoMontar();
        }
    }
    atualizarCombate(dt, e);
    sons_.ambiente(dt, relogio_.intensidadeTochas());
    populacao_.atualizar(dt, relogio_, *terreno_, col_, jogador_.pos);
    for (size_t i = 0; i < cavalos_.size(); i++)
        if (int(i) != montadoEm_) moverCavalo(cavalos_[i], Entrada{}, dt, *terreno_, col_);
    atualizarCamera(dt, e);
    terrenoR_->atualizar(jogador_.pos, luz_.efeitos().distanciaVisao, tempo_);
    cenario_.atualizar(jogador_.pos, tempo_);
    grama_.atualizar(jogador_.pos, 3);

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
    if (flashT_ > 0) luzes_.insert(luzes_.begin(), {flashPos_, Rgb{1.f, 0.75f, 0.4f} * 6.f, 9.f, 0.f});  // clarao do tiro
}

// fora do campo de visao (atras da camera, com folga): nao calcula pose nem desenha
bool Jogo::atrasDaCamera(Vec3f p) const {
    Vec3f f = normalizar(Vec3f{cam_.target.x - cam_.position.x, cam_.target.y - cam_.position.y, cam_.target.z - cam_.position.z});
    Vec3f d = p - Vec3f{cam_.position.x, cam_.position.y, cam_.position.z};
    float dist = len(d);
    if (dist < 4.f) return false;
    return dot(d, f) < dist * 0.35f;  // ~70 graus para cada lado (a tela tem ~53)
}

void Jogo::desenharJogador(Material& matPele, bool hd) {
    const Esqueleto& ej = modeloJogador_.esqueleto();
    if (montadoEm_ >= 0) {
        const Cavalo& c = cavalos_[montadoEm_];
        Pose pc = poseCavalo(modelosCavalo_[montadoEm_].esqueleto(), c.passada, int(c.andadura), tempo_ + float(montadoEm_) * 3.7f);
        Vec3f q = modelosCavalo_[montadoEm_].assento(c.corpo.pos, c.corpo.yaw, pc);
        modeloJogador_.desenhar(matPele, q - Vec3f{0, ej.ossos[QUADRIL].inicio.y, 0}, c.corpo.yaw, poseMontado(ej, c.passada, int(c.andadura)), hd);
        return;
    }
    modeloJogador_.desenhar(matPele, jogador_.pos, jogador_.yaw, poseDoJogador(ej), hd);
}

void Jogo::desenharAtores(Material& mat, Material& matPele, bool sombraBolha, float distMax) {
    for (auto& n : populacao_.npcs()) {
        if (!n.ativo) continue;
        float dx = n.corpo.pos.x - cam_.position.x, dz = n.corpo.pos.z - cam_.position.z;
        float d2 = dx * dx + dz * dz;
        if (d2 > distMax * distMax) continue;
        if (passePrincipal_ && atrasDaCamera(n.corpo.pos)) continue;
        const ModeloHumano& m = roupasNpc_[n.semente % roupasNpc_.size()];
        Pose pose = n.corpo.vel > 0.1f ? poseAndar(m.esqueleto(), n.passada, n.corpo.vel) : poseParado(m.esqueleto(), tempo_ + float(n.semente % 97));
        bool hd = passePrincipal_ && d2 < 12.f * 12.f;
        m.desenhar(matPele, n.corpo.pos, n.corpo.yaw, pose, hd);
        trisAtores_ += m.triangulos(hd);
        chamadasAtores_++;
        if (sombraBolha && d2 < 35.f * 35.f) sombra_.desenhar(n.corpo.pos, terreno_->normal(n.corpo.pos.x, n.corpo.pos.z), 0.42f);
    }
    for (size_t i = 0; i < cavalos_.size(); i++) {
        const Cavalo& c = cavalos_[i];
        Vector3 d{c.corpo.pos.x - cam_.position.x, 0, c.corpo.pos.z - cam_.position.z};
        if (d.x * d.x + d.z * d.z > distMax * distMax) continue;
        if (passePrincipal_ && int(i) != montadoEm_ && atrasDaCamera(c.corpo.pos)) continue;
        Pose pc = poseCavalo(modelosCavalo_[i].esqueleto(), c.passada, int(c.andadura), tempo_ + float(i) * 3.7f);
        modelosCavalo_[i].desenhar(matPele, c.corpo.pos, c.corpo.yaw, pc);
        trisAtores_ += modelosCavalo_[i].triangulos();
        chamadasAtores_++;
        if (int(i) == montadoEm_) desenharJogador(matPele, passePrincipal_);
    }
    if (montadoEm_ < 0) {
        desenharJogador(matPele, passePrincipal_);
        trisAtores_ += modeloJogador_.triangulos(passePrincipal_);
        chamadasAtores_++;
        if (sombraBolha) sombra_.desenhar(jogador_.pos, terreno_->normal(jogador_.pos.x, jogador_.pos.z), 0.45f);
    }
    desenharCombatentes(matPele, sombraBolha, distMax);
    for (auto& c : cavalos_) {
        if (!sombraBolha) break;
        Vec3f f = frenteDe(c.corpo.yaw);
        for (float s : {0.6f, -0.6f}) {
            Vec3f q = c.corpo.pos + f * s;
            sombra_.desenhar({q.x, terreno_->altura(q.x, q.z), q.z}, terreno_->normal(q.x, q.z), 0.6f);
        }
    }
}

void Jogo::teleportar(float x, float z, float yawGraus, float hora) {
    if (montadoEm_ >= 0) { cavalos_[montadoEm_].montado = false; montadoEm_ = -1; rig_.distAlvo = 4.f; }
    jogador_.pos = {x, terreno_->altura(x, z), z};
    jogador_.yaw = rig_.yaw = yawGraus * DEG2RAD;
    relogio_ = DayClock(hora);
    for (int i = 0; i < 30; i++) rig_.atualizar(jogador_.pos, 0, 0, *terreno_, col_, 0.05f);
    atualizarCamera(0, Entrada{});
    terrenoR_->atualizar(jogador_.pos, luz_.efeitos().distanciaVisao, tempo_, 100000);
    cenario_.atualizar(jogador_.pos, tempo_, 100000);
    grama_.atualizar(jogador_.pos, 100000);
}

void Jogo::trocarNivel(Nivel n) {
    luz_.carregar(n);
    op_.nivel = n;
    configurarGrama();
    if (!op_.arquivoConfig.empty()) salvarConfig(op_.arquivoConfig, n);
}

void Jogo::desenhar() {
    luz_.comecarCena(cam_, relogio_, luzes_, jogador_.pos, tempo_);
    if (op_.estudio) {
        // luz principal vinda de cima, a frente e a esquerda do personagem
        Vec3f dirLuz = rotY(normalizar({0.5f, 0.62f, 0.62f}), jogador_.yaw);
        luz_.modoEstudio(dirLuz, op_.argila);
        if (luz_.usaSombraPerto()) {
            luz_.comecarSombraPerto(jogador_.pos + Vec3f{0, 1.0f, 0});
            desenharJogador(luz_.materialSombraPele(), true);
            luz_.terminarSombraPerto();
        }
        ClearBackground({22, 22, 24, 255});
        BeginMode3D(cam_);
        passePrincipal_ = true;
        desenharJogador(luz_.materialPele(), true);
        EndMode3D();
        return;
    }
    if (luz_.usaSombras()) {
        float a = Iluminacao::AREA_SOMBRA * 1.2f;
        passePrincipal_ = false;  // sombras usam o modelo leve
        luz_.comecarSombra(jogador_.pos);
        terrenoR_->desenharArea(jogador_.pos, a, luz_.materialSombra());
        cenario_.desenharArea(jogador_.pos, a, luz_.materialSombra());
        desenharAtores(luz_.materialSombra(), luz_.materialSombraPele(), false, a);
        luz_.terminarSombra();
        passePrincipal_ = true;
        if (luz_.usaSombraPerto()) {  // o caçador em alta definicao, so ele, num mapa de 3 m
            Vec3f centro = jogador_.pos + Vec3f{0, montadoEm_ >= 0 ? 1.9f : 1.0f, 0};
            luz_.comecarSombraPerto(centro);
            desenharJogador(luz_.materialSombraPele(), true);
            luz_.terminarSombraPerto();
        }
    }
    if (luz_.usaPos()) luz_.comecarPos();
    ClearBackground(BLACK);
    BeginMode3D(cam_);
    luz_.desenharCeu(cam_);
    float dv = luz_.efeitos().distanciaVisao;
    luz_.modoHorizonte(dv - 40.f);
    terrenoR_->desenharHorizonte(luz_.material());
    luz_.modoHorizonte(0);
    terrenoR_->desenhar(cam_, dv, luz_.material());
    cenario_.desenhar(cam_, dv, luz_.material());
    luz_.definirRaioGrama(grama_.raio());
    rlDisableBackfaceCulling();  // folhas vistas dos dois lados
    grama_.desenhar(cam_, luz_.material());
    rlEnableBackfaceCulling();
    trisAtores_ = 0;
    chamadasAtores_ = 0;
    desenharAtores(luz_.material(), luz_.materialPele(), !luz_.usaSombras(), dv);
    desenharEfeitos();
    EndMode3D();
    if (luz_.usaPos()) luz_.terminarPos(cam_);
    tris_ = terrenoR_->triangulosDesenhados() + cenario_.triangulos() + trisAtores_ + grama_.triangulos();
    chamadas_ = terrenoR_->chamadasDesenho() + cenario_.chamadas() + chamadasAtores_ + grama_.chamadas();
}

// Grama densa por nivel: Baixo sem (so os tufos), Medio 22 m, Alto 34 m e mais folhas por m2.
void Jogo::configurarGrama() {
    if (op_.nivel == Nivel::Baixo) grama_.configurar(0, 0);
    else if (op_.nivel == Nivel::Medio) grama_.configurar(22.f, 12.f);
    else grama_.configurar(34.f, 20.f);
}
