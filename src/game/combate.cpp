// Recompensas, tiroteio, captura e entrega (a regra fica em core/cacada, core/bando e core/combat).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/anim.h"
#include "core/noise.h"
#include "game/jogo.h"
#include "rlgl.h"

namespace {
float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vec3f direitaDe(float yaw) { return {-std::cos(yaw), 0, std::sin(yaw)}; }
// troca o braco direito de uma pose pelo da mira (andar mirando)
void bracoDeMira(Pose& p, const Pose& mira) {
    for (int o : {BRACO_D, ANTEBRACO_D, MAO_D}) p.rot[o] = mira.rot[o];
    p.rot[CABECA] = mira.rot[CABECA];
}
void bracosPresos(Pose& p, const Pose& amarrado) {
    for (int o : {BRACO_E, BRACO_D, ANTEBRACO_E, ANTEBRACO_D, MAO_E, MAO_D}) p.rot[o] = amarrado.rot[o];
}
}  // namespace

void Jogo::iniciarCombate() {
    Image fumaca = GenImageColor(64, 64, BLANK);
    for (int y = 0; y < fumaca.height; ++y) {
        for (int x = 0; x < fumaca.width; ++x) {
            float u = (x + 0.5f) / 32.f - 1.f, v = (y + 0.5f) / 32.f - 1.f;
            float r = std::hypot(u, v);
            float n = std::sin(13.f * u + 8.f * v) * std::sin(11.f * v - 3.f * u);
            float alfa = (1.f - smoothstep(0.05f, 0.95f, r + 0.09f * n)) * (0.87f + 0.13f * n);
            ImageDrawPixel(&fumaca, x, y, Fade(WHITE, alfa));
        }
    }
    texturaFumaca_ = LoadTextureFromImage(fumaca);
    UnloadImage(fumaca);
    SetTextureFilter(texturaFumaca_, TEXTURE_FILTER_BILINEAR);
    Visual chefe, cap, xer;
    chefe.revolver = cap.revolver = true;
    chefe.casaco = {0.36f, 0.12f, 0.1f};
    chefe.calca = {0.16f, 0.14f, 0.13f};
    chefe.chapeu = {0.08f, 0.07f, 0.07f};
    chefe.lenco = {0.1f, 0.1f, 0.1f};
    chefe.barba = true;
    chefe.pele = {0.7f, 0.5f, 0.38f};
    modelosBando_.emplace_back();
    modelosBando_.back().carregar(Proporcoes{1.84f, 1.08f, 1.f, 1.05f}, chefe);
    const Rgb casacos[] = {{0.2f, 0.18f, 0.16f}, {0.28f, 0.24f, 0.18f}, {0.18f, 0.2f, 0.22f}};
    const Rgb peles[] = {{0.62f, 0.44f, 0.32f}, {0.82f, 0.62f, 0.48f}, {0.5f, 0.35f, 0.25f}};
    for (int k = 0; k < 3; k++) {
        cap.casaco = casacos[k];
        cap.calca = casacos[(k + 1) % 3] * 0.8f;
        cap.chapeu = casacos[(k + 2) % 3] * 0.9f;
        cap.lenco = Rgb{0.3f, 0.08f, 0.06f} * (0.7f + 0.15f * k);
        cap.pele = peles[k];
        cap.sobretudo = k != 1;
        cap.barba = k == 2;
        modelosBando_.emplace_back();
        modelosBando_.back().carregar(Proporcoes{1.72f + 0.05f * k, 1.f, 1.f, 1.f}, cap);
    }
    xer.casaco = {0.2f, 0.17f, 0.14f};
    xer.camisa = {0.82f, 0.8f, 0.74f};
    xer.chapeu = {0.6f, 0.5f, 0.34f};
    xer.lenco = {0.15f, 0.2f, 0.35f};
    xer.barba = true;
    xer.sobretudo = false;
    modeloXerife_.carregar(Proporcoes{1.8f, 1.05f, 1.05f, 1.1f}, xer);
    // um xerife ao lado de cada quadro, de frente para quem chega
    for (auto& q : cenario_.quadros) {
        Vec3f p = q.pos + direitaDe(q.yaw) * 1.6f + frenteDe(q.yaw) * 0.9f;
        p.y = terreno_->altura(p.x, p.z);
        xerifes_.push_back(p);
        yawXerifes_.push_back(q.yaw);
    }
}

