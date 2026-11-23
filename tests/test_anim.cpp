#include <doctest/doctest.h>
#include <algorithm>
#include <cmath>
#include "core/anim.h"
#include "core/horse.h"

static const Esqueleto& esq() {
    static Esqueleto e = esqueletoHumano({});
    return e;
}

TEST_CASE("parado: pernas paradas, mas respira") {
    Pose a = poseParado(esq(), 0.f), b = poseParado(esq(), 0.8f);
    for (int o : {COXA_E, COXA_D, CANELA_E, CANELA_D}) CHECK(std::abs(a.rot[o].x) < 0.05f);
    CHECK(std::abs(a.rot[PEITO].x - b.rot[PEITO].x) > 0.005f);
}
TEST_CASE("andar: pernas em oposição") {
    // com cinematica inversa o angulo da coxa inclui o joelho: compara onde estao os pes
    Pose p = poseAndar(esq(), 1.5707963f, 1.6f);
    std::vector<Mat4> m;
    calcularMatrizes(esq(), p, m);
    float zE = m[PE_E].ponto(esq().ossos[PE_E].inicio).z, zD = m[PE_D].ponto(esq().ossos[PE_D].inicio).z;
    CHECK(zE > 0.15f);   // pe esquerdo a frente
    CHECK(zD < -0.15f);  // direito atras
    CHECK(p.rot[BRACO_E].x > 0);  // braco esquerdo atras (oposto a perna do mesmo lado)
}
TEST_CASE("correr balança mais que andar") {
    float ja = 0, jc = 0;  // dobra maxima do joelho no ciclo
    for (float f = 0; f < 6.3f; f += 0.1f) {
        ja = std::max(ja, poseAndar(esq(), f, 1.6f).rot[CANELA_E].x);
        jc = std::max(jc, poseAndar(esq(), f, 5.f).rot[CANELA_E].x);
    }
    CHECK(jc > ja * 1.3f);
}
TEST_CASE("montado: coxas para a frente") {
    for (int and_ = 0; and_ < 4; and_++) {
        Pose p = poseMontado(esq(), 0.7f, and_);
        CHECK(p.rot[COXA_E].x < -1.05f);
        CHECK(p.rot[COXA_D].x < -1.05f);
    }
}
TEST_CASE("pés no chão em todas as fases") {
    for (float v : {0.f, 1.6f, 5.f})
        for (float f = 0; f < 6.3f; f += 0.3f) {
            Pose p = v > 0 ? poseAndar(esq(), f, v) : poseParado(esq(), f);
            float h = alturaDosPes(esq(), p);
            if (v < 3.f) CHECK(h == doctest::Approx(0).epsilon(0.01));
            else {  // correndo ha fase de voo: os pes nunca afundam e sobem pouco
                CHECK(h > -0.01f);
                CHECK(h < 0.1f);
            }
        }
}
TEST_CASE("proporções diferentes também ficam no chão") {
    Esqueleto baixo = esqueletoHumano({1.6f, 0.9f, 1.1f, 1.f});
    for (float f = 0; f < 6.3f; f += 0.7f) CHECK(alturaDosPes(baixo, poseAndar(baixo, f, 1.6f)) == doctest::Approx(0).epsilon(0.01));
}

