# Paredes baratas — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Mesmo visual das paredes com raytracing, mas com o material assado em texturas na abertura e a luz traçada guardada por face e recalculada aos poucos.

**Architecture:** Funções de material periódicas (4 m) num shader de "assar" desenhado uma vez em RenderTextures; atlas de luz por face de parede atualizado em rodízio por um shader que reaproveita `sombraCena/oclusaoCena/reboteCena`; o FS das construções lê texturas em vez de calcular.

**Tech Stack:** C++17, raylib 5.5, GLSL 330, doctest.

**Spec:** `docs/superpowers/specs/2026-11-22-paredes-cache-design.md`

## Global Constraints
- Sem mudança visual perceptível; sem arquivos; Baixo e web iguais.
- Material: 3 × (A, B) 2048², 4 m de período, mipmaps. Luz: atlas 2048², 0,25 m/texel, borda 1 texel, 48 faces/quadro, raio 70 m.
- Testes atuais passando; versão 0.19.1.

## Review Focus
1. Costura na repetição de 4 m ou entre faces vizinhas da mesma casa → nenhuma linha visível. Task 2/3: capturas de perto numa quina e numa parede longa.
2. Sol andando (velocidade do tempo alta no F1) → a luz guardada acompanha sem "saltos" grandes. Task 3: captura com `--hora` diferentes + revisão do rodízio.
3. Casa entrando no raio de 70 m (andando/teleporte) → face calculada na hora, nunca preta. Task 3: face nova vai para o início da fila.
4. Atlas cheio (cidade grande) → faces sem lugar caem no caminho por pixel atual, sem lixo. Task 1: teste de atlas cheio.
5. Trocar nível no F1 / janela redimensionada → texturas continuam válidas. Task 3: manual.

---

### Task 1: Faces de parede e atlas (core)

**Files:** Modify `src/core/formas.{h,cpp}`, `src/core/meshbuilder.{h,cpp}`; Test `tests/test_formas.cpp`

**Produces:**
```cpp
// FormaCena ganha: bool parede = false;  (caixa registrada com tipo de parede 5–7)
// Vert ganha: int forma = -1;  (indice da forma registrada que gerou o vertice)
struct FaceParede { Vec3f origem, eixoU, normal; float largura, altura; };   // origem = canto de baixo, eixoU horizontal
FaceParede faceDe(const FormaCena& caixa, int k);   // k: 0 +x, 1 -x, 2 +z, 3 -z (local)
struct RetAtlas { int x = 0, y = 0, w = 0, h = 0; };  // w == 0: sem lugar
std::vector<std::array<RetAtlas, 4>> alocarAtlas(const std::vector<FormaCena>& fs, float metrosPorTexel, int tam);  // so formas parede; borda de 1 texel
```
- [ ] Testes: `faces: cantos batem com a caixa` (caixa girada 0,7: os 4 cantos de cada face são vértices da caixa ±1e-4; normal para fora); `atlas: sem sobreposicao e com borda` (20 caixas parede: retângulos dentro de [0,tam), não se tocam (1 texel de folga), w = ceil(largura/0,25)+2); `atlas: cheio nao inventa lugar` (tam pequeno → algumas faces w==0, nenhuma fora); `meshbuilder: vertices de parede levam o indice da forma`.
- [ ] RED → implementar (prateleiras por altura) → GREEN → commit `feat: faces de parede e atlas de luz`.

### Task 2: Material assado em texturas

**Files:** Modify `src/render/shaders.{h,cpp}` (`FS_ASSAR_MATERIAL`, bloco PAREDES), `src/render/lighting.{h,cpp}`

**Produces:** `Iluminacao` gera na carga (desktop) `texMatA_[3]`, `texMatB_[3]` 2048² com mipmaps; uniforms `matA0..2`, `matB0..2` (amostradores) no shader das construções.
- Funções de material periódicas (lattice com `mod` no período) no shader de assar; o FS das construções usa `hParede(uv)` = leitura de A.a da textura do material em `uv/4`, e cor = `mix(base·A.rgb, B.rgb, B.a)` (madeira: dessaturação antes).
- [ ] Implementar; capturas antes/depois (vilarejo 10 h/16 h, cidade 17 h, quina de perto): mesmo aspecto; sem costura. Suíte verde. Commit `perf: material das paredes assado em texturas`.

### Task 3: Luz traçada guardada por face

**Files:** Modify `src/render/meshconv.cpp` (texcoord2.x = forma+1), `src/render/shaders.cpp` (`VS_LIT` repassa, `FS_LUZ_PAREDE`, PAREDES lê atlas), `src/render/lighting.{h,cpp}`, `src/render/props.{h,cpp}`, `src/game/jogo.cpp`

**Produces:** `Iluminacao::atualizarLuzParedes(const std::vector<FormaCena>&, const std::vector<std::array<RetAtlas,4>>&, Vec3f camera, int porQuadro = 48)`; tabela das faces em textura float (por forma: 4 retângulos + centro/rot/meia); atlas L1/L2.
- Rodízio: fila das faces de parede a < 70 m; novas primeiro. Cada face: listar formas perto (formasPerto do centro, 32), desenhar o retângulo no L1 (sombra/oclusão) e no L2 (rebote). Borda: clamp do uv ao retângulo − ½ texel.
- FS das construções: fragmento de parede com forma ≥ 0 e face com lugar lê L1/L2; senão, caminho atual por pixel.
- [ ] Implementar; capturas: vilarejo/cidade/noite iguais às de antes; teleporte + 1 quadro sem preto; sol andando. Benchmark Baixo, Windows e web. Versão 0.19.1, commit `perf: luz das paredes guardada por face`.