void Jogo::descarregarCombate() {
    UnloadTexture(texturaFumaca_);
    for (auto& m : modelosBando_) m.descarregar();
    modeloXerife_.descarregar();
}

void Jogo::avisar(const std::string& t) {
    mensagem_ = t;
    mensagemT_ = 3.5f;
}

void Jogo::aceitarCartaz(int q, int i) {
    if (q < 0 || q >= int(cartazes_.size()) || i < 0 || i >= int(cartazes_[q].size())) return;
    cacada_.aceitar(cartazes_[q][i], q, i, mapa_, *terreno_, col_);
    estadoChefeAntes_ = EstadoBandido::Vigiando;
    avisar("Recompensa aceita: " + cartazes_[q][i].nome + ". Siga a bussola.");
}

void Jogo::prepararTiroteio(bool bandoGrande) {
    if (bandoGrande && !cartazes_.empty()) {
        int melhor = 0;
        for (int i = 0; i < int(cartazes_[0].size()); i++)
            if (cartazes_[0][i].recompensaVivo > cartazes_[0][melhor].recompensaVivo) melhor = i;
        aceitarCartaz(0, melhor);
    }
    if (!cacada_.ativa()) aceitarCartaz(0, op_.recompensa >= 0 ? op_.recompensa : 0);
    if (!cacada_.ativa()) return;
    Vec3f e = cacada_.esconderijo();
    // um ponto a ~22 m com visao livre do esconderijo, em terra
    Vec3f p = col_.resolver(e + normalizar(Vec3f{-e.x, 0, -e.z}) * 22.f, RAIO_PESSOA);
    bool achou = false;
    for (int k = 0; k < 24 * 5 && !achou; k++) {
        float a = std::atan2(-e.x, -e.z) + float(k % 24) * 0.2618f, r = 22.f + float(k / 24) * 5.f * ((k / 24) % 2 ? -1.f : 1.f);
        Vec3f q = col_.resolver(e + Vec3f{std::sin(a), 0, std::cos(a)} * r, RAIO_PESSOA);
        q.y = terreno_->altura(q.x, q.z);
        if (terreno_->mascaraRio(q.x, q.z) < 0.2f && q.y > terreno_->nivelAgua() + 0.3f && terreno_->dentroDoJogavel(q.x, q.z) &&
            col_.raioLivre(e + Vec3f{0, 1.5f, 0}, q + Vec3f{0, 1.4f, 0})) {
            p = q;
            achou = true;
        }
    }
    float yaw = std::atan2(e.x - p.x, e.z - p.z);
    teleportar(p.x, p.z, yaw * RAD2DEG, relogio_.hora());
    rig_.yaw += op_.girarCamera * DEG2RAD;
    for (int i = 0; i < 30; i++) rig_.atualizar(jogador_.pos, 0, 0, *terreno_, col_, 0.05f);
    atualizarCamera(0, Entrada{});
    cacada_.atualizar(0.05f, jogador_.pos, 0, false, true, *terreno_, col_);  // cria o bando ja alerta
}

Vec3f Jogo::canoDoJogador() const {
    return jogador_.pos + Vec3f{0, 1.45f, 0} + frenteDe(jogador_.yaw) * 0.55f + direitaDe(jogador_.yaw) * 0.2f;
}

bool Jogo::entregarNoXerife() {
    if (xerifePerto_ < 0 || cacada_.fase() != FaseCacada::Levando) return false;
    std::string nome = cacada_.cartaz().nome;
    int v = cacada_.entregar(jogador_.pos, xerifes_[xerifePerto_]);
    if (v <= 0) return false;
    dinheiro_ += v;
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s entregue. Recompensa: %d pratas", nome.c_str(), v);
    avisar(buf);
    int q = cacada_.quadro(), i = cacada_.indiceCartaz();
    if (q >= 0 && q < int(cartazes_.size()) && i >= 0 && i < int(cartazes_[q].size()))
        cartazes_[q][i] = novoCartaz(uint32_t(tempoTotal_ * 1000.f) + uint32_t(q) * 7919u, mapa_, cartazes_[q]);
    cacada_.cancelar();
    return true;
}

