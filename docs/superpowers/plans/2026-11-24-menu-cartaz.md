# Tela inicial como cartaz — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** o menu inicial vira um cartaz de papel pregado sobre a paisagem da entrada.

**Architecture:** a geometria do cartaz (retângulo girado, posição de cada opção) sai de uma
função pura em `src/core/` testada pelo doctest; o desenho (raylib) fica em `src/game/menu_inicial.cpp`,
usando essa geometria também para o mouse. Nada de textura nova: papel, rasgo, manchas e carimbo
são primitivas 2D com ruído determinístico.

**Tech Stack:** C++17, raylib 5.5, doctest.

**Spec:** `docs/superpowers/specs/2026-11-24-menu-cartaz-design.md`

## Global Constraints
- Mesmas opções e comportamento: Principal (Jogar, Personagem, Configurações, Sair; sem Sair na web),
  Personagem (penteados + Voltar), Configurações (Baixo, Médio, Alto, Voltar); Esc volta; setas, Enter e mouse.
- Só as fontes existentes (`estilo::Fonte::Titulo` = Rye, `Texto`/`Forte` = EB Garamond).
- Sem arquivos de imagem novos.
- Legível de 1280x720 a 3840x2160.

## Review Focus
- Janela redimensionada com o menu aberto: o cartaz e as áreas de clique se recalculam juntos.
- Clique com o cartaz girado: a área de cada opção acompanha o giro (teste de geometria).
- Janela estreita (retrato, ~800x900): o cartaz cabe inteiro na tela.
- Trocar de tela (Principal ↔ submenu) com o mouse parado sobre um item não aciona nada sozinho.
- Carregando ("Carregando o mundo..."): o cartaz continua, sem opções clicáveis.

---

### Task 1: Geometria do cartaz (core, testada)

**Files:**
- Create: `src/core/cartaz.h`, `src/core/cartaz.cpp`
- Test: `tests/test_cartaz.cpp`

**Interfaces:**
- Produces:
  - `struct Cartaz { float cx, cy, largura, altura, angulo; /* graus */ };`
  - `Cartaz cartazNaTela(float larguraTela, float alturaTela, float tempo);` — cartaz à esquerda
    (centro em ~30% da largura), altura ~86% da tela limitada pela largura disponível, ângulo base
    -2° com balanço de ±0,35° (período ~6 s).
  - `float escalaDoCartaz(const Cartaz&);` — `altura / 900` (unidades de desenho → pixels).
  - `Vec2 noCartaz(const Cartaz&, float u, float v);` — ponto em unidades do cartaz (0..640 x 0..900,
    origem no canto superior esquerdo) para a tela, já girado.
  - `Vec2 paraCartaz(const Cartaz&, float x, float y);` — inverso (mouse → unidades do cartaz).
  - `Vec2` = `struct Vec2 { float x, y; };` em `cartaz.h` (core não usa raylib).

- [ ] **Step 1: Testes** — `cartaz: cabe na tela` (para 1280x720, 1920x1080, 3840x2160 e 800x900,
  os quatro cantos de `noCartaz` dentro da tela); `cartaz: ida e volta` (`paraCartaz(noCartaz(u,v))`
  ≈ (u,v) com 0,01 de tolerância, ângulo -2°); `cartaz: balanço pequeno` (|angulo + 2| ≤ 0,36 para
  tempos 0..20 s).
- [ ] **Step 2:** rodar `./build/testes.exe --test-case='cartaz*'` — falha (não compila).
- [ ] **Step 3:** implementar em `src/core/cartaz.cpp`.
- [ ] **Step 4:** rodar de novo — PASSA.

### Task 2: Desenho do cartaz e menu sobre ele

**Files:**
- Modify: `src/game/menu_inicial.cpp`, `src/game/menu_inicial.h`

**Interfaces:**
- Consumes: Task 1 (`cartazNaTela`, `noCartaz`, `paraCartaz`, `escalaDoCartaz`).
- Produces: `MenuInicial::item(int)` passa a devolver o retângulo do item em unidades do cartaz;
  o clique testa `paraCartaz(mouse)` contra ele.

- [ ] **Step 1:** fundo — foto atual com tom sépia leve, vinheta e grão (ruído por quadro).
- [ ] **Step 2:** papel — quadrilátero girado (via `rlPushMatrix`/`rlRotatef` em torno do centro),
  cor de papel envelhecido, borda irregular (polígono com ruído determinístico), manchas de umidade
  (círculos translúcidos), sombra projetada, prego no topo.
- [ ] **Step 3:** conteúdo em tinta escura: "AVISO", "Caçador" (Rye), "de recompensas", filetes;
  título da folha nos submenus ("Escolha o cabelo", "Qualidade gráfica").
- [ ] **Step 4:** opções; a focada recebe círculo de tinta desenhado à mão que se completa em 0,2 s;
  a escolha salva leva um tique carimbado; carimbo vermelho torto "VÁLIDO EM TODO O OESTE".
- [ ] **Step 5:** dica de teclas fora do cartaz; "Carregando o mundo..." escrito no cartaz.
- [ ] **Step 6:** verificar — compilar e capturar o menu em 1280x720, 1920x1080 e 800x900
  (opção `--menu` nova em `main.cpp`: abre no menu e permite `--screenshot`); conferir os itens do
  Review Focus à mão (clique em item girado, redimensionar, trocar de tela).
- [ ] **Step 7:** rodar todos os testes; commit.
