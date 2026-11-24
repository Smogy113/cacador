#include "core/inimigo_teste.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/terrain.h"

namespace {
constexpr float METROS_POR_CICLO = 1.4f;  // dois passos de gente andando
constexpr float TEMPO_DESISTIR = 2.f;     // s travado antes de olhar dali mesmo

float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
float angDif(float a) { return std::remainder(a, 6.2831853f); }
float yawPara(Vec3f de, Vec3f para) { return std::atan2(para.x - de.x, para.z - de.z); }

void virar(InimigoTeste& in, float yawAlvo, float dt, const ParametrosInimigo& p) {
    const float passo = p.giro * dt;
    in.corpo.yaw += std::clamp(angDif(yawAlvo - in.corpo.yaw), -passo, passo);
}

// Um passo em direção a 'destino' a 'vel' m/s (já com a lentidão), respeitando paredes e o rio.
// 'olharParaOnde' = o corpo vira para onde anda (o sentinela anda de lado, olhando a frente).
void andar(InimigoTeste& in, Vec3f destino, float vel, float dt, const Terreno& t, const MundoColisao& col, const ParametrosInimigo& p,
           bool olharParaOnde) {
    Vec3f& pos = in.corpo.pos;
    const float dist = distXZ(pos, destino);
    in.vel = 0;
    if (dist < 1e-3f || vel <= 0.f || dt <= 0.f) return;
    const Vec3f dir{(destino.x - pos.x) / dist, 0, (destino.z - pos.z) / dist};
    Vec3f q = pos + dir * std::min(dist, vel * dt);
    if (t.mascaraRio(q.x, q.z) > 0.5f) return;  // não entra no rio
    q = col.resolver(q, p.raioCorpo);
    q.y = t.altura(q.x, q.z);
    const float andou = distXZ(pos, q);
    pos = q;
    in.vel = andou / dt;
    in.passada += andou / METROS_POR_CICLO;
    if (olharParaOnde) virar(in, std::atan2(dir.x, dir.z), dt, p);
}

int pontoMaisPerto(const InimigoTeste& in) {
    int melhor = 0;
    float md = 1e30f;
    for (size_t i = 0; i < in.rota.size(); i++) {
        float d = distXZ(in.rota[i], in.corpo.pos);
        if (d < md) {
            md = d;
            melhor = int(i);
        }
    }
    return melhor;
}

void voltarAPatrulha(InimigoTeste& in) {
    in.conduta = Conduta::Patrulhando;
    in.motivo = MotivoAlerta::Nenhum;
    in.inspecao = in.travado = 0.f;
    in.proximo = pontoMaisPerto(in);
}

void atualizarInvestigacao(InimigoTeste& in, float mult, float dt, const Terreno& t, const MundoColisao& col, const ParametrosInimigo& p) {
    const bool chegou = in.posto || distXZ(in.corpo.pos, in.interesse) <= p.distInspecao || in.travado > TEMPO_DESISTIR;
    if (!chegou) {
        const float vel = p.velInvestigar * mult;
        andar(in, in.interesse, vel, dt, t, col, p, true);
        const bool avancou = in.vel > vel * 0.25f;
        in.travado = avancou ? 0.f : in.travado + dt;
        return;
    }
    // no ponto (ou do posto): vira para ele e olha até a inspeção acabar
    in.vel = 0;
    virar(in, yawPara(in.corpo.pos, in.interesse), dt, p);
    in.inspecao -= dt;
    if (in.inspecao <= 0.f) voltarAPatrulha(in);
}

void atualizarPatrulha(InimigoTeste& in, float mult, float dt, const Terreno& t, const MundoColisao& col, const ParametrosInimigo& p) {
    if (in.rota.empty()) {
        in.vel = 0;
        return;
    }
    const int n = int(in.rota.size());
    in.proximo = std::clamp(in.proximo, 0, n - 1);
    if (distXZ(in.corpo.pos, in.rota[size_t(in.proximo)]) <= p.chegada) {
        if (n == 1) {  // posto de um ponto só: parado, olhando a frente
            in.vel = 0;
            virar(in, in.yawVigia, dt, p);
            return;
        }
        in.proximo = (in.proximo + 1) % n;
    }
    const float vel = (in.agachado ? p.velAgachado : p.velPatrulha) * mult;
    andar(in, in.rota[size_t(in.proximo)], vel, dt, t, col, p, !in.posto);
    if (in.posto) virar(in, in.yawVigia, dt, p);
}
}  // namespace

const char* nome(EstadoInimigo e) {
    switch (e) {
    case EstadoInimigo::Patrulha: return "Patrulha";
    case EstadoInimigo::Alerta: return "Alerta";
    case EstadoInimigo::Congelado: return "Congelado";
    case EstadoInimigo::Atordoado: return "Atordoado";
    case EstadoInimigo::Derrotado: return "Derrotado";
    }
    return "?";
}

const char* nome(MotivoAlerta m) {
    switch (m) {
    case MotivoAlerta::Nenhum: return "nada";
    case MotivoAlerta::Vestigio: return "vestígio";
    case MotivoAlerta::Atacado: return "atacado";
    }
    return "?";
}

Alvo InimigoTeste::alvo(const ParametrosInimigo& p) const {
    Alvo a{pes(), agachado ? p.alturaAgachado : p.alturaEmPe};
    if (!ativo()) a.pes.y = -1e6f;  // derrotado: fora do alcance
    return a;
}