void Jogo::aoMontar() {
    if (montadoEm_ >= 0 && cacada_.levarAoMontar(cavalos_[montadoEm_].corpo.pos))
        avisar("Na garupa. Leve ao xerife (siga a bussola).");
}

void Jogo::aoDesmontar() {
    if (cacada_.carga() != Carga::Garupa) return;
    cacada_.soltarCarga();
    if (Bandido* c = cacada_.bando().chefe()) {
        Vec3f p = col_.resolver(jogador_.pos + direitaDe(jogador_.yaw) * 1.4f, RAIO_PESSOA);
        p.y = terreno_->altura(p.x, p.z);
        c->corpo.pos = p;
    }
}

bool Jogo::interagirCacada(Entrada& e) {
    Vec3f p = jogador_.pos;
    if (cacada_.podeAmarrar(p)) {  // segurar E amarra; o toque nao abre quadro nem monta
        e.interagir = false;
        return true;
    }
    if (!e.interagir) return false;
    if (entregarNoXerife()) {
        e.interagir = false;
        return true;
    }
    const Bandido* c = cacada_.bando().chefe();
    if (c && cacada_.podeLevar(p) && c->estado == EstadoBandido::Amarrado && cavaloPerto_ < 0) {
        if (cacada_.levar(Carga::Seguindo)) avisar(cacada_.cartaz().nome + " vai te seguir. Monte num cavalo para levar na garupa.");
        e.interagir = false;
        return true;
    }
    return false;
}

