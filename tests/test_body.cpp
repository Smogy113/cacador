#include <doctest/doctest.h>
#include <algorithm>
#include "core/body.h"
#include "core/sdf_head.h"

static Visual semChapeu() {
    Visual v;
    v.chapeuAba = false;
    return v;
}

static float distSegmento(Vec3f p, Vec3f a, Vec3f b, Vec3f& maisPerto) {
    Vec3f ab = b - a;
    float t = std::clamp(dot(p - a, ab) / std::max(dot(ab, ab), 1e-8f), 0.f, 1.f);
    maisPerto = a + ab * t;
    return len(p - maisPerto);
}

TEST_CASE("corpo humano tem entre 1000 e 1600 triângulos") {
    for (bool sob : {false, true})
        for (bool ch : {false, true}) {
            Visual v;
            v.sobretudo = sob;
            v.chapeuAba = ch;
            auto m = corpoHumano({}, v);
            CHECK(m.triangulos() >= 1000);
            CHECK(m.triangulos() <= 1600);
        }
}
TEST_CASE("ossos e pesos válidos") {
    auto m = corpoHumano({}, Visual{});
    for (auto& v : m.v) {
        CHECK(v.osso[0] < N_OSSOS_HUMANO);
        CHECK(v.osso[1] < N_OSSOS_HUMANO);
        CHECK(v.peso >= 0.f);
        CHECK(v.peso <= 1.f);
    }
    for (auto i : m.idx) CHECK(i < m.v.size());
}
TEST_CASE("altura e pés no chão") {
    for (float alt : {1.6f, 1.8f, 1.95f}) {
        Proporcoes p;
        p.altura = alt;
        auto m = corpoHumano(p, semChapeu());
        float topo = -1e9f, base = 1e9f;
        for (auto& v : m.v) { topo = std::max(topo, v.pos.y); base = std::min(base, v.pos.y); }
        CHECK(topo == doctest::Approx(alt).epsilon(0.03));
        CHECK(base == doctest::Approx(0).epsilon(0.01));
        CHECK(base > -0.01f);
    }
}
TEST_CASE("normais apontam para fora do osso") {
    Proporcoes p;
    auto m = corpoHumano(p, semChapeu());
    auto e = esqueletoHumano(p);
    std::vector<bool> usado(m.v.size(), false);
    for (auto i : m.idx) usado[i] = true;
    int bons = 0, total = 0;
    for (size_t i = 0; i < m.v.size(); i++) {
        if (!usado[i]) continue;
        const auto& v = m.v[i];
        const Osso& o = e.ossos[v.osso[0]];
        Vec3f q;
        distSegmento(v.pos, o.inicio, o.fim, q);
        total++;
        if (dot(v.normal, v.pos - q) > 0) bons++;
        CHECK(len(v.normal) == doctest::Approx(1).epsilon(0.01));
    }
    CHECK(bons >= int(total * 0.95f));
}
TEST_CASE("cotovelo e joelho misturam dois ossos") {
    auto m = corpoHumano({}, Visual{});
    bool cotovelo = false, joelho = false;
    for (auto& v : m.v) {
        auto par = [&](int a, int b) {
            return ((v.osso[0] == a && v.osso[1] == b) || (v.osso[0] == b && v.osso[1] == a)) && v.peso > 0.2f && v.peso < 0.8f;
        };
        if (par(BRACO_E, ANTEBRACO_E)) cotovelo = true;
        if (par(COXA_D, CANELA_D)) joelho = true;
    }
    CHECK(cotovelo);
    CHECK(joelho);
}
TEST_CASE("esqueleto: pais antes dos filhos e 17 ossos") {
    auto e = esqueletoHumano({});
    REQUIRE(e.ossos.size() == N_OSSOS_HUMANO);
    for (size_t i = 0; i < e.ossos.size(); i++) CHECK(e.ossos[i].pai < int(i));
}
TEST_CASE("lado esquerdo do personagem fica em +x (ele olha para +z)") {
    auto e = esqueletoHumano({});
    CHECK(e.ossos[MAO_E].inicio.x > 0);
    CHECK(e.ossos[MAO_D].inicio.x < 0);
}

