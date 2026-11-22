#include "render/props.h"
#include <algorithm>
#include <cmath>
#include <map>
#include "core/meshbuilder.h"
#include "core/noise.h"
#include "raymath.h"
#include "render/meshconv.h"

namespace {
// PI vem da raylib

// ---------- paleta (sRGB) ----------
const Rgb PEDRA{0.56f, 0.54f, 0.50f}, PEDRA_ESC{0.42f, 0.41f, 0.40f}, REBOCO{0.86f, 0.80f, 0.68f},
    REBOCO2{0.80f, 0.74f, 0.60f}, MADEIRA{0.45f, 0.32f, 0.21f}, MADEIRA_ESC{0.27f, 0.19f, 0.13f},
    TELHA_V{0.62f, 0.28f, 0.19f}, TELHA_M{0.42f, 0.27f, 0.18f}, ARDOSIA{0.33f, 0.36f, 0.42f},
    PALHA{0.72f, 0.60f, 0.34f}, JANELA{1.0f, 0.72f, 0.36f}, CHAMA{1.0f, 0.52f, 0.14f},
    CHAMA2{1.0f, 0.85f, 0.4f}, FERRO{0.2f, 0.2f, 0.22f}, ESCURO{0.03f, 0.03f, 0.035f};
const Rgb LONAS[] = {{0.72f, 0.2f, 0.15f}, {0.22f, 0.38f, 0.58f}, {0.85f, 0.68f, 0.3f}, {0.3f, 0.52f, 0.3f}, {0.8f, 0.75f, 0.65f}};
const Rgb COR_LUZ_FOGO{1.0f, 0.55f, 0.22f};

// Sistema de coordenadas local de um objeto (origem + rotacao).
struct Frame {
    Vec3f o;
    float rot;
    Vec3f P(float x, float y, float z) const { return o + rotY({x, y, z}, rot); }
};

struct Gerador {
    const Terreno& t;
    MundoColisao& col;
    std::vector<LuzPontual>& luzes;
    std::vector<EstruturaMapa>& estruturas;
    uint32_t rng;
    std::map<int, MeshBuilder> constr;  // por pedaco
    std::map<int, MeshBuilder> predioDet, predioCasca;  // predios por pedaco: detalhados e simples (longe, Baixo, sombras)
    int n;
    std::vector<AABB> ocupado;          // pegadas ja usadas (evita sobrepor)
    std::vector<FormaCena>* formas = nullptr;   // construcoes como formas exatas (luz tracada)
    std::vector<JanelaLuz>* janelas = nullptr;
    std::vector<PortaMundo>* portas = nullptr;
    std::vector<InteriorMundo>* interiores = nullptr;
    RegioesCapim* regioesCapim = nullptr;
    int proximoGrupo = 0;

    float r01() { return aleatorio01(rng); }
    float r(float a, float b) { return a + (b - a) * r01(); }
    int pedaco(Vec3f p) const {
        int i = std::clamp(int((p.x + Terreno::META) / Cenario::TAM), 0, n - 1);
        int j = std::clamp(int((p.z + Terreno::META) / Cenario::TAM), 0, n - 1);
        return j * n + i;
    }
    MeshBuilder& b(Vec3f p) { return constr[pedaco(p)]; }
    static int quadrante(Vec3f p) {  // qual quarto (32 x 32 m) do pedaco
        float lx = std::fmod(p.x + Terreno::META, Cenario::TAM), lz = std::fmod(p.z + Terreno::META, Cenario::TAM);
        return (lx >= Cenario::TAM * 0.5f ? 1 : 0) + (lz >= Cenario::TAM * 0.5f ? 2 : 0);
    }
    float h(float x, float z) const { return t.altura(x, z); }

    // caixa no frame local
    void caixa(MeshBuilder& m, const Frame& f, Vec3f c, Vec3f tam, Rgb cor, float rotExtra = 0) {
        m.caixa(f.P(c.x, c.y, c.z), tam, f.rot + rotExtra, cor);
    }
    // colisao para um retangulo girado: varias caixas pequenas (AABB girada seria grande demais)
    void colisaoRet(const Frame& f, float cx, float cz, float w, float d, float alt) {
        float passo = 1.2f;
        int nw = std::max(1, int(std::ceil(w / passo))), nd = std::max(1, int(std::ceil(d / passo)));
        for (int a = 0; a < nw; a++)
            for (int c = 0; c < nd; c++) {
                float lx = cx - w / 2 + (a + 0.5f) * w / nw, lz = cz - d / 2 + (c + 0.5f) * d / nd;
                Vec3f p = f.P(lx, 0, lz);
                float hw = std::min(w / nw, d / nd) * 0.5f + 0.15f;
                col.adicionar({{p.x - hw, f.o.y - 2, p.z - hw}, {p.x + hw, f.o.y + alt, p.z + hw}});
            }
    }
    bool livre(const Frame& f, float w, float d, float margem) const {
        AABB a = pegada(f, w + margem, d + margem);
        for (auto& o : ocupado)
            if (a.min.x < o.max.x && a.max.x > o.min.x && a.min.z < o.max.z && a.max.z > o.min.z) return false;
        // nao construir em cima de estrada nem de agua
        for (float sx : {-0.5f, 0.f, 0.5f})
            for (float sz : {-0.5f, 0.f, 0.5f}) {
                Vec3f p = f.P(sx * w, 0, sz * d);
                if (t.mascaraEstrada(p.x, p.z) > 0.08f || t.mascaraRio(p.x, p.z) > 0.01f) return false;
            }
        return true;
    }
    static AABB pegada(const Frame& f, float w, float d) {
        Vec3f mn{1e9f, 0, 1e9f}, mx{-1e9f, 0, -1e9f};
        for (float sx : {-0.5f, 0.5f})
            for (float sz : {-0.5f, 0.5f}) {
                Vec3f p = f.P(sx * w, 0, sz * d);
                mn = {std::min(mn.x, p.x), 0, std::min(mn.z, p.z)};
                mx = {std::max(mx.x, p.x), 0, std::max(mx.z, p.z)};
            }
        return {mn, mx};
    }
    void marcar(const Frame& f, float w, float d) { estruturas.push_back({f.o, w, d, f.rot}); }
    void ocupar(const Frame& f, float w, float d) { ocupado.push_back(pegada(f, w, d)); }

    // registra no "formas" so as pecas grandes que bloqueiam luz (parede, alicerce, telhado, chamine, muros)
    void registrar(MeshBuilder& m, bool sim) { m.registrar(sim ? formas : nullptr); }
    static float tipoParede(Rgb c) {
        if (std::abs(c.r - c.b) < 0.12f) return TIPO_PEDRA;  // cinza
        if (c.r + c.g + c.b > 1.8f) return TIPO_REBOCO;      // claro
        return TIPO_MADEIRA;
    }

    // ---------- pecas ----------
    void tocha(Vec3f base, float altura = 2.2f) {
        MeshBuilder& m = b(base);
        m.cilindro(base + Vec3f{0, -0.5f, 0}, 0.07f, altura + 0.5f, 5, MADEIRA_ESC);
        m.cilindro(base + Vec3f{0, altura, 0}, 0.16f, 0.12f, 6, FERRO);
        chama(base + Vec3f{0, altura + 0.1f, 0}, 1.f);
        col.adicionar({{base.x - 0.15f, base.y - 1, base.z - 0.15f}, {base.x + 0.15f, base.y + altura, base.z + 0.15f}});
    }
    void chama(Vec3f p, float esc) {
        MeshBuilder& m = b(p);
        m.tipo(TIPO_CHAMA);
        m.cone(p, 0.14f * esc, 0.42f * esc, 5, CHAMA);
        m.cone(p + Vec3f{0, 0.04f, 0}, 0.08f * esc, 0.28f * esc, 4, CHAMA2);
        m.tipo(TIPO_NORMAL);
        luzes.push_back({p + Vec3f{0, 0.35f * esc, 0}, COR_LUZ_FOGO * (3.0f * esc), 11.f * std::sqrt(esc), 1.f});
    }
    void tochaParede(const Frame& f, float x, float y, float z) {
        Vec3f p = f.P(x, y, z);
        MeshBuilder& m = b(p);
        m.caixa(p, {0.08f, 0.35f, 0.08f}, f.rot, MADEIRA_ESC);
        chama(p + Vec3f{0, 0.2f, 0}, 0.8f);
    }
    void janela(MeshBuilder& m, const Frame& f, float x, float y, float z, float rotExtra = 0) {
        if (janelas) {
            Vec3f nl = rotExtra != 0 ? Vec3f{x >= 0 ? 1.f : -1.f, 0, 0} : Vec3f{0, 0, z >= 0 ? 1.f : -1.f};
            janelas->push_back({f.P(x, y, z), rotY(nl, f.rot)});
        }
        caixa(m, f, {x, y, z}, {0.8f, 0.85f, 0.08f}, MADEIRA_ESC, rotExtra);
        m.tipo(TIPO_JANELA);
        Vec3f off = rotY({0, 0, 0.03f}, rotExtra);
        caixa(m, f, Vec3f{x, y, z} + off, {0.6f, 0.65f, 0.06f}, JANELA, rotExtra);
        m.tipo(TIPO_NORMAL);
    }