void Jogo::atualizarCombate(float dt, Entrada& e) {
    bool vivoJ = morteT_ < 0;
    bool aPe = montadoEm_ < 0;
    mirando_ = e.mirar && aPe && vivoJ;
    mira_ += ((mirando_ ? 1.f : 0.f) - mira_) * std::min(1.f, dt * 10.f);
    if (aPe) {
        rig_.distAlvo = 4.f - 1.7f * mira_;
        rig_.ombroAlvo = 0.45f + 0.45f * mira_;
    }
    cam_.fovy = dbg_.fov - 12.f * mira_;
    arma_.atualizar(dt);
    tiroT_ = std::max(0.f, tiroT_ - dt);
    marcaAcerto_ = std::max(0.f, marcaAcerto_ - dt);
    mensagemT_ = std::max(0.f, mensagemT_ - dt);
    for (auto& f : claroes_) f.vida -= dt;
    claroes_.erase(std::remove_if(claroes_.begin(), claroes_.end(), [](const Clarao& f) { return f.vida <= 0; }), claroes_.end());
    for (auto& p : particulas_) {
        p.vida -= dt;
        p.pos = p.pos + p.vel * dt;
        p.vel = p.vel * std::exp(-3.8f * dt) + Vec3f{0, 0.38f * dt, 0};
        p.tam += dt * p.expansao;
    }
    particulas_.erase(std::remove_if(particulas_.begin(), particulas_.end(), [](const Particula& p) { return p.vida <= 0; }), particulas_.end());
    bool recarregavaAntes = arma_.recarregando();
    if (e.recarregar && vivoJ) arma_.recarregar();

    Vec3f camPos{cam_.position.x, cam_.position.y, cam_.position.z};
    Vec3f camDir = normalizar(Vec3f{cam_.target.x, cam_.target.y, cam_.target.z} - camPos);
    if (mirando_) {
        jogador_.yaw = rig_.yaw;
        anguloMira_ = std::asin(std::clamp(camDir.y, -0.9f, 0.9f));
    }
    int interior = dentroDeInterior(cenario_.interiores(), jogador_.pos);
    // ---- tiro do jogador ----
    tiroOuvido_ = false;
    if (e.atirar && aPe && vivoJ && quadroAberto_ < 0) {
        if (arma_.balas == 0) arma_.recarregar();
        if (arma_.podeAtirar()) {
            arma_.atirar();
            if (dbg_.municaoInf) arma_.balas = Revolver::CAPACIDADE;
            sons_.tiro(1.f, 0.5f, interior >= 0);
            tiroOuvido_ = true;
            tiroT_ = 0.18f;
            jogador_.yaw = rig_.yaw;
            std::vector<Alvo> alvos = cacada_.bando().vazio() ? std::vector<Alvo>{} : cacada_.bando().alvos();
            Acerto pc = lancarTiro(camPos, camDir, 200.f, alvos, *terreno_, col_);  // para onde a mira aponta
            Vec3f cano = canoDoJogador();
            Vec3f dir = normalizar(pc.ponto - cano);
            float esp = espalhamento(mirando_, jogador_.vel);
            Vec3f a = normalizar(cross(dir, Vec3f{0, 1, 0})), b = cross(a, dir);
            float ang = aleatorio01(rngTiro_) * 6.2831f, r = std::sqrt(aleatorio01(rngTiro_)) * esp;
            dir = normalizar(dir + a * (std::cos(ang) * r) + b * (std::sin(ang) * r));
            anguloMira_ = std::asin(std::clamp(dir.y, -0.9f, 0.9f));
            Acerto t = lancarTiro(cano, dir, 200.f, alvos, *terreno_, col_);
            if (t.indice >= 0) {
                marcaMatou_ = cacada_.bando().levarTiro(t.indice, t.parte);
                marcaAcerto_ = 0.2f;
            } else if (t.dist < 199.f) {
                for (int k = 0; k < 5; k++)  // poeira onde a bala bateu
                    particulas_.push_back({t.ponto, Vec3f{aleatorio01(rngTiro_) - 0.5f, 1.2f + aleatorio01(rngTiro_), aleatorio01(rngTiro_) - 0.5f},
                                           0.45f, 0.45f, 0.09f, 0.22f, Rgb{0.55f, 0.48f, 0.38f}});
            }
            const Esqueleto& e = modeloJogador_.esqueleto();
            const Osso& mao = e.ossos[MAO_D];
            std::vector<Mat4> ossos;
            calcularMatrizes(e, poseDoJogador(e), ossos);
            Vec3f eixo = normalizar(mao.fim - mao.inicio);
            float escala = e.ossos[CABECA].fim.y / 1.8f;
            Vec3f p = ossos[MAO_D].ponto(bocaDoRevolver(e, escala));
            Vec3f frente = ossos[MAO_D].vetor(eixo);
            efeitoTiro(jogador_.pos + rotY(p, jogador_.yaw), normalizar(rotY(frente, jogador_.yaw)));
        }
    }
    // ---- amarrar (segurar E) ----
    if (aPe && vivoJ && cacada_.podeAmarrar(jogador_.pos) && e.segurarInteragir) {
        amarrarT_ += dt;
        if (amarrarT_ >= 1.5f) {
            cacada_.amarrar();
            amarrarT_ = 0;
            avisar(cacada_.cartaz().nome + " amarrado. E: puxar pela corda / monte para levar na garupa.");
        }
    } else {
        amarrarT_ = 0;
    }
    if (cacada_.carga() == Carga::Seguindo) e.correr = false;
    // ---- bando ----
    EventosBando ev = cacada_.atualizar(dt, jogador_.pos, montadoEm_ >= 0 ? cavalos_[montadoEm_].corpo.vel : jogador_.vel,
                                        montadoEm_ >= 0, tiroOuvido_, *terreno_, col_);
    if (vivoJ && ev.tirosNoJogador > 0 && !dbg_.invencivel) vida_.dano(DANO_BANDIDO * float(ev.tirosNoJogador));
    if (!recarregavaAntes && arma_.recarregando()) sons_.recarga();
    for (Vec3f c : ev.claroes) {
        Vec3f d = c - jogador_.pos;
        float dist = std::hypot(d.x, d.z), lado = dist > 0.1f ? dot(d * (1.f / dist), direitaDe(rig_.yaw)) : 0.f;
        int origem = dentroDeInterior(cenario_.interiores(), c);
        Vec3f ouvido = jogador_.pos + Vec3f{0, montadoEm_ >= 0 ? 2.4f : 1.55f, 0};
        bool abafado = !col_.raioLivre(c, ouvido);
        float ganho = 0.84f / (1.f + dist * 0.035f);
        sons_.tiro(ganho * (abafado ? 0.38f : 1.f), 0.5f - 0.4f * lado, origem >= 0 && origem == interior, abafado);
        efeitoTiro(c, normalizar(Vec3f{-d.x, 0, -d.z}));
    }
    if (const Bandido* c = cacada_.bando().chefe()) {
        if (c->estado != estadoChefeAntes_) {
            const std::string& n = cacada_.cartaz().nome;
            if (c->estado == EstadoBandido::Rendido) avisar(n + " se rendeu! Chegue perto e segure E para amarrar.");
            if (c->estado == EstadoBandido::Fugindo) avisar(n + " esta fugindo! Alcance ele.");
            if (c->estado == EstadoBandido::Morto) avisar(n + " morreu. Leve o corpo ao xerife (paga menos).");
            estadoChefeAntes_ = c->estado;
        }
    }
    // ---- vida e morte ----
    vida_.atualizar(dt, true);
    if (vivoJ && vida_.morto()) {
        morteT_ = 0;
        std::printf("morte\n");
        if (montadoEm_ >= 0) {
            desmontar(cavalos_[montadoEm_], jogador_, col_);
            montadoEm_ = -1;
            aoDesmontar();
        }
        cacada_.soltarCarga();
    }
    if (morteT_ >= 0) {
        morteT_ += dt;
        if (morteT_ > 2.5f) {  // acorda junto ao quadro mais proximo
            int melhor = -1;
            float md = 1e18f;
            for (size_t q = 0; q < cenario_.quadros.size(); q++) {
                float d = distXZ(cenario_.quadros[q].pos, jogador_.pos);
                if (d < md) { md = d; melhor = int(q); }
            }
            if (melhor >= 0) {
                const Quadro& q = cenario_.quadros[melhor];
                Vec3f p = col_.resolver(q.pos + frenteDe(q.yaw) * 2.5f, RAIO_PESSOA);
                teleportar(p.x, p.z, q.yaw * RAD2DEG + 180.f, relogio_.hora());
            }
            vida_.curar();
            arma_ = Revolver();
            cacada_.morrerJogador();
            morteT_ = -1;
            std::printf("reaparece\n");
            avisar("Voce apagou e acordou na cidade. A recompensa continua.");
        }
    }
    populacao_.definirExclusao(cacada_.esconderijo(), cacada_.ativa() ? RAIO_EXCLUSAO : 0.f);
    xerifePerto_ = -1;
    for (size_t i = 0; i < xerifes_.size(); i++)
        if (distXZ(xerifes_[i], jogador_.pos) <= DIST_ENTREGAR) xerifePerto_ = int(i);
    constexpr size_t maxParticulas = 72;
    if (particulas_.size() > maxParticulas)
        particulas_.erase(particulas_.begin(), particulas_.end() - maxParticulas);
}