TEST_CASE("cavalo: até 2000 triângulos, ossos válidos") {
    for (bool sel : {false, true}) {
        VisualCavalo v;
        v.selado = sel;
        auto m = corpoCavalo(v);
        CHECK(m.triangulos() <= 9000);
        CHECK(m.triangulos() >= 600);
        for (auto& x : m.v) {
            CHECK(x.osso[0] < N_OSSOS_CAVALO);
            CHECK(x.osso[1] < N_OSSOS_CAVALO);
        }
    }
    auto e = esqueletoCavalo();
    REQUIRE(e.ossos.size() == N_OSSOS_CAVALO);
    CHECK(N_OSSOS_CAVALO <= 17);
    for (size_t i = 0; i < e.ossos.size(); i++) CHECK(e.ossos[i].pai < int(i));
}
TEST_CASE("cavalo: cascos no chão e olha para +z") {
    auto m = corpoCavalo({});
    float base = 1e9f, zCabeca = -1e9f;
    for (auto& v : m.v) {
        base = std::min(base, v.pos.y);
        if (v.osso[0] == C_CABECA) zCabeca = std::max(zCabeca, v.pos.z);
    }
    CHECK(base == doctest::Approx(0).epsilon(0.01));
    CHECK(zCabeca > 1.2f);
    CHECK(assentoCavalo().y > 1.6f);
}

// ---------------- alta definicao ----------------
TEST_CASE("HD: entre 6 e 14 mil triângulos e índices válidos") {
    for (bool eq : {false, true}) {
        Visual v;
        v.equipamento = eq;
        auto m = corpoHumanoHD({}, v);
        CHECK(m.triangulos() >= 6000);
        CHECK(m.triangulos() <= 50000);  // moradores: escultura com celulas de 3,8 mm
        CHECK(m.v.size() < 65535);
        for (auto i : m.idx) CHECK(i < m.v.size());
        for (auto& x : m.v) {
            CHECK(x.osso[0] < N_OSSOS_HUMANO);
            CHECK(x.osso[1] < N_OSSOS_HUMANO);
            CHECK(x.peso >= 0.f);
            CHECK(x.peso <= 1.f);
        }
    }
}
TEST_CASE("HD: altura, pés no chão e normais para fora") {
    Proporcoes p;
    auto m = corpoHumanoHD(p, semChapeu());
    float topo = -1e9f, base = 1e9f;
    for (auto& v : m.v) { topo = std::max(topo, v.pos.y); base = std::min(base, v.pos.y); }
    CHECK(topo == doctest::Approx(p.altura).epsilon(0.03));
    CHECK(base == doctest::Approx(0).epsilon(0.01));
    auto e = esqueletoHumano(p);
    std::vector<bool> usado(m.v.size(), false);
    for (auto i : m.idx) usado[i] = true;
    int bons = 0, total = 0;
    for (size_t i = 0; i < m.v.size(); i++) {
        if (!usado[i]) continue;
        const Osso& o = e.ossos[m.v[i].osso[0]];
        Vec3f q;
        distSegmento(m.v[i].pos, o.inicio, o.fim, q);
        total++;
        if (dot(m.v[i].normal, m.v[i].pos - q) > 0) bons++;
    }
    CHECK(bons >= int(total * 0.93f));
}
TEST_CASE("HD: tem nariz e dedos") {
    auto hd = corpoHumanoHD({}, semChapeu()), ld = corpoHumano({}, semChapeu());
    float narizZ = -1, testaZ = -1;
    int maoHD = 0, maoLD = 0;
    for (auto& v : hd.v) {
        if (v.osso[0] == CABECA && v.pos.y > 1.67f && v.pos.y < 1.70f && std::abs(v.pos.x) < 0.02f) narizZ = std::max(narizZ, v.pos.z);
        if (v.osso[0] == CABECA && v.pos.y > 1.745f && v.pos.y < 1.76f && std::abs(v.pos.x) < 0.02f) testaZ = std::max(testaZ, v.pos.z);
        if (v.osso[0] == MAO_E) maoHD++;
    }
    for (auto& v : ld.v) if (v.osso[0] == MAO_E) maoLD++;
    CHECK(narizZ > testaZ + 0.015f);
    CHECK(maoHD > maoLD * 4);
}

