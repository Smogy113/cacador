#include "game/jogo.h"
#include "core/meshbuilder.h"
#include "render/meshconv.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include "game/ui.h"
#include "render/estilo.h"
#include "versao.h"
#include "rlgl.h"
#include "core/anim.h"
#include "core/bussola.h"

Jogo::Jogo(const OpcoesJogo& op) : op_(op), relogio_(op.hora) {
    mapa_ = carregarMapa(op.arquivoMapa);
    terreno_ = std::make_unique<Terreno>(mapa_, 1337);
    terrenoR_ = std::make_unique<TerrenoRender>(*terreno_);
#if !defined(PLATFORM_WEB)
    terrenoR_->definirMalhaFina(op.nivel == Nivel::Alto);
#endif
    cenario_.construir(mapa_, *terreno_, col_, 1337);
    luz_.carregar(op.nivel);
    luz_.dbg.semCabecaRaio = op.semCabecaRaio;
    luz_.dbg.semLuzPersonagem = op.semLuzPersonagem;
    luz_.dbg.semFoco = op.semFoco;
    luz_.dbg.semParedes = op.semParedes;
    estilo::carregar();
    grama_.iniciar(*terreno_, col_, cenario_.interiores(), cenario_.regioesCapim());
    nav_ = std::make_unique<Navegador>(*terreno_, &mapa_);
    mapaTela_.carregar(*terreno_, mapa_, cenario_.estruturasMapa);
    configurarGrama();

    // comeca na entrada de Pedrabranca, olhando para a ponte
    float x = op.temPos ? op.x : -40.f, z = op.temPos ? op.z : 10.f, yaw = (op.temPos ? op.yaw : 160.f) * DEG2RAD;
    jogador_.pos = {x, terreno_->altura(x, z), z};
    jogador_.yaw = yaw;
    rig_.yaw = yaw + op.girarCamera * DEG2RAD;
    rig_.pitch = op.pitch > -900 ? op.pitch * DEG2RAD : 0.22f;
    cam_.up = {0, 1, 0};
    cam_.fovy = 60.f;
    cam_.projection = CAMERA_PERSPECTIVE;
    Visual r;
    r.casaco = {0.30f, 0.18f, 0.11f};
    r.calca = {0.24f, 0.21f, 0.2f};
    r.camisa = {0.72f, 0.66f, 0.55f};
    r.chapeu = {0.2f, 0.15f, 0.11f};
    r.pele = {0.78f, 0.58f, 0.45f};
    r.lenco = {0.66f, 0.054f, 0.037f};
    r.cabelo = {0.27f, 0.2f, 0.14f};  // castanho com mechas mais claras
    r.barba = false;  // rosto limpo (pedido do usuario)
    r.equipamento = false;
    r.heroi = true;
    r.corpoBase = true;
    r.semCabelo = !op.cabeloOndulado;
    r.cabeloOndulado = op.cabeloOndulado;
    r.cacadorDeFronteira = false;
    r.revolver = false;
    r.cabeloLongo = false;
    r.chapeuAba = false;
    r.sobretudo = false;
    modeloJogador_.carregarHeroi(Proporcoes{1.82f, 1.07f, 1.f, 1.10f}, r);
    {   // folha de porta: tabuas, travessas, dobradicas de ferro (dobradica na origem, folha ao longo de +x)
        MeshBuilder m;
        const Rgb tons[4] = {{0.36f, 0.25f, 0.16f}, {0.32f, 0.22f, 0.14f}, {0.39f, 0.27f, 0.17f}, {0.34f, 0.23f, 0.15f}};
        for (int k = 0; k < 4; k++) m.caixa({0.1375f + k * 0.275f, 1.075f, 0}, {0.27f, 2.15f, 0.05f}, 0, tons[k]);
        for (float y : {0.45f, 1.7f})
            for (float z : {-0.035f, 0.035f}) m.caixa({0.55f, y, z}, {1.0f, 0.14f, 0.03f}, 0, Rgb{0.27f, 0.19f, 0.13f});
        for (float y : {0.45f, 1.7f})
            for (float z : {-0.055f, 0.055f}) m.caixa({0.3f, y, z}, {0.55f, 0.05f, 0.012f}, 0, Rgb{0.15f, 0.15f, 0.16f});
        m.caixa({0.95f, 1.05f, 0.06f}, {0.05f, 0.16f, 0.03f}, 0, Rgb{0.15f, 0.15f, 0.16f});  // puxador
        m.calcularAO(nullptr, 6);
        malhaPorta_ = paraMesh(m);
    }
    if (op.portasAbertas)
        for (auto& p : cenario_.portas()) p.querAberta = true, p.abertura = 1.f;
    if (op.semEntrada) modeloJogador_.esperarCabeca();
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
    const Rgb pelos[] = {{0.43f, 0.28f, 0.20f}, {0.16f, 0.13f, 0.12f}, {0.78f, 0.74f, 0.68f}, {0.6f, 0.42f, 0.24f}};
    const Rgb crinas[] = {{0.18f, 0.13f, 0.10f}, {0.08f, 0.07f, 0.07f}, {0.55f, 0.5f, 0.45f}, {0.85f, 0.8f, 0.65f}};
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
    mapaAberto_ = op.abrirMapa;
    dbg_.aberto = op.abrirDepuracao;
    atualizarRota(0.f);
    if (!op.semEntrada) DisableCursor();
}