static const Esqueleto& cav() {
    static Esqueleto e = esqueletoCavalo();
    return e;
}
TEST_CASE("cavalo parado com cascos no chão") {
    for (float t = 0; t < 5; t += 0.7f) CHECK(alturaDosCascos(cav(), poseCavalo(cav(), 0, 0, t)) == doctest::Approx(0).epsilon(0.01));
}
TEST_CASE("cavalo mantém apoio ao passo e ao trote") {
    for (int a = 1; a <= 2; a++)
        for (float f = 0; f < 6.3f; f += 0.4f) CHECK(alturaDosCascos(cav(), poseCavalo(cav(), f, a, 0)) == doctest::Approx(0).epsilon(0.01));
}
TEST_CASE("a sola da malha do cavalo acompanha o apoio das andaduras") {
    for (bool perto : {false, true}) {
        auto malha = corpoCavalo({}, perto);
        std::vector<Mat4> matrizes;
        for (int andadura = 0; andadura < 4; ++andadura) {
            for (float fase = 0; fase < 6.3f; fase += .4f) {
                calcularMatrizes(cav(), poseCavalo(cav(), fase, andadura, fase), matrizes);
                float apoio = 1.f;
                for (const auto& v : malha.v) {
                    if (v.pos.y == 0.f) apoio = std::min(apoio, matrizes[v.osso[0]].ponto(v.pos).y);
                }
                CHECK(apoio >= -.005f);
                CHECK(apoio < (andadura == int(Andadura::Galope) ? .19f : .005f));
            }
        }
    }
}
TEST_CASE("galope: os cascos plantam nas batidas e avançam juntos com o chão") {
    const int cascos[] = {C_CASCO_TD, C_CASCO_TE, C_CASCO_DD, C_CASCO_DE};
    std::vector<Mat4> matrizes;
    constexpr float trecho = .65f;  // ainda no apoio, alguns quadros depois do toque
    for (size_t i = 0; i < TOQUES_GALOPE.size(); ++i) {
        int casco = cascos[i];
        Vec3f sola = cav().ossos[casco].fim;
        float fase = TOQUES_GALOPE[i] + .001f;
        calcularMatrizes(cav(), poseCavalo(cav(), fase, int(Andadura::Galope), 0), matrizes);
        Vec3f primeiro = matrizes[casco].ponto(sola);
        CHECK(std::abs(primeiro.y) < .002f);
        calcularMatrizes(cav(), poseCavalo(cav(), fase + trecho, int(Andadura::Galope), 0), matrizes);
        Vec3f segundo = matrizes[casco].ponto(sola);
        CHECK(std::abs(segundo.y) < .002f);
        CHECK(segundo.z + trecho / FASE_GALOPE_POR_METRO == doctest::Approx(primeiro.z).epsilon(.002));
    }
}

TEST_CASE("galope: há suspensão e transição suave a partir do trote") {
    CHECK(alturaDosCascos(cav(), poseCavalo(cav(), 5.95f, int(Andadura::Galope), 0)) > .08f);
    Cavalo c;
    c.andadura = Andadura::Trote;
    c.passada = 5.95f;
    c.corpo.vel = 8.49f;
    Pose antes = poseCavalo(cav(), c, 0);
    c.andadura = Andadura::Galope;
    c.corpo.vel = 8.51f;
    Pose depois = poseCavalo(cav(), c, 0);
    CHECK(len(depois.raiz - antes.raiz) < .008f);
    for (size_t i = 0; i < antes.rot.size(); ++i) CHECK(len(depois.rot[i] - antes.rot[i]) < .04f);
}

TEST_CASE("trote: diagonais juntas") {
    Pose p = poseCavalo(cav(), 1.2f, 2, 0);
    CHECK(p.rot[C_BRACO_DE].x == doctest::Approx(p.rot[C_COXA_TD].x).epsilon(0.05));
    CHECK(p.rot[C_BRACO_DE].x * p.rot[C_BRACO_DD].x < 0);
}
TEST_CASE("galope balança mais que passo") {
    float passo = 0, galope = 0;
    for (float f = 0; f < 6.3f; f += 0.1f) {
        passo = std::max(passo, std::abs(poseCavalo(cav(), f, 1, 0).rot[C_BRACO_DE].x));
        galope = std::max(galope, std::abs(poseCavalo(cav(), f, 3, 0).rot[C_BRACO_DE].x));
    }
    CHECK(galope > passo * 1.5f);
}

