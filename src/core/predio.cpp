// Gerador de predios por gramatica de formas. Lote -> volume (paredes com espessura) -> andares ->
// fachadas divididas em vaos (porta, janelas, parede cheia) -> pecas (paredes recortadas, molduras,
// venezianas, enxaimel, telhado em placas, chamine, varanda...). Estilos: medieval e faroeste.
#include "core/predio.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float PI = 3.14159265f;

struct Rng {
    uint32_t s;
    float f() { s = s * 1664525u + 1013904223u; return float((s >> 8) & 0xFFFFFF) / 16777216.f; }
    float r(float a, float b) { return a + (b - a) * f(); }
    bool p(float prob) { return f() < prob; }
};

Rgb mul(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }

const Rgb PEDRA{0.56f, 0.54f, 0.50f}, PEDRA_ESC{0.42f, 0.41f, 0.40f}, MADEIRA_ESC{0.27f, 0.19f, 0.13f},
    PISO{0.36f, 0.26f, 0.17f}, FERRO{0.2f, 0.2f, 0.22f}, ESCURO{0.03f, 0.03f, 0.035f}, VIDRO{1.0f, 0.72f, 0.36f};
const Rgb VENEZIANAS[] = {{0.25f, 0.33f, 0.24f}, {0.45f, 0.2f, 0.16f}, {0.3f, 0.24f, 0.18f}, {0.24f, 0.3f, 0.38f}, {0.5f, 0.42f, 0.3f}};

struct Construtor {
    Planta& p;
    void caixa(Vec3f c, Vec3f t, MatPeca m, Rgb cor, bool forma = false, bool detalhe = false, float rotY = 0, float incl = 0) {
        PecaPredio q;
        q.centro = c, q.tam = t, q.mat = m, q.cor = cor, q.forma = forma && incl == 0, q.detalhe = detalhe, q.rotY = rotY, q.inclinacao = incl;
        p.pecas.push_back(q);
    }
    void tri(Vec3f a, Vec3f b, Vec3f c, MatPeca m, Rgb cor) {
        PecaPredio q;
        q.tipo = PecaPredio::Triangulo, q.a = a, q.b = b, q.c = c, q.mat = m, q.cor = cor;
        q.centro = (a + b + c) * (1.f / 3.f);
        p.pecas.push_back(q);
    }
    void soForma(Vec3f c, Vec3f t, float rotY, bool prisma) {
        PecaPredio q;
        q.tipo = prisma ? PecaPredio::Prisma : PecaPredio::Caixa, q.centro = c, q.tam = t, q.rotY = rotY, q.forma = true, q.soForma = true;
        q.mat = MatPeca::Telhado;
        p.pecas.push_back(q);
    }
    void colisao(Vec3f c, float mx, float mz, float y0, float y1) { p.paredes.push_back({c, mx, mz, 0.f, y0, y1}); }
};

// lado: 0 frente (+z), 1 fundos (-z), 2 direita (+x), 3 esquerda (-x). u = x (frente/fundos) ou z (lados).
// Ponto da fachada (u, y) a uma distancia "fora" do plano externo da parede (positivo = para fora).
Vec3f naFachada(int lado, float u, float y, float fora, float W, float D) {
    switch (lado) {
        case 0: return {u, y, D * 0.5f + fora};
        case 1: return {u, y, -D * 0.5f - fora};
        case 2: return {W * 0.5f + fora, y, u};
        default: return {-W * 0.5f - fora, y, u};
    }
}
Vec3f normalLado(int lado) {
    switch (lado) {
        case 0: return {0, 0, 1};
        case 1: return {0, 0, -1};
        case 2: return {1, 0, 0};
        default: return {-1, 0, 0};
    }
}
// tamanho de uma caixa encostada na fachada: "ao longo" (u), altura, espessura (normal)
Vec3f tamFachada(int lado, float ao, float alt, float esp) { return lado < 2 ? Vec3f{ao, alt, esp} : Vec3f{esp, alt, ao}; }
}  // namespace