Pose Jogo::poseDoJogador(const Esqueleto& ej) const {
    if (op_.estudio) {
        return poseRepouso(ej);
    }
    if (morteT_ >= 0) return poseCair(ej, morteT_ / 0.6f);
    if (arma_.recarregando()) {
        Pose p = jogador_.vel > 0.1f ? poseAndar(ej, passada_, jogador_.vel) : poseRecarregar(ej, 1.f - arma_.recarga / Revolver::TEMPO_RECARGA);
        if (jogador_.vel > 0.1f) {
            Pose r = poseRecarregar(ej, 1.f - arma_.recarga / Revolver::TEMPO_RECARGA);
            for (int o : {BRACO_E, BRACO_D, ANTEBRACO_E, ANTEBRACO_D}) p.rot[o] = r.rot[o];
        }
        return p;
    }
    if (mira_ > 0.5f || tiroT_ > 0) {
        Pose m = poseMirar(ej, anguloMira_, tiroT_ / 0.18f);
        if (jogador_.vel > 0.1f) {
            Pose p = poseAndar(ej, passada_, jogador_.vel);
            bracoDeMira(p, m);
            return p;
        }
        return m;
    }
    if (pesoPassada_ < 0.005f) return poseParado(ej, tempo_);
    Pose passo = poseAndar(ej, passada_, std::max(jogador_.vel, VEL_ANDAR));
    if (pesoPassada_ < 0.995f) {
        Pose repouso = poseParado(ej, tempo_);
        for (size_t i = 0; i < passo.rot.size(); ++i)
            passo.rot[i] = repouso.rot[i] + (passo.rot[i] - repouso.rot[i]) * pesoPassada_;
        passo.raiz = repouso.raiz + (passo.raiz - repouso.raiz) * pesoPassada_;
        apoiarNoChao(ej, passo);
    }
    return passo;
}