// ---------- juntas nao podem achatar ao dobrar ----------
static float espessuraRelativa(const MalhaPele& m, const Esqueleto& e, const Pose& p, int pai, int filho) {
    std::vector<Mat4> mt;
    calcularMatrizes(e, p, mt);
    Vec3f j = e.ossos[filho].inicio;                       // junta em repouso
    Vec3f jp = mt[pai].ponto(j);                           // junta na pose
    float soma = 0, somaRep = 0;
    int n = 0;
    for (auto& v : m.v) {
        bool doPar = (v.osso[0] == pai || v.osso[0] == filho) && (v.osso[1] == pai || v.osso[1] == filho);
        if (!doPar || std::abs(v.pos.y - j.y) > 0.03f) continue;
        Vec3f q = mt[v.osso[0]].ponto(v.pos) * v.peso + mt[v.osso[1]].ponto(v.pos) * (1 - v.peso);
        soma += len(q - jp);
        somaRep += len(v.pos - j);
        n++;
    }
    REQUIRE(n > 0);
    return soma / somaRep;
}
static Pose maisDobrado(const Esqueleto& e, int osso) {
    Pose melhor;
    float maior = -1;
    for (float f = 0; f < 6.3f; f += 0.1f) {
        Pose p = poseAndar(e, f, 5.f);
        if (std::abs(p.rot[osso].x) > maior) { maior = std::abs(p.rot[osso].x); melhor = p; }
    }
    return melhor;
}
TEST_CASE("joelho e cotovelo não achatam correndo (modelo leve e HD)") {
    Proporcoes pr;
    Esqueleto e = esqueletoHumano(pr);
    Visual v;
    v.sobretudo = false;
    for (int hd = 0; hd < 2; hd++) {
        MalhaPele m = hd ? corpoHumanoHD(pr, v) : corpoHumano(pr, v);
        // joelho: a corrida real dobra ~95 graus (dados de marcha); a malha aguenta ate ~85% da espessura
        CHECK_MESSAGE(espessuraRelativa(m, e, maisDobrado(e, CANELA_E), COXA_E, CANELA_E) > 0.84f, "joelho hd=" << hd);
        CHECK_MESSAGE(espessuraRelativa(m, e, maisDobrado(e, ANTEBRACO_E), BRACO_E, ANTEBRACO_E) > 0.88f, "cotovelo hd=" << hd);
    }
}