InimigoTeste criarInimigo(const std::string& nomeInimigo, std::vector<Vec3f> rota, const ParametrosInimigo& p) {
    InimigoTeste in;
    in.nome = nomeInimigo;
    in.rota = std::move(rota);
    if (!in.rota.empty()) in.corpo.base = in.corpo.pos = in.rota[0];
    in.corpo.tempoReaparecer = p.tempoReaparecer;
    in.proximo = in.rota.size() > 1 ? 1 : 0;
    if (in.rota.size() > 1) in.corpo.yaw = yawPara(in.rota[0], in.rota[1]);
    return in;
}

EstadoInimigo estado(const InimigoTeste& in) {
    if (!in.ativo()) return EstadoInimigo::Derrotado;
    const FisicaAlvo& f = in.corpo.fisica;
    if (f.atordoado > 0.f || f.derrubado > 0.f || f.noAr()) return EstadoInimigo::Atordoado;
    if (f.congelado > 0.f || f.lento > 0.f) return EstadoInimigo::Congelado;
    if (in.conduta == Conduta::Investigando) return EstadoInimigo::Alerta;
    return EstadoInimigo::Patrulha;
}

std::string descreverEstado(const InimigoTeste& in) {
    const FisicaAlvo& f = in.corpo.fisica;
    char buf[96];
    switch (estado(in)) {
    case EstadoInimigo::Derrotado: std::snprintf(buf, sizeof buf, "Derrotado (volta em %.0f s)", std::ceil(in.corpo.reaparecer)); break;
    case EstadoInimigo::Atordoado:
        if (f.noAr()) std::snprintf(buf, sizeof buf, "Atordoado (no ar)");
        else if (f.derrubado > 0.f) std::snprintf(buf, sizeof buf, "Atordoado (caído, %.1f s)", std::max(f.derrubado, f.atordoado));
        else std::snprintf(buf, sizeof buf, "Atordoado (%.1f s)", f.atordoado);
        break;
    case EstadoInimigo::Congelado:
        if (f.congelado > 0.f) std::snprintf(buf, sizeof buf, "Congelado (parado, %.1f s)", f.congelado);
        else std::snprintf(buf, sizeof buf, "Congelado (lento: %.0f%% da vel.)", f.fatorLento * 100.f);
        break;
    case EstadoInimigo::Alerta:
        std::snprintf(buf, sizeof buf, in.motivo == MotivoAlerta::Vestigio ? "Alerta: investigando vestígio" : "Alerta: atacado, procurando");
        break;
    case EstadoInimigo::Patrulha: std::snprintf(buf, sizeof buf, in.posto ? "Patrulha (posto)" : "Patrulha"); break;
    }
    return buf;
}

bool percebe(const InimigoTeste& in, Vec3f ponto, const MundoColisao& col, const ParametrosInimigo& p) {
    if (!in.ativo() || in.corpo.fisica.ocupado()) return false;
    const float d = distXZ(in.corpo.pos, ponto);
    if (d <= p.raioPercepcaoProxima) return true;
    if (d > p.raioVisaoVestigio) return false;
    if (std::abs(angDif(yawPara(in.corpo.pos, ponto) - in.corpo.yaw)) > p.meioConeVisao) return false;
    const Vec3f olhos = in.corpo.pos + Vec3f{0, in.agachado ? p.alturaOlhosAgachado : p.alturaOlhos, 0};
    return col.raioLivre(olhos, ponto + Vec3f{0, 0.1f, 0});  // a cobertura e as paredes escondem
}

int vestigioPercebido(const InimigoTeste& in, const std::vector<CapsulaVestigio>& capsulas, const MundoColisao& col,
                      const ParametrosInimigo& p) {
    for (size_t i = 0; i < capsulas.size(); i++)
        if (!capsulas[i].descoberta && percebe(in, capsulas[i].pos, col, p)) return int(i);
    return -1;
}

void investigar(InimigoTeste& in, Vec3f ponto, MotivoAlerta motivo, const ParametrosInimigo& p) {
    if (!in.ativo()) return;
    in.conduta = Conduta::Investigando;
    in.motivo = motivo;
    in.interesse = ponto;
    in.inspecao = p.tempoInspecao;
    in.travado = 0.f;
}

bool golpear(InimigoTeste& in, const GolpeAlvo& g) {
    const bool derrotou = golpear(in.corpo, g);  // o do boneco de treino (core/fisica_alvo)
    if (derrotou) in.vel = 0.f;
    return derrotou;
}

void atualizar(InimigoTeste& in, float dt, const Terreno& t, const MundoColisao& col, const ParametrosInimigo& p) {
    const bool estavaAtivo = in.ativo();
    atualizar(in.corpo, dt, t, col);  // física dos golpes, vida e reaparecimento
    if (!in.ativo()) {
        in.vel = 0.f;
        return;
    }
    if (!estavaAtivo) {  // acabou de reaparecer no começo da rota: cabeça zerada
        voltarAPatrulha(in);
        in.proximo = in.rota.size() > 1 ? 1 : 0;
        in.vel = 0.f;
        return;
    }
    if (in.corpo.fisica.ocupado()) {  // zonzo, caído, no ar ou congelado: a cabeça espera
        in.vel = 0.f;
        return;
    }
    const float mult = in.corpo.fisica.multVelocidade();  // < 1 na zona congelante
    if (in.conduta == Conduta::Investigando) atualizarInvestigacao(in, mult, dt, t, col, p);
    else atualizarPatrulha(in, mult, dt, t, col, p);
}