    // Predio gerado pela gramatica (core/predio): malha detalhada + casca simples, formas da luz
    // tracada, colisao girada, pisos, janelas, porta e interior. Devolve a entrada (mundo).
    static float tipoMaterial(MatPeca m) {
        switch (m) {
            case MatPeca::Reboco: return TIPO_REBOCO;
            case MatPeca::Madeira: case MatPeca::MadeiraEsc: return TIPO_MADEIRA;
            case MatPeca::Pedra: return TIPO_PEDRA;
            case MatPeca::Vidro: return TIPO_JANELA;
            default: return TIPO_NORMAL;
        }
    }
    static void desenharPeca(MeshBuilder& m, const PecaPredio& pc, const Frame& f) {
        auto P = [&](Vec3f q) { return f.P(q.x, q.y, q.z); };
        float rot = f.rot + pc.rotY;
        if (pc.tipo == PecaPredio::Triangulo) m.triangulo(P(pc.a), P(pc.b), P(pc.c), pc.cor);
        else if (pc.tipo == PecaPredio::Prisma) m.prisma(P(pc.centro), pc.tam, rot, pc.cor);
        else if (pc.inclinacao != 0) m.caixaInclinada(P(pc.centro), pc.tam, rot, pc.inclinacao, pc.cor);
        else m.caixa(P(pc.centro), pc.tam, rot, pc.cor);
    }
    Vec3f predio(const Frame& f, Lote l) {
        // piso acima do chao mais alto da pegada (terreno inclinado: o alicerce desce ate o mais baixo)
        float hmax = -1e9f;
        for (float sx : {-0.5f, 0.f, 0.5f})
            for (float sz : {-0.5f, 0.f, 0.5f}) {
                Vec3f q = f.P(sx * l.largura, 0, sz * l.profundidade);
                hmax = std::max(hmax, h(q.x, q.z));
            }
        l.piso = hmax - f.o.y + 0.35f;
        Planta pl = gerarPredio(l);
        int idx = pedaco(f.o) * 4 + quadrante(f.o);
        MeshBuilder& m = predioDet[idx];
        MeshBuilder& ml = predioCasca[idx];
        m.tamanhoMaximoFace(3.5f);    // as paredes usam a luz guardada: pouca subdivisao basta
        m.grupo(proximoGrupo++);
        ml.tamanhoMaximoFace(1000.f);  // casca: faces inteiras
        std::vector<std::pair<int, Vec3f>> paredesReg;  // formas de parede desta casa (detalhes usam a luz delas)
        for (const auto& pc : pl.pecas) {
            float tipoV = tipoMaterial(pc.mat);
            bool deParede = tipoV > 4.5f && tipoV < 7.5f;
            if (pc.soForma) {
                if (formas) m.soForma(pc.tipo == PecaPredio::Prisma ? TipoForma::Prisma : TipoForma::Caixa, f.P(pc.centro.x, pc.centro.y, pc.centro.z), pc.tam * 0.5f, f.rot + pc.rotY, pc.grossa);
                continue;
            }
            if (!pc.forma && deParede && !paredesReg.empty()) {
                Vec3f c = f.P(pc.centro.x, pc.centro.y, pc.centro.z);
                int melhor = paredesReg[0].first;
                float dm = 1e9f;
                for (auto& [i, w] : paredesReg)
                    if (len(w - c) < dm) dm = len(w - c), melhor = i;
                m.formaFixa(melhor);
            }
            m.tipo(tipoV);
            if (pc.forma) registrar(m, true);
            desenharPeca(m, pc, f);
            if (pc.forma) registrar(m, false);
            if (pc.forma && deParede && formas) paredesReg.push_back({int(formas->size()) - 1, f.P(pc.centro.x, pc.centro.y, pc.centro.z)});
            m.formaFixa(-1);
            m.tipo(TIPO_NORMAL);
        }
        for (const auto& pc : pl.casca) {  // versao simples (longe, Baixo, sombras)
            ml.tipo(tipoMaterial(pc.mat));
            desenharPeca(ml, pc, f);
            ml.tipo(TIPO_NORMAL);
        }
        m.grupo(-1);
        for (const auto& c : pl.paredes) {
            Vec3f w = f.P(c.centro.x, 0, c.centro.z);
            col.adicionarGirada({w, c.meiaX, c.meiaZ, f.rot + c.rot, f.o.y + c.y0, f.o.y + c.y1});
        }
        for (const auto& c : pl.pisos) {
            Vec3f w = f.P(c.centro.x, 0, c.centro.z);
            col.adicionarPisoGirado({w, c.meiaX, c.meiaZ, f.rot + c.rot, f.o.y + c.y0, f.o.y + c.y1});
        }
        if (janelas)
            for (const auto& j : pl.janelas) janelas->push_back({f.P(j.pos.x, j.pos.y, j.pos.z), rotY(j.normal, f.rot)});
        Vec3f portaC = f.P(pl.porta.dobradica.x + pl.porta.largura * 0.5f, pl.piso, pl.porta.dobradica.z);
        if (portas)
            portas->push_back({f.P(pl.porta.dobradica.x, pl.porta.dobradica.y, pl.porta.dobradica.z), pl.porta.largura, pl.porta.altura,
                               f.rot + pl.porta.rotFechada, pl.porta.sentido});
        if (interiores) interiores->push_back({f.P(pl.interiorCentro.x, pl.interiorCentro.y, pl.interiorCentro.z), pl.interiorMeia, f.rot, portaC});
        marcar(f, l.largura, l.profundidade);
        ocupar(f, l.largura + 1.2f, l.profundidade + (l.estilo == Estilo::Faroeste ? 4.5f : 1.5f));
        Vec3f e = f.P(pl.entrada.x, 0, pl.entrada.z);
        e.y = h(e.x, e.z);
        return e;
    }

    void cerca(Vec3f a, Vec3f c, Rgb cor = MADEIRA) {
        regioesCapim->cercas.push_back({a, c});
        Vec3f d = c - a;
        float l = len(d);
        int n = std::max(1, int(l / 2.4f));
        float ang = std::atan2(d.x, d.z);
        for (int k = 0; k <= n; k++) {
            Vec3f p = a + d * (k / float(n));
            p.y = h(p.x, p.z);
            b(p).caixa(p + Vec3f{0, 0.5f, 0}, {0.14f, 1.3f, 0.14f}, ang, cor * 0.85f);
        }
        for (int k = 0; k < n; k++) {
            Vec3f p0 = a + d * (k / float(n)), p1 = a + d * ((k + 1) / float(n));
            Vec3f m = (p0 + p1) * 0.5f;
            m.y = h(m.x, m.z);
            for (float y : {0.45f, 0.95f}) b(m).caixa(m + Vec3f{0, y, 0}, {0.08f, 0.1f, l / n}, ang, cor);
            col.adicionar({{m.x - 0.25f, m.y - 1, m.z - 0.25f}, {m.x + 0.25f, m.y + 1.1f, m.z + 0.25f}});
        }
    }

    void muro(Vec3f a, Vec3f c, float alt, float esp, Rgb cor, bool ameias) {
        Vec3f d = c - a;
        float l = len(d), ang = std::atan2(d.x, d.z);
        Vec3f m = (a + c) * 0.5f;
        float y = std::min(h(a.x, a.z), h(c.x, c.z));
        marcar({m, ang}, esp, l);
        MeshBuilder& mb = b(m);
        registrar(mb, true);
        mb.caixa({m.x, y + alt / 2 - 1.f, m.z}, {esp, alt + 2.f, l}, ang, cor);
        registrar(mb, false);
        if (ameias) {
            int n = int(l / 1.6f);
            for (int k = 0; k < n; k++) {
                if (k % 2) continue;
                Vec3f p = a + d * ((k + 0.5f) / n);
                mb.caixa({p.x, y + alt + 0.35f, p.z}, {esp + 0.1f, 0.7f, l / n * 0.9f}, ang, cor * 0.95f);
            }
        }
        int ns = std::max(1, int(l / 1.2f));
        for (int k = 0; k <= ns; k++) {
            Vec3f p = a + d * (k / float(ns));
            float r = esp * 0.5f + 0.2f;
            col.adicionar({{p.x - r, y - 2, p.z - r}, {p.x + r, y + alt, p.z + r}});
        }
    }

    void torre(Vec3f p, float raio, float alt, Rgb cor, bool telhado, Rgb corTelhado = ARDOSIA) {
        MeshBuilder& m = b(p);
        registrar(m, true);
        marcar({p, 0}, 2 * raio, 2 * raio);
        m.cilindro(p + Vec3f{0, -2, 0}, raio, alt + 2, 8, cor);
        registrar(m, false);
        m.cilindro(p + Vec3f{0, alt, 0}, raio + 0.3f, 0.6f, 8, cor * 0.92f);
        if (telhado) m.cone(p + Vec3f{0, alt + 0.6f, 0}, raio + 0.6f, raio * 2.2f, 8, corTelhado);
        col.adicionar({{p.x - raio, p.y - 2, p.z - raio}, {p.x + raio, p.y + alt, p.z + raio}});
    }

    void poco(Vec3f p) {
        regioesCapim->pocos.push_back(p);
        marcar({p, 0}, 2.4f, 1.6f);
        MeshBuilder& m = b(p);
        m.cilindro(p + Vec3f{0, -0.5f, 0}, 1.0f, 1.3f, 8, PEDRA);
        m.cilindro(p + Vec3f{0, 0.75f, 0}, 0.8f, 0.02f, 8, Rgb{0.1f, 0.18f, 0.2f});
        for (float s : {-1.f, 1.f}) m.caixa(p + Vec3f{s * 0.9f, 1.5f, 0}, {0.15f, 1.6f, 0.15f}, 0, MADEIRA_ESC);
        m.prisma(p + Vec3f{0, 2.6f, 0}, {2.4f, 0.8f, 1.6f}, 0, TELHA_M);
        col.adicionar({{p.x - 1.1f, p.y - 1, p.z - 1.1f}, {p.x + 1.1f, p.y + 1.5f, p.z + 1.1f}});
    }

