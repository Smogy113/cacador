# Paredes com "raytracing" — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Paredes das casas com relevo desenhado pixel a pixel (reboco, tábuas, pedra) e luz traçada contra as construções reais (sombra suave, oclusão, rebote, luz das janelas).

**Architecture:** O `MeshBuilder` (core) passa a registrar, quando pedido, as formas analíticas (caixa girada, prisma, cilindro) que gera; o `Cenario` guarda essa lista e, a cada quadro, envia ao `lit_` as ~32 formas e ~12 janelas mais próximas. Os vértices de parede ganham tipos novos; no `FS_LIT`, só esses fragmentos fazem relevo (raio num campo de alturas) e traçam raios contra a fórmula de distância das formas.

**Tech Stack:** C++17, raylib 5.5, GLSL 330 (web sem mudança), doctest.

**Spec:** `docs/superpowers/specs/2026-11-21-paredes-raytracing-design.md`

## Global Constraints
- Tudo gerado por código; sem triângulos novos nas paredes (detalhe por pixel).
- Baixo e web: sem mudança. Benchmark Baixo ≤ 350 mil triângulos / 350 chamadas.
- Alto: relevo + sombra própria do relevo + sombra suave + oclusão + rebote + janelas.
- Médio: relevo com menos passos, sem sombra própria do relevo; oclusão pela fórmula; sem marcha de sombra e sem rebote (mapa de sombras como hoje).
- Luz dos raios = `dirSombra` (sol de dia, lua à noite).
- Nas paredes dentro do alcance (Alto), a sombra vem só da fórmula; fora dele e no chão, o mapa de sombras como hoje.
- Alcance: efeito completo até 40 m, some até 70 m; relevo até 25 m, só normal até 45 m.
- Sem ruído/granulado (direções fixas), sem relevo "nadando", sem saturar cores.
- Testes atuais passando; versão 0.19.0 ao final.

## Review Focus
1. Câmera encostada numa parede (< 0,3 m) e olhando rente a ela → relevo não estoura (sem faixas nem buracos). Task 3: captura `--pos` encostado + ângulo rasante.
2. Prédios girados (rotação qualquer) → coordenadas do relevo e formas alinhadas à face, não ao mundo. Task 1: teste de distância com caixa girada 0,7 rad; Task 3: captura de casa girada.
3. Mais de 32 formas perto (centro da cidade) → as mais próximas do ponto de vista entram, sem "piscar" ao andar. Task 2: teste de `formasPerto` estável (ordem por distância) + captura andando.
4. Noite → sombras pela lua, janelas iluminando a parede; nada preto absoluto. Task 4: captura `--hora 22`.
5. Trocar Alto→Médio→Baixo no F1 → efeitos somem/voltam sem erro. Task 4: manual + capturas por nível.

---

### Task 1: Formas analíticas no core

**Files:** Create `src/core/formas.h`, `src/core/formas.cpp`; Modify `src/core/meshbuilder.{h,cpp}`; Test `tests/test_formas.cpp`

**Interfaces — Produces:**
```cpp
enum class TipoForma : uint8_t { Caixa, Prisma, Cilindro };
struct FormaCena { TipoForma tipo; Vec3f centro; Vec3f meia; float rotY; Rgb cor; };  // cilindro: meia = (raio, meia altura, raio), centro no meio
float distanciaForma(const FormaCena& f, Vec3f p);           // exata para caixa e cilindro; prisma: limite inferior (nunca maior que a real)
float distanciaCena(const std::vector<FormaCena>& fs, Vec3f p);
// MeshBuilder: void registrar(std::vector<FormaCena>* destino);  // null = nao registra; caixa/prisma/cilindro passam a anexar sua forma
```
- [ ] **Step 1: Testes** `formas: caixa girada` (caixa 4×3×2 girada 0,7 rad: distância 0 nos 8 cantos ±1e-4, = 1 a 1 m da face na normal girada, < 0 no centro = −1 (meia menor)); `formas: prisma` (ponto na cumeeira → 0; 1 m acima da cumeeira → ≤ 1 e > 0,7; dentro → < 0); `formas: cilindro` (lateral e topo); `formas: o meshbuilder registra o que desenha` (MeshBuilder com registrar: caixa girada + prisma + cilindro; todo vértice gerado tem `|distanciaCena| < 0.002`); `formas: sem registrar nao muda nada` (vértices idênticos com/sem registrar).
- [ ] **Step 2:** falham. **Step 3:** implementar (prisma = interseção dos 2 semiplanos das águas com a caixa de base, limitada). **Step 4:** passam + suíte. **Step 5:** commit `feat: formas analiticas das construcoes`.

### Task 2: Paredes marcadas e lista de formas do cenário

**Files:** Modify `src/core/meshbuilder.h` (tipos), `src/render/props.{h,cpp}`; Test `tests/test_formas.cpp`