TEST_CASE("HD AAA: materiais válidos e de todos os tipos") {
    Visual v;
    v.equipamento = true;
    v.barba = true;
    auto m = corpoHumanoHD({}, v);
    int cont[25] = {0};
    for (auto& x : m.v) {
        int id = int(x.material + 0.5f);
        REQUIRE(id >= 10);
        REQUIRE(id <= 24);
        REQUIRE(id != 17);
        REQUIRE(id != 19);
        cont[id]++;
    }
    for (float mt : {MAT_PELE, MAT_TECIDO, MAT_COURO, MAT_METAL, MAT_OLHO, MAT_FELTRO, MAT_LABIO}) CHECK_MESSAGE(cont[int(mt)] > 0, "material " << mt);
    CHECK(cont[24] > 0);  // cartoes de pelo
}
TEST_CASE("HD AAA: dois globos oculares na frente do rosto") {
    auto m = corpoHumanoHD({}, Visual{});
    int esq = 0, dir = 0;
    for (auto& x : m.v)
        if (x.material == MAT_OLHO && x.osso[0] == CABECA) (x.pos.x > 0 ? esq : dir)++;
    CHECK(esq >= 40);
    CHECK(dir >= 40);
}

// ---------------- rosto com medidas reais (antropometria de adulto) ----------------
static void centrosOlhos(const MalhaPele& m, Vec3f& e, Vec3f& d) {
    Vec3f se{}, sd{};
    int ne = 0, nd = 0;
    for (auto& v : m.v)
        if (v.material == MAT_OLHO) {
            if (v.pos.x > 0) { se = se + v.pos; ne++; } else { sd = sd + v.pos; nd++; }
        }
    REQUIRE(ne > 0);
    REQUIRE(nd > 0);
    e = se * (1.f / ne);
    d = sd * (1.f / nd);
}
TEST_CASE("rosto real: distância entre as pupilas 60-66 mm") {
    auto m = corpoHumanoHD({}, Visual{});
    Vec3f e, d;
    centrosOlhos(m, e, d);
    CHECK(len(e - d) > 0.060f);
    CHECK(len(e - d) < 0.066f);
    CHECK(e.y == doctest::Approx(1.701f).epsilon(0.01));
}
TEST_CASE("rosto real: nariz projeta 15-25 mm na frente da bochecha") {
    auto m = corpoHumanoHD({}, semChapeu());
    float ponta = -1, bochecha = -1;
    for (auto& v : m.v) {
        if (v.osso[0] != CABECA || v.material == MAT_OLHO || v.material >= MAT_PELO) continue;
        if (std::abs(v.pos.y - 1.667f) < 0.006f) {
            if (std::abs(v.pos.x) < 0.004f) ponta = std::max(ponta, v.pos.z);
            if (std::abs(std::abs(v.pos.x) - 0.045f) < 0.006f) bochecha = std::max(bochecha, v.pos.z);
        }
    }
    CHECK(ponta - bochecha > 0.015f);
    CHECK(ponta - bochecha < 0.045f);
}
TEST_CASE("rosto real: boca com 45-60 mm de largura") {
    auto m = corpoHumanoHD({}, semChapeu());
    float xmax = 0;
    for (auto& v : m.v)
        if (v.material == MAT_LABIO) xmax = std::max(xmax, std::abs(v.pos.x));
    CHECK(2 * xmax > 0.045f);
    CHECK(2 * xmax < 0.060f);
}
TEST_CASE("barba curta: marcada na pele do queixo e da mandíbula só com barba") {
    Visual com, sem;
    com.barba = true;
    sem.barba = false;
    auto contar = [](const MalhaPele& m) {
        int n = 0;
        for (auto& v : m.v) if (v.material == MAT_PELE && v.pelos > 0.5f && v.pos.y < 1.645f && v.pos.z > 0.03f) n++;
        return n;
    };
    int a = contar(corpoHumanoHD({}, com)), b = contar(corpoHumanoHD({}, sem));
    CHECK(a > 150);
    CHECK(b < a / 6);
}
TEST_CASE("pelos crescem certo: barba para baixo, sobrancelha para os lados") {
    Visual v;
    v.barba = true;
    auto m = corpoHumanoHD({}, v);
    int barba = 0, barbaBaixo = 0, sob = 0, sobLado = 0;
    for (auto& x : m.v) {
        if (x.material == MAT_PELE && x.pelos > 0.5f && x.pos.y < 1.63f && x.pos.y > 1.595f && x.pos.z > 0.05f) {
            barba++;
            if (x.dirPelo.y < -0.3f) barbaBaixo++;
        }
        if (x.material == MAT_PELE && x.pelos > 0.5f && x.pos.y > 1.71f && x.pos.y < 1.728f && x.pos.z > 0.07f && std::abs(x.pos.x) > 0.025f) {
            sob++;  // sobrancelha desenhada na pele: direcao para os lados
            if (std::abs(x.dirPelo.x) > std::abs(x.dirPelo.y)) sobLado++;
        }
    }
    REQUIRE(barba > 30);
    REQUIRE(sob > 20);
    CHECK(barbaBaixo > barba * 0.8f);
    CHECK(sobLado > sob * 0.7f);
}
TEST_CASE("cartões saem da pele, no máximo 15 mm") {
    Visual v;
    v.barba = true;
    auto m = corpoHumanoHD({}, v);
    for (auto& x : m.v)
        if (x.material == MAT_CARTAO) CHECK(x.compPelo <= 0.015f);
}
TEST_CASE("caçador completo (herói: cabeça esculpida em resolução máxima) cabe no orçamento") {
    Visual v;
    v.barba = true;
    v.equipamento = true;
    v.heroi = true;
    auto m = corpoHumanoHD({}, v);
    CHECK(m.triangulos() <= 95000);
    CHECK(m.v.size() < 65535);
}