    void barril(Vec3f p, float rot = 0) {
        (void)rot;
        MeshBuilder& m = b(p);
        m.cilindro(p, 0.38f, 0.95f, 7, MADEIRA);
        m.cilindro(p + Vec3f{0, 0.2f, 0}, 0.4f, 0.07f, 7, FERRO);
        m.cilindro(p + Vec3f{0, 0.7f, 0}, 0.4f, 0.07f, 7, FERRO);
        col.adicionar({{p.x - .4f, p.y, p.z - .4f}, {p.x + .4f, p.y + 1, p.z + .4f}});
    }
    void caixote(Vec3f p, float rot, float s = 0.9f) {
        b(p).caixa(p + Vec3f{0, s / 2, 0}, {s, s, s}, rot, Rgb{0.55f, 0.42f, 0.27f});
        col.adicionar({{p.x - s * .6f, p.y, p.z - s * .6f}, {p.x + s * .6f, p.y + s, p.z + s * .6f}});
    }

    void quadroRecompensas(const Frame& f, std::vector<Quadro>& quadros, const std::string& lugar) {
        MeshBuilder& m = b(f.o);
        for (float s : {-1.f, 1.f}) caixa(m, f, {s * 1.1f, 1.2f, 0}, {0.18f, 2.4f, 0.18f}, MADEIRA_ESC);
        caixa(m, f, {0, 1.55f, 0}, {2.4f, 1.4f, 0.1f}, MADEIRA);
        caixa(m, f, {0, 2.45f, 0.1f}, {2.7f, 0.15f, 0.5f}, MADEIRA_ESC);
        // cartazes
        const float xs[] = {-0.75f, 0.f, 0.75f, -0.4f, 0.4f};
        const float ys[] = {1.8f, 1.85f, 1.75f, 1.2f, 1.25f};
        for (int k = 0; k < 5; k++) caixa(m, f, {xs[k], ys[k], 0.06f}, {0.5f, 0.6f, 0.02f}, Rgb{0.88f, 0.82f, 0.64f}, r(-0.06f, 0.06f));
        colisaoRet(f, 0, 0, 2.4f, 0.4f, 2.4f);
        quadros.push_back({f.P(0, 0, 1.4f), f.rot, lugar});
    }

    void barraca(const Frame& f, Rgb lona) {
        MeshBuilder& m = b(f.o);
        for (float sx : {-1.2f, 1.2f})
            for (float sz : {-0.8f, 0.8f}) caixa(m, f, {sx, 1.1f, sz}, {0.12f, 2.2f, 0.12f}, MADEIRA_ESC);
        caixa(m, f, {0, 0.9f, 0.5f}, {2.4f, 0.12f, 0.8f}, MADEIRA);
        m.prisma(f.P(0, 2.5f, 0), {2.9f, 0.6f, 2.1f}, f.rot, lona);
        // mercadorias
        for (int k = 0; k < 3; k++) caixa(m, f, {-0.7f + k * 0.7f, 1.05f, 0.5f}, {0.4f, 0.2f, 0.4f}, LONAS[(k + 2) % 5] * 0.9f, r(0, 1));
        colisaoRet(f, 0, 0, 2.5f, 1.8f, 2.4f);
        marcar(f, 2.9f, 2.1f);
        ocupar(f, 2.6f, 2.0f);
    }
};

int indiceDoTipo(TipoLugar t) { return int(t); }
}  // namespace

// ================== lugares ==================

static std::vector<float> angulosPortoes(const Terreno& t, const Lugar& l, float R) {
    std::vector<float> res;
    bool dentro = false;
    float ini = 0;
    // procura onde as estradas cruzam o circulo de raio R
    const int N = 360;
    float primeiroIni = -1;
    for (int k = 0; k <= N + 20; k++) {
        float a = 2 * PI * k / N;
        float m = t.mascaraEstrada(l.x + std::sin(a) * R, l.z + std::cos(a) * R);
        if (m > 0.3f && !dentro) { dentro = true; ini = a; }
        if (m <= 0.3f && dentro) {
            dentro = false;
            float c = (ini + a) * 0.5f;
            if (k > N && primeiroIni >= 0) break;
            if (primeiroIni < 0) primeiroIni = ini;
            res.push_back(std::fmod(c, 2 * PI));
        }
    }
    // remove duplicados (por causa da volta)
    std::vector<float> u;
    for (float a : res) {
        bool dup = false;
        for (float b : u) if (std::abs(std::remainder(a - b, 2 * PI)) < 0.15f) dup = true;
        if (!dup) u.push_back(a);
    }
    return u;
}

static bool pertoDePortao(const std::vector<float>& p, float a, float tol) {
    for (float g : p) if (std::abs(std::remainder(a - g, 2 * PI)) < tol) return true;
    return false;
}

static void gerarCidade(Gerador& g, const Lugar& l, Cenario& c, std::vector<Vec3f>& rota) {
    float y0 = g.h(l.x, l.z);
    Vec3f centro{l.x, y0, l.z};
    float R = l.raio * 0.9f;
    auto portoes = angulosPortoes(g.t, l, R);
    // muralha com torres
    const int SEG = 28;
    for (int k = 0; k < SEG; k++) {
        float a0 = 2 * PI * k / SEG, a1 = 2 * PI * (k + 1) / SEG, am = (a0 + a1) / 2;
        Vec3f p0{l.x + std::sin(a0) * R, 0, l.z + std::cos(a0) * R}, p1{l.x + std::sin(a1) * R, 0, l.z + std::cos(a1) * R};
        p0.y = g.h(p0.x, p0.z);
        p1.y = g.h(p1.x, p1.z);
        if (!pertoDePortao(portoes, am, 0.09f)) g.muro(p0, p1, 5.5f, 1.6f, PEDRA, true);
        if (k % 2 == 0 && !pertoDePortao(portoes, a0, 0.1f)) g.torre(p0, 2.4f, 8.f, PEDRA * 0.97f, true, ARDOSIA);
    }
    for (float a : portoes) {
        for (float s : {-1.f, 1.f}) {
            float aa = a + s * 0.085f;
            Vec3f p{l.x + std::sin(aa) * R, 0, l.z + std::cos(aa) * R};
            p.y = g.h(p.x, p.z);
            g.torre(p, 2.6f, 9.5f, PEDRA, true, TELHA_V);
            Vec3f fora{std::sin(a), 0, std::cos(a)};
            Vec3f tp = p + fora * 2.9f;
            tp.y = g.h(tp.x, tp.z);
            g.tocha(tp, 2.4f);
            rota.push_back(p - fora * 6.f);
        }
        // arco do portao
        Vec3f pa{l.x + std::sin(a) * R, 0, l.z + std::cos(a) * R};
        pa.y = g.h(pa.x, pa.z);
        g.b(pa).caixa(pa + Vec3f{0, 8.2f, 0}, {1.8f, 1.6f, 2 * R * std::sin(0.085f)}, a + PI / 2, PEDRA_ESC);
        // nao construir na rua que vem do portao
        for (float d = 0; d < R; d += 4) {
            Vec3f q{l.x + std::sin(a) * d, 0, l.z + std::cos(a) * d};
            g.ocupado.push_back({{q.x - 3.5f, 0, q.z - 3.5f}, {q.x + 3.5f, 0, q.z + 3.5f}});
        }
    }
    // praca central
    g.ocupado.push_back({{l.x - 14, 0, l.z - 14}, {l.x + 14, 0, l.z + 14}});
    g.poco(centro + Vec3f{0, 0, 0});
    rota.push_back(centro + Vec3f{4, 0, 3});
    rota.push_back(centro + Vec3f{-5, 0, -4});
    for (int k = 0; k < 4; k++) {
        float a = PI / 4 + k * PI / 2;
        Vec3f p{l.x + std::sin(a) * 9.f, 0, l.z + std::cos(a) * 9.f};
        p.y = g.h(p.x, p.z);
        g.tocha(p, 2.6f);
    }
    // mercado: barracas em volta da praca
    for (int k = 0; k < 5; k++) {
        float a = 0.5f + k * 0.42f;
        Vec3f p{l.x + std::sin(a) * 12.f, 0, l.z + std::cos(a) * 12.f};
        p.y = g.h(p.x, p.z);
        Frame f{p, a + PI};
        if (g.livre(f, 2.6f, 2.0f, 0.5f) || true) g.barraca(f, LONAS[k % 5]);
        rota.push_back(p + Vec3f{std::sin(a + PI) * 2.2f, 0, std::cos(a + PI) * 2.2f});
    }
    // lotes em aneis: predios especiais primeiro, perto da praca
    struct Especial { float w, d; int andares; Rgb parede, telhado; bool placa; Rgb corPlaca; int tipo; };
    // tipo: 0 casa, 1 taverna, 2 xerife, 3 cadeia, 4 estabulo
    std::vector<Especial> fila = {
        {11, 8, 2, REBOCO, TELHA_V, true, {0.7f, 0.5f, 0.15f}, 1},
        {8, 7, 1, PEDRA, ARDOSIA, true, {0.25f, 0.3f, 0.5f}, 2},
        {7, 6, 1, PEDRA_ESC, ARDOSIA, false, {}, 3},
        {12, 8, 1, MADEIRA, TELHA_M, false, {}, 4},
    };
    int casas = 0;
    for (float raioAnel : {22.f, 36.f, 50.f, 64.f, 76.f}) {
        int nl = int(2 * PI * raioAnel / 15.f);
        float desloc = g.r(0, 1);
        for (int k = 0; k < nl; k++) {
            float a = 2 * PI * (k + desloc) / nl;
            Vec3f p{l.x + std::sin(a) * raioAnel, 0, l.z + std::cos(a) * raioAnel};
            p.y = g.h(p.x, p.z);
            Frame f{p, a + PI};  // frente para o centro
            Especial e = {g.r(6.5f, 9.f), g.r(5.5f, 7.f), g.r01() < 0.45f ? 2 : 1, g.r01() < 0.6f ? REBOCO : REBOCO2,
                          g.r01() < 0.5f ? TELHA_V : (g.r01() < 0.5f ? TELHA_M : ARDOSIA), false, {}, 0};
            if (!fila.empty()) e = fila.front();
            if (!g.livre(f, e.w, e.d, 2.5f)) continue;
            if (!fila.empty()) fila.erase(fila.begin());
            Vec3f entrada = f.P(e.w * 0.18f, 0, e.d / 2 + 1.8f);
            if (e.tipo == 4) {
                // estabulo: telhado sobre postes, cocheiras e cerca
                MeshBuilder& m = g.b(p);
                for (float sx : {-0.5f, 0.f, 0.5f})
                    for (float sz : {-0.5f, 0.5f}) g.caixa(m, f, {sx * e.w, 1.6f, sz * e.d}, {0.25f, 3.2f, 0.25f}, MADEIRA_ESC);
                g.registrar(m, true);
                m.tipo(TIPO_MADEIRA);
                g.caixa(m, f, {0, 1.5f, -e.d / 2}, {e.w, 3.0f, 0.2f}, MADEIRA);
                for (float sx : {-0.5f, 0.5f}) g.caixa(m, f, {sx * e.w, 1.5f, 0}, {0.2f, 3.0f, e.d}, MADEIRA);
                m.tipo(TIPO_NORMAL);
                g.registrar(m, false);
                for (int b2 = 1; b2 < 4; b2++) g.caixa(m, f, {-e.w / 2 + b2 * e.w / 4, 0.7f, -e.d / 4}, {0.12f, 1.4f, e.d / 2}, MADEIRA_ESC);
                g.registrar(m, true);
                m.prisma(f.P(0, 3.8f, 0), {e.w + 0.8f, 1.6f, e.d + 1.0f}, f.rot, PALHA);
                g.registrar(m, false);
                g.caixa(m, f, {e.w * 0.3f, 0.4f, -e.d * 0.3f}, {1.4f, 0.8f, 1.0f}, PALHA * 0.9f);  // feno
                g.colisaoRet(f, 0, -e.d / 2, e.w, 0.4f, 3);
                g.colisaoRet(f, -e.w / 2, 0, 0.4f, e.d, 3);
                g.colisaoRet(f, e.w / 2, 0, 0.4f, e.d, 3);
                g.marcar(f, e.w, e.d);
                g.ocupar(f, e.w, e.d);
                c.cavalos.push_back(f.P(0, 0, e.d / 2 + 2.5f));
                g.tochaParede(f, -e.w / 2 - 0.2f, 2.4f, e.d / 2 + 0.1f);
            } else {
                Lote lt;
                lt.largura = e.w, lt.profundidade = e.d, lt.andares = e.andares, lt.estilo = Estilo::Medieval;
                lt.tipo = e.tipo == 1 ? TipoPredio::Taverna : (e.tipo == 2 ? TipoPredio::Xerife : (e.tipo == 3 ? TipoPredio::Cadeia : TipoPredio::Casa));
                lt.semente = g.rng ^ uint32_t(k * 7919 + int(raioAnel) * 131);
                lt.corParede = e.parede, lt.corTelhado = e.telhado;
                entrada = g.predio(f, lt);
                if (e.tipo == 1) {  // taverna: barris e tochas na frente
                    g.barril(f.P(-e.w / 2 + 0.8f, 0, e.d / 2 + 1.0f));
                    g.barril(f.P(-e.w / 2 + 1.7f, 0, e.d / 2 + 1.2f));
                    g.tochaParede(f, -e.w * 0.05f, 3.0f, e.d / 2 + 0.25f);
                    g.tochaParede(f, e.w * 0.42f, 3.0f, e.d / 2 + 0.25f);
                }
                if (e.tipo == 2) {  // xerife: quadro de recompensas na frente
                    Frame fq{f.P(-e.w * 0.15f, 0, e.d / 2 + 3.2f), f.rot};
                    fq.o.y = g.h(fq.o.x, fq.o.z);
                    g.quadroRecompensas(fq, c.quadros, l.nome);
                    g.tochaParede(f, e.w * 0.45f, 2.8f, e.d / 2 + 0.25f);
                }
                if (e.tipo == 0) casas++;
            }
            rota.push_back(entrada);
        }
    }
    (void)casas;
}

