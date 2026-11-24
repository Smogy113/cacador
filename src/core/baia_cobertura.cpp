#include "core/baia_cobertura.h"
#include <cmath>
#include "core/terrain.h"

// Convenção de CaixaGirada (core/collision): o eixo z local aponta para frenteDe(rot) = (sen, 0, cos)
// e o eixo x local para (cos, 0, -sen). meiaX/meiaZ são as metades nesses eixos.

Vec3f BaiaCobertura::frente() const { return {std::sin(yawFrente), 0, std::cos(yawFrente)}; }
Vec3f BaiaCobertura::lateral() const { return {std::cos(yawFrente), 0, -std::sin(yawFrente)}; }

Vec3f BaiaCobertura::miraRicochete(int i, float altura) const {
    const float lado = i == 0 ? 1.f : -1.f;
    // A parede passa pelo seu centro na profundidade do alvo; a face interna fica a meia espessura
    // do centro, medida na normal (que está a 45°): sqrt(2) vezes isso no eixo lateral.
    const float face = afastamentoParede - paredes[i].meiaX * 1.41421356f;
    return alvo + lateral() * (lado * face) + Vec3f{0, altura, 0};
}

BaiaCobertura montarBaia(Vec3f alvo, float yawFrente, const Terreno& t, const ParametrosBaia& p) {
    BaiaCobertura b;
    b.alvo = alvo;
    b.yawFrente = yawFrente;
    b.afastamentoParede = p.afastamentoParede;
    const Vec3f u = b.frente(), lx = b.lateral();
    auto chao = [&](Vec3f q) { return t.altura(q.x, q.z); };

    // Cobertura: comprida no eixo lateral (x local), fina na direção do atirador (z local).
    const Vec3f cc = alvo + u * p.distCobertura;
    const float yc = chao(cc);
    b.cobertura = {cc, p.meiaLarguraCobertura, p.meiaEspessuraCobertura, yawFrente, yc - 0.4f, yc + p.alturaCobertura};

    // Paredes laterais: o comprimento (z local) corre a 45° entre a frente e o lado de fora, de modo
    // que a face interna olha ao mesmo tempo para o atirador e para o alvo.
    for (int i = 0; i < 2; i++) {
        const float lado = i == 0 ? 1.f : -1.f;
        const Vec3f centro = alvo + lx * (lado * p.afastamentoParede);
        const Vec3f eixo = normalizar(u + lx * lado);
        const float yw = chao(centro);
        b.paredes[i] = {centro, p.meiaEspessuraParede, p.meioComprimentoParede, std::atan2(eixo.x, eixo.z), yw - 0.5f, yw + p.alturaParede};
    }
    return b;
}