Entrada Jogo::lerEntrada() {
    Entrada e;
    e.mirar = op_.mirar;
    e.frente = op_.andar > 0 ? 1.f : 0.f;
    e.correr = op_.andar == 2;
    if (op_.semEntrada) return e;
    e.frente = (IsKeyDown(KEY_W) ? 1.f : 0.f) - (IsKeyDown(KEY_S) ? 1.f : 0.f);
    e.lado = (IsKeyDown(KEY_D) ? 1.f : 0.f) - (IsKeyDown(KEY_A) ? 1.f : 0.f);
    e.correr = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    e.interagir = IsKeyPressed(KEY_E);
    e.segurarInteragir = IsKeyDown(KEY_E);
    e.mirar = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    e.atirar = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    e.recarregar = IsKeyPressed(KEY_R);
    if (dbg_.aberto) {  // o mouse e do painel
        e.mirar = e.atirar = false;
        e.mouseDx = e.mouseDy = 0;
    }
    Vector2 md = GetMouseDelta();
    e.mouseDx = md.x;
    e.mouseDy = md.y;
    return e;
}

void Jogo::atualizarCamera(float dt, const Entrada& e) {
    Vec3f alvo = montadoEm_ >= 0 ? cavalos_[montadoEm_].corpo.pos + Vec3f{0, 0.9f, 0} : jogador_.pos;
    // dentro de casa: camera perto do ombro (o combate redefine a distancia todo quadro; so aqui e limitada)
    float distAntes = rig_.distAlvo, ombroAntes = rig_.ombroAlvo;
    if (montadoEm_ < 0 && dentroDeInterior(cenario_.interiores(), jogador_.pos) >= 0) {
        rig_.distAlvo = std::min(rig_.distAlvo, 1.8f);
        rig_.ombroAlvo = std::min(rig_.ombroAlvo, 0.35f);
    }
    rig_.atualizar(alvo, e.mouseDx, e.mouseDy, *terreno_, col_, dt);
    rig_.distAlvo = distAntes;
    rig_.ombroAlvo = ombroAntes;
    Vec3f p = rig_.posicao(), o = rig_.olhar();
    if (op_.retrato > 0) {
        float alt = op_.estudio && op_.retrato > 1.f ? 0.91f : modeloJogador_.esqueleto().ossos[CABECA].inicio.y + 0.06f;
        Vec3f f = frenteDe(jogador_.yaw + op_.girarCamera * DEG2RAD);
        o = jogador_.pos + Vec3f{0, alt - (op_.estudio && op_.retrato > 1.f ? 0.f : 0.1f * op_.retrato), 0};
        p = o + f * op_.retrato + Vec3f{0, 0.05f, 0};
    }
    cam_.position = {p.x, p.y, p.z};
    cam_.target = {o.x, o.y, o.z};
}