static void gerarVilarejo(Gerador& g, const Lugar& l, Cenario& c, std::vector<Vec3f>& rota) {
    Vec3f centro{l.x, g.h(l.x, l.z), l.z};
    auto piso = [&](float dx, float dz) {
        return Vec3f{l.x + dx, g.h(l.x + dx, l.z + dz), l.z + dz};
    };
    g.poco(piso(3, 2));
    if (l.nome == "Pedrabranca") {
        // Bancas voltadas para o corredor central da feira.
        g.barraca({piso(-5, 3), PI / 2}, LONAS[0]);
        g.barraca({piso(-5, -2), PI / 2}, LONAS[3]);
        Frame sino{piso(4, -4), 0};
        g.marcar(sino, 3.2f, 2.3f);
        MeshBuilder& m = g.b(sino.o);
        for (float x : {-0.85f, 0.85f}) {
            g.caixa(m, sino, {x, 2.8f, 0}, {0.24f, 5.6f, 0.32f}, MADEIRA_ESC);
        }
        g.caixa(m, sino, {0, 5.35f, 0}, {2.3f, 0.28f, 0.38f}, MADEIRA);
        m.prisma(sino.P(0, 5.95f, 0), {3.2f, 1.1f, 2.3f}, 0, TELHA_M);
        Rgb bronze{0.57f, 0.38f, 0.16f};
        m.cilindro(sino.P(0, 4.25f, 0), 0.43f, 0.82f, 10, bronze);
        m.cilindro(sino.P(0, 4.13f, 0), 0.57f, 0.15f, 10, bronze);
        g.colisaoRet(sino, 0, 0, 2.f, 0.45f, 5.6f);
    }
    g.ocupado.push_back({{l.x - 7, 0, l.z - 7}, {l.x + 7, 0, l.z + 7}});
    rota.push_back(centro + Vec3f{-2, 0, -3});
    int feitas = 0;
    for (float raioAnel : {16.f, 27.f, 38.f}) {
        int nl = int(2 * PI * raioAnel / 17.f);
        float desloc = g.r(0, 1);
        for (int k = 0; k < nl && feitas < 9; k++) {
            float a = 2 * PI * (k + desloc) / nl;
            Vec3f p{l.x + std::sin(a) * raioAnel, 0, l.z + std::cos(a) * raioAnel};
            p.y = g.h(p.x, p.z);
            Frame f{p, a + PI + g.r(-0.25f, 0.25f)};
            float w = g.r(7.f, 10.f), d = g.r(6.f, 8.5f);
            if (!g.livre(f, w, d, 3.f)) continue;
            Lote lt;
            lt.largura = w, lt.profundidade = d, lt.andares = g.r01() < 0.25f ? 2 : 1, lt.estilo = Estilo::Medieval;
            lt.semente = g.rng ^ uint32_t(k * 104729 + int(raioAnel) * 7);
            lt.corParede = g.r01() < 0.5f ? REBOCO2 : MADEIRA * 1.3f;
            lt.corTelhado = g.r01() < 0.6f ? PALHA : TELHA_M;
            rota.push_back(g.predio(f, lt));
            if (g.r01() < 0.5f) g.barril(f.P(-w / 2 - 0.6f, 0, d / 2 - 0.5f));
            if (g.r01() < 0.4f) g.caixote(f.P(w / 2 + 0.7f, 0, 0.5f), g.r(0, 1));
            feitas++;
        }
    }
    for (int k = 0; k < 3; k++) {
        float a = k * 2.1f + 0.4f;
        Vec3f p{l.x + std::sin(a) * 9.f, 0, l.z + std::cos(a) * 9.f};
        p.y = g.h(p.x, p.z);
        g.tocha(p);
    }
    (void)c;
}

static void gerarFazenda(Gerador& g, const Lugar& l, Cenario& c, std::vector<Vec3f>& rota) {
    float rot = l.rot * PI / 180.f;
    Frame fl{{l.x, g.h(l.x, l.z), l.z}, rot};
    // celeiro e casa no estilo faroeste
    Frame fb{fl.P(-10, 0, -8), rot};
    fb.o.y = g.h(fb.o.x, fb.o.z);
    Lote lc;
    lc.largura = 12, lc.profundidade = 8, lc.estilo = Estilo::Faroeste, lc.tipo = TipoPredio::Celeiro;
    lc.semente = uint32_t(l.x * 13 + l.z * 7) + 5u;
    lc.corParede = Rgb{0.55f, 0.22f, 0.16f}, lc.corTelhado = TELHA_M;
    Vec3f entCeleiro = g.predio(fb, lc);
    Frame fc{fl.P(8, 0, -9), rot};
    fc.o.y = g.h(fc.o.x, fc.o.z);
    Lote lf;
    lf.largura = 9, lf.profundidade = 7, lf.estilo = Estilo::Faroeste, lf.tipo = TipoPredio::Casa;
    lf.semente = uint32_t(l.x * 31 + l.z * 17) + 9u;
    lf.corParede = Rgb{0.62f, 0.52f, 0.4f}, lf.corTelhado = Rgb{0.36f, 0.3f, 0.26f};
    rota.push_back(g.predio(fc, lf));
    rota.push_back(entCeleiro);
    // plantacao com cerca
    float pw = 24, pd = 16;
    Vec3f c0 = fl.P(-pw / 2, 0, 4), c1 = fl.P(pw / 2, 0, 4), c2 = fl.P(pw / 2, 0, 4 + pd), c3 = fl.P(-pw / 2, 0, 4 + pd);
    g.cerca(c0, c1);
    g.cerca(c1, c2);
    g.cerca(c2, c3);
    g.cerca(c3, c0);
    Rgb trigo{0.82f, 0.7f, 0.32f}, verde{0.35f, 0.55f, 0.22f};
    for (int fila = 0; fila < 7; fila++) {
        Vec3f p = fl.P(0, 0, 6 + fila * 2.0f);
        p.y = g.h(p.x, p.z);
        g.b(p).caixa(p + Vec3f{0, 0.35f, 0}, {pw - 3, 0.7f, 0.9f}, rot, (fila % 2) ? trigo : verde);
    }
    rota.push_back(fl.P(0, 0, 2));
    // fardos de feno
    for (int k = 0; k < 4; k++) {
        Vec3f p = fl.P(16 + g.r(-2, 2), 0, -2 + k * 2.6f);
        p.y = g.h(p.x, p.z);
        g.b(p).cilindro(p + Vec3f{0, -0.1f, 0}, 0.9f, 1.3f, 8, PALHA);
    }
    g.tocha(fl.P(4, 0, -4));
    (void)c;
}