Planta gerarPredio(const Lote& l) {
    Planta p;
    Construtor k{p};
    Rng g{l.semente * 2654435761u + 12345u};
    g.f();
    const bool med = l.estilo == Estilo::Medieval;
    const TipoPredio tipo = l.tipo;
    const bool pedra = tipo == TipoPredio::Cadeia || tipo == TipoPredio::Xerife || tipo == TipoPredio::Capela;
    const float W = l.largura, D = l.profundidade;
    const float t = pedra ? 0.4f : (med ? 0.24f : 0.18f);
    float H = 2.95f;
    if (tipo == TipoPredio::Capela) H = 4.3f;
    if (tipo == TipoPredio::Celeiro) H = 4.0f;
    const int andares = (tipo == TipoPredio::Capela || tipo == TipoPredio::Celeiro || tipo == TipoPredio::Barracao) ? 1 : std::max(1, l.andares);
    const float piso = l.piso, yT = piso + H * andares;
    p.piso = piso;
    p.peDireito = H - 0.15f;
    MatPeca matParede = pedra ? MatPeca::Pedra : (med ? (l.corParede.r + l.corParede.g + l.corParede.b > 1.8f ? MatPeca::Reboco : MatPeca::Madeira) : MatPeca::Madeira);
    Rgb corParede = pedra ? (tipo == TipoPredio::Cadeia ? PEDRA_ESC : PEDRA) : l.corParede;
    Rgb corVeneziana = VENEZIANAS[int(g.f() * 5) % 5];
    const bool enxaimel = med && !pedra;

    // ---------------- vaos por fachada ----------------
    float largPorta = tipo == TipoPredio::Celeiro ? 2.6f : (tipo == TipoPredio::Capela ? 1.4f : (tipo == TipoPredio::Taverna ? 1.3f : 1.1f));
    float altPorta = tipo == TipoPredio::Celeiro ? 3.0f : (tipo == TipoPredio::Capela ? 2.6f : 2.15f);
    float largJan = tipo == TipoPredio::Capela ? 0.6f : (tipo == TipoPredio::Cadeia ? 0.6f : 0.9f);
    float altJan = tipo == TipoPredio::Capela ? 1.8f : (tipo == TipoPredio::Cadeia ? 0.7f : 1.1f);
    float peitoril = tipo == TipoPredio::Capela ? 1.6f : (tipo == TipoPredio::Cadeia || tipo == TipoPredio::Celeiro ? 1.6f : 0.9f);
    float vaoMedio = g.r(1.8f, 2.3f);
    float portaU = 0;
    struct Fachada { float L; int n; float bw; };
    Fachada fs[4];
    for (int lado = 0; lado < 4; lado++) {
        float L = lado < 2 ? W : D;
        int n = std::max(1, int(std::round(L / vaoMedio)));
        fs[lado] = {L, n, L / n};
    }
    {   // porta na frente: vao do meio (ou um dos dois do meio)
        int n = fs[0].n;
        int i = (n % 2 == 1 || !med) ? n / 2 : n / 2 - int(g.p(0.5f));
        portaU = -W * 0.5f + (i + 0.5f) * fs[0].bw;
        if (tipo == TipoPredio::Celeiro || tipo == TipoPredio::Capela || !med) portaU = 0;
        float lim = W * 0.5f - t - 0.25f - largPorta * 0.5f;  // longe da quina
        portaU = std::clamp(portaU, -lim, lim);
        p.vaos.push_back({0, 0, portaU - largPorta * 0.5f, portaU + largPorta * 0.5f, piso, piso + altPorta, true});
    }
    const float probJ[4] = {0.85f, 0.45f, 0.55f, 0.55f};
    for (int lado = 0; lado < 4; lado++) {
        const Fachada& f = fs[lado];
        float limite = f.L * 0.5f - t - 0.3f - largJan * 0.5f;  // janela longe da quina
        for (int i = 0; i < f.n; i++) {
            float u = -f.L * 0.5f + (i + 0.5f) * f.bw;
            if (f.bw < largJan + 0.5f || std::abs(u) > limite) continue;
            bool temPorta = lado == 0 && std::abs(u - portaU) < (largPorta + largJan) * 0.5f + 0.3f;
            // terreo
            if (!temPorta && tipo != TipoPredio::Celeiro && g.p(tipo == TipoPredio::Capela ? (lado >= 2 ? 1.f : 0.f) : probJ[lado]))
                p.vaos.push_back({lado, 0, u - largJan * 0.5f, u + largJan * 0.5f, piso + peitoril, piso + peitoril + altJan, false});
            if (tipo == TipoPredio::Celeiro && lado >= 2 && i % 2 == 0)
                p.vaos.push_back({lado, 0, u - 0.4f, u + 0.4f, piso + 2.6f, piso + 3.3f, false});
            // andares de cima: alinhadas com os vaos de baixo (e sobre a porta)
            for (int a = 1; a < andares; a++) {
                bool alinhada = temPorta || std::any_of(p.vaos.begin(), p.vaos.end(), [&](const Vao& v) { return v.lado == lado && v.andar == 0 && std::abs((v.u0 + v.u1) * 0.5f - u) < 0.01f; });
                if (alinhada ? g.p(0.9f) : g.p(0.3f)) {
                    float y0 = piso + a * H + 0.9f;
                    p.vaos.push_back({lado, a, u - largJan * 0.5f, u + largJan * 0.5f, y0, y0 + altJan, false});
                }
            }
        }
    }

    // ---------------- alicerce, piso, forro ----------------
    k.caixa({0, (piso - 0.02f - 1.5f) * 0.5f, 0}, {W + 0.12f, piso - 0.02f + 1.5f, D + 0.12f}, MatPeca::Pedra, med ? PEDRA : PEDRA_ESC, true);
    k.caixa({0, piso - 0.04f, 0}, {W - 2 * t, 0.08f, D - 2 * t}, MatPeca::MadeiraEsc, PISO);
    k.caixa({0, piso + H - 0.075f, 0}, {W - 2 * t, 0.15f, D - 2 * t}, MatPeca::MadeiraEsc, mul(PISO, 0.9f));
    for (int a = 1; a < andares; a++)  // vigas do piso de cima aparecendo por fora
        k.caixa({0, piso + a * H, 0}, {W + 0.04f, 0.2f, D + 0.04f}, MatPeca::MadeiraEsc, MADEIRA_ESC, false, !enxaimel);
    p.pisos.push_back({{0, 0, 0}, W * 0.5f, D * 0.5f, 0.f, -1.f, piso});
    p.interiorCentro = {0, piso + (H - 0.15f) * 0.5f, 0};
    p.interiorMeia = {W * 0.5f - t, (H - 0.15f) * 0.5f, D * 0.5f - t};

    // ---------------- paredes recortadas pelos vaos ----------------
    for (int lado = 0; lado < 4; lado++) {
        float Lw = lado < 2 ? W : D - 2 * t;  // frente/fundos inteiras; lados entre elas
        for (int a = 0; a < andares; a++) {
            float y0 = a == 0 ? piso - 0.02f : piso + a * H, y1 = piso + (a + 1) * H;
            std::vector<Vao> vs;
            for (auto& v : p.vaos)
                if (v.lado == lado && v.andar == a) vs.push_back(v);
            std::sort(vs.begin(), vs.end(), [](const Vao& x, const Vao& y) { return x.u0 < y.u0; });
            auto pedaco = [&](float u0, float u1, float h0, float h1) {
                if (u1 - u0 < 0.01f || h1 - h0 < 0.01f) return;
                float um = (u0 + u1) * 0.5f;
                Vec3f c = naFachada(lado, um, (h0 + h1) * 0.5f, -t * 0.5f, W, D);
                Vec3f tm = tamFachada(lado, u1 - u0, h1 - h0, t);
                k.caixa(c, tm, matParede, corParede, true);
                if (h0 < piso + 1.9f) k.colisao(c, tm.x * 0.5f, tm.z * 0.5f, h0, h1);
            };
            float u = -Lw * 0.5f;
            for (auto& v : vs) {
                pedaco(u, v.u0, y0, y1);
                pedaco(v.u0, v.u1, y0, v.v0);  // abaixo (peitoril; na porta nao ha)
                pedaco(v.u0, v.u1, v.v1, y1);  // acima (verga)
                u = v.u1;
            }
            pedaco(u, Lw * 0.5f, y0, y1);
        }
    }

    // ---------------- molduras, peitoris, venezianas, vidros ----------------
    for (auto& v : p.vaos) {
        float um = (v.u0 + v.u1) * 0.5f, w = v.u1 - v.u0, h = v.v1 - v.v0, ym = (v.v0 + v.v1) * 0.5f;
        Rgb corMold = pedra ? mul(PEDRA, 0.85f) : MADEIRA_ESC;
        MatPeca matMold = pedra ? MatPeca::Pedra : MatPeca::MadeiraEsc;
        float prof = t + 0.06f, fora = 0.03f - (t + 0.06f) * 0.5f;
        // batentes e verga
        k.caixa(naFachada(v.lado, v.u0 - 0.05f, ym, fora, W, D), tamFachada(v.lado, 0.1f, h, prof), matMold, corMold, false, true);
        k.caixa(naFachada(v.lado, v.u1 + 0.05f, ym, fora, W, D), tamFachada(v.lado, 0.1f, h, prof), matMold, corMold, false, true);
        k.caixa(naFachada(v.lado, um, v.v1 + 0.07f, fora, W, D), tamFachada(v.lado, w + 0.3f, 0.14f, prof), matMold, corMold);
        if (v.porta) {
            p.porta.largura = w;
            p.porta.altura = h;
            p.porta.dobradica = {v.u0, piso, D * 0.5f - t * 0.5f};
            p.porta.rotFechada = 0;
            p.porta.sentido = 1;
            continue;
        }
        k.caixa(naFachada(v.lado, um, v.v0 - 0.04f, 0.06f - (t + 0.12f) * 0.5f, W, D), tamFachada(v.lado, w + 0.2f, 0.08f, t + 0.12f), matMold, corMold, false, true);
        Vec3f n = normalLado(v.lado);
        p.janelas.push_back({naFachada(v.lado, um, ym, 0, W, D), n});
        if (v.andar > 0)  // andar de cima nao e acessivel: vidro (acende a noite)
            k.caixa(naFachada(v.lado, um, ym, -t * 0.5f, W, D), tamFachada(v.lado, w, h, 0.04f), MatPeca::Vidro, VIDRO);
        if (tipo == TipoPredio::Cadeia)  // grades
            for (int b = 0; b < 4; b++)
                k.caixa(naFachada(v.lado, v.u0 + (b + 0.5f) * w / 4, ym, -0.05f, W, D), tamFachada(v.lado, 0.04f, h, 0.04f), MatPeca::Ferro, FERRO, false, true);
        else if (tipo != TipoPredio::Capela && (med || g.p(0.7f))) {  // venezianas abertas contra a parede
            for (float sd : {-1.f, 1.f}) {
                float uc = sd < 0 ? v.u0 - 0.1f - w * 0.25f : v.u1 + 0.1f + w * 0.25f;
                k.caixa(naFachada(v.lado, uc, ym, 0.03f, W, D), tamFachada(v.lado, w * 0.5f, h + 0.05f, 0.04f), MatPeca::Madeira, corVeneziana, false, true);
            }
        }
    }

    // ---------------- enxaimel / cantos ----------------
    if (enxaimel) {
        for (float sx : {-1.f, 1.f})
            for (float sz : {-1.f, 1.f})
                k.caixa({sx * (W * 0.5f - 0.06f), (piso + yT) * 0.5f, sz * (D * 0.5f - 0.06f)}, {0.26f, yT - piso, 0.26f}, MatPeca::MadeiraEsc, MADEIRA_ESC);
        for (int lado = 0; lado < 4; lado++) {
            const Fachada& f = fs[lado];
            for (int a = 0; a <= andares; a++)  // soleira, frechal e vigas de andar
                k.caixa(naFachada(lado, 0, piso + a * H - (a == andares ? 0.1f : 0.f), 0.02f, W, D), tamFachada(lado, f.L, 0.2f, 0.06f), MatPeca::MadeiraEsc, MADEIRA_ESC, false, a > 0 && a < andares);
            for (int i = 1; i < f.n; i++) {  // esteios nas divisoes dos vaos
                float u = -f.L * 0.5f + i * f.bw;
                bool cruza = std::any_of(p.vaos.begin(), p.vaos.end(), [&](const Vao& v) { return v.lado == lado && u > v.u0 - 0.15f && u < v.u1 + 0.15f; });
                if (cruza) continue;
                k.caixa(naFachada(lado, u, (piso + yT) * 0.5f, 0.02f, W, D), tamFachada(lado, 0.16f, yT - piso, 0.06f), MatPeca::MadeiraEsc, MADEIRA_ESC, false, true);
            }
        }
    } else {
        for (float sx : {-1.f, 1.f})  // cantoneiras (faroeste) ou pedras de quina
            for (float sz : {-1.f, 1.f})
                k.caixa({sx * (W * 0.5f - 0.04f), (piso + yT) * 0.5f, sz * (D * 0.5f - 0.04f)}, {0.14f, yT - piso, 0.14f}, pedra ? MatPeca::Pedra : MatPeca::MadeiraEsc,
                        pedra ? mul(PEDRA, 0.9f) : mul(l.corParede, 0.7f), false, true);
    }

    // ---------------- telhado: duas placas, empenas, cumeeira ----------------
    {
        bool cumeX = W >= D || tipo == TipoPredio::Capela;  // cumeeira ao longo de x
        if (tipo == TipoPredio::Capela) cumeX = false;      // capela: nave comprida para tras (cumeeira em z)
        float vao = cumeX ? D : W, comp = cumeX ? W : D;
        float beiral = med ? 0.6f : 0.45f, empena = 0.3f;
        float incl = med ? g.r(0.5f, 0.62f) : g.r(0.28f, 0.36f);
        if (tipo == TipoPredio::Capela) incl = 0.72f;
        float s = vao * 0.5f, rh = s * incl * 1.6f;  // altura da cumeeira
        float a = std::atan2(rh, s);
        float esp = med ? 0.22f : 0.12f;
        float L = (s + beiral) / std::cos(a);
        float rot = cumeX ? 0.f : PI * 0.5f;
        for (float lado : {1.f, -1.f}) {
            // centro da placa: meio do caminho entre a cumeeira e a ponta do beiral, acima do plano
            float zc = lado * (s + beiral) * 0.5f, yc = yT + rh - (s + beiral) * 0.5f * std::tan(a);
            Vec3f c = cumeX ? Vec3f{0, yc, zc} : Vec3f{zc, yc, 0};
            Vec3f nrm = cumeX ? Vec3f{0, std::cos(a), lado * std::sin(a)} : Vec3f{lado * std::sin(a), std::cos(a), 0};
            c = c + nrm * (esp * 0.5f);
            float ry = lado > 0 ? rot : rot + PI;
            k.caixa(c, {comp + 2 * empena, esp, L}, MatPeca::Telhado, l.corTelhado, false, false, ry, a);
        }
        // empenas (triangulos na cor da parede, para fora)
        for (float e : {1.f, -1.f}) {
            float pc = comp * 0.5f * e;
            Vec3f A, B, C;
            if (cumeX) A = {pc, yT, -s}, B = {pc, yT, s}, C = {pc, yT + rh, 0};
            else A = {-s, yT, pc}, B = {s, yT, pc}, C = {0, yT + rh, pc};
            Vec3f n = cross(B - A, C - A);
            Vec3f fora = cumeX ? Vec3f{e, 0, 0} : Vec3f{0, 0, e};
            if (dot(n, fora) < 0) std::swap(A, B);
            k.tri(A, B, C, matParede, corParede);
        }
        // cumeeira e caibros sob o beiral
        k.caixa(cumeX ? Vec3f{0, yT + rh + 0.05f, 0} : Vec3f{0, yT + rh + 0.05f, 0}, cumeX ? Vec3f{comp + 2 * empena, 0.16f, 0.2f} : Vec3f{0.2f, 0.16f, comp + 2 * empena},
                MatPeca::MadeiraEsc, mul(l.corTelhado, 0.7f));
        int nc = int(comp / 0.6f);
        for (int i = 0; i <= nc && med; i++)
            for (float lado : {1.f, -1.f}) {
                float u = -comp * 0.5f + i * comp / nc;
                float zz = lado * (s + beiral * 0.5f), yy = yT + rh - (s + beiral * 0.5f) * std::tan(a) - 0.06f;
                k.caixa(cumeX ? Vec3f{u, yy, zz} : Vec3f{zz, yy, u}, cumeX ? Vec3f{0.08f, 0.1f, beiral} : Vec3f{beiral, 0.1f, 0.08f}, MatPeca::MadeiraEsc, MADEIRA_ESC, false, true);
            }
        // forma do telhado para a luz tracada (prisma)
        k.soForma({0, yT + rh * 0.5f, 0}, {comp + 2 * empena, rh, 2 * (s + beiral)}, rot, true);
        // chamine numa empena (casas, taverna)
        if ((tipo == TipoPredio::Casa || tipo == TipoPredio::Taverna) && g.p(med ? 0.85f : 0.6f)) {
            float e = g.p(0.5f) ? 1.f : -1.f;
            float alt = yT + rh + 0.7f + 1.5f;
            Vec3f c = cumeX ? Vec3f{e * (W * 0.5f + 0.35f), alt * 0.5f - 0.75f, g.r(-0.4f, 0.4f)} : Vec3f{g.r(-0.4f, 0.4f), alt * 0.5f - 0.75f, e * (D * 0.5f + 0.35f)};
            if (!cumeX && e > 0) c.z = -(D * 0.5f + 0.35f);  // nunca na frente (porta)
            k.caixa(c, cumeX ? Vec3f{0.7f, alt, 0.9f} : Vec3f{0.9f, alt, 0.7f}, MatPeca::Pedra, PEDRA_ESC, true);
            k.colisao(c, cumeX ? 0.35f : 0.45f, cumeX ? 0.45f : 0.35f, -1.5f, alt);
        }
        // fachada falsa (faroeste, taverna/xerife)
        if (!med && (tipo == TipoPredio::Taverna || tipo == TipoPredio::Xerife)) {
            float hf = (cumeX ? rh : rh) + 1.2f;
            k.caixa({0, yT + hf * 0.5f, D * 0.5f - 0.1f}, {W + 0.2f, hf, 0.2f}, MatPeca::Madeira, mul(l.corParede, 1.05f), true);
            k.caixa({0, yT + hf + 0.06f, D * 0.5f - 0.05f}, {W + 0.4f, 0.12f, 0.35f}, MatPeca::MadeiraEsc, MADEIRA_ESC);
        }
    }

    // ---------------- varanda (faroeste), degrau, entrada ----------------
    float frente = D * 0.5f;
    if (!med && (tipo == TipoPredio::Casa || tipo == TipoPredio::Taverna || tipo == TipoPredio::Xerife)) {
        float pv = 2.0f, larg = W + 0.3f;
        k.caixa({0, (piso - 0.04f - 1.2f) * 0.5f, frente + pv * 0.5f}, {larg, piso - 0.04f + 1.2f, pv}, MatPeca::MadeiraEsc, PISO, true);
        p.pisos.push_back({{0, 0, frente + pv * 0.5f}, larg * 0.5f, pv * 0.5f, 0.f, -1.f, piso - 0.04f});
        int np = std::max(2, int(larg / 2.5f) + 1);
        float hv = piso + H - 0.3f;
        for (int i = 0; i < np; i++) {
            float x = -larg * 0.5f + 0.1f + i * (larg - 0.2f) / (np - 1);
            if (std::abs(x - portaU) < largPorta * 0.5f + 0.3f) continue;  // poste nao na frente da porta
            Vec3f c{x, (piso + hv) * 0.5f, frente + pv - 0.1f};
            k.caixa(c, {0.14f, hv - piso, 0.14f}, MatPeca::MadeiraEsc, MADEIRA_ESC);
            k.colisao(c, 0.07f, 0.07f, piso - 0.5f, hv);
        }
        float ia = std::atan2(0.35f, pv);
        k.caixa({0, hv + 0.2f, frente + pv * 0.5f}, {larg + 0.2f, 0.08f, pv / std::cos(ia) + 0.2f}, MatPeca::Telhado, l.corTelhado, false, false, 0.f, ia);
        frente += pv;
    }
    {
        float hd = std::max(piso * 0.5f, 0.05f);
        k.caixa({portaU, (hd - 1.0f) * 0.5f, frente + 0.4f}, {largPorta + 0.5f, hd + 1.0f, 0.8f}, MatPeca::Pedra, PEDRA_ESC);
        p.pisos.push_back({{portaU, 0, frente + 0.4f}, largPorta * 0.5f + 0.25f, 0.4f, 0.f, -1.f, hd});
        p.pisos.push_back({{portaU, 0, D * 0.5f - t * 0.5f}, largPorta * 0.5f, t * 0.5f + 0.02f, 0.f, -1.f, piso});  // soleira
        p.entrada = {portaU, 0, frente + 1.3f};
    }
    // placa (taverna, xerife)
    if (tipo == TipoPredio::Taverna || tipo == TipoPredio::Xerife) {
        Rgb cp = tipo == TipoPredio::Taverna ? Rgb{0.7f, 0.5f, 0.15f} : Rgb{0.25f, 0.3f, 0.5f};
        float xp = portaU + (portaU > 0 ? -1.f : 1.f) * (largPorta * 0.5f + 0.9f);
        k.caixa({xp, piso + 2.95f, D * 0.5f + 0.55f}, {0.06f, 0.06f, 1.1f}, MatPeca::Ferro, FERRO, false, true);
        k.caixa({xp, piso + 2.5f, D * 0.5f + 0.85f}, {0.07f, 0.65f, 0.85f}, MatPeca::Placa, cp, false, true);
    }
    // ---------------- casca: volume + telhado + janelas escuras ----------------
    {
        std::vector<PecaPredio> c;
        auto copia = [&](const PecaPredio& q) { PecaPredio r = q; r.forma = false; r.detalhe = false; c.push_back(r); };
        PecaPredio vol;
        vol.centro = {0, (piso - 1.5f + yT) * 0.5f, 0}, vol.tam = {W, yT - piso + 1.5f, D}, vol.mat = matParede, vol.cor = corParede;
        c.push_back(vol);
        for (auto& q : p.pecas) {
            bool telhado = q.mat == MatPeca::Telhado && !q.soForma;
            bool empena = q.tipo == PecaPredio::Triangulo;
            bool chamine = q.mat == MatPeca::Pedra && q.forma && q.tam.y > yT - piso;
            bool varanda = !med && q.mat == MatPeca::MadeiraEsc && q.forma;
            bool fachadaFalsa = !med && q.mat == MatPeca::Madeira && q.forma && q.centro.y > yT;
            if (telhado || empena || chamine || varanda || fachadaFalsa) copia(q);
        }
        for (auto& v : p.vaos) {  // janelas e porta como placas (2 triangulos) para fora; as janelas acendem a noite
            Vec3f a = naFachada(v.lado, v.u0, v.v0, 0.01f, W, D), b = naFachada(v.lado, v.u1, v.v0, 0.01f, W, D);
            Vec3f cc = naFachada(v.lado, v.u1, v.v1, 0.01f, W, D), d = naFachada(v.lado, v.u0, v.v1, 0.01f, W, D);
            Vec3f n = normalLado(v.lado);
            if (dot(cross(b - a, cc - a), n) < 0) std::swap(b, d);
            for (int tq = 0; tq < 2; tq++) {
                PecaPredio j;
                j.tipo = PecaPredio::Triangulo;
                j.a = a, j.b = tq ? cc : b, j.c = tq ? d : cc;
                j.centro = (j.a + j.b + j.c) * (1.f / 3.f);
                j.mat = v.porta ? MatPeca::Escuro : MatPeca::Vidro;
                j.cor = v.porta ? MADEIRA_ESC : VIDRO;
                c.push_back(j);
            }
        }
        p.casca = std::move(c);
        PecaPredio g;  // volume grosso (paredes ate o topo): e assim que os vizinhos "veem" este predio
        g.centro = vol.centro, g.tam = vol.tam, g.forma = true, g.soForma = true, g.grossa = true;
        p.pecas.push_back(g);
    }
    return p;
}

