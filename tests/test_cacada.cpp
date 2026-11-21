#include <cmath>
#include <memory>
#include "core/cacada.h"
#include "core/terrain.h"
#include "doctest/doctest.h"

static const Mapa& mapa() {
    static Mapa m = carregarMapa(DATA_DIR "/lugares.txt");
    return m;
}
static const Terreno& ter() {
    static std::unique_ptr<Terreno> t;
    if (!t) t = std::make_unique<Terreno>(mapa(), 1);
    return *t;
}
static float distXZ(Vec3f a, Vec3f b) { return std::hypot(a.x - b.x, a.z - b.z); }
static Vec3f chao(float x, float z) { return {x, ter().altura(x, z), z}; }
static Cartaz cartazEm(const char* lugar, Temperamento tp, int vivo = 80) {
    Cartaz c;
    c.nome = "Fulano Teste";
    c.ultimoVisto = lugar;
    c.temperamento = tp;
    c.recompensaVivo = vivo;
    c.recompensaMorto = vivo / 2;
    return c;
}
static void passo(Cacada& c, Vec3f jog, float seg = 0.1f, bool montado = false) {
    for (float t = 0; t < seg; t += 1 / 30.f) c.atualizar(1 / 30.f, jog, 0, montado, false, ter(), MundoColisao{});
}
static void renderChefe(Cacada& c) {
    auto& b = c.bando().bandidos();
    int i = int(c.bando().chefe() - &b[0]);
    c.bando().levarTiro(i, Parte::Corpo);
    c.bando().levarTiro(i, Parte::Corpo);
}

