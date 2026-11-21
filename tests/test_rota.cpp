#include <cmath>
#include <memory>
#include "core/places.h"
#include "core/rota.h"
#include "core/terrain.h"
#include "doctest/doctest.h"

static const Mapa& mapaR() {
    static Mapa m = carregarMapa(DATA_DIR "/lugares.txt");
    return m;
}
static const Terreno& terR() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(mapaR(), 1337);
    return *t;
}
static const Navegador& nav() {
    static Navegador n(terR(), &mapaR());
    return n;
}
static Vec3f lugar(const char* nome) {
    const Lugar* l = mapaR().achar(nome);
    REQUIRE(l);
    return {l->x, 0, l->z};
}
static float dxz(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }

TEST_CASE("rota: comeca no jogador, termina no alvo e e continua") {
    Vec3f a = lugar("Pedrabranca"), b = lugar("Valdoro");
    auto r = nav().rota(a, b);
    REQUIRE(r.size() >= 2);
    CHECK(dxz(r.front(), a) < 12.f);
    CHECK(dxz(r.back(), b) < 12.f);
    for (size_t i = 1; i < r.size(); i++) CHECK(dxz(r[i], r[i - 1]) < 60.f);
}
TEST_CASE("rota: prefere a estrada entre lugares ligados por estrada") {
    auto r = nav().rotaDensa(lugar("Valdoro"), lugar("Pedrabranca"));
    int naEstrada = 0;
    for (auto& p : r) naEstrada += terR().mascaraEstrada(p.x, p.z) > 0.25f;
    CHECK(float(naEstrada) / r.size() > 0.6f);
}
TEST_CASE("rota: atravessa o rio so pela ponte") {
    auto r = nav().rotaDensa(lugar("Pedrabranca"), lugar("Acampamento_Garra_Rubra"));
    int agua = 0;
    Vec3f ponte = lugar("Ponte_Velha");
    for (auto& p : r)
        agua += terR().mascaraRio(p.x, p.z) > 0.6f && terR().mascaraEstrada(p.x, p.z) < 0.25f && dxz(p, ponte) > 16.f;  // na ponte pode
    CHECK(agua == 0);
}
TEST_CASE("rota: ate um ponto fora da estrada tambem chega") {
    Vec3f a = lugar("Valdoro"), b = lugar("Mata_Sombria") + Vec3f{40, 0, 30};
    auto r = nav().rota(a, b);
    REQUIRE(r.size() >= 2);
    CHECK(dxz(r.back(), b) < 12.f);
}