void Jogo::desenharInterface() {
    desenharAvisos(mapa_.erros, tempoTotal_);
    if (quadroAberto_ >= 0) {
        int aceito = cacada_.ativa() && cacada_.quadro() == quadroAberto_ ? cacada_.indiceCartaz() : -1;
        desenharQuadro(cartazes_[quadroAberto_], cartazSel_, nomeLegivel(cenario_.quadros[quadroAberto_].lugar), aceito);
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
        desenharHud(vida_.fracao(), arma_.balas, arma_.recarregando(), dinheiro_, mira_ > 0.5f, marcaAcerto_, marcaMatou_, MapaTela::centroMini(), MapaTela::RAIO_MINI);
        desenharMensagem(mensagem_, mensagemT_);
        if (!mapaAberto_)
            mapaTela_.desenharMini(jogador_.pos, montadoEm_ >= 0 ? cavalos_[montadoEm_].corpo.yaw : jogador_.yaw, rig_.yaw, rota_,
                                   cacada_.ativa(), objetivoRota_);
        else
            mapaTela_.desenharCheio(jogador_.pos, montadoEm_ >= 0 ? cavalos_[montadoEm_].corpo.yaw : jogador_.yaw, rota_, cacada_.ativa(),
                                    objetivoRota_);
        if (dbg_.aberto) desenharDepuracao();
        if (cacada_.podeAmarrar(jogador_.pos)) desenharDica(amarrarT_ > 0 ? "" : "Segure E", amarrarT_ > 0 ? "Amarrando..." : "amarrar");
        else if (xerifePerto_ >= 0 && cacada_.fase() == FaseCacada::Levando) desenharDica("E", "entregar ao xerife");
        else if (montadoEm_ < 0 && cavaloPerto_ < 0 && cacada_.podeLevar(jogador_.pos) && cacada_.bando().chefe()->estado == EstadoBandido::Amarrado) desenharDica("E", "puxar pela corda");
        else if (montadoEm_ < 0 && cavaloPerto_ < 0 && cacada_.podeLevar(jogador_.pos)) desenharDica("", "Traga um cavalo: monte perto do corpo para levar");
        else if (quadroPerto_ >= 0) desenharDica("E", "ver recompensas");
        else if (cavaloPerto_ >= 0) desenharDica("E", "montar");
        else if (portaPerto_ >= 0) desenharDica("E", cenario_.portas()[portaPerto_].querAberta ? "fechar a porta" : "abrir a porta");
        return;
    }
    desenharPausa(int(luz_.nivel()), VERSAO_JOGO);
}

Jogo::~Jogo() {
    descarregarCombate();
    estilo::descarregar();
    mapaTela_.descarregar();
    grama_.descarregar();
    sons_.descarregar();
    for (auto& m : roupasNpc_) m.descarregar();
    for (auto& m : modelosCavalo_) m.descarregar();
    cenario_.descarregar();
    modeloJogador_.descarregar();
    if (malhaPorta_.vertexCount > 0) UnloadMesh(malhaPorta_);
    sombra_.descarregar();
    terrenoR_.reset();
    luz_.descarregar();
}

PisoSonoro Jogo::pisoSonoro(Vec3f p) const {
    const Terreno& t = *terreno_;
    float terrenoY = t.altura(p.x, p.z);
    float pisoY = col_.alturaPiso(p.x, p.z, p.y);
    if (pisoY > terrenoY + .08f && std::abs(p.y - pisoY) < .23f) return PisoSonoro::Madeira;
    if (p.y < t.nivelAgua() - .06f && t.mascaraRio(p.x, p.z) > .16f) return PisoSonoro::Agua;
    if (terrenoY > 50.f || t.normal(p.x, p.z).y < .79f) return PisoSonoro::Pedra;
    if (t.mascaraEstrada(p.x, p.z) > .43f || terrenoY < t.nivelAgua() + .9f) return PisoSonoro::Terra;
    return PisoSonoro::Capim;
}

float Jogo::panSonoro(Vec3f fonte, float abertura) const {
    Vec3f d{fonte.x - jogador_.pos.x, 0, fonte.z - jogador_.pos.z};
    Vec3f direitaCamera = rotY({-1, 0, 0}, rig_.yaw);
    return .5f - abertura * dot(d, direitaCamera) / std::max(.2f, len(d));
}

void Jogo::atualizarAmbienteSonoro(float dt, bool pausado) {
    if (!sons_.ok()) return;
    bool interior = dentroDeInterior(cenario_.interiores(), jogador_.pos) >= 0;
    sons_.ambiente(dt, relogio_.intensidadeTochas(), interior, pausado);
}