int triangulosPlanta(const Planta& p, bool comDetalhe) {
    int n = 0;
    for (auto& q : p.pecas) {
        if (q.soForma || (!comDetalhe && q.detalhe)) continue;
        n += q.tipo == PecaPredio::Caixa ? 12 : (q.tipo == PecaPredio::Prisma ? 8 : 1);
    }
    return n;
}

CaixaGirada folhaPorta(const PortaMundo& p) {
    float ang = p.rot + p.abertura * PI * 0.5f * p.sentido;
    Vec3f dir = rotY({1, 0, 0}, ang);
    Vec3f c = p.dobradica + dir * (p.largura * 0.5f);
    return {c, p.largura * 0.5f, 0.035f, ang, p.dobradica.y, p.dobradica.y + p.altura};
}

bool portaPodeFechar(const PortaMundo& p, Vec3f pos, float raio) {
    // a folha varre de onde esta ate fechada: nenhuma posicao intermediaria pode tocar o circulo
    for (int k = 0; k <= 8; k++) {
        PortaMundo q = p;
        q.abertura = p.abertura * float(k) / 8.f;
        MundoColisao m;
        m.definirDinamicas({folhaPorta(q)});
        Vec3f r = m.resolver({pos.x, p.dobradica.y, pos.z}, raio);
        if (std::abs(r.x - pos.x) + std::abs(r.z - pos.z) > 1e-4f) return false;
    }
    return true;
}

int dentroDeInterior(const std::vector<InteriorMundo>& is, Vec3f p) {
    for (size_t i = 0; i < is.size(); i++) {
        const InteriorMundo& it = is[i];
        Vec3f d = p - it.centro;
        float c = std::cos(it.rot), s = std::sin(it.rot);
        float lx = d.x * c - d.z * s, lz = d.x * s + d.z * c;
        // o pe do personagem fica no piso (embaixo do centro): a faixa de altura comeca 0,3 m abaixo do piso
        if (std::abs(lx) <= it.meia.x && std::abs(lz) <= it.meia.z && d.y >= -it.meia.y - 0.3f && d.y <= it.meia.y) return int(i);
    }
    return -1;
}