void Jogo::desenharCombatentes(Material& matPele, bool sombraBolha, float distMax) {
    // xerifes
    for (size_t i = 0; i < xerifes_.size(); i++) {
        Vec3f p = xerifes_[i];
        float d2 = (p.x - cam_.position.x) * (p.x - cam_.position.x) + (p.z - cam_.position.z) * (p.z - cam_.position.z);
        if (d2 > distMax * distMax || (passePrincipal_ && atrasDaCamera(p))) continue;
        bool hd = passePrincipal_ && d2 < 144.f;
        modeloXerife_.desenhar(matPele, p, yawXerifes_[i], poseParado(modeloXerife_.esqueleto(), tempo_ + float(i) * 5.f), hd);
        trisAtores_ += modeloXerife_.triangulos(hd);
        chamadasAtores_++;
        if (sombraBolha) sombra_.desenhar(p, terreno_->normal(p.x, p.z), 0.42f);
    }
    if (cacada_.bando().vazio()) return;
    const auto& bs = cacada_.bando().bandidos();
    int cap = 0;
    for (const Bandido& b : bs) {
        const ModeloHumano& m = modelosBando_[b.chefe ? 0 : 1 + (cap++ % 3)];
        if (b.estado == EstadoBandido::Sumiu) continue;
        const Esqueleto& e = m.esqueleto();
        Vec3f pos = b.corpo.pos;
        float yaw = b.corpo.yaw;
        Pose pose;
        bool naGarupa = b.chefe && cacada_.carga() == Carga::Garupa && montadoEm_ >= 0;
        if (naGarupa) {
            const Cavalo& c = cavalos_[montadoEm_];
            Pose pc = poseCavalo(modelosCavalo_[montadoEm_].esqueleto(), c.passada, int(c.andadura), tempo_);
            Vec3f q = modelosCavalo_[montadoEm_].assento(c.corpo.pos, c.corpo.yaw, pc) - frenteDe(c.corpo.yaw) * 0.45f;
            pos = q - Vec3f{0, e.ossos[QUADRIL].inicio.y - 0.22f, 0};
            yaw = c.corpo.yaw + 1.5708f;
            pose = poseGarupa(e);
        } else {
            float fase = tempo_ * std::max(b.corpo.vel, 0.f) * fasePorMetro(b.corpo.vel);
            switch (b.estado) {
            case EstadoBandido::Vigiando: pose = poseParado(e, tempo_ + float(b.semente % 97)); break;
            case EstadoBandido::Alerta: pose = poseMirar(e, 0, 0); break;
            case EstadoBandido::Combate: {
                float recuo = std::clamp((b.arma.espera - 0.2f) / 0.15f, 0.f, 1.f);
                Pose mira = poseMirar(e, 0.02f, recuo);
                if (b.corpo.vel > 0.3f) {
                    pose = poseAndar(e, fase, b.corpo.vel);
                    bracoDeMira(pose, mira);
                } else {
                    pose = mira;
                }
                break;
            }
            case EstadoBandido::Fugindo: pose = poseAndar(e, fase, b.corpo.vel); break;
            case EstadoBandido::Rendido: pose = poseRendido(e, tempo_); break;
            case EstadoBandido::Amarrado:
                if (b.chefe && cacada_.carga() == Carga::Seguindo && b.corpo.vel > 0.1f) {
                    pose = poseAndar(e, fase, b.corpo.vel);
                    bracosPresos(pose, poseAmarrado(e, tempo_));
                } else {
                    pose = poseAmarrado(e, tempo_);
                }
                break;
            case EstadoBandido::Morto: pose = poseCair(e, b.tempoMorto / 0.6f); break;
            default: pose = poseParado(e, tempo_); break;
            }
            if (b.tranco > 0 && b.estado != EstadoBandido::Morto) aplicarTranco(pose, b.tranco);
        }
        float d2 = (pos.x - cam_.position.x) * (pos.x - cam_.position.x) + (pos.z - cam_.position.z) * (pos.z - cam_.position.z);
        if (d2 > distMax * distMax || (passePrincipal_ && !naGarupa && atrasDaCamera(pos))) continue;
        bool hd = passePrincipal_ && d2 < 144.f;
        m.desenhar(matPele, pos, yaw, pose, hd);
        trisAtores_ += m.triangulos(hd);
        chamadasAtores_++;
        if (sombraBolha && !naGarupa) sombra_.desenhar(pos, terreno_->normal(pos.x, pos.z), 0.42f);
    }
}