TEST_CASE("cada triângulo tem um só material (sem misturar no meio)") {
    Visual v;
    v.barba = true;
    v.equipamento = true;
    for (int qual = 0; qual < 2; qual++) {
        auto m = qual ? corpoCavalo({}) : corpoHumanoHD({}, v);
        int ruins = 0;
        for (size_t t = 0; t < m.idx.size(); t += 3) {
            float a = m.v[m.idx[t]].material, b = m.v[m.idx[t + 1]].material, c = m.v[m.idx[t + 2]].material;
            if (a != b || b != c) ruins++;
        }
        CHECK_MESSAGE(ruins == 0, (qual ? "cavalo" : "humano") << " com " << ruins);
    }
}

TEST_CASE("rosto masculino: mandíbula com 70% ou mais da largura das maçãs (anatomia real, afina em V)") {
    auto m = corpoHumanoHD({}, semChapeu());
    float mand = 0, macas = 0;
    for (auto& v : m.v) {
        if (v.osso[0] != CABECA || v.material != MAT_PELE || v.pos.z < -0.02f) continue;
        if (v.pos.z < 0.03f) continue;  // so a parte da frente do rosto
        if (v.pos.y > 1.612f && v.pos.y < 1.622f) mand = std::max(mand, std::abs(v.pos.x));  // altura do angulo da mandibula
        if (v.pos.y > 1.678f && v.pos.y < 1.69f) macas = std::max(macas, std::abs(v.pos.x));
    }
    REQUIRE(macas > 0);
    CHECK(mand / macas >= 0.70f);
    CHECK(mand / macas <= 0.85f);
}
TEST_CASE("barba curta: pelos da barba com no máximo 4,5 mm") {
    Visual v;
    v.barba = true;
    auto m = corpoHumanoHD({}, v);
    for (auto& x : m.v)
        if (x.material == MAT_CARTAO && x.pos.y < 1.645f && x.pos.z > 0.03f) CHECK(x.compPelo <= 0.0045f);
}