// posicao no mundo (pose aplicada) do inicio/fim de um osso
static Vec3f juntaPose(const Pose& p, int osso, bool fim) {
    std::vector<Mat4> m;
    calcularMatrizes(esq(), p, m);
    const Osso& o = esq().ossos[osso];
    Vec3f r = m[osso].ponto(fim ? o.fim : o.inicio);
    return r + p.raiz * 0.f;  // a raiz ja entra nas matrizes
}
static float maisAlto(const Pose& p) {
    float y = -1e9f;
    for (int o = 0; o < N_OSSOS_HUMANO; o++) y = std::max({y, juntaPose(p, o, false).y, juntaPose(p, o, true).y});
    return y;
}
static float maisBaixo(const Pose& p) {
    float y = 1e9f;
    for (int o = 0; o < N_OSSOS_HUMANO; o++) y = std::min({y, juntaPose(p, o, false).y, juntaPose(p, o, true).y});
    return y;
}
TEST_CASE("mirar: mao direita a frente na altura do ombro") {
    Pose p = poseMirar(esq(), 0, 0);
    Vec3f mao = juntaPose(p, MAO_D, true), ombro = juntaPose(p, BRACO_D, false), peito = juntaPose(p, PEITO, true);
    CHECK(mao.z - peito.z > 0.5f);
    CHECK(std::abs(mao.y - ombro.y) < 0.1f);
    CHECK(std::abs(mao.x) < std::abs(ombro.x) + 0.02f);  // aponta para a frente do corpo, nao para fora
}
TEST_CASE("mirar: angulo levanta a mao e recuo levanta o antebraco") {
    CHECK(juntaPose(poseMirar(esq(), 0.5f, 0), MAO_D, true).y > juntaPose(poseMirar(esq(), 0, 0), MAO_D, true).y + 0.15f);
    CHECK(juntaPose(poseMirar(esq(), 0, 1), MAO_D, true).y > juntaPose(poseMirar(esq(), 0, 0), MAO_D, true).y + 0.05f);
}
TEST_CASE("cair: no fim deitado no chao") {
    Pose p = poseCair(esq(), 1.f);
    CHECK(maisAlto(p) < 0.45f);
    CHECK(maisBaixo(p) > -0.02f);
    CHECK(maisBaixo(p) < 0.15f);
    CHECK(maisAlto(poseCair(esq(), 0.f)) > 1.6f);  // comeca em pe
}
TEST_CASE("rendido: maos acima da cabeca e joelho no chao") {
    Pose p = poseRendido(esq(), 0.3f);
    float cab = juntaPose(p, CABECA, false).y;
    CHECK(juntaPose(p, MAO_E, true).y > cab + 0.1f);
    CHECK(juntaPose(p, MAO_D, true).y > cab + 0.1f);
    CHECK(std::min(juntaPose(p, CANELA_E, false).y, juntaPose(p, CANELA_D, false).y) < 0.15f);
    CHECK(maisBaixo(p) > -0.02f);
}
TEST_CASE("amarrado: sentado com as maos para tras") {
    Pose p = poseAmarrado(esq(), 0.3f);
    float zPeito = juntaPose(p, PEITO, false).z;
    CHECK(juntaPose(p, MAO_E, true).z < zPeito);
    CHECK(juntaPose(p, MAO_D, true).z < zPeito);
    CHECK(juntaPose(p, QUADRIL, false).y < 0.5f);
    CHECK(maisBaixo(p) > -0.02f);
}
TEST_CASE("recarregar e tranco mexem no corpo") {
    Pose a = poseRecarregar(esq(), 0.5f);
    CHECK(juntaPose(a, MAO_D, true).z > 0.15f);  // maos na frente do corpo
    Pose p = poseParado(esq(), 0), q = p;
    aplicarTranco(q, 1.f);
    CHECK(juntaPose(q, CABECA, true).z < juntaPose(p, CABECA, true).z - 0.03f);  // joga o tronco para tras
}
TEST_CASE("garupa: dobrado na cintura") {
    Pose p = poseGarupa(esq());
    // cabeca e pes abaixo do quadril (atravessado no cavalo)
    float q = juntaPose(p, QUADRIL, false).y;
    CHECK(juntaPose(p, CABECA, true).y < q);
    CHECK(juntaPose(p, PE_E, true).y < q);
}
// mede quanto o pe de apoio (o mais baixo) escorrega no chao enquanto o corpo anda v m/s
static float escorregaoPorMetro(float v) {
    const Esqueleto& e = esq();
    float dist = 0, esc = 0, zAnt = 0;
    int peAnt = -1;
    std::vector<Mat4> m;
    for (int k = 0; k < 400; k++) {
        float d = k * 0.01f;  // metros andados
        Pose p = poseAndar(e, d * fasePorMetro(v), v);
        calcularMatrizes(e, p, m);
        int pe = -1;
        float baixo = 1e9f, z = 0;
        for (int o : {PE_E, PE_D}) {  // calcanhar e ponta (os pontos que tocam o chao)
            const Osso& ob = e.ossos[o];
            Vec3f pts[2] = {{ob.inicio.x, 0.f, ob.inicio.z - 0.045f}, {ob.fim.x, 0.f, ob.fim.z + 0.01f}};
            for (int h = 0; h < 2; h++) {
                Vec3f q = m[o].ponto(pts[h]);
                if (q.y < baixo) { baixo = q.y; pe = o * 2 + h; z = q.z + d; }
            }
        }
        if (pe == peAnt && baixo < 0.01f) esc += std::abs(z - zAnt);  // so conta com o pe no chao (nao no voo)
        peAnt = pe;
        zAnt = z;
        dist = d;
    }
    return esc / dist;
}
TEST_CASE("andar e correr: o pe de apoio nao escorrega") {
    CHECK(escorregaoPorMetro(1.6f) < 0.12f);
    CHECK(escorregaoPorMetro(5.f) < 0.12f);
}

// ---------- marcha parecida com a de pessoas (numeros de laboratorio) ----------
static float alturaQuadril(const Pose& p) { return juntaPose(p, QUADRIL, false).y; }
static float cadencia(float v) { return v / (3.14159265f / fasePorMetro(v)) * 60.f; }  // passos por minuto