static void gerarCastelo(Gerador& g, const Lugar& l, Cenario& c, std::vector<Vec3f>& rota) {
    float rot = l.rot * PI / 180.f;
    Frame f{{l.x, g.h(l.x, l.z), l.z}, rot};
    const float L = 27.f;
    Vec3f cant[4] = {f.P(-L, 0, -L), f.P(L, 0, -L), f.P(L, 0, L), f.P(-L, 0, L)};
    for (auto& p : cant) p.y = g.h(p.x, p.z);
    Rgb pedraVelha = PEDRA * 0.9f;
    for (int k = 0; k < 4; k++) {
        Vec3f a = cant[k], b = cant[(k + 1) % 4];
        // muro quebrado: trechos com alturas diferentes e buracos
        int nseg = 6;
        for (int s = 0; s < nseg; s++) {
            if ((k == 2 && s == 2) || (k == 2 && s == 3)) continue;  // entrada principal (lado +z)
            if (g.r01() < 0.18f) continue;                            // desmoronado
            Vec3f p0 = a + (b - a) * (s / float(nseg)), p1 = a + (b - a) * ((s + 1) / float(nseg));
            g.muro(p0, p1, g.r(2.5f, 7.f), 2.0f, pedraVelha * g.r(0.92f, 1.05f), g.r01() < 0.5f);
        }
        bool inteira = k % 2 == 0;
        g.torre(a, 3.2f, inteira ? 12.f : g.r(4.f, 7.f), pedraVelha, false);
    }
    // torre de menagem parcialmente caida
    Frame fk{f.P(-6, 0, -8), rot};
    fk.o.y = g.h(fk.o.x, fk.o.z);
    {
        MeshBuilder& m = g.b(fk.o);
        g.caixa(m, fk, {0, 3.5f, 0}, {12, 9, 10}, pedraVelha);
        g.caixa(m, fk, {-3, 10.5f, 0}, {6, 5, 10}, pedraVelha * 0.95f);
        g.caixa(m, fk, {3.5f, 8.6f, -2}, {5, 1.2f, 6}, pedraVelha * 0.9f);
        for (int k = 0; k < 4; k++) g.caixa(m, fk, {-5.3f + k * 1.6f, 13.4f, 4.6f}, {0.8f, 0.9f, 0.8f}, pedraVelha);
        g.janela(m, fk, -3, 10.5f, 5.06f);
        g.colisaoRet(fk, 0, 0, 12, 10, 12);
        g.marcar(fk, 12, 10);
        g.ocupar(fk, 12, 10);
    }
    // entulho
    for (int k = 0; k < 26; k++) {
        Vec3f p = f.P(g.r(-24, 24), 0, g.r(-24, 24));
        p.y = g.h(p.x, p.z);
        float s = g.r(0.5f, 1.6f);
        g.b(p).caixa(p + Vec3f{0, s * 0.3f, 0}, {s, s * 0.7f, s * g.r(0.7f, 1.3f)}, g.r(0, 3), pedraVelha * g.r(0.85f, 1.05f));
    }
    // entrada da masmorra: arco escuro com tochas
    Frame fe{f.P(9, 0, -4), rot + PI * 0.5f};
    fe.o.y = g.h(fe.o.x, fe.o.z);
    {
        MeshBuilder& m = g.b(fe.o);
        g.caixa(m, fe, {0, 1.5f, -1.2f}, {6, 4.2f, 3.5f}, PEDRA_ESC);
        for (float s : {-1.f, 1.f}) g.caixa(m, fe, {s * 1.5f, 1.4f, 0.6f}, {0.9f, 3.2f, 0.9f}, PEDRA);
        g.caixa(m, fe, {0, 3.3f, 0.6f}, {4.0f, 0.8f, 1.0f}, PEDRA);
        g.caixa(m, fe, {0, 1.3f, 0.52f}, {2.1f, 2.6f, 0.1f}, ESCURO);
        // degraus descendo
        for (int k = 0; k < 3; k++) g.caixa(m, fe, {0, 0.05f - k * 0.2f, 1.6f - k * 0.4f}, {2.0f, 0.2f, 0.4f}, PEDRA_ESC);
        g.tochaParede(fe, -2.2f, 2.3f, 1.1f);
        g.tochaParede(fe, 2.2f, 2.3f, 1.1f);
        g.colisaoRet(fe, 0, -1.2f, 6, 3.5f, 4);
    }
    rota.push_back(f.P(0, 0, 20));
    rota.push_back(f.P(5, 0, 5));
    (void)c;
}

static void gerarAcampamento(Gerador& g, const Lugar& l, Cenario& c, std::vector<Vec3f>& rota) {
    Vec3f centro{l.x, g.h(l.x, l.z), l.z};
    // fogueira
    {
        MeshBuilder& m = g.b(centro);
        for (int k = 0; k < 8; k++) {
            float a = k * PI / 4;
            m.caixa(centro + Vec3f{std::sin(a) * 1.1f, 0.12f, std::cos(a) * 1.1f}, {0.35f, 0.25f, 0.35f}, a, PEDRA_ESC);
        }
        for (int k = 0; k < 3; k++) m.caixa(centro + Vec3f{0, 0.2f, 0}, {0.18f, 0.18f, 1.3f}, k * 1.05f, MADEIRA_ESC);
        g.chama(centro + Vec3f{0, 0.2f, 0}, 2.0f);
        g.col.adicionar({{centro.x - 1, centro.y - 1, centro.z - 1}, {centro.x + 1, centro.y + 0.5f, centro.z + 1}});
    }
    rota.push_back(centro + Vec3f{2.5f, 0, 0});
    rota.push_back(centro + Vec3f{-1.5f, 0, 2.5f});
    // tendas
    for (int k = 0; k < 6; k++) {
        float a = k * 2 * PI / 6 + g.r(-0.2f, 0.2f);
        Vec3f p{l.x + std::sin(a) * 10.f, 0, l.z + std::cos(a) * 10.f};
        p.y = g.h(p.x, p.z);
        Frame f{p, a + PI / 2};
        if (!g.livre(f, 4, 3, 1)) continue;
        MeshBuilder& m = g.b(p);
        Rgb lona = LONAS[(k + 1) % 5] * 0.85f;
        m.prisma(f.P(0, 0.95f, 0), {3.6f, 1.9f, 3.0f}, f.rot, lona);
        g.caixa(m, f, {0, 0.95f, 1.52f}, {0.9f, 1.4f, 0.05f}, ESCURO);
        g.colisaoRet(f, 0, 0, 3.6f, 3, 2);
        g.marcar(f, 3.6f, 3);
        g.ocupar(f, 3.6f, 3);
        rota.push_back(f.P(0, 0, -2.5f));
    }
    // caixotes e barris
    for (int k = 0; k < 7; k++) {
        float a = g.r(0, 2 * PI), r = g.r(5, 14);
        Vec3f p{l.x + std::sin(a) * r, 0, l.z + std::cos(a) * r};
        p.y = g.h(p.x, p.z);
        if (g.r01() < 0.5f) g.caixote(p, g.r(0, 2)); else g.barril(p);
    }
    // paliçada parcial
    for (int k = 0; k < 70; k++) {
        float a = k * 2 * PI / 70;
        if (pertoDePortao(angulosPortoes(g.t, l, 20.f), a, 0.25f)) continue;
        if (k % 9 == 4) continue;
        Vec3f p{l.x + std::sin(a) * 20.f, 0, l.z + std::cos(a) * 20.f};
        p.y = g.h(p.x, p.z);
        MeshBuilder& m = g.b(p);
        float alt = g.r(2.4f, 3.0f);
        m.cilindro(p + Vec3f{0, -0.5f, 0}, 0.22f, alt + 0.5f, 5, MADEIRA);
        m.cone(p + Vec3f{0, alt, 0}, 0.22f, 0.5f, 5, MADEIRA * 0.9f);
        g.col.adicionar({{p.x - 0.35f, p.y - 1, p.z - 0.35f}, {p.x + 0.35f, p.y + alt, p.z + 0.35f}});
    }
    (void)c;
}