TEST_CASE("esconderijo livre para todos os lugares do mapa padrao") {
    for (auto& l : mapa().lugares) {
        MundoColisao col;
        col.adicionar({{l.x - 4, -50, l.z - 4}, {l.x + 4, 300, l.z + 4}});  // construcao no centro
        Vec3f p = escolherEsconderijo(l, 7, ter(), col);
        CHECK(distXZ(col.resolver(p, 1.5f), p) < 0.01f);
        CHECK(ter().mascaraRio(p.x, p.z) < 0.2f);
        CHECK(ter().dentroDoJogavel(p.x, p.z));
        CHECK(distXZ(p, {l.x, 0, l.z}) <= l.raio + 1.f);
    }
}
TEST_CASE("cacada: bando nasce a 150 m e some a 400 m sem luta") {
    Cacada c;
    c.aceitar(cartazEm("Acampamento Garra Rubra", Temperamento::Covarde), 0, 0, mapa(), ter(), MundoColisao{});
    CHECK(c.fase() == FaseCacada::Indo);
    Vec3f e = c.esconderijo();
    passo(c, chao(e.x + 300, e.z));
    CHECK(c.bando().vazio());
    passo(c, chao(e.x + 140, e.z));
    CHECK_FALSE(c.bando().vazio());
    passo(c, chao(e.x + 450, e.z));
    CHECK(c.bando().vazio());
    CHECK(c.fase() == FaseCacada::Indo);
}
TEST_CASE("cacada: bando nao some depois do combate") {
    Cacada c;
    c.aceitar(cartazEm("Acampamento Garra Rubra", Temperamento::Fujao), 0, 0, mapa(), ter(), MundoColisao{});
    Vec3f e = c.esconderijo();
    passo(c, chao(e.x + 15, e.z), 1.5f);
    CHECK(c.fase() == FaseCacada::Combate);
    passo(c, chao(e.x + 500, e.z));
    CHECK_FALSE(c.bando().vazio());
}
TEST_CASE("cacada: capangas 2 ou 3 pela recompensa") {
    Cacada a, b;
    a.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 80), 0, 0, mapa(), ter(), MundoColisao{});
    b.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 150), 0, 0, mapa(), ter(), MundoColisao{});
    passo(a, a.esconderijo() + Vec3f{100, 0, 0});
    passo(b, b.esconderijo() + Vec3f{100, 0, 0});
    CHECK(a.bando().bandidos().size() == 3);
    CHECK(b.bando().bandidos().size() == 4);
}
TEST_CASE("cacada: amarrar, levar e entregar vivo paga recompensaVivo") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 90), 0, 0, mapa(), ter(), MundoColisao{});
    Vec3f e = c.esconderijo();
    passo(c, chao(e.x + 15, e.z));
    renderChefe(c);
    Vec3f pc = c.bando().chefe()->corpo.pos;
    CHECK_FALSE(c.podeAmarrar(pc + Vec3f{5, 0, 0}));
    REQUIRE(c.podeAmarrar(pc + Vec3f{1.5f, 0, 0}));
    c.amarrar();
    CHECK(c.fase() == FaseCacada::Coletar);
    CHECK(c.podeLevar(pc + Vec3f{7, 0, 0}));
    CHECK(c.levar(Carga::Garupa));
    CHECK(c.fase() == FaseCacada::Levando);
    Vec3f xerife{300, 0, 300};
    CHECK(c.entregar({310, 0, 300}, xerife) == 0);  // longe demais
    CHECK(c.entregar({303, 0, 300}, xerife) == 90);
    CHECK(c.fase() == FaseCacada::Entregue);
    CHECK(c.entregar({303, 0, 300}, xerife) == 0);  // nao paga duas vezes
}
TEST_CASE("cacada: morto so vai na garupa e paga recompensaMorto") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Teimoso, 90), 0, 0, mapa(), ter(), MundoColisao{});
    Vec3f e = c.esconderijo();
    passo(c, chao(e.x + 15, e.z));
    auto& b = c.bando().bandidos();
    c.bando().levarTiro(int(c.bando().chefe() - &b[0]), Parte::Cabeca);
    passo(c, chao(e.x + 15, e.z));
    CHECK(c.fase() == FaseCacada::Coletar);
    CHECK_FALSE(c.vivo());
    Vec3f pc = c.bando().chefe()->corpo.pos;
    REQUIRE(c.podeLevar(pc + Vec3f{2, 0, 0}));
    CHECK_FALSE(c.levar(Carga::Seguindo));
    CHECK(c.carga() == Carga::Nenhuma);
    CHECK(c.levar(Carga::Garupa));
    CHECK(c.entregar({0, 0, 0}, {1, 0, 0}) == 45);
}
TEST_CASE("cacada: entregar exige carga") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 90), 0, 0, mapa(), ter(), MundoColisao{});
    CHECK(c.entregar({0, 0, 0}, {1, 0, 0}) == 0);
}
TEST_CASE("cacada: preso seguindo acompanha o jogador") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 90), 0, 0, mapa(), ter(), MundoColisao{});
    Vec3f e = c.esconderijo();
    passo(c, chao(e.x + 15, e.z));
    renderChefe(c);
    Vec3f pc = c.bando().chefe()->corpo.pos;
    c.amarrar();
    REQUIRE(c.levar(Carga::Seguindo));
    Vec3f jog = chao(pc.x + 12, pc.z);
    passo(c, jog, 8.f);
    CHECK(distXZ(c.bando().chefe()->corpo.pos, jog) < 2.5f);
}
TEST_CASE("cacada: trocar recompensa limpa o bando") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Covarde), 0, 0, mapa(), ter(), MundoColisao{});
    passo(c, c.esconderijo() + Vec3f{20, 0, 0});
    REQUIRE_FALSE(c.bando().vazio());
    c.aceitar(cartazEm("Ruinas de Corvalto", Temperamento::Covarde), 0, 1, mapa(), ter(), MundoColisao{});
    CHECK(c.bando().vazio());
    CHECK(c.fase() == FaseCacada::Indo);
    CHECK(c.indiceCartaz() == 1);
    c.cancelar();
    CHECK(c.fase() == FaseCacada::Nenhuma);
    CHECK_FALSE(c.ativa());
}
TEST_CASE("cacada: morrer com alvo amarrado nao perde o alvo") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 90), 0, 0, mapa(), ter(), MundoColisao{});
    Vec3f e = c.esconderijo();
    passo(c, chao(e.x + 15, e.z));
    renderChefe(c);
    c.amarrar();
    c.levar(Carga::Garupa);
    c.morrerJogador();
    CHECK(c.fase() == FaseCacada::Coletar);
    CHECK(c.carga() == Carga::Nenhuma);
    CHECK(c.bando().chefe()->estado == EstadoBandido::Amarrado);
    CHECK(c.ativa());
}
TEST_CASE("cacada: objetivo aponta esconderijo, depois chefe, depois xerife mais proximo") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 90), 0, 0, mapa(), ter(), MundoColisao{});
    std::vector<Vec3f> xs{{300, 0, 300}, {-60, 0, 60}};
    Vec3f e = c.esconderijo();
    CHECK(distXZ(c.objetivo({0, 0, 0}, xs), e) < 0.01f);
    passo(c, chao(e.x + 15, e.z));
    renderChefe(c);
    CHECK(distXZ(c.objetivo({0, 0, 0}, xs), c.bando().chefe()->corpo.pos) < 0.01f);
    c.amarrar();
    c.levar(Carga::Garupa);
    CHECK(distXZ(c.objetivo({-50, 0, 50}, xs), xs[1]) < 0.01f);
}
TEST_CASE("final: corpo vai na garupa ao montar perto (cavalo a ate 8 m)") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Teimoso, 90), 0, 0, mapa(), ter(), MundoColisao{});
    Vec3f e = c.esconderijo();
    passo(c, chao(e.x + 15, e.z));
    auto& b = c.bando().bandidos();
    c.bando().levarTiro(int(c.bando().chefe() - &b[0]), Parte::Cabeca);
    passo(c, chao(e.x + 15, e.z));
    Vec3f pc = c.bando().chefe()->corpo.pos;
    CHECK_FALSE(c.levarAoMontar(pc + Vec3f{12, 0, 0}));
    CHECK(c.levarAoMontar(pc + Vec3f{6, 0, 0}));
    CHECK(c.carga() == Carga::Garupa);
}
TEST_CASE("final: aceitar de novo o mesmo cartaz nao recomeca a cacada") {
    Cacada c;
    c.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 90), 0, 2, mapa(), ter(), MundoColisao{});
    Vec3f e = c.esconderijo();
    passo(c, chao(e.x + 15, e.z));
    renderChefe(c);
    c.amarrar();
    c.aceitar(cartazEm("Mina Funda", Temperamento::Covarde, 90), 0, 2, mapa(), ter(), MundoColisao{});
    CHECK(c.fase() == FaseCacada::Coletar);
    CHECK(c.bando().chefe()->estado == EstadoBandido::Amarrado);
}
TEST_CASE("final: esconderijo valido mesmo com o lugar todo ocupado") {
    for (auto& l : mapa().lugares) {
        MundoColisao col;
        col.adicionar({{l.x - l.raio - 2, -50, l.z - l.raio - 2}, {l.x + l.raio + 2, 300, l.z + l.raio + 2}});
        Vec3f p = escolherEsconderijo(l, 7, ter(), col);
        CHECK(distXZ(col.resolver(p, 1.5f), p) < 0.01f);
        CHECK(ter().mascaraRio(p.x, p.z) < 0.2f);
        CHECK(ter().dentroDoJogavel(p.x, p.z));
    }
}