void Jogo::atualizar(float dt) {
    double tA = GetTime();
    struct Medir {  // tempo de processador da logica (painel F1)
        double t;
        float& saida;
        ~Medir() { saida = saida * 0.9f + float(GetTime() - t) * 1000.f * 0.1f; }
    } medir{tA, dbg_.msAtualizar};
    dt = limitarDt(dt);
    tempoTotal_ += dt;
    if (quadroAberto_ >= 0) {
        int n = int(cartazes_[quadroAberto_].size());
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) cartazSel_ = (cartazSel_ + 1) % n;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) cartazSel_ = (cartazSel_ + n - 1) % n;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) aceitarCartaz(quadroAberto_, cartazSel_);
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ESCAPE)) quadroAberto_ = -1;
        if (!op_.congelarHora) relogio_.avancar(dt * dbg_.velTempo);
        atualizarAmbienteSonoro(dt);
        return;
    }
    // painel de depuracao (F1): mouse livre; "pausar mundo" congela tudo menos o painel
    dbg_.msQuadro = dbg_.msQuadro * 0.9f + GetFrameTime() * 1000.f * 0.1f;
    if (!op_.semEntrada && IsKeyPressed(KEY_F1)) {
        dbg_.aberto = !dbg_.aberto;
        if (dbg_.aberto) EnableCursor(); else if (!pausado_) DisableCursor();
    }
    if (dbg_.aberto && dbg_.pausado) {
        atualizarAmbienteSonoro(dt, true);
        return;
    }
    // mapa grande (M): o jogo continua, o jogador fica parado
    if (!op_.semEntrada && IsKeyPressed(KEY_M) && !pausado_) mapaAberto_ = !mapaAberto_;
    if (mapaAberto_) {
        if (IsKeyPressed(KEY_ESCAPE)) mapaAberto_ = false;
        if (!op_.congelarHora) relogio_.avancar(dt * dbg_.velTempo);
        atualizarRota(dt);
        atualizarAmbienteSonoro(dt);
        return;
    }
    // pausa: Esc, ou a janela perdeu o foco
    if (!op_.semEntrada) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            pausado_ = !pausado_;
            if (pausado_) EnableCursor(); else DisableCursor();
        }
        if (!IsWindowFocused() && !pausado_ && !dbg_.aberto) { pausado_ = true; EnableCursor(); }
        if (pausado_) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !dbg_.aberto) { pausado_ = false; DisableCursor(); }
            if (IsKeyPressed(KEY_Q)) sair_ = true;
            if (IsKeyPressed(KEY_ONE)) trocarNivel(Nivel::Baixo);
            if (IsKeyPressed(KEY_TWO)) trocarNivel(Nivel::Medio);
            if (IsKeyPressed(KEY_THREE)) trocarNivel(Nivel::Alto);
            atualizarAmbienteSonoro(dt, pausado_);
            return;
        }
    }
    tempo_ += dt;
    if (!op_.congelarHora) relogio_.avancar(dt * dbg_.velTempo);
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
            float faseAnterior = c.passada;
            moverCavalo(c, e, dt * dbg_.velJogador, *terreno_, col_);
            jogador_.pos = c.corpo.pos;
            jogador_.yaw = c.corpo.yaw;
            jogador_.vel = 0;
            if (sons_.ok()) {
                ContatosCavalo contatos = contatosCavalo(c.andadura, faseAnterior, c.passada);
                if (contatos.total > 0) {
                    PisoSonoro piso = pisoSonoro(c.corpo.pos);
                    float volume = lerp(.44f, .67f, c.corpo.vel / VEL_GALOPE);
                    for (int i = 0; i < contatos.total; ++i) {
                        const BatidaCasco& batida = contatos.batidas[i];
                        Vec3f fonte = c.corpo.pos + direitaJogador() * (batida.lado * .22f);
                        sons_.casco(piso, volume * batida.intensidade, panSonoro(fonte, .09f));
                    }
                }
            }
        }
    } else {
        cavaloPerto_ = -1;  // calculado antes da cacada: perto de cavalo, o E monta
        for (size_t i = 0; i < cavalos_.size(); i++)
            if (std::hypot(cavalos_[i].corpo.pos.x - jogador_.pos.x, cavalos_[i].corpo.pos.z - jogador_.pos.z) < DIST_MONTAR) cavaloPerto_ = int(i);
        interagirCacada(e);
        if (cacada_.carga() == Carga::Seguindo || mira_ > 0.5f) e.correr = false;
        Vec3f antes = jogador_.pos;
        moverAPe(jogador_, e, rig_.yaw, dt * dbg_.velJogador, *terreno_, col_);
        if (mira_ > 0.5f) jogador_.yaw = rig_.yaw;
        // a animacao segue o que o corpo andou de fato (parede segura o passo); mirando, o yaw segue a
        // camera e o corpo nao tomba nas puxadas do mouse
        loco_.atualizar(dt, jogador_.pos - antes, jogador_.yaw, mira_ <= 0.5f);
        if (loco_.passo() != passoAntes_) {  // um som a cada toque de calcanhar
            passoAntes_ = loco_.passo();
            if (sons_.ok() && loco_.vel > .1f) {
                float lado = (passoAntes_ % 2 != 0) ? .14f : -.14f;
                Vec3f fonte = jogador_.pos + direitaJogador() * lado;
                float esforco = smoothstep(.2f, 4.6f, loco_.vel);
                sons_.passo(pisoSonoro(fonte), lerp(.27f, .55f, esforco), panSonoro(fonte, .09f));
            }
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
            e.interagir = false;
        }
    }
    atualizarPortas(dt, montadoEm_ < 0 && e.interagir);
    atualizarCombate(dt, e);
    atualizarCabelo(dt);
    atualizarRota(dt);
    atualizarAmbienteSonoro(dt);
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
    for (const Clarao& flash : claroes_) {
        float brilho = clamp01(flash.vida / DURACAO_FLASH);
        luzes_.insert(luzes_.begin(), {flash.pos, Rgb{1.f, 0.64f, 0.32f} * (4.5f * brilho), 4.f, 0.f});
    }
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
    // cabeca por raymarching: so no passe principal (as sombras usam a malha), Alto/Medio, perto da camera
    Material* matCabeca = nullptr;
    if (hd && &matPele == &luz_.materialPele() && luz_.usaCabecaRaio()) {
        Vec3f d = jogador_.pos + Vec3f{0, 1.7f, 0} - Vec3f{cam_.position.x, cam_.position.y, cam_.position.z};
        if (dot(d, d) < 15.f * 15.f) matCabeca = &luz_.materialCabeca();
    }
    luz_.definirHeroi(true);
    auto desenhar = [&](Vec3f pos, float yaw, const Pose& pose) {
        if (op_.cabeloOndulado)
            luz_.definirCabelo(matPele, cabelo_.deformacao(), ej.ossos[CABECA].fim.y / 1.8f);
        modeloJogador_.desenhar(matPele, pos, yaw, pose, hd, matCabeca);
        if (op_.cabeloOndulado) luz_.definirCabelo(matPele, {}, 0.f);
    };
    if (montadoEm_ >= 0) {
        const Cavalo& c = cavalos_[montadoEm_];
        Pose pc = poseCavalo(modelosCavalo_[montadoEm_].esqueleto(), c.passada, int(c.andadura), tempo_ + float(montadoEm_) * 3.7f);
        Vec3f q = modelosCavalo_[montadoEm_].assento(c.corpo.pos, c.corpo.yaw, pc);
        desenhar(q - Vec3f{0, ej.ossos[QUADRIL].inicio.y, 0}, c.corpo.yaw, poseMontado(ej, c.passada, int(c.andadura)));
    } else {
        desenhar(jogador_.pos, jogador_.yaw, poseDoJogador(ej));
    }
    luz_.definirHeroi(false);
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
    constexpr float DIST_DETALHE_CAVALO = 22.f;
    for (size_t i = 0; i < cavalos_.size(); i++) {
        const Cavalo& c = cavalos_[i];
        float dx = c.corpo.pos.x - cam_.position.x, dz = c.corpo.pos.z - cam_.position.z;
        float d2 = dx * dx + dz * dz;
        if (d2 > distMax * distMax) continue;
        if (passePrincipal_ && int(i) != montadoEm_ && atrasDaCamera(c.corpo.pos)) continue;
        // O mapa de sombras usa a silhueta leve com os mesmos ossos.
        bool perto = passePrincipal_ && d2 < DIST_DETALHE_CAVALO * DIST_DETALHE_CAVALO;
        const ModeloCavaloAnat& modelo = modelosCavalo_[i];
        Pose pc = poseCavalo(modelo.esqueleto(), c.passada, int(c.andadura), tempo_ + float(i) * 3.7f);
        modelo.desenhar(matPele, c.corpo.pos, c.corpo.yaw, pc, perto);
        trisAtores_ += modelo.triangulos(perto);
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
    cabelo_.reiniciar();
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
#if !defined(PLATFORM_WEB)
    terrenoR_->definirMalhaFina(n == Nivel::Alto);
#endif
    op_.nivel = n;
    configurarGrama();
    if (!op_.arquivoConfig.empty()) salvarConfig(op_.arquivoConfig, {n, op_.cabeloOndulado});
}