static void gerarMina(Gerador& g, const Lugar& l, Cenario& c, std::vector<Vec3f>& rota) {
    float rot = l.rot * PI / 180.f;
    Frame f{{l.x, g.h(l.x, l.z), l.z}, rot};
    // monte de pedra com a boca da mina
    {
        MeshBuilder& m = g.b(f.o);
        for (int k = 0; k < 9; k++) {
            float s = g.r(6, 11);
            g.caixa(m, f, {g.r(-9, 9), s * 0.25f, g.r(-16, -6)}, {s, s * 0.9f, s}, PEDRA * g.r(0.8f, 1.0f), g.r(0, 3));
        }
        g.caixa(m, f, {0, 3.0f, -8}, {16, 7, 10}, PEDRA * 0.86f);
        g.colisaoRet(f, 0, -11, 22, 12, 8);
        // escoramento
        for (float s : {-1.f, 1.f}) g.caixa(m, f, {s * 1.6f, 1.6f, -2.8f}, {0.4f, 3.2f, 0.4f}, MADEIRA);
        g.caixa(m, f, {0, 3.35f, -2.8f}, {4.0f, 0.45f, 0.5f}, MADEIRA);
        g.caixa(m, f, {0, 1.5f, -3.05f}, {2.8f, 3.0f, 0.1f}, ESCURO);
        // trilhos
        for (float s : {-0.5f, 0.5f}) g.caixa(m, f, {s, 0.06f, 3}, {0.1f, 0.1f, 12}, FERRO);
        for (int k = 0; k < 12; k++) g.caixa(m, f, {0, 0.02f, float(k - 2)}, {1.5f, 0.08f, 0.25f}, MADEIRA_ESC);
        // vagonete
        g.caixa(m, f, {0, 0.75f, 4}, {1.3f, 0.9f, 1.8f}, MADEIRA * 0.8f);
        g.caixa(m, f, {0, 1.1f, 4}, {1.1f, 0.3f, 1.6f}, PEDRA_ESC);
        g.colisaoRet(f, 0, 4, 1.3f, 1.8f, 1.3f);
        g.tochaParede(f, -2.3f, 2.4f, -2.5f);
    }
    {   // barracao do minerador (faroeste)
        Frame fm{f.P(-9, 0, 4), rot + PI * 0.5f};
        fm.o.y = g.h(fm.o.x, fm.o.z);
        Lote lb;
        lb.largura = 4.2f, lb.profundidade = 3.6f, lb.estilo = Estilo::Faroeste, lb.tipo = TipoPredio::Barracao;
        lb.semente = uint32_t(l.x * 5 + l.z * 3) + 1u;
        lb.corParede = Rgb{0.5f, 0.42f, 0.33f}, lb.corTelhado = Rgb{0.3f, 0.27f, 0.25f};
        rota.push_back(g.predio(fm, lb));
    }
    rota.push_back(f.P(3, 0, 2));
    rota.push_back(f.P(-4, 0, 6));
    for (int k = 0; k < 5; k++) g.caixote(f.P(g.r(3, 7), 0, g.r(0, 6)), g.r(0, 2));
    g.tocha(f.P(4, 0, 8));
    (void)c;
}

static void gerarCemiterio(Gerador& g, const Lugar& l, Cenario& c, std::vector<Vec3f>& rota) {
    float rot = l.rot * PI / 180.f;
    Frame f{{l.x, g.h(l.x, l.z), l.z}, rot};
    float L = 18;
    Vec3f c0 = f.P(-L, 0, -L), c1 = f.P(L, 0, -L), c2 = f.P(L, 0, L), c3 = f.P(-L, 0, L);
    g.cerca(c0, c1, PEDRA_ESC);
    g.cerca(c1, c2, PEDRA_ESC);
    g.cerca(c3, c0, PEDRA_ESC);
    // portao no lado +z
    g.cerca(c2, f.P(2, 0, L), PEDRA_ESC);
    g.cerca(f.P(-2, 0, L), c3, PEDRA_ESC);
    // lapides
    for (int i = 0; i < 7; i++)
        for (int j = 0; j < 4; j++) {
            if (g.r01() < 0.15f) continue;
            Vec3f p = f.P(-12 + i * 4.f + g.r(-0.4f, 0.4f), 0, -2 + j * 4.f);
            p.y = g.h(p.x, p.z);
            MeshBuilder& m = g.b(p);
            float inc = g.r(-0.15f, 0.15f);
            if (g.r01() < 0.6f) m.caixa(p + Vec3f{0, 0.5f, 0}, {0.7f, 1.1f, 0.18f}, rot + inc, PEDRA * g.r(0.75f, 1.0f));
            else {
                m.caixa(p + Vec3f{0, 0.7f, 0}, {0.14f, 1.4f, 0.14f}, rot + inc, MADEIRA_ESC);
                m.caixa(p + Vec3f{0, 1.05f, 0}, {0.7f, 0.12f, 0.12f}, rot + inc, MADEIRA_ESC);
            }
            m.caixa(p + Vec3f{0, 0.04f, 0.9f}, {0.8f, 0.12f, 1.6f}, rot, Rgb{0.35f, 0.3f, 0.22f});
        }
    // capela
    Frame fcap{f.P(0, 0, -12), rot};
    fcap.o.y = g.h(fcap.o.x, fcap.o.z);
    Lote lcap;
    lcap.largura = 6, lcap.profundidade = 9, lcap.estilo = Estilo::Medieval, lcap.tipo = TipoPredio::Capela;
    lcap.semente = uint32_t(l.x * 3 + l.z * 11) + 3u;
    lcap.corParede = PEDRA, lcap.corTelhado = ARDOSIA;
    g.predio(fcap, lcap);
    Vec3f tor = fcap.P(0, 0, -(9 * 0.5f + 1.3f));  // torre nos fundos (a porta fica na frente)
    g.torre(tor, 1.2f, 7.5f, PEDRA * 0.95f, true, ARDOSIA);
    rota.push_back(f.P(0, 0, L - 3));
    rota.push_back(f.P(-6, 0, 3));
    g.tocha(f.P(3, 0, L + 1.5f));
    // arvore seca
    Vec3f pa = f.P(13, 0, 10);
    pa.y = g.h(pa.x, pa.z);
    MeshBuilder& m = g.b(pa);
    m.cilindro(pa, 0.3f, 4.5f, 5, Rgb{0.3f, 0.27f, 0.24f});
    m.caixa(pa + Vec3f{0.8f, 3.6f, 0}, {1.8f, 0.18f, 0.18f}, 0.4f, Rgb{0.3f, 0.27f, 0.24f});
    m.caixa(pa + Vec3f{-0.6f, 4.1f, 0.3f}, {1.4f, 0.15f, 0.15f}, 2.2f, Rgb{0.3f, 0.27f, 0.24f});
    (void)c;
}

static void gerarPonte(Gerador& g, const Lugar& l, Cenario& c, std::vector<Vec3f>& rota) {
    // acha a direcao da estrada que atravessa o rio (onde a margem e mais proxima)
    float melhorAng = 0, melhor = 1e9f;
    for (int k = 0; k < 90; k++) {
        float a = k * PI / 90;
        float soma = 0;
        for (float d = -20; d <= 20; d += 2) soma += g.t.mascaraRio(l.x + std::sin(a) * d, l.z + std::cos(a) * d);
        if (soma < melhor) { melhor = soma; melhorAng = a; }
    }
    Frame f{{l.x, g.t.nivelAgua(), l.z}, melhorAng};
    // comprimento: ate a terra seca dos dois lados
    float meio = 8;
    while (meio < 30 && (g.t.mascaraRio(f.P(0, 0, meio).x, f.P(0, 0, meio).z) > 0.01f || g.t.mascaraRio(f.P(0, 0, -meio).x, f.P(0, 0, -meio).z) > 0.01f)) meio += 1;
    meio += 3;
    g.marcar(f, 4.2f, meio * 2);
    float yTab = g.t.nivelAgua() + 1.6f;
    MeshBuilder& m = g.b(f.o);
    // tabuleiro em rampas: sobe da margem ate o meio
    int n = int(meio * 2 / 1.0f);
    for (int k = 0; k < n; k++) {
        float z = -meio + (k + 0.5f) * (2 * meio / n);
        Vec3f p = f.P(0, 0, z);
        float chao = g.h(p.x, p.z);
        float y = std::max(yTab - std::max(0.f, std::abs(z) - (meio - 6)) * 0.35f, chao + 0.05f);
        y = std::min(y, yTab);
        g.caixa(m, Frame{{f.o.x, 0, f.o.z}, f.rot}, {0, y - 0.15f, z}, {4.2f, 0.3f, 2 * meio / n + 0.05f}, MADEIRA * (k % 2 ? 0.95f : 1.05f));
        Vec3f mn = p - Vec3f{2.1f, 0, 2.1f}, mx = p + Vec3f{2.1f, 0, 2.1f};
        g.col.adicionarPiso({{mn.x, y - 1.f, mn.z}, {mx.x, y, mx.z}});
        // corrimao
        if (std::abs(z) < meio - 2.5f) {
            for (float s : {-1.f, 1.f}) {
                g.caixa(m, Frame{{f.o.x, 0, f.o.z}, f.rot}, {s * 2.0f, y + 0.5f, z}, {0.12f, 0.1f, 2 * meio / n}, MADEIRA_ESC);
                Vec3f q = f.P(s * 2.0f, 0, z);
                g.col.adicionar({{q.x - 0.25f, y - 0.2f, q.z - 0.25f}, {q.x + 0.25f, y + 1.2f, q.z + 0.25f}});
            }
            if (k % 3 == 0)
                for (float s : {-1.f, 1.f}) g.caixa(m, Frame{{f.o.x, 0, f.o.z}, f.rot}, {s * 2.0f, y + 0.3f, z}, {0.15f, 0.7f, 0.15f}, MADEIRA_ESC);
        }
        // pilares na agua
        if (k % 4 == 0 && std::abs(z) < meio - 3)
            for (float s : {-1.6f, 1.6f}) {
                Vec3f q = f.P(s, 0, z);
                m.cilindro({q.x, g.h(q.x, q.z) - 0.5f, q.z}, 0.25f, y - g.h(q.x, q.z), 6, MADEIRA_ESC);
            }
    }
    Vec3f lan = f.P(2.4f, 0, meio - 3);
    lan.y = g.h(lan.x, lan.z);
    g.tocha(lan, 2.0f);
    rota.push_back(f.P(0, 0, meio + 2));
    rota.push_back(f.P(0, 0, -meio - 2));
    (void)c;
}

// ================== vegetacao ==================