**Interfaces — Consumes:** Task 1. **Produces:**
```cpp
constexpr float TIPO_REBOCO = 5.f, TIPO_MADEIRA = 6.f, TIPO_PEDRA = 7.f;   // meshbuilder.h (4 = grama ja existe)
struct JanelaLuz { Vec3f pos, normal; };
// core/formas.h:
std::vector<int> formasPerto(const std::vector<FormaCena>& fs, Vec3f foco, float raio, int max);  // indices ordenados por distancia ate a forma
// Cenario: const std::vector<FormaCena>& formas() const; const std::vector<JanelaLuz>& janelas() const;
```
- `Gerador::casa`: parede → tipo pelo material (REBOCO/REBOCO2 → REBOCO, MADEIRA → MADEIRA, PEDRA → PEDRA), alicerce → PEDRA, vigas/diagonais → MADEIRA; telhado, chaminé, porta: tipo atual. Prédios especiais e paredes do estábulo também. Registra formas de parede, telhado e chaminé; muro/torres da cidade registrados (só como bloqueadores). Janelas viram `JanelaLuz` (centro e normal da face).
- [ ] **Step 1: Teste** `formas perto: ordem e limite` (10 caixas em linha; foco no início; max 4 → os 4 primeiros em ordem; foco a 0,1 m de diferença → mesma lista).
- [ ] **Step 2–4:** RED/GREEN; capturas antes/depois no vilarejo (`--pos -45 75 --hora 16`) **idênticas** (só marcação, sem mudança visual ainda: o shader ignora tipos 5–7 por enquanto).
- [ ] **Step 5:** commit `feat: paredes marcadas e formas do cenario`.

### Task 3: Relevo das paredes (pixel a pixel)

**Files:** Modify `src/render/shaders.cpp` (`FS_LIT`, `VS_LIT`), `src/render/lighting.{h,cpp}`, `src/game/depuracao.cpp`, `src/main.cpp`, `src/game/jogo.{h,cpp}`

**Interfaces — Produces:** uniforms `paredes` (0 desliga, 1 Médio, 2 Alto), `Iluminacao::Depuracao::semParedes`, opção `--sem-paredes`.
- Coordenadas da face: `t = normalize(cross(vec3(0,1,0), n))` (n horizontal), `uv = vec2(dot(p, t), p.y)`; face de cima/baixo sem relevo.
- Campos de altura `hReboco/hMadeira/hPedra(uv)` em metros ∈ [−0,03, 0] e cor por material (reboco caído mostra pedra; sujeira até 0,4 m acima do chão = `p.y` − altura do alicerce passada por `extra.x`? → usar `ao` do vértice, ver ruling se faltar).
- Raio no relevo: ≤ 24 passos (Alto) / 12 (Médio) + 2 de bisseção, ao longo de `-v` projetado; normal por diferenças do campo; sombra própria do relevo ≤ 10 passos até `dirSombra` (só Alto). Some com a distância (25 m relevo, 45 m normal).
- [ ] **Step 1:** implementar. **Step 2:** F1 "Paredes com raytracing" + tempo de quadro. **Step 3: Capturas** (Alto, comparar `--sem-paredes`): de perto 2 m, rua 10 m, encostado rasante (Review 1), casa girada (Review 2), Médio. **Step 4:** suíte + benchmark Baixo. **Step 5:** commit `feat: relevo das paredes`.

### Task 4: Luz traçada contra as construções

**Files:** Modify `src/render/shaders.cpp`, `src/render/lighting.{h,cpp}`, `src/game/jogo.cpp`

**Interfaces — Consumes:** Tasks 1–3. **Produces:** `void Iluminacao::definirFormas(const std::vector<FormaCena>& fs, const std::vector<int>& idx, const std::vector<JanelaLuz>& janelas, Vec3f foco)` (uniforms `formaA[32]` = centro.xyz+rot, `formaB[32]` = meia.xyz+tipo, `formaCor[32]`, `numFormas`, `janelaP[12]`, `janelaN[12]`, `numJanelas`).
- Jogo: a cada quadro `formasPerto(cenario.formas(), camera, 70, 32)` e as 12 janelas mais próximas → `definirFormas`.
- Shader (só fragmentos de parede): `mapaCena(p)` = mínimo das formas (portar `distanciaForma`); sombra suave até `dirSombra` (≤ 24 passos, penumbra pelo menor afastamento, partida afastada da parede 2 cm) — no Alto substitui o mapa de sombras dentro do alcance; oclusão 5 amostras pela normal + chão; rebote: 5 direções fixas no hemisfério (giradas pela face), raio ≤ 12 m, se acerta forma soma `formaCor × max(dot(nAcerto, dirSombra),0) × corSol × k`; janelas acesas (à noite): `corJanela × queda(dist) × max(dot(n, dirJanela),0)`. Fade 40→70 m.
- [ ] **Step 1:** implementar. **Step 2: Capturas** (comparar `--sem-paredes`): beco entre casas, pôr do sol, rua de meio-dia, noite `--hora 22` (Review 4), Médio e Baixo (Review 5). **Step 3:** suíte, benchmark Baixo, build Windows e web (web sem erro de shader no Chromium). **Step 4:** versão 0.19.0, README. **Step 5:** commit `feat: luz tracada nas paredes e versao 0.19.0`.