void Jogo::efeitoTiro(Vec3f pos, Vec3f dir) {
    claroes_.push_back({pos, dir, DURACAO_FLASH});
    // Jato curto junto ao cano e nuvem mais lenta que se dispersa.
    for (int i = 0; i < 5; ++i) {
        float t = float(i) / 4.f;
        float duracao = 0.20f + t * 0.20f;
        Vec3f deriva{aleatorio01(rngEfeito_) - 0.5f, aleatorio01(rngEfeito_) * 0.12f, aleatorio01(rngEfeito_) - 0.5f};
        Vec3f p = pos + dir * (0.025f + t * 0.08f);
        Vec3f v = dir * (0.75f + t * 0.70f) + deriva * 0.38f;
        particulas_.push_back({p, v, duracao, duracao, 0.035f + t * 0.025f, 0.18f, Rgb{0.68f, 0.65f, 0.61f}});
    }
}

void Jogo::desenharEfeitos() {
    if (!texturaFumaca_.id || (particulas_.empty() && claroes_.empty())) return;
    Vec3f olho{cam_.position.x, cam_.position.y, cam_.position.z};
    Vec3f frente = normalizar(Vec3f{cam_.target.x, cam_.target.y, cam_.target.z} - olho);
    std::vector<const Particula*> ordenadas;
    ordenadas.reserve(particulas_.size());
    for (const Particula& p : particulas_) ordenadas.push_back(&p);
    std::sort(ordenadas.begin(), ordenadas.end(), [&](const Particula* a, const Particula* b) {
        return dot(a->pos - olho, frente) > dot(b->pos - olho, frente);
    });

    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const Particula* part : ordenadas) {
        const Particula& p = *part;
        float vida = clamp01(p.vida / p.duracao);
        DrawBillboard(cam_, texturaFumaca_, {p.pos.x, p.pos.y, p.pos.z}, p.tam,
                      Fade(Color{static_cast<unsigned char>(p.cor.r * 255), static_cast<unsigned char>(p.cor.g * 255),
                                 static_cast<unsigned char>(p.cor.b * 255), 255}, 0.31f * vida * vida));
    }
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Clarao& f : claroes_) {
        float a = clamp01(f.vida / DURACAO_FLASH);
        for (int i = 0; i < 3; ++i) {
            Vec3f p = f.pos + f.dir * (0.012f + 0.034f * i);
            DrawBillboard(cam_, texturaFumaca_, {p.x, p.y, p.z}, (0.065f + 0.018f * i) * a,
                          Fade(i == 0 ? Color{255, 246, 210, 255} : Color{255, 147, 57, 255}, a * (1.f - 0.22f * i)));
        }
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}