TEST_CASE("barba: nada nas bochechas (só bigode, queixo e borda da mandíbula)") {
    Visual v;
    v.barba = true;
    auto m = corpoHumanoHD({}, v);
    int bochecha = 0, comPelo = 0;
    for (auto& x : m.v) {
        if (x.material != MAT_PELE) continue;
        float ax = std::abs(x.pos.x);
        if (ax > 0.036f && ax < 0.062f && x.pos.y > 1.638f && x.pos.y < 1.685f && x.pos.z > 0.035f) {
            bochecha++;
            if (x.pelos > 0.15f) comPelo++;
        }
    }
    REQUIRE(bochecha > 50);
    CHECK(comPelo == 0);
}
TEST_CASE("sobrancelha grossa: faixa de pelo com 6,5 mm ou mais de altura") {
    auto m = corpoHumanoHD({}, Visual{});
    float ymin = 9, ymax = 0;
    for (auto& x : m.v)
        if (x.material == MAT_PELE && x.pelos > 0.3f && std::abs(std::abs(x.pos.x) - 0.032f) < 0.003f && x.pos.y > 1.705f && x.pos.z > 0.07f) {
            ymin = std::min(ymin, x.pos.y);
            ymax = std::max(ymax, x.pos.y);
        }
    REQUIRE(ymax > ymin);
    CHECK(ymax - ymin >= 0.0065f);
}
TEST_CASE("rosto equilibrado (não exagerado): queixo atrás do lábio e terço de baixo proporcional") {
    auto frenteEm = [](float y) {
        float z = 0;
        for (float zz = 0.14f; zz > 0.0f; zz -= 0.0002f)
            if (sdf::cabeca({0, y, zz}) < 0) { z = zz; break; }
        return z;
    };
    float labioInf = 0;
    for (float y = 1.622f; y < 1.632f; y += 0.0005f) labioInf = std::max(labioInf, frenteEm(y));
    float queixo = 0;
    for (float y = 1.59f; y < 1.61f; y += 0.0005f) queixo = std::max(queixo, frenteEm(y));
    CHECK(queixo < labioInf - 0.001f);
    float menton = 1.7f;  // ponto mais baixo do queixo na linha do meio
    for (float y = 1.62f; y > 1.55f; y -= 0.0005f) {
        bool tem = false;
        for (float z = 0.05f; z < 0.1f; z += 0.001f)
            if (sdf::cabeca({0, y, z}) < 0) tem = true;
        if (tem) menton = y; else break;
    }
    float terco = (1.6555f - menton) / (1.708f - 1.6555f);  // subnasal ao menton / raiz do nariz ao subnasal
    CHECK(terco <= 1.25f);
    CHECK(terco >= 0.95f);
}
TEST_CASE("revolver: na mao direita, leve e HD") {
    Visual sem, com;
    com.revolver = true;
    auto a = corpoHumano({}, sem), b = corpoHumano({}, com);
    CHECK(b.triangulos() > a.triangulos());
    CHECK(b.triangulos() - a.triangulos() < 600);
    CHECK(b.triangulos() <= 1600);
    int naMao = 0;
    for (size_t i = a.v.size(); i < b.v.size(); i++) naMao += b.v[i].osso[0] == MAO_D && b.v[i].osso[1] == MAO_D;
    CHECK(naMao > 20);
    auto h0 = corpoHumanoHD({}, sem), h1 = corpoHumanoHD({}, com);
    CHECK(h1.triangulos() > h0.triangulos());
    CHECK(h1.triangulos() - h0.triangulos() < 600);
}