TEST_CASE("andar: a bacia sobe e desce suave, sem tranco na pisada") {
    const float dfase = 6.2831853f / 200;  // 200 amostras por ciclo (~6 ms cada a 1,6 m/s)
    float menor = 1e9f, maior = -1e9f, maiorSalto = 0, uMaior = 0;
    float ant = alturaQuadril(poseAndar(esq(), 1.5707963f - dfase, 1.6f));
    for (int i = 0; i < 200; i++) {
        float y = alturaQuadril(poseAndar(esq(), 1.5707963f + dfase * i, 1.6f));
        maiorSalto = std::max(maiorSalto, std::abs(y - ant));
        if (y > maior) { maior = y; uMaior = i / 200.f; }
        menor = std::min(menor, y);
        ant = y;
    }
    CHECK(maior - menor > 0.03f);  // pessoas: ~4-5 cm de sobe e desce andando
    CHECK(maior - menor < 0.06f);
    CHECK(maiorSalto < 0.003f);    // antes: ~2,5 cm de queda em 7 ms logo apos cada pisada
    // mais alta perto do meio do apoio de um dos pes (u ~ 0,25 ou ~ 0,75)
    float meio = std::fmod(uMaior, 0.5f);
    CHECK(meio > 0.15f);
    CHECK(meio < 0.35f);
}
TEST_CASE("andar: joelho de apoio quase esticado no meio do apoio") {
    // u = 0,30 do pe esquerdo: o corpo passa por cima do pe de apoio (pessoas: ~5-15 graus)
    Pose p = poseAndar(esq(), 1.5707963f + 6.2831853f * 0.30f, 1.6f);
    CHECK(p.rot[CANELA_E].x < 0.30f);
    CHECK(p.rot[CANELA_E].x > 0.f);
}
TEST_CASE("cadência perto da medida em pessoas") {
    CHECK(cadencia(1.6f) > 110.f);  // andar rapido: ~115-125 passos/min
    CHECK(cadencia(1.6f) < 130.f);
    CHECK(cadencia(5.f) > 170.f);   // correndo a 18 km/h: ~175-200 (antes ~275)
    CHECK(cadencia(5.f) < 225.f);
}
TEST_CASE("correr tem fase de voo; andar sempre tem um pé no chão") {
    bool voou = false;
    for (float f = 0; f < 6.3f; f += 0.05f) {
        if (alturaDosPes(esq(), poseAndar(esq(), f, 5.f)) > 0.01f) voou = true;
        CHECK(alturaDosPes(esq(), poseAndar(esq(), f, 1.6f)) < 0.005f);
    }
    CHECK(voou);
}
TEST_CASE("andar: o pé no ar não raspa no chão") {
    // pe direito no meio do balanco (u do direito entre 0,70 e 0,90 -> u do esquerdo entre 0,20 e 0,40)
    std::vector<Mat4> m;
    for (float u = 0.20f; u <= 0.40f; u += 0.01f) {
        Pose p = poseAndar(esq(), 1.5707963f + 6.2831853f * u, 1.6f);
        calcularMatrizes(esq(), p, m);
        const Osso& o = esq().ossos[PE_D];
        float ponta = m[PE_D].ponto({o.fim.x, 0.f, o.fim.z + 0.01f}).y;
        float calc = m[PE_D].ponto({o.inicio.x, 0.f, o.inicio.z - 0.045f}).y;
        CHECK(std::min(ponta, calc) > 0.008f);
    }
}
TEST_CASE("devagar os passos encurtam, a cadência quase não muda e os pés ficam no chão") {
    // comecando a andar ou parando: passos curtos no mesmo ritmo, como as pessoas
    CHECK(cadencia(0.5f) == doctest::Approx(cadencia(1.0f)).epsilon(0.02));
    CHECK(6.2831853f / fasePorMetro(0.5f) < 0.6f * 6.2831853f / fasePorMetro(1.6f));
    for (float f = 0; f < 6.3f; f += 0.3f) {
        Pose p = poseAndar(esq(), f, 0.5f);
        CHECK(alturaDosPes(esq(), p) == doctest::Approx(0).epsilon(0.01));
        CHECK(std::abs(p.raiz.y) < 0.03f);  // passos curtos: corpo quase na altura de em pe
    }
    CHECK(escorregaoPorMetro(0.5f) < 0.12f);
}
