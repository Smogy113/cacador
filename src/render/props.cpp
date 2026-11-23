#include "render/props.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include "core/meshbuilder.h"
#include "core/noise.h"
#include "raymath.h"
#include "render/frustum.h"
#include "render/meshconv.h"
#include "rlgl.h"

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

// Converte coordenadas locais em metros para o mundo. A frente aponta para +z local.
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
    void colidirCaixa(const Frame& f, Vec3f c, Vec3f tam, float rotExtra = 0.f) {
        Vec3f p = f.P(c.x, c.y, c.z);
        col.adicionarGirada({p, tam.x * .5f, tam.z * .5f, f.rot + rotExtra, p.y - tam.y * .5f, p.y + tam.y * .5f});
    }
    // Volume simples para elementos feitos de várias peças, como a bancada.
    void colisaoRet(const Frame& f, float cx, float cz, float w, float d, float alt) {
        colidirCaixa(f, {cx, alt * .5f - 1.f, cz}, {w, alt + 2.f, d});
    }
    bool livre(const Frame& f, float largura, float profundidade, float margem) const {
        // A margem separa construcoes. Estrada e rio sao testados na pegada informada.
        AABB a = pegada(f, largura + margem, profundidade + margem);
        for (auto& o : ocupado)
            if (a.min.x < o.max.x && a.max.x > o.min.x && a.min.z < o.max.z && a.max.z > o.min.z) return false;
        // Amostra toda a pegada. Uma estrada estreita pode passar entre os cantos de um lote grande.
        int nx = std::max(2, int(std::ceil(largura / 2.f)));
        int nz = std::max(2, int(std::ceil(profundidade / 2.f)));
        for (int iz = 0; iz <= nz; iz++)
            for (int ix = 0; ix <= nx; ix++) {
                Vec3f p = f.P((float(ix) / nx - 0.5f) * largura, 0, (float(iz) / nz - 0.5f) * profundidade);
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
    static float tipoMaterial(MatPeca m, Cobertura cob = Cobertura::Telha) {
        switch (m) {
            case MatPeca::Telhado: return cob == Cobertura::Palha ? TIPO_PALHA : (cob == Cobertura::Tabua ? TIPO_TABUADO : TIPO_TELHA);
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
        // Usa a maior altura amostrada na pegada como referencia para elevar o piso.
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
            float tipoV = tipoMaterial(pc.mat, pl.cobertura);
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
            ml.tipo(tipoMaterial(pc.mat, pl.cobertura));
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
        float l = std::hypot(d.x, d.z);
        if (l < .01f) return;
        int n = std::max(1, int(std::ceil(l / 2.4f)));
        float ang = std::atan2(d.x, d.z);
        for (int k = 0; k <= n; k++) {
            Vec3f p = a + d * (k / float(n));
            p.y = h(p.x, p.z);
            b(p).caixa(p + Vec3f{0, 0.5f, 0}, {0.14f, 1.3f, 0.14f}, ang, cor * 0.85f);
            col.adicionarGirada({p, .07f, .07f, ang, p.y - .15f, p.y + 1.15f});
        }
        for (int k = 0; k < n; k++) {
            Vec3f p0 = a + d * (k / float(n)), p1 = a + d * ((k + 1) / float(n));
            p0.y = h(p0.x, p0.z);
            p1.y = h(p1.x, p1.z);
            Vec3f m = (p0 + p1) * 0.5f;
            float incl = -std::atan2(p1.y - p0.y, l / n);
            for (float y : {.45f, .95f})
                b(m).caixaInclinada(m + Vec3f{0, y, 0}, {.08f, .10f, len(p1 - p0)}, ang, incl, cor);
            col.adicionarGirada({m, .04f, l / (2.f * n), ang, std::min(p0.y, p1.y), std::max(p0.y, p1.y) + 1.f});
        }
    }

    void muro(Vec3f a, Vec3f c, float alt, float esp, Rgb cor, bool ameias) {
        Vec3f d = c - a;
        float l = std::hypot(d.x, d.z), ang = std::atan2(d.x, d.z);
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
        col.adicionarGirada({m, esp * .5f, l * .5f, ang, y - 2.f, y + alt});
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
        constexpr float raio = .31f, altura = .88f, raioAro = .325f;
        m.cilindro(p, raio, altura, 7, MADEIRA);
        for (float y : {.18f, .65f}) m.cilindro(p + Vec3f{0, y, 0}, raioAro, .045f, 7, FERRO);
        col.adicionar({{p.x - raioAro, p.y, p.z - raioAro}, {p.x + raioAro, p.y + altura, p.z + raioAro}});
    }
    void caixote(Vec3f p, float rot, float s = .62f) {
        p.y = h(p.x, p.z);
        Vec3f centro{0, s * .5f, 0}, tamanho{s, s, s};
        caixa(b(p), {p, rot}, centro, tamanho, {0.55f, 0.42f, 0.27f});
        colidirCaixa({p, rot}, centro, tamanho);
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
    // Os lotes perto da praca recebem primeiro os edificios de servico.
    struct LoteUrbano {
        float largura, profundidade;
        int andares;
        Rgb parede, telhado;
        std::optional<TipoPredio> tipo;  // o estabulo aberto usa uma malha propria
    };
    std::vector<LoteUrbano> fila = {
        {11, 8, 2, REBOCO, TELHA_V, TipoPredio::Taverna},
        {8, 7, 1, PEDRA, ARDOSIA, TipoPredio::Xerife},
        {7, 6, 1, PEDRA_ESC, ARDOSIA, TipoPredio::Cadeia},
        {12, 8, 1, MADEIRA, TELHA_M, std::nullopt},
    };
    constexpr float CASA_MIN = 9.f, CASA_MAX = 12.f;  // largura e profundidade das paredes, em metros
    for (float raioAnel : {22.f, 36.f, 51.f, 66.f, 78.f}) {
        int nl = int(2 * PI * raioAnel / 15.f);
        float desloc = g.r(0, 1);
        for (int k = 0; k < nl; k++) {
            float a = 2 * PI * (k + desloc) / nl;
            Vec3f p{l.x + std::sin(a) * raioAnel, 0, l.z + std::cos(a) * raioAnel};
            p.y = g.h(p.x, p.z);
            Frame f{p, a + PI};  // frente para o centro
            const float chanceSobrado = raioAnel <= 36.f ? 0.85f : 0.6f;
            // Consome os mesmos sorteios em cada candidato, inclusive para os lotes de servico.
            LoteUrbano lote = {
                g.r(CASA_MIN, CASA_MAX), g.r(CASA_MIN, CASA_MAX),
                g.r01() < chanceSobrado ? 2 : 1,
                g.r01() < 0.6f ? REBOCO : REBOCO2,
                g.r01() < 0.5f ? TELHA_V : (g.r01() < 0.5f ? TELHA_M : ARDOSIA),
                TipoPredio::Casa,
            };
            if (!fila.empty()) lote = fila.front();
            const float largura = lote.largura, profundidade = lote.profundidade;
            // Deixa a passagem interna da muralha livre, incluindo beirais e chamines.
            const float raioExterno = std::hypot(raioAnel + profundidade * 0.5f + 1.f,
                                                  largura * 0.5f + 1.f);
            if (raioExterno > R - 3.f) continue;
            if (!g.livre(f, largura, profundidade, 2.5f)) continue;
            if (!fila.empty()) fila.erase(fila.begin());
            Vec3f entrada = f.P(largura * 0.18f, 0, profundidade / 2 + 1.8f);
            if (!lote.tipo) {
                // estabulo: telhado sobre postes, cocheiras e cerca
                MeshBuilder& m = g.b(p);
                for (float sx : {-0.5f, 0.f, 0.5f})
                    for (float sz : {-0.5f, 0.5f}) g.caixa(m, f, {sx * largura, 1.6f, sz * profundidade}, {0.25f, 3.2f, 0.25f}, MADEIRA_ESC);
                g.registrar(m, true);
                m.tipo(TIPO_MADEIRA);
                g.caixa(m, f, {0, 1.5f, -profundidade / 2}, {largura, 3.0f, 0.2f}, MADEIRA);
                for (float sx : {-0.5f, 0.5f}) g.caixa(m, f, {sx * largura, 1.5f, 0}, {0.2f, 3.0f, profundidade}, MADEIRA);
                m.tipo(TIPO_NORMAL);
                g.registrar(m, false);
                for (int divisoria = 1; divisoria < 4; divisoria++) {
                    float x = -largura / 2 + divisoria * largura / 4;
                    g.caixa(m, f, {x, 0.7f, -profundidade / 4}, {0.12f, 1.4f, profundidade / 2}, MADEIRA_ESC);
                }
                g.registrar(m, true);
                m.tipo(TIPO_PALHA);
                m.prisma(f.P(0, 3.8f, 0), {largura + 0.8f, 1.6f, profundidade + 1.0f}, f.rot, PALHA);
                m.tipo(TIPO_NORMAL);
                g.registrar(m, false);
                g.caixa(m, f, {largura * 0.3f, 0.4f, -profundidade * 0.3f}, {1.4f, 0.8f, 1.0f}, PALHA * 0.9f);  // feno
                g.colisaoRet(f, 0, -profundidade / 2, largura, 0.4f, 3);
                g.colisaoRet(f, -largura / 2, 0, 0.4f, profundidade, 3);
                g.colisaoRet(f, largura / 2, 0, 0.4f, profundidade, 3);
                g.marcar(f, largura, profundidade);
                g.ocupar(f, largura, profundidade);
                c.cavalos.push_back(f.P(0, 0, profundidade / 2 + 2.5f));
                g.tochaParede(f, -largura / 2 - 0.2f, 2.4f, profundidade / 2 + 0.1f);
            } else {
                Lote lt;
                lt.largura = largura;
                lt.profundidade = profundidade;
                lt.andares = lote.andares;
                lt.estilo = Estilo::Medieval;
                lt.tipo = *lote.tipo;
                lt.semente = g.rng ^ uint32_t(k * 7919 + int(raioAnel) * 131);
                lt.corParede = lote.parede, lt.corTelhado = lote.telhado;
                entrada = g.predio(f, lt);
                if (lote.tipo == TipoPredio::Taverna) {  // taverna: barris e tochas na frente
                    g.barril(f.P(-largura / 2 + 0.8f, 0, profundidade / 2 + 1.0f));
                    g.barril(f.P(-largura / 2 + 1.7f, 0, profundidade / 2 + 1.2f));
                    g.tochaParede(f, -largura * 0.05f, 3.0f, profundidade / 2 + 0.25f);
                    g.tochaParede(f, largura * 0.42f, 3.0f, profundidade / 2 + 0.25f);
                }
                if (lote.tipo == TipoPredio::Xerife) {  // xerife: quadro de recompensas na frente
                    Frame fq{f.P(-largura * 0.15f, 0, profundidade / 2 + 3.2f), f.rot};
                    fq.o.y = g.h(fq.o.x, fq.o.z);
                    g.quadroRecompensas(fq, c.quadros, l.nome);
                    g.tochaParede(f, largura * 0.45f, 2.8f, profundidade / 2 + 0.25f);
                }
            }
            rota.push_back(entrada);
        }
    }
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
    constexpr int MAX_CASAS = 9;
    int casas = 0;
    for (float raioAnel : {18.f, 31.f, 44.f}) {
        int nl = int(2 * PI * raioAnel / 18.f);
        float desloc = g.r(0, 1);
        for (int k = 0; k < nl && casas < MAX_CASAS; k++) {
            float a = 2 * PI * (k + desloc) / nl;
            Vec3f p{l.x + std::sin(a) * raioAnel, 0, l.z + std::cos(a) * raioAnel};
            p.y = g.h(p.x, p.z);
            Frame f{p, a + PI + g.r(-0.25f, 0.25f)};
            float largura = g.r(6.8f, 9.2f), profundidade = g.r(5.8f, 7.8f);
            if (!g.livre(f, largura, profundidade, 3.f)) continue;
            Lote lt;
            lt.largura = largura, lt.profundidade = profundidade, lt.andares = g.r01() < 0.25f ? 2 : 1, lt.estilo = Estilo::Medieval;
            lt.semente = g.rng ^ uint32_t(k * 104729 + int(raioAnel) * 7);
            lt.corParede = g.r01() < 0.5f ? REBOCO2 : MADEIRA * 1.3f;
            bool palha = g.r01() < 0.6f;
            lt.corTelhado = palha ? PALHA : TELHA_M;
            lt.cobertura = palha ? Cobertura::Palha : Cobertura::Auto;
            rota.push_back(g.predio(f, lt));
            if (g.r01() < 0.5f) g.barril(f.P(-largura / 2 - 0.6f, 0, profundidade / 2 - 0.5f));
            if (g.r01() < 0.4f) g.caixote(f.P(largura / 2 + 0.7f, 0, 0.5f), g.r(0, 1));
            casas++;
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
    // x e z sao a posicao preferida no terreno da fazenda. Tenta o mesmo recuo
    // com deslocamentos alternados para fora e para dentro antes de ir mais para os fundos.
    auto implantar = [&](const Lote& lote, float x, float z, float avancoFrontal) -> std::optional<Vec3f> {
        for (float recuo : {0.f, 4.f, 8.f, 12.f, 16.f, 20.f, 24.f}) {
            for (float lateral : {0.f, 4.f, -4.f, 8.f, -8.f, 12.f, -12.f, 16.f, -16.f}) {
                Vec3f origem = fl.P(x + (x < 0.f ? -lateral : lateral), 0, z - recuo);
                // O avanco da varanda ou do degrau entra inteiro no teste do terreno.
                const Frame pegada{origem + rotY({0, 0, avancoFrontal * 0.5f}, rot), rot};
                if (!g.livre(pegada, lote.largura + 1.2f, lote.profundidade + 1.2f + avancoFrontal, 1.f)) continue;
                origem.y = g.h(origem.x, origem.z);
                return g.predio({origem, rot}, lote);
            }
        }
        return std::nullopt;
    };
    Lote celeiro;
    celeiro.largura = 12;
    celeiro.profundidade = 8;
    celeiro.estilo = Estilo::Faroeste;
    celeiro.tipo = TipoPredio::Celeiro;
    celeiro.semente = uint32_t(l.x * 13 + l.z * 7) + 5u;
    celeiro.corParede = Rgb{0.55f, 0.22f, 0.16f}, celeiro.corTelhado = TELHA_M;
    Lote casa;
    casa.largura = 9.5f;
    casa.profundidade = 7.5f;
    casa.estilo = Estilo::Faroeste;
    casa.tipo = TipoPredio::Casa;
    casa.semente = uint32_t(l.x * 31 + l.z * 17) + 9u;
    casa.corParede = Rgb{0.62f, 0.52f, 0.4f}, casa.corTelhado = Rgb{0.36f, 0.3f, 0.26f};
    // O celeiro reserva sua pegada primeiro. A rota dos moradores comeca pela casa.
    auto entradaCeleiro = implantar(celeiro, -10.f, -8.f, 0.8f);
    auto entradaCasa = implantar(casa, 8.f, -10.5f, 2.8f);  // varanda e degrau
    if (entradaCasa) rota.push_back(*entradaCasa);
    if (entradaCeleiro) rota.push_back(*entradaCeleiro);
    // plantacao com cerca
    float pw = 24, pd = 16;
    Vec3f c0 = fl.P(-pw / 2, 0, 4), c1 = fl.P(pw / 2, 0, 4), c2 = fl.P(pw / 2, 0, 4 + pd), c3 = fl.P(-pw / 2, 0, 4 + pd);
    g.cerca(c0, fl.P(-.75f, 0, 4));
    g.cerca(fl.P(.75f, 0, 4), c1);
    g.cerca(c1, c2);
    g.cerca(c2, c3);
    g.cerca(c3, c0);
    Rgb trigo{0.82f, 0.7f, 0.32f}, verde{0.35f, 0.55f, 0.22f};
    for (int fila = 0; fila < 12; fila++) {
        Vec3f p = fl.P(0, 0, 6 + fila * 1.05f);
        p.y = g.h(p.x, p.z);
        g.b(p).caixa(p + Vec3f{0, .30f, 0}, {pw - 3, .60f, .48f}, rot, (fila % 2) ? trigo : verde);
    }
    rota.push_back(fl.P(0, 0, 2));
    // fardos de feno
    for (int k = 0; k < 4; k++) {
        Vec3f p = fl.P(16 + g.r(-2, 2), 0, -2 + k * 2.6f);
        p.y = g.h(p.x, p.z);
        g.b(p).cilindro(p + Vec3f{0, -0.1f, 0}, 0.9f, 1.3f, 8, PALHA);
    }
    g.tocha(fl.P(3, 0, -2.3f));
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
    constexpr float largura = medidas::PONTE_LARGURA, espessura = .24f;
    g.marcar(f, largura, meio * 2);
    MeshBuilder& m = g.b(f.o);
    // As extremidades da mesma peça definem o plano usado pela malha e pelos pés.
    const int n = int(std::ceil(meio * 2));
    const float passo = meio * 2 / n;
    auto alturaMargem = [&](float z) {
        Vec3f p = f.P(0, 0, z);
        return g.h(p.x, p.z) + .08f;
    };
    const float entradaA = alturaMargem(-meio), entradaB = alturaMargem(meio);
    const float yTab = std::max({g.t.nivelAgua() + 1.6f, entradaA, entradaB});
    auto alturaTabuleiro = [&](float z) {
        float borda = z < 0.f ? entradaA : entradaB;
        float rampa = lerp(borda, yTab, clamp01((meio - std::abs(z)) / 6.f));
        return std::max(rampa, alturaMargem(z));
    };
    for (int k = 0; k < n; k++) {
        float z0 = -meio + k * passo, z = z0 + passo * .5f;
        float y0 = alturaTabuleiro(z0), y1 = alturaTabuleiro(z0 + passo);
        float y = (y0 + y1) * .5f, desnivel = y1 - y0, incl = -std::atan2(desnivel, passo);
        float comprimento = std::hypot(passo, desnivel);
        Vec3f topo = f.P(0, y - f.o.y, z);
        Vec3f normal = rotY({0, std::cos(incl), std::sin(incl)}, f.rot);
        m.caixaInclinada(topo - normal * (espessura * .5f), {largura, espessura, comprimento}, f.rot, incl,
                         MADEIRA * (k % 2 ? .95f : 1.05f));
        g.col.adicionarPisoGirado({topo, largura * .5f, passo * .5f, f.rot, y - espessura, y}, desnivel / passo);
        if (std::abs(z) < meio - 2.5f) {
            constexpr float espGuarda = .12f, altura = medidas::PONTE_GUARDA_CORPO;
            constexpr float lado = (largura - espGuarda) * .5f;
            for (float s : {-1.f, 1.f}) {
                Vec3f q = f.P(s * lado, y - f.o.y, z);
                m.caixaInclinada(q + Vec3f{0, altura - .05f, 0}, {espGuarda, .10f, comprimento}, f.rot, incl, MADEIRA_ESC);
                if (k % 3 == 0) m.caixa(q + Vec3f{0, altura * .5f, 0}, {espGuarda, altura, espGuarda}, f.rot, MADEIRA_ESC);
                g.col.adicionarGirada({q, espGuarda * .5f, passo * .5f, f.rot,
                                       std::min(y0, y1), std::max(y0, y1) + altura});
            }
        }
        // pilares na agua
        if (k % 4 == 0 && std::abs(z) < meio - 3)
            for (float s : {-1.6f, 1.6f}) {
                Vec3f q = f.P(s, 0, z);
                constexpr float raio = .25f;
                float base = g.h(q.x, q.z) - .45f, topoPilar = y - espessura * .5f;
                if (topoPilar <= base) continue;
                m.cilindro({q.x, base, q.z}, raio, topoPilar - base, 6, MADEIRA_ESC);
                g.col.adicionar({{q.x - raio, base, q.z - raio}, {q.x + raio, topoPilar, q.z + raio}});
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

static const medidas::Arvore& medidasArvore(const ArvoreInst& a) {
    return a.tipo == 0 ? medidas::PINHEIRO : medidas::CARVALHO;
}

static void copaCarvalho(MeshBuilder& m, Vec3f base, float r, float h, float rot, int lados, Rgb cor) {
    auto ponto = [&](int k, float y, float raio) {
        float a = rot + 2.f * PI * k / lados;
        return base + Vec3f{std::cos(a) * raio * r, y * h, std::sin(a) * raio * r};
    };
    for (int k = 0; k < lados; ++k) {
        Vec3f a = ponto(k, .08f, .48f), b = ponto(k + 1, .08f, .48f);
        Vec3f c = ponto(k, .48f, 1.f), d = ponto(k + 1, .48f, 1.f);
        Vec3f e = ponto(k, .79f, .70f), f = ponto(k + 1, .79f, .70f);
        m.triangulo(a, d, b, cor * .88f);
        m.triangulo(a, c, d, cor * .88f);
        m.triangulo(c, f, d, cor);
        m.triangulo(c, e, f, cor);
        m.triangulo(e, base + Vec3f{0, h, 0}, f, cor * 1.05f);
        m.triangulo(a, b, base, cor * .82f);
    }
}

static void modeloArvore(MeshBuilder& m, const ArvoreInst& a, bool longe) {
    float s = a.escala;
    Vec3f p = a.pos;
    m.tamanhoMaximoFace(1000.f);  // o tamanho do exemplar não multiplica as faces
    if (a.tipo == 0) {  // pinheiro
        Rgb verde = lerp(Rgb{0.16f, 0.30f, 0.17f}, Rgb{0.24f, 0.38f, 0.2f}, a.tom);
        const auto& arvore = medidasArvore(a);
        float h = s * arvore.altura, r = s * arvore.raioCopa;
        if (longe) {
            m.cone(p + Vec3f{0, .20f * h, 0}, r, .80f * h, 5, verde);
            return;
        }
        m.cilindro(p - Vec3f{0, .30f, 0}, s * arvore.raioTronco, s * arvore.alturaTronco + .30f, 5, {0.33f, 0.23f, 0.16f});
        m.cone(p + Vec3f{0, .20f * h, 0}, r, .44f * h, 7, verde);
        m.cone(p + Vec3f{0, .42f * h, 0}, .74f * r, .40f * h, 7, verde * 1.06f);
        m.cone(p + Vec3f{0, .59f * h, 0}, .50f * r, .41f * h, 6, verde * 1.12f);
    } else if (a.tipo == 1) {  // carvalho
        Rgb verde = lerp(Rgb{0.27f, 0.42f, 0.18f}, Rgb{0.42f, 0.5f, 0.2f}, a.tom);
        const auto& arvore = medidasArvore(a);
        float h = s * arvore.altura, r = s * arvore.raioCopa;
        if (!longe)
            m.cilindro(p - Vec3f{0, .30f, 0}, s * arvore.raioTronco, s * arvore.alturaTronco + .30f, 6, {0.36f, 0.26f, 0.18f});
        copaCarvalho(m, p + Vec3f{0, .36f * h, 0}, r, .64f * h, a.rot, longe ? 5 : 8, verde);
        if (!longe)
            for (float lado : {-1.f, 1.f}) {
                Vec3f desvio = rotY({lado * .51f * r, .33f * h, 0}, a.rot);
                copaCarvalho(m, p + desvio, .45f * r, .43f * h, lado + a.rot, 5, verde * (1.f + lado * .06f));
            }
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
    constexpr float borda = medidas::MEIO_AREA_JOGAVEL + 15.f;
    for (float z = -borda; z < borda; z += PASSO)
        for (float x = -borda; x < borda; x += PASSO) {
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
                const auto& arvore = medidasArvore(a);
                float r = a.escala * arvore.raioTronco;
                g.col.adicionar({{px - r, a.pos.y - .30f, pz - r},
                                 {px + r, a.pos.y + a.escala * arvore.alturaTronco, pz + r}});
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
                    p.limitesConstr = GetMeshBoundingBox(p.construcoes);
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
                    Q.limitesDet = GetMeshBoundingBox(Q.det);
                    Q.limitesCasca = GetMeshBoundingBox(Q.casca);
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
                    p.limitesArvLonge = GetMeshBoundingBox(p.arvLonge);
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
        float altura = a.tipo < 2 ? medidasArvore(a).altura : 1.f;
        m.aoVertical(a.pos.y, a.pos.y + .65f * altura * a.escala);
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
                float h = lerp(medidas::CAPIM_ALTURA_MIN, medidas::CAPIM_ALTURA_MAX, aleatorio01(rng)) * ambiente.altura;
                float verde = .85f + aleatorio01(rng) * .3f;
                Rgb cor = corCapimUmido({0.28f * verde, 0.40f * verde, 0.16f * verde}, ambiente.umidade);
                Rgb ponta = corCapimUmido({0.43f * verde, 0.51f * verde, 0.23f * verde}, ambiente.umidade);
                for (int k = 0; k < 3; k++) {
                    float a = aleatorio01(rng) * 6.2831f, l = 0.008f + aleatorio01(rng) * 0.009f;
                    Vec3f lado{std::cos(a) * l, 0, std::sin(a) * l};
                    Vec3f top = base + Vec3f{std::sin(a + 1.3f) * h * 0.38f * ambiente.inclinacao, h * (0.68f + 0.16f * k),
                                             std::cos(a + 1.3f) * h * 0.38f * ambiente.inclinacao};
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
    p.limitesArvPerto = GetMeshBoundingBox(p.arvPerto);
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

void Cenario::prepararPassePrincipal(const Camera3D& cam, float distMax, float distDetalhe) {
    construcoesVisiveis_.clear();
    arvoresVisiveis_.clear();
    tris_ = 0;
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    float meio = std::acos(std::cos(cam.fovy * 0.5f * 1.9f * DEG2RAD));
    const Frustum frustum = Frustum::doPasse();
    auto selecionarMalha = [&](const Mesh& malha, const BoundingBox& limites, std::vector<const Mesh*>& visiveis) {
        if (!frustum.contemParte(limites)) return;
        visiveis.push_back(&malha);
        tris_ += malha.triangleCount;
    };
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
        if (p.temConstr) selecionarMalha(p.construcoes, p.limitesConstr, construcoesVisiveis_);
        for (const auto& q : p.q) {  // predios detalhados perto e a casca longe
            if (!q.tem) continue;
            float dq = std::hypot(q.centro.x - cam.position.x, q.centro.z - cam.position.z) - TAM * 0.36f;
            if (dq > distDetalhe) selecionarMalha(q.casca, q.limitesCasca, construcoesVisiveis_);
            else selecionarMalha(q.det, q.limitesDet, construcoesVisiveis_);
        }
        bool perto = dh < DIST_PERTO && p.temPerto;
        if (perto) {
            p.usoPerto = tempo_;
            selecionarMalha(p.arvPerto, p.limitesArvPerto, arvoresVisiveis_);
        } else if (p.temLonge) {
            selecionarMalha(p.arvLonge, p.limitesArvLonge, arvoresVisiveis_);
        }
    }
    chamadas_ = int(construcoesVisiveis_.size() + arvoresVisiveis_.size());
}

void Cenario::desenharProfundidadeConstrucoes(const Material& mat) const {
    if (construcoesVisiveis_.empty()) return;
    // O passe inicial escreve so a profundidade. A ordem dos passes de cor continua a mesma.
    rlColorMask(false, false, false, false);
    desenharConstrucoes(mat);
    rlColorMask(true, true, true, true);
}

static void desenharMalhas(const std::vector<const Mesh*>& malhas, const Material& mat) {
    const Matrix identidade = MatrixIdentity();
    for (const Mesh* malha : malhas) DrawMesh(*malha, mat, identidade);
}

void Cenario::desenharConstrucoes(const Material& mat) const {
    desenharMalhas(construcoesVisiveis_, mat);
}

void Cenario::desenharArvores(const Material& mat) const {
    desenharMalhas(arvoresVisiveis_, mat);
}

void Cenario::desenharArea(Vec3f centro, float raio, const Material& mat, const Material& matOpaca) {
    const Matrix id = MatrixIdentity();
    const Frustum frustum = Frustum::doPasse();
    auto desenharMalha = [&](const Mesh& malha, const BoundingBox& limites, const Material& material) {
        if (frustum.contemParte(limites)) DrawMesh(malha, material, id);
    };
    for (const auto& p : p_) {
        if (std::hypot(p.centro.x - centro.x, p.centro.z - centro.z) > raio + p.raio) continue;
        if (p.temConstr) desenharMalha(p.construcoes, p.limitesConstr, mat);
        for (const auto& q : p.q) {  // sombras: casca (leve); perto, o predio detalhado (o sol entra pelos vaos)
            if (!q.tem) continue;
            bool perto = std::hypot(q.centro.x - centro.x, q.centro.z - centro.z) < 25.f + TAM * 0.36f;
            if (perto) desenharMalha(q.det, q.limitesDet, mat);
            else desenharMalha(q.casca, q.limitesCasca, mat);
        }
        if (p.temPerto) desenharMalha(p.arvPerto, p.limitesArvPerto, matOpaca);
        else if (p.temLonge) desenharMalha(p.arvLonge, p.limitesArvLonge, matOpaca);
    }
}

void Cenario::descarregar() {
    construcoesVisiveis_.clear();
    arvoresVisiveis_.clear();
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