void Jogo::desenhar() {
    double t0 = GetTime();
    perfil_.ligar(dbg_.aberto);  // so mede com o painel F1 aberto
    perfil_.comecarQuadro();
    luz_.comecarCena(cam_, relogio_, luzes_, jogador_.pos, tempo_);
    if (!luzParedesPronta_ && luz_.nivel() != Nivel::Baixo && !luz_.dbg.semParedes) {
        luz_.prepararLuzParedes(cenario_.formas());
        luzParedesPronta_ = true;
    }
    perfil_.comecar(P_LUZ_PAREDES);
    luz_.atualizarLuzParedes({cam_.position.x, cam_.position.y, cam_.position.z});
    perfil_.terminar();
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
    perfil_.comecar(P_SOMBRAS);
    if (luz_.usaSombras()) {
        float a = Iluminacao::AREA_SOMBRA * 1.2f;
        passePrincipal_ = false;  // sombras usam o modelo leve
        luz_.comecarSombra(jogador_.pos);
        terrenoR_->desenharArea(jogador_.pos, a, luz_.materialSombraOpaca());
        cenario_.desenharArea(jogador_.pos, a, luz_.materialSombra(), luz_.materialSombraOpaca());
        desenharPortas(luz_.materialSombra(), 30.f);
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
    perfil_.terminar();
    Vec3f posCamera{cam_.position.x, cam_.position.y, cam_.position.z};
    luz_.definirInteriores(cenario_.interiores(), cenario_.portas(), posCamera);
    luz_.definirJanelas(cenario_.janelas(), posCamera);
    if (luz_.usaPos()) luz_.comecarPos();
    ClearBackground(BLACK);
    BeginMode3D(cam_);
    luz_.comecarPassePrincipal();
    float dv = luz_.efeitos().distanciaVisao;
    float distDetalhe = luz_.nivel() == Nivel::Alto ? 35.f : (luz_.nivel() == Nivel::Medio ? 30.f : -1e9f);
    perfil_.comecar(P_TERRENO);
    cenario_.prepararPassePrincipal(cam_, dv, distDetalhe);
    // A profundidade das construcoes evita sombrear o terreno encoberto. As chamas ficam para a cor.
    if (luz_.usaProfundidadeConstrucoes()) cenario_.desenharProfundidadeConstrucoes(luz_.materialSombra());
    luz_.modoHorizonte(dv - 40.f);
    terrenoR_->desenharHorizonte(luz_.material());
    luz_.modoHorizonte(0);
    terrenoR_->desenhar(cam_, dv, luz_.materialSolo(), luz_.material());
    perfil_.terminar();
    perfil_.comecar(P_CONSTRUCOES);
    cenario_.desenharConstrucoes(luz_.materialConstrucoes());
    perfil_.terminar();
    desenharPortas(luz_.material(), 45.f);
    perfil_.comecar(P_ARVORES);
    cenario_.desenharArvores(luz_.material());
    perfil_.terminar();
    perfil_.comecar(P_GRAMA);
    luz_.definirRaioGrama(grama_.raio());
    rlDisableBackfaceCulling();  // folhas vistas dos dois lados
    grama_.desenhar(cam_, luz_.material());
    rlEnableBackfaceCulling();
    perfil_.terminar();
    trisAtores_ = 0;
    chamadasAtores_ = 0;
    perfil_.comecar(P_ATORES);
    desenharAtores(luz_.material(), luz_.materialPele(), !luz_.usaSombras(), dv);
    perfil_.terminar();
    perfil_.comecar(P_CEU);
    luz_.desenharCeu(cam_);  // depois da cena: o ceu (e as nuvens) so e calculado onde aparece
    perfil_.terminar();
    perfil_.comecar(P_EFEITOS);
    desenharEfeitos();
    perfil_.terminar();
    EndMode3D();
    {
        Vec3f heroi = jogador_.pos + Vec3f{0, montadoEm_ >= 0 ? 2.3f : 1.5f, 0};
        luz_.definirFoco(len(heroi - Vec3f{cam_.position.x, cam_.position.y, cam_.position.z}));
    }
    if (luz_.usaPos()) {
        float tempoGrao = op_.semEntrada ? tempo_ : float(GetTime());
        luz_.terminarPos(cam_, tempoGrao, &perfil_);
    }
    luz_.terminarPassePrincipal();
    dbg_.msDesenharCPU = dbg_.msDesenharCPU * 0.9f + float(GetTime() - t0) * 1000.f * 0.1f;
    tris_ = terrenoR_->triangulosDesenhados() + cenario_.triangulos() + trisAtores_ + grama_.triangulos();
    chamadas_ = terrenoR_->chamadasDesenho() + cenario_.chamadas() + chamadasAtores_ + grama_.chamadas();
}

// Grama densa por nivel: Baixo sem (so os tufos), Medio 22 m, Alto 34 m e mais folhas por m2.
void Jogo::configurarGrama() {
    if (op_.nivel == Nivel::Baixo) grama_.configurar(0, 0);
    else if (op_.nivel == Nivel::Medio) grama_.configurar(22.f, 12.f);
    else grama_.configurar(34.f, 20.f);
}

void Jogo::atualizarCabelo(float dt) {
    if (!op_.cabeloOndulado || dt <= 0.f) return;
    const Esqueleto& e = modeloJogador_.esqueleto();
    Vec3f base = jogador_.pos;
    float yaw = jogador_.yaw;
    Pose pose;
    if (montadoEm_ >= 0) {
        const Cavalo& cavalo = cavalos_[montadoEm_];
        const auto& modelo = modelosCavalo_[montadoEm_];
        Pose cav = poseCavalo(modelo.esqueleto(), cavalo.passada, int(cavalo.andadura), tempo_ + float(montadoEm_) * 3.7f);
        base = modelo.assento(cavalo.corpo.pos, cavalo.corpo.yaw, cav) - Vec3f{0, e.ossos[QUADRIL].inicio.y, 0};
        yaw = cavalo.corpo.yaw;
        pose = poseMontado(e, cavalo.passada, int(cavalo.andadura));
    } else {
        pose = poseDoJogador(e);
    }
    static thread_local std::vector<Mat4> ossos;
    calcularMatrizes(e, pose, ossos);
    Mat4 mundo = Mat4::trans(base) * Mat4::rotY(yaw);
    Vec3f vento{1.0f + .7f * std::sin(tempo_ * 1.9f), 0, .8f * std::cos(tempo_ * 1.1f)};
    cabelo_.atualizar(dt, mundo * ossos[CABECA], mundo * ossos[PESCOCO], e.ossos[CABECA].fim.y / 1.8f, vento);
}

// Rota ate o objetivo da recompensa (esconderijo -> procurado -> xerife), recalculada quando o
// objetivo se move, quando o jogador sai do caminho ou a cada 2 s.
void Jogo::atualizarRota(float dt) {
    if (!cacada_.ativa() || !nav_) {
        rota_.clear();
        return;
    }
    Vec3f obj = cacada_.objetivo(jogador_.pos, xerifes_);
    tempoRota_ -= dt;
    float longe = 1e9f;  // distancia do jogador ao caminho atual
    for (size_t i = 1; i < rota_.size(); i++) {
        Vec3f a = rota_[i - 1], b = rota_[i], ab = b - a, ap = jogador_.pos - a;
        float t = std::clamp((ap.x * ab.x + ap.z * ab.z) / std::max(1e-3f, ab.x * ab.x + ab.z * ab.z), 0.f, 1.f);
        longe = std::min(longe, std::hypot(ap.x - ab.x * t, ap.z - ab.z * t));
    }
    bool mudou = std::hypot(obj.x - objetivoRota_.x, obj.z - objetivoRota_.z) > 12.f;
    if (rota_.empty() || mudou || longe > 20.f || tempoRota_ <= 0) {
        rota_ = nav_->rota(jogador_.pos, obj);
        objetivoRota_ = obj;
        tempoRota_ = 2.f;
    } else if (!rota_.empty()) {
        rota_.front() = jogador_.pos;  // a linha sai sempre de onde o jogador esta
    }
}

// Portas: a mais perto na frente do cacador abre/fecha com E (nao fecha com alguem no vao);
// as folhas perto giram ate o alvo em ~0,4 s e entram na colisao como caixas dinamicas.
void Jogo::atualizarPortas(float dt, bool interagir) {
    auto& ps = cenario_.portas();
    portaPerto_ = -1;
    float melhor = 1.7f;
    Vec3f frente = frenteDe(jogador_.yaw);
    for (size_t i = 0; i < ps.size(); i++) {
        const PortaMundo& p = ps[i];
        Vec3f c = p.dobradica + rotY({p.largura * 0.5f, 0, 0}, p.rot);
        Vec3f d{c.x - jogador_.pos.x, 0, c.z - jogador_.pos.z};
        float dist = len(d);
        if (dist >= melhor) continue;
        if (dist > 0.8f && dot(frente, d * (1.f / dist)) < 0.25f) continue;  // de costas para a porta
        melhor = dist;
        portaPerto_ = int(i);
    }
    if (interagir && portaPerto_ >= 0) {
        PortaMundo& p = ps[portaPerto_];
        if (!p.querAberta) {
            p.querAberta = true;
            sons_.moverPorta(1.f / (1.f + melhor * .27f), panSonoro(p.dobradica, .30f));
        } else if (portaPodeFechar(p, jogador_.pos, RAIO_PESSOA)) {
            p.querAberta = false;
            sons_.moverPorta(.75f / (1.f + melhor * .27f), panSonoro(p.dobradica, .30f));
        }
    }
    std::vector<CaixaGirada> folhas;
    for (auto& p : ps) {
        float alvo = p.querAberta ? 1.f : 0.f;
        if (p.abertura != alvo) {
            if (alvo == 0.f && !portaPodeFechar(p, jogador_.pos, RAIO_PESSOA)) {
                p.querAberta = true;
            } else {
                p.abertura = alvo > p.abertura ? std::min(alvo, p.abertura + dt / 0.4f) : std::max(alvo, p.abertura - dt / 0.4f);
                if (p.abertura == 0.f && sons_.ok()) {
                    Vec3f fecho = p.dobradica + rotY({p.largura, 0, 0}, p.rot);
                    float dist = std::hypot(fecho.x - jogador_.pos.x, fecho.z - jogador_.pos.z);
                    sons_.fecharPorta(smoothstep(12.f, 2.f, dist) / (1.f + dist * .28f), panSonoro(fecho, .34f));
                }
            }
        }
        if (std::hypot(p.dobradica.x - jogador_.pos.x, p.dobradica.z - jogador_.pos.z) < 25.f) folhas.push_back(folhaPorta(p));
    }
    col_.definirDinamicas(std::move(folhas));
}

void Jogo::desenharPortas(Material& mat, float raio) {
    if (malhaPorta_.vertexCount == 0) return;
    Vec3f c{cam_.position.x, cam_.position.y, cam_.position.z};
    for (auto& p : cenario_.portas()) {
        if (std::hypot(p.dobradica.x - c.x, p.dobradica.z - c.z) > raio) continue;
        float ang = p.rot + p.abertura * PI * 0.5f * p.sentido;
        Matrix t = MatrixMultiply(MatrixMultiply(MatrixScale(p.largura / 1.1f, p.altura / 2.15f, 1.f), MatrixRotateY(ang)),
                                  MatrixTranslate(p.dobradica.x, p.dobradica.y, p.dobradica.z));
        DrawMesh(malhaPorta_, mat, t);
        chamadasAtores_++;
    }
}