static void modeloArvore(MeshBuilder& m, const ArvoreInst& a, bool longe) {
    float s = a.escala;
    Vec3f p = a.pos;
    if (a.tipo == 0) {  // pinheiro
        Rgb verde = lerp(Rgb{0.16f, 0.30f, 0.17f}, Rgb{0.24f, 0.38f, 0.2f}, a.tom);
        if (longe) {
            m.cone(p + Vec3f{0, 0.8f * s, 0}, 1.7f * s, 5.2f * s, 5, verde);
            return;
        }
        m.cilindro(p + Vec3f{0, -0.3f, 0}, 0.2f * s, 1.8f * s, 5, Rgb{0.33f, 0.23f, 0.16f});
        m.cone(p + Vec3f{0, 1.2f * s, 0}, 1.7f * s, 2.5f * s, 7, verde);
        m.cone(p + Vec3f{0, 2.4f * s, 0}, 1.3f * s, 2.3f * s, 7, verde * 1.06f);
        m.cone(p + Vec3f{0, 3.6f * s, 0}, 0.9f * s, 2.0f * s, 6, verde * 1.12f);
    } else if (a.tipo == 1) {  // carvalho
        Rgb verde = lerp(Rgb{0.27f, 0.42f, 0.18f}, Rgb{0.42f, 0.5f, 0.2f}, a.tom);
        if (longe) {
            m.caixa(p + Vec3f{0, 3.4f * s, 0}, Vec3f{3.2f, 2.6f, 3.2f} * s, a.rot, verde);
            return;
        }
        m.cilindro(p + Vec3f{0, -0.3f, 0}, 0.28f * s, 2.8f * s, 6, Rgb{0.36f, 0.26f, 0.18f});
        m.caixa(p + Vec3f{0, 3.3f * s, 0}, Vec3f{3.0f, 2.2f, 3.0f} * s, a.rot, verde);
        m.caixa(p + Vec3f{0.8f * s, 2.9f * s, 0.5f * s}, Vec3f{2.0f, 1.6f, 2.0f} * s, a.rot + 0.7f, verde * 0.93f);
        m.caixa(p + Vec3f{-0.7f * s, 3.0f * s, -0.6f * s}, Vec3f{2.1f, 1.7f, 1.9f} * s, a.rot + 1.3f, verde * 1.05f);
        m.caixa(p + Vec3f{0.1f * s, 4.5f * s, 0}, Vec3f{1.8f, 1.2f, 1.8f} * s, a.rot + 0.4f, verde * 1.1f);
    } else if (a.tipo == 2) {  // arbusto
        if (longe) return;
        Rgb verde = lerp(Rgb{0.25f, 0.4f, 0.17f}, Rgb{0.36f, 0.45f, 0.2f}, a.tom);
        m.caixa(p + Vec3f{0, 0.35f * s, 0}, Vec3f{1.2f, 0.8f, 1.1f} * s, a.rot, verde);
        m.caixa(p + Vec3f{0.4f * s, 0.3f * s, 0.3f * s}, Vec3f{0.8f, 0.6f, 0.8f} * s, a.rot + 0.8f, verde * 0.92f);
    } else {  // pedra
        if (longe) return;
        Rgb cinza = lerp(Rgb{0.45f, 0.44f, 0.42f}, Rgb{0.58f, 0.55f, 0.5f}, a.tom);
        m.caixa(p + Vec3f{0, 0.2f * s, 0}, Vec3f{1.3f, 0.9f, 1.0f} * s, a.rot, cinza);
        m.caixa(p + Vec3f{0.5f * s, 0.1f * s, 0.4f * s}, Vec3f{0.7f, 0.5f, 0.6f} * s, a.rot + 1.1f, cinza * 0.9f);
    }
}

static void espalharVegetacao(Gerador& g, const Mapa& mapa, std::vector<std::vector<ArvoreInst>>& porPedaco, int n, uint32_t semente) {
    const Lugar* floresta = nullptr;
    for (auto& l : mapa.lugares) if (l.tipo == TipoLugar::Floresta) floresta = &l;
    const float PASSO = 5.f;
    uint32_t rng = semente * 7919u + 17u;
    for (float z = -715; z < 715; z += PASSO)
        for (float x = -715; x < 715; x += PASSO) {
            float px = x + (aleatorio01(rng) - 0.5f) * PASSO * 0.9f, pz = z + (aleatorio01(rng) - 0.5f) * PASSO * 0.9f;
            float h = g.t.altura(px, pz);
            if (h < g.t.nivelAgua() + 0.8f) continue;
            if (g.t.mascaraEstrada(px, pz) > 0.01f || g.t.mascaraEstrada(px + 2.5f, pz) > 0.01f || g.t.mascaraEstrada(px - 2.5f, pz) > 0.01f ||
                g.t.mascaraEstrada(px, pz + 2.5f) > 0.01f || g.t.mascaraEstrada(px, pz - 2.5f) > 0.01f)
                continue;
            if (g.t.mascaraRio(px, pz) > 0.0f) continue;
            float ny = g.t.normal(px, pz).y;
            bool dentroLugar = false;
            for (auto& l : mapa.lugares) {
                if (l.tipo == TipoLugar::Floresta) continue;
                float d = std::hypot(px - l.x, pz - l.z);
                if (d < l.raio * 1.12f + (l.tipo == TipoLugar::Ponte ? 12 : 0)) dentroLugar = true;
            }
            if (dentroLugar) continue;
            // densidade
            float bosque = fbm2D(px * 0.008f, pz * 0.008f, semente + 31, 3);
            float dens = 0.012f + smoothstep(0.15f, 0.45f, bosque) * 0.18f;
            bool montanha = h > 42.f;
            if (floresta) {
                float d = std::hypot(px - floresta->x, pz - floresta->z);
                dens = std::max(dens, (1.f - smoothstep(floresta->raio * 0.6f, floresta->raio * 1.15f, d)) * 0.55f);
            }
            if (montanha) dens = (h < 85 ? 0.16f : 0.f) * smoothstep(0.55f, 0.75f, ny);
            else if (ny < 0.8f) dens *= 0.3f;
            float sorte = aleatorio01(rng);
            ArvoreInst a{{px, h, pz}, 0.8f + aleatorio01(rng) * 0.6f, aleatorio01(rng) * 6.28f, 0, aleatorio01(rng)};
            if (sorte < dens) {
                bool pinho = montanha || (floresta && std::hypot(px - floresta->x, pz - floresta->z) < floresta->raio) ? aleatorio01(rng) < 0.75f
                                                                                                                     : aleatorio01(rng) < 0.35f;
                a.tipo = pinho ? 0 : 1;
                a.pos.y = h - 0.1f;
                g.col.adicionar({{px - 0.3f * a.escala, h - 1, pz - 0.3f * a.escala}, {px + 0.3f * a.escala, h + 4, pz + 0.3f * a.escala}});
            } else if (sorte < dens + 0.02f + (montanha ? 0.03f : 0.f)) {
                a.tipo = (montanha || aleatorio01(rng) < 0.35f) ? 3 : 2;
            } else
                continue;
            int i = std::clamp(int((px + Terreno::META) / Cenario::TAM), 0, n - 1);
            int j = std::clamp(int((pz + Terreno::META) / Cenario::TAM), 0, n - 1);
            porPedaco[j * n + i].push_back(a);
        }
}

// ================== Cenario ==================

void Cenario::construir(const Mapa& mapa, const Terreno& t, MundoColisao& col, uint32_t semente) {
    ter_ = &t;
    col_ = &col;
    n_ = int(std::ceil(2 * Terreno::META / TAM));
    p_.assign(n_ * n_, Pedaco{});
    estruturasMapa.clear();
    Gerador g{t, col, luzes, estruturasMapa, semente, {}, {}, {}, n_, {}};
    formas_.clear();
    janelas_.clear();
    portas_.clear();
    interiores_.clear();
    regioesCapim_ = {};
    g.regioesCapim = &regioesCapim_;
    g.formas = &formas_;
    g.janelas = &janelas_;
    g.portas = &portas_;
    g.interiores = &interiores_;
    rotasNPC.clear();
    for (auto& l : mapa.lugares) {
        std::vector<Vec3f> rota;
        switch (l.tipo) {
            case TipoLugar::Cidade: gerarCidade(g, l, *this, rota); break;
            case TipoLugar::Vilarejo: gerarVilarejo(g, l, *this, rota); break;
            case TipoLugar::Fazenda: gerarFazenda(g, l, *this, rota); break;
            case TipoLugar::Castelo: gerarCastelo(g, l, *this, rota); break;
            case TipoLugar::Acampamento: gerarAcampamento(g, l, *this, rota); break;
            case TipoLugar::Mina: gerarMina(g, l, *this, rota); break;
            case TipoLugar::Cemiterio: gerarCemiterio(g, l, *this, rota); break;
            case TipoLugar::Ponte: gerarPonte(g, l, *this, rota); break;
            case TipoLugar::Floresta: break;
        }
        for (auto& p : rota) p.y = t.altura(p.x, p.z);
        if (!rota.empty()) rotasNPC.push_back(rota);
    }
    // cavalo extra no vilarejo
    for (auto& l : mapa.lugares)
        if (l.tipo == TipoLugar::Vilarejo) cavalos.push_back({l.x + 8, t.altura(l.x + 8, l.z - 6), l.z - 6});

    std::vector<std::vector<ArvoreInst>> arv(n_ * n_);
    espalharVegetacao(g, mapa, arv, n_, semente);

    for (int j = 0; j < n_; j++)
        for (int i = 0; i < n_; i++) {
            Pedaco& p = p_[j * n_ + i];
            p.centro = {-Terreno::META + (i + .5f) * TAM, t.altura(-Terreno::META + (i + .5f) * TAM, -Terreno::META + (j + .5f) * TAM),
                        -Terreno::META + (j + .5f) * TAM};
            p.raio = TAM * 0.75f + 25.f;
            p.arvores = std::move(arv[j * n_ + i]);
            {
                int k = j * n_ + i;
                auto io = g.constr.find(k);
                if (io != g.constr.end() && !io->second.vazio()) {
                    io->second.calcularAO(&t, 10);
                    p.construcoes = paraMesh(io->second);
                    p.temConstr = true;
                }
                for (int qd = 0; qd < 4; qd++) {  // predios por quadrante: detalhados e casca
                    auto id = g.predioDet.find(k * 4 + qd);
                    auto ic = g.predioCasca.find(k * 4 + qd);
                    if (id == g.predioDet.end() || id->second.vazio() || ic == g.predioCasca.end()) continue;
                    Pedaco::Quadrante& Q = p.q[qd];
                    Q.centro = {-Terreno::META + (i + 0.25f + 0.5f * (qd & 1)) * TAM, p.centro.y, -Terreno::META + (j + 0.25f + 0.5f * (qd >> 1)) * TAM};
                    id->second.calcularAO(&t, 8);
                    ic->second.calcularAO(&t, 4);
                    Q.det = paraMesh(id->second);
                    Q.casca = paraMesh(ic->second);
                    Q.tem = true;
                }
            }
            if (!p.arvores.empty()) {
                MeshBuilder m;
                for (auto& a : p.arvores) modeloArvore(m, a, true);
                if (!m.vazio()) {
                    m.aoVertical(p.centro.y - 10, p.centro.y + 6);
                    for (auto& a : p.arvores) (void)a;
                    p.arvLonge = paraMesh(m);
                    p.temLonge = true;
                }
            }
        }
}

void Cenario::construirPerto(Pedaco& p) {
    MeshBuilder total;
    for (auto& a : p.arvores) {
        MeshBuilder m;
        modeloArvore(m, a, false);
        m.aoVertical(a.pos.y, a.pos.y + 3.5f * a.escala);
        total.anexar(m);
    }
    // grama: tufos de 3 laminas (dos dois lados) no campo aberto, so nos pedacos perto do jogador
    if (ter_ && col_) {
        const Terreno& t = *ter_;
        RegioesCapim regioes = regioesCapim_.proximas(p.centro.x - TAM * .5f, p.centro.z - TAM * .5f,
                                                       p.centro.x + TAM * .5f, p.centro.z + TAM * .5f);
        uint32_t rng = hash32(uint32_t(p.centro.x * 7.f) * 73856093u ^ uint32_t(p.centro.z * 7.f) * 19349663u);
        for (float gz = -TAM * 0.5f; gz < TAM * 0.5f; gz += 2.2f)
            for (float gx = -TAM * 0.5f; gx < TAM * 0.5f; gx += 2.2f) {
                float x = p.centro.x + gx + (aleatorio01(rng) - 0.5f) * 2.f, z = p.centro.z + gz + (aleatorio01(rng) - 0.5f) * 2.f;
                if (aleatorio01(rng) > 0.75f || !t.dentroDoJogavel(x, z)) continue;
                float y = t.altura(x, z), estrada = t.mascaraEstrada(x, z);
                if (t.mascaraRio(x, z) > 0.05f || y < t.nivelAgua() + 0.4f) continue;
                AmbienteCapim ambiente = ambienteCapim(x, z, estrada, 0.f, regioes);
                uint32_t sorte = rng ^ 0x9e3779b9u;
                if ((ambiente.densidade < 1.f && aleatorio01(sorte) > ambiente.densidade) || t.normal(x, z).y < 0.88f) continue;
                ambiente.umidade = t.umidadeMargem(x, z, y);
                if (dentroDeInterior(interiores_, {x, y + 0.60f, z}) >= 0) continue;
                Vec3f base{x, y, z};
                Vec3f r = col_->resolver(base, 0.35f);
                if (std::hypot(r.x - x, r.z - z) > 0.01f) continue;  // nao nasce dentro de casa/cerca
                float h = (0.28f + aleatorio01(rng) * 0.3f) * ambiente.altura, verde = 0.85f + aleatorio01(rng) * 0.3f;
                Rgb cor = corCapimUmido({0.36f * verde, 0.5f * verde, 0.2f * verde}, ambiente.umidade);
                Rgb ponta = corCapimUmido({0.62f * verde, 0.66f * verde, 0.3f * verde}, ambiente.umidade);
                for (int k = 0; k < 3; k++) {
                    float a = aleatorio01(rng) * 6.2831f, l = 0.07f + aleatorio01(rng) * 0.05f;
                    Vec3f lado{std::cos(a) * l, 0, std::sin(a) * l};
                    Vec3f top = base + Vec3f{std::sin(a + 1.3f) * h * 0.35f * ambiente.inclinacao, h,
                                             std::cos(a + 1.3f) * h * 0.35f * ambiente.inclinacao};
                    Rgb c = k == 1 ? ponta : cor;
                    total.triangulo(base - lado, base + lado, top, c);
                    total.triangulo(base + lado, base - lado, top, c);
                }
            }
    }
    if (total.vazio()) {
        p.vazioPerto = true;
        return;
    }
    p.arvPerto = paraMesh(total);
    p.temPerto = true;
}

static constexpr float DIST_PERTO = 120.f;

void Cenario::atualizar(Vec3f foco, float tempo, int maxConstrucoes) {
    tempo_ = tempo;
    int feitas = 0;
    std::vector<std::pair<float, int>> fila;
    for (int k = 0; k < int(p_.size()); k++) {
        Pedaco& p = p_[k];
        if (p.vazioPerto) continue;
        float d = std::hypot(p.centro.x - foco.x, p.centro.z - foco.z) - TAM * 0.7f;
        if (d < DIST_PERTO && !p.temPerto) fila.push_back({d, k});
        if (p.temPerto && tempo - p.usoPerto > 10.f && d > DIST_PERTO + 30) {
            UnloadMesh(p.arvPerto);
            p.temPerto = false;
        }
    }
    std::sort(fila.begin(), fila.end());
    for (auto& [d, k] : fila) {
        if (feitas++ >= maxConstrucoes) break;
        construirPerto(p_[k]);
        p_[k].usoPerto = tempo;
    }
}

void Cenario::desenhar(const Camera3D& cam, float distMax, Material& mat, Material* matConstr, int partes, float distDetalhe) {
    if (partes & 1) {  // a contagem recomeca na primeira parte do quadro
        tris_ = 0;
        chamadas_ = 0;
    }
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float meio = std::acos(std::cos(cam.fovy * 0.5f * 1.9f * DEG2RAD));
    Matrix id = MatrixIdentity();
    for (auto& p : p_) {
        if (!p.temConstr && !p.temLonge && !p.temPerto && !p.q[0].tem && !p.q[1].tem && !p.q[2].tem && !p.q[3].tem) continue;
        Vector3 v{p.centro.x - cam.position.x, p.centro.y - cam.position.y, p.centro.z - cam.position.z};
        float d = Vector3Length(v);
        float dh = std::hypot(v.x, v.z) - TAM * 0.7f;
        if (dh > distMax) continue;
        if (d > p.raio) {
            float ang = std::acos(std::clamp(Vector3DotProduct(Vector3Scale(v, 1.f / d), fwd), -1.f, 1.f));
            if (ang - std::asin(std::min(1.f, p.raio / d)) > meio) continue;
        }
        if (partes & 1) {
            if (p.temConstr) {
                DrawMesh(p.construcoes, matConstr ? *matConstr : mat, id);
                tris_ += p.construcoes.triangleCount;
                chamadas_++;
            }
            for (auto& Q : p.q) {  // predios: detalhados perto, casca longe (Baixo: sempre casca)
                if (!Q.tem) continue;
                float dq = std::hypot(Q.centro.x - cam.position.x, Q.centro.z - cam.position.z) - TAM * 0.36f;
                const Mesh& mq = dq > distDetalhe ? Q.casca : Q.det;
                DrawMesh(mq, matConstr ? *matConstr : mat, id);
                tris_ += mq.triangleCount;
                chamadas_++;
            }
        }
        if (!(partes & 2)) continue;
        bool perto = dh < DIST_PERTO && p.temPerto;
        if (perto) {
            p.usoPerto = tempo_;
            DrawMesh(p.arvPerto, mat, id);
            tris_ += p.arvPerto.triangleCount;
            chamadas_++;
        } else if (p.temLonge) {
            DrawMesh(p.arvLonge, mat, id);
            tris_ += p.arvLonge.triangleCount;
            chamadas_++;
        }
    }
}

void Cenario::desenharArea(Vec3f centro, float raio, Material& mat) {
    Matrix id = MatrixIdentity();
    for (auto& p : p_) {
        if (std::hypot(p.centro.x - centro.x, p.centro.z - centro.z) > raio + p.raio) continue;
        if (p.temConstr) DrawMesh(p.construcoes, mat, id);
        for (auto& Q : p.q) {  // sombras: casca (leve); perto, o predio detalhado (o sol entra pelos vaos)
            if (!Q.tem) continue;
            bool perto = std::hypot(Q.centro.x - centro.x, Q.centro.z - centro.z) < 25.f + TAM * 0.36f;
            DrawMesh(perto ? Q.det : Q.casca, mat, id);
        }
        if (p.temPerto) DrawMesh(p.arvPerto, mat, id);
        else if (p.temLonge) DrawMesh(p.arvLonge, mat, id);
    }
}

void Cenario::descarregar() {
    for (auto& p : p_) {
        if (p.temConstr) UnloadMesh(p.construcoes);
        for (auto& Q : p.q)
            if (Q.tem) {
                UnloadMesh(Q.det);
                UnloadMesh(Q.casca);
            }
        if (p.temPerto) UnloadMesh(p.arvPerto);
        if (p.temLonge) UnloadMesh(p.arvLonge);
    }
    p_.clear();
}

size_t Cenario::totalArvores() const {
    size_t n = 0;
    for (auto& p : p_) n += p.arvores.size();
    return n;
}
