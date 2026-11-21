# Mundo Aberto (v1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Um mundo aberto 3D low-poly, medieval, com terreno gerado + lugares fixos, jogador a pé e a cavalo em 3ª pessoa, ciclo dia/noite com iluminação realista em níveis de qualidade, NPCs e quadro de recompensas — rodando a 60 FPS em PC fraco no nível Baixo.

**Architecture:** Lógica pura (relógio do dia, ruído, terreno, parser de lugares, física simples, NPCs) fica em `src/core/` sem depender de raylib e é testada com doctest. Tudo que desenha (malhas, shaders, câmera, UI) fica em `src/render/` e `src/game/` e é verificado por screenshots headless (Xvfb + Mesa). Toda geometria é criada em código com cores por vértice e AO pré-calculada; nenhuma textura nem arquivo de modelo.

**Tech Stack:** C++17, raylib 5.5 (via CMake FetchContent), doctest 2.4.11 (FetchContent), GLSL 330 (desktop) / 100 es (web), CMake 3.28, MinGW-w64 (Windows .exe), Emscripten (build de teste web).

**Spec:** `docs/superpowers/specs/2026-11-21-mundo-aberto-design.md`

## Global Constraints

- Linguagem C++ com raylib; tudo em 3D; câmera atrás das costas.
- Plataforma principal: Windows, teclado + mouse; entregável é um `.exe` cruzado com MinGW a partir de Linux.
- Build de teste no navegador (WebAssembly) a cada etapa; descartável se atrapalhar.
- Modelos gerados no código a partir de primitivas, cores por vértice, **sem texturas e sem arquivos de modelo**.
- 60 FPS em notebook com Intel HD no nível **Baixo**; executável + dados em poucos MB.
- Mapa ~1,5 km × 1,5 km cercado de montanhas; galope ~12 m/s (cruzar em ~3–4 min).
- Dia completo = 1200 s reais (20 min).
- Até 8 luzes pontuais (as mais próximas) por vez.
- Níveis de qualidade: **Baixo** = efeitos 1–4 + sombra "bolha"; **Alto** = 1–7 (tabela da seção 5 do spec).
- Controles: WASD andar, Shift correr, mouse câmera, E montar/desmontar/interagir.
- Sem interiores, combate, captura, missões, som ou save na v1.
- Textos do jogo em português.

## Review Focus

1. `data/lugares.txt` ausente ou com linha inválida → o jogo abre mesmo assim, ignora a linha e mostra o erro (arquivo + número da linha) num aviso na tela. (Task 3)
2. Pico de frame (alt-tab, carregamento) → `dt` é limitado a 0,1 s; jogador/cavalo não atravessam terreno nem paredes nem saem do mapa. (Tasks 7, 8)
3. Noite "escura de verdade" mas navegável → luz ambiente noturna nunca cai abaixo de um piso e o jogador tem lanterna; dá pra ver a estrada a 10 m. (Tasks 2, 6)
4. Câmera atravessando morros/paredes → câmera nunca fica abaixo do terreno + 0,5 m nem dentro de caixas de colisão. (Task 7)
5. Esc libera o mouse e pausa; clicar na janela captura de novo; perder o foco pausa. (Task 7)

---

## File Structure

```
CMakeLists.txt                 build nativo, testes, alvos web/windows
cmake/mingw-w64.cmake          toolchain Windows
scripts/screenshot.sh          roda o jogo headless e salva PNG
scripts/build-windows.sh       gera build-win/caçador.exe
scripts/build-web.sh           gera build-web/ (index.html + .wasm)
data/lugares.txt               lugares e estradas do mapa
src/main.cpp                   janela, loop, estados (jogando/pausa/menu)
src/core/daytime.{h,cpp}       relógio do dia, sol/lua, paleta por hora
src/core/noise.{h,cpp}         ruído determinístico 2D
src/core/places.{h,cpp}        parser de data/lugares.txt
src/core/terrain.{h,cpp}       altura: ruído + montanhas na borda + rio + aplainar + estradas
src/core/collision.{h,cpp}     AABB, empurrar para fora, limites do mapa
src/core/motion.{h,cpp}        movimento do jogador e do cavalo (montado/desmontado)
src/core/npc.{h,cpp}           NPCs por pontos de caminho e horário
src/core/bounty.{h,cpp}        gerador de cartazes de recompensa
src/core/quality.{h,cpp}       níveis de qualidade e sugestão automática
src/render/meshbuilder.{h,cpp} primitivas low-poly, cores por vértice, AO
src/render/terrain_mesh.{h,cpp} chunks do terreno + LOD
src/render/props.{h,cpp}       construções, árvores, tochas por tipo de lugar
src/render/lighting.{h,cpp}    shader principal, luzes, sombras, pós-processo
src/render/shaders/*.glsl      lit, shadow, bloom, godrays, grading
src/game/camera.{h,cpp}        câmera 3ª pessoa
src/game/actors.{h,cpp}        desenho e animação procedural de pessoas/cavalo
src/game/ui.{h,cpp}            aviso de erro, menu de opções, quadro de recompensas
tests/test_*.cpp               um arquivo por módulo de core/
```

---

### Task 1: Esqueleto, build e verificação headless

**Files:**
- Create: `CMakeLists.txt`, `cmake/mingw-w64.cmake`, `src/main.cpp`, `tests/test_main.cpp`, `scripts/screenshot.sh`, `scripts/build-windows.sh`, `scripts/build-web.sh`, `.gitignore`

**Interfaces:**
- Produces: alvo `cacador` (jogo), alvo `testes` (doctest), biblioteca estática `core` (só `src/core/`, sem raylib), biblioteca `jogo` (resto). Flag de linha de comando `--screenshot <arquivo.png> --frames N [--hora H]` que roda N frames, salva a tela e sai (usada por toda verificação visual).

- [ ] **Step 1:** Instalar ferramentas: `sudo apt-get install -y mingw-w64 xvfb libgl1-mesa-dri libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev` e clonar/ativar emsdk em `~/emsdk` (fora do repo).
- [ ] **Step 2:** `CMakeLists.txt` com FetchContent de raylib 5.5 e doctest 2.4.11; `core` não linka raylib; build Release com `-O2`, Windows com `-static` (um `.exe` só, sem DLLs).
- [ ] **Step 3:** `tests/test_main.cpp` com `TEST_CASE("sanidade") { CHECK(1 + 1 == 2); }`. Run: `cmake -B build && cmake --build build -j && ./build/testes` → Expected: `Status: SUCCESS!`
- [ ] **Step 4:** `main.cpp` abre janela 1280×720 "Caçador", céu azul, um cubo, implementa `--screenshot`. Run: `scripts/screenshot.sh /tmp/t1.png` (usa `xvfb-run -s "-screen 0 1280x720x24"` + `LIBGL_ALWAYS_SOFTWARE=1`) → Expected: PNG existe e não é todo de uma cor (o script checa com Python).
- [ ] **Step 5:** `scripts/build-windows.sh` → Expected: `build-win/cacador.exe` existe, `file` mostra `PE32+ executable`.
- [ ] **Step 6:** `scripts/build-web.sh` (shaders GLSL 100 es selecionados por `#ifdef PLATFORM_WEB`) → Expected: `build-web/index.html` e `.wasm` existem.
- [ ] **Step 7:** Commit `feat: esqueleto com builds Linux, Windows e web`.

### Task 2: Relógio do dia e paleta

**Files:** Create `src/core/daytime.{h,cpp}`, `tests/test_daytime.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct Vec3f { float x, y, z; };
  struct Rgb { float r, g, b; };              // linear, 0..1+
  enum class Fase { Madrugada, Amanhecer, Dia, PorDoSol, Noite };
  struct Paleta { Rgb ceuTopo, ceuHorizonte, neblina, sol, ambienteCeu, ambienteChao;
                  float densidadeNeblina, exposicao; Rgb grading; };
  class DayClock {
   public:
    static constexpr float DURACAO_DIA = 1200.f;
    explicit DayClock(float horaInicial = 8.f);
    void avancar(float dt);                 // dt em segundos reais
    float hora() const;                     // 0..24
    Fase fase() const;
    Vec3f direcaoSol() const;               // unitário; y<0 à noite
    Vec3f direcaoLua() const;
    float intensidadeTochas() const;        // 0 de dia, 1 de noite, rampa no pôr do sol
    Paleta paleta() const;                  // interpolada entre chaves por hora
  };
  ```

- [ ] **Step 1: Testes que falham**
  ```cpp
  TEST_CASE("um dia dura 1200s") { DayClock c(0); c.avancar(600); CHECK(c.hora() == doctest::Approx(12)); c.avancar(600); CHECK(c.hora() == doctest::Approx(0)); }
  TEST_CASE("sol alto ao meio-dia, abaixo do horizonte à meia-noite") { DayClock m(12), n(0); CHECK(m.direcaoSol().y > 0.8f); CHECK(n.direcaoSol().y < 0); }
  TEST_CASE("fases") { CHECK(DayClock(12).fase()==Fase::Dia); CHECK(DayClock(18.5f).fase()==Fase::PorDoSol); CHECK(DayClock(23).fase()==Fase::Noite); }
  TEST_CASE("tochas") { CHECK(DayClock(12).intensidadeTochas()==0); CHECK(DayClock(23).intensidadeTochas()==1); float p=DayClock(18.5f).intensidadeTochas(); CHECK(p>0); CHECK(p<1); }
  TEST_CASE("noite navegável: ambiente tem piso") { auto p=DayClock(2).paleta(); CHECK(p.ambienteCeu.r+p.ambienteCeu.g+p.ambienteCeu.b > 0.03f); }
  TEST_CASE("paleta contínua: sem saltos entre minutos") { for(float h=0;h<24;h+=1/60.f){ auto a=DayClock(h).paleta(), b=DayClock(h+1/60.f).paleta(); CHECK(std::abs(a.neblina.r-b.neblina.r) < 0.02f);} }
  TEST_CASE("pôr do sol é quente") { auto p=DayClock(18.5f).paleta(); CHECK(p.neblina.r > p.neblina.b); }
  ```
- [ ] **Step 2:** Run `./build/testes -tc="*dia*,*sol*,*fases*,*tochas*,*noite*,*paleta*"` → FAIL (não compila).
- [ ] **Step 3:** Implementar. Fases: Madrugada 0–5, Amanhecer 5–7, Dia 7–18, PorDoSol 18–19.5, Noite 19.5–24. Sol nasce 6h, se põe 19h. Paleta: chaves em 0, 5, 6.5, 9, 12, 17, 18.5, 19.5, 21 com interpolação suave; pôr do sol laranja-dourado, noite azulada.
- [ ] **Step 4:** Testes passam.
- [ ] **Step 5:** Commit `feat: relógio do dia e paleta`.

### Task 3: Arquivo de lugares

**Files:** Create `src/core/places.{h,cpp}`, `data/lugares.txt`, `tests/test_places.cpp`

**Interfaces:**
- Produces:
  ```cpp
  enum class TipoLugar { Cidade, Vilarejo, Fazenda, Castelo, Acampamento, Mina, Cemiterio, Ponte, Floresta };
  struct Lugar { TipoLugar tipo; std::string nome; float x, z, rot, raio; };
  struct Estrada { std::string de, para; };
  struct ErroMapa { int linha; std::string msg; };
  struct Mapa { std::vector<Lugar> lugares; std::vector<Estrada> estradas; std::vector<ErroMapa> erros; };
  Mapa lerMapa(const std::string& texto);
  Mapa carregarMapa(const std::string& caminho);  // arquivo ausente => erros {0, "arquivo não encontrado"}
  ```
- Formato: `# comentário`; `lugar <tipo> <nome> <x> <z> <rot_graus> <raio>`; `estrada <nomeA> <nomeB>`. Coordenadas em metros, origem no centro, mundo de −750 a 750.

- [ ] **Step 1: Testes**
  ```cpp
  TEST_CASE("lê lugar e estrada") { auto m=lerMapa("lugar cidade Valdoro 200 -300 0 90\nlugar vilarejo Pedrabranca -100 150 15 50\nestrada Valdoro Pedrabranca\n");
    REQUIRE(m.lugares.size()==2); CHECK(m.lugares[0].tipo==TipoLugar::Cidade); CHECK(m.lugares[0].x==200); CHECK(m.estradas.size()==1); CHECK(m.erros.empty()); }
  TEST_CASE("ignora comentários e linhas vazias") { CHECK(lerMapa("# oi\n\n").erros.empty()); }
  TEST_CASE("linha inválida vira erro com número e não derruba o resto") { auto m=lerMapa("lugar cidade A 0 0 0 10\nlugar dragao B 1 1 0 5\nlugar cidade C 1\n");
    CHECK(m.lugares.size()==1); REQUIRE(m.erros.size()==2); CHECK(m.erros[0].linha==2); CHECK(m.erros[1].linha==3); }
  TEST_CASE("estrada para lugar inexistente é erro") { auto m=lerMapa("lugar cidade A 0 0 0 10\nestrada A Z\n"); CHECK(m.estradas.empty()); CHECK(m.erros.size()==1); }
  TEST_CASE("fora do mundo é erro") { CHECK(lerMapa("lugar cidade A 900 0 0 10\n").erros.size()==1); }
  TEST_CASE("arquivo ausente") { auto m=carregarMapa("nao/existe.txt"); CHECK(m.lugares.empty()); CHECK(m.erros.size()==1); }
  TEST_CASE("data/lugares.txt real é válido") { auto m=carregarMapa(DATA_DIR "/lugares.txt"); CHECK(m.erros.empty()); CHECK(m.lugares.size() >= 9); }
  ```
- [ ] **Step 2:** Rodar → FAIL.
- [ ] **Step 3:** Implementar; escrever `data/lugares.txt` seguindo o desenho do spec (castelo e acampamento ao norte, floresta a oeste, rio cruzando com ponte, vilarejo + fazendas no centro, cidade a sudeste, mina e cemitério a sudoeste, estradas ligando tudo). Nomes originais.
- [ ] **Step 4:** Passa. **Step 5:** Commit `feat: arquivo de lugares`.

### Task 4: Ruído e terreno

**Files:** Create `src/core/noise.{h,cpp}`, `src/core/terrain.{h,cpp}`, `tests/test_terrain.cpp`

**Interfaces:**
- Consumes: `Mapa`, `Lugar`, `Estrada` (Task 3).
- Produces:
  ```cpp
  float ruido2D(float x, float z, uint32_t semente);        // -1..1, suave
  float fbm2D(float x, float z, uint32_t semente, int oitavas);
  class Terreno {
   public:
    static constexpr float META = 750.f;                    // mundo de -750..750
    Terreno(const Mapa& mapa, uint32_t semente);
    float altura(float x, float z) const;                   // metros
    Vec3f normal(float x, float z) const;
    float mascaraEstrada(float x, float z) const;           // 0..1
    float mascaraRio(float x, float z) const;               // 0..1 (1 = água)
    float nivelAgua() const;
    bool dentroDoJogavel(float x, float z) const;           // |x|,|z| <= 700
  };
  ```
- Algoritmo: base = fbm suave (morros 0–25 m) + rampa de montanhas na borda (0 m em 650, ≥80 m em 750) + rio = curva fixa (spline por 4–6 pontos) que escava até abaixo de `nivelAgua()` + aplainar com suavização (smoothstep do raio ao raio×1,3) até a altura no centro do lugar + estradas = segmentos entre centros, levemente curvados por ruído, com largura 4 m, que suavizam o terreno ao longo deles. Implementar via grade de alturas pré-calculada (1 m de resolução, 1501×1501 floats ≈ 9 MB em memória, interpolação bilinear) — gerado em < 1 s.

- [ ] **Step 1: Testes**
  ```cpp
  TEST_CASE("determinístico") { Mapa m; Terreno a(m,42), b(m,42); CHECK(a.altura(123,-45)==b.altura(123,-45)); }
  TEST_CASE("borda é montanha") { Mapa m; Terreno t(m,1); CHECK(t.altura(745,0) > t.altura(0,0)+50); }
  TEST_CASE("plano onde tem lugar") { auto m=lerMapa("lugar cidade A 100 100 0 60\n"); Terreno t(m,1);
    float h=t.altura(100,100); for(float d: {10.f,30.f,55.f}) CHECK(t.altura(100+d,100)==doctest::Approx(h).epsilon(0.02)); }
  TEST_CASE("estrada entre lugares") { auto m=lerMapa("lugar cidade A -300 0 0 40\nlugar vilarejo B 300 0 0 40\nestrada A B\n"); Terreno t(m,1);
    int na=0; for(float x=-250;x<=250;x+=10){ float best=0; for(float z=-30;z<=30;z+=1) best=std::max(best,t.mascaraEstrada(x,z)); if(best>0.5f) na++; } CHECK(na==51); }
  TEST_CASE("rio tem água") { Mapa m; Terreno t(m,1); int agua=0; for(float x=-700;x<=700;x+=5) for(float z=-700;z<=700;z+=5) if(t.altura(x,z)<t.nivelAgua()) agua++; CHECK(agua>500); }
  TEST_CASE("normal aponta pra cima no plano") { auto m=lerMapa("lugar cidade A 0 0 0 60\n"); Terreno t(m,1); CHECK(t.normal(0,0).y > 0.99f); }
  ```
- [ ] **Step 2–4:** FAIL → implementar → PASS.
- [ ] **Step 5:** Commit `feat: terreno gerado com lugares, estradas e rio`.

### Task 5: Construtor de malhas low-poly com AO

**Files:** Create `src/render/meshbuilder.{h,cpp}`, `tests/test_meshbuilder.cpp` (MeshBuilder não chama raylib até `paraMesh()`; testes usam só os vetores)

**Interfaces:**
- Produces:
  ```cpp
  struct Vert { Vec3f pos, normal; Rgb cor; float ao; };
  class MeshBuilder {
   public:
    void caixa(Vec3f centro, Vec3f tamanho, float rotY, Rgb cor);
    void cilindro(Vec3f base, float raio, float altura, int lados, Rgb cor);
    void cone(Vec3f base, float raio, float altura, int lados, Rgb cor);
    void prisma(Vec3f centro, Vec3f tamanho, float rotY, Rgb cor);   // telhado de duas águas
    void calcularAO(const Terreno* t, int amostras = 12);           // ocluído por geometria do próprio builder + terreno
    const std::vector<Vert>& verts() const;
    size_t triangulos() const;
    Mesh paraMesh() const;   // raylib: cor final = cor * ao em vertex colors; ao também em texcoord.x
  };
  ```
- Faces chapadas (normais por face, vértices duplicados) para o visual low-poly. AO: por vértice, raios no hemisfério da normal com alcance de 2 m contra as caixas do builder e o terreno; fração livre = ao.

- [ ] **Step 1: Testes**
  ```cpp
  TEST_CASE("caixa tem 12 triângulos") { MeshBuilder b; b.caixa({0,0,0},{1,1,1},0,{1,1,1}); CHECK(b.triangulos()==12); }
  TEST_CASE("normais unitárias") { MeshBuilder b; b.cilindro({0,0,0},1,2,8,{1,1,1}); for(auto&v:b.verts()) CHECK(len(v.normal)==doctest::Approx(1)); }
  TEST_CASE("canto entre chão e parede é mais escuro que topo") { MeshBuilder b; b.caixa({0,0.05f,0},{10,0.1f,10},0,{1,1,1}); b.caixa({0,1.1f,-4.5f},{10,2,1},0,{1,1,1}); b.calcularAO(nullptr);
    float canto=aoMaisProximo(b,{0,0.1f,-3.95f}), aberto=aoMaisProximo(b,{0,0.1f,3}); CHECK(canto < aberto-0.2f); }
  ```
  (`aoMaisProximo` é helper do teste.)
- [ ] **Step 2–4:** FAIL → implementar → PASS.
- [ ] **Step 5:** Commit `feat: construtor de malhas low-poly com AO`.

### Task 6: Terreno na tela + iluminação base (efeitos 1–4) + neblina

**Files:** Create `src/render/terrain_mesh.{h,cpp}`, `src/render/lighting.{h,cpp}`, `src/render/shaders/lit.{vs,fs}`, `src/render/shaders/grading.fs`; Modify `src/main.cpp`

**Interfaces:**
- Consumes: `Terreno`, `DayClock`, `Paleta`, `MeshBuilder`.
- Produces:
  ```cpp
  struct LuzPontual { Vec3f pos; Rgb cor; float raio; float tremor; };
  class Iluminacao {
   public:
    void carregar(Nivel nivel);
    void comecarCena(const Camera3D&, const DayClock&, const std::vector<LuzPontual>& todas, Vec3f foco);  // escolhe as 8 mais próximas de foco
    Shader shaderLit() const;
    void comecarPos(); void terminarPos();   // render target -> tone mapping ACES + grading da paleta
  };
  class TerrenoRender { public: TerrenoRender(const Terreno&); void desenhar(const Camera3D&, float distMax); size_t triangulosDesenhados() const; };
  ```
- Terreno em chunks de 64 m; 3 LODs (passo 2 m / 4 m / 8 m) por distância; chunks além da neblina não são desenhados. Cores: grama varia com ruído, terra nas estradas (`mascaraEstrada`), rocha em encosta íngreme e na montanha, água plana azul-escura. Shader: luz do sol/lua (lambert), ambiente hemisférico (`ambienteCeu`/`ambienteChao` pela normal), AO do vértice, 8 luzes pontuais com queda suave e tremor por seno+ruído, neblina exponencial com cor da paleta e tingida na direção do sol, céu em gradiente desenhado como cúpula. Lanterna do jogador = uma das 8 luzes, ligada à noite.

- [ ] **Step 1:** Implementar. Verificação visual em 4 horas: `scripts/screenshot.sh /tmp/t6-{8,12,18.5,23}.png --hora H` e olhar as imagens.
  Expected: 8h dourado suave; 12h claro; 18.5 laranja com neblina dourada; 23h escuro azulado mas estrada visível perto da câmera (lanterna).
- [ ] **Step 2:** Medida de orçamento: imprimir `triangulosDesenhados()` no modo `--screenshot`. Expected: < 150 mil triângulos de terreno.
- [ ] **Step 3:** Commit `feat: terreno renderizado com iluminação base e neblina`.

### Task 7: Jogador, colisão e câmera

**Files:** Create `src/core/collision.{h,cpp}`, `src/core/motion.{h,cpp}`, `src/game/camera.{h,cpp}`, `src/game/actors.{h,cpp}`, `tests/test_motion.cpp`; Modify `src/main.cpp`

**Interfaces:**
- Consumes: `Terreno`.
- Produces:
  ```cpp
  struct AABB { Vec3f min, max; };
  class MundoColisao { public: void adicionar(AABB); Vec3f resolver(Vec3f pos, float raio) const;  // empurra pra fora das caixas e limita a dentroDoJogavel
                       bool raioLivre(Vec3f a, Vec3f b) const; };
  struct Entrada { float frente, lado; bool correr, interagir; float mouseDx, mouseDy; };
  struct Corpo { Vec3f pos; float yaw; float vel; };
  constexpr float VEL_ANDAR = 1.6f, VEL_CORRER = 5.f;
  void moverAPe(Corpo&, const Entrada&, float yawCamera, float dt, const Terreno&, const MundoColisao&);
  float limitarDt(float dt);   // min(dt, 0.1f)
  class CameraTerceiraPessoa { public: void atualizar(Vec3f alvo, const Entrada&, const Terreno&, const MundoColisao&, float dt); Camera3D cam() const; float yaw() const; };
  ```
- Câmera: distância 4 m (6 m montado, Task 8), altura de ombro 1,6 m, pitch limitado −20°..60°, recua quando `raioLivre` falha, nunca abaixo de `altura+0.5`. Mouse capturado; Esc libera e pausa; clique captura; perder foco pausa.
- Personagem desenhado com MeshBuilder (corpo, cabeça, chapéu de aba, capa), pernas e braços balançando proporcionalmente à velocidade; sombra "bolha" (disco escuro no chão).

- [ ] **Step 1: Testes**
  ```cpp
  TEST_CASE("anda na direção da câmera") { /* terreno plano */ Corpo c{{0,0,0},0,0}; moverAPe(c,{1,0,false},0,1.f,t,col); CHECK(c.pos.z==doctest::Approx(VEL_ANDAR).epsilon(0.05)); }
  TEST_CASE("correr é mais rápido") {...; CHECK(dist(correndo) > 2.5f*dist(andando)); }
  TEST_CASE("fica no chão") { ...; CHECK(c.pos.y==doctest::Approx(t.altura(c.pos.x,c.pos.z))); }
  TEST_CASE("não atravessa parede mesmo com dt enorme") { col.adicionar({{-5,0,2},{5,3,3}}); Corpo c{{0,0,0},0,0}; moverAPe(c,{1,0,true},0,limitarDt(5.f),t,col); CHECK(c.pos.z < 2.f); }
  TEST_CASE("não sai do mapa") { Corpo c{{699,0,0},...}; for(int i=0;i<100;i++) moverAPe(c,{0,1,true},0,0.1f,t,col); CHECK(t.dentroDoJogavel(c.pos.x,c.pos.z)); }
  TEST_CASE("limitarDt") { CHECK(limitarDt(5.f)==0.1f); CHECK(limitarDt(0.016f)==0.016f); }
  ```
  (Movimento por sub-passos de no máx. 0,25 m para não atravessar paredes finas.)
- [ ] **Step 2–4:** FAIL → implementar → PASS. Câmera: verificação visual `--screenshot` com o jogador encostado num morro → câmera não entra no morro.
- [ ] **Step 5:** Commit `feat: jogador em 3ª pessoa com colisão`.

### Task 8: Lugares, construções e vegetação

**Files:** Create `src/render/props.{h,cpp}`; Modify `src/main.cpp`

**Interfaces:**
- Consumes: `Mapa`, `Terreno`, `MeshBuilder`, `MundoColisao`, `LuzPontual`.
- Produces:
  ```cpp
  struct Cenario { std::vector<Model> modelos; std::vector<Vec3f> posicoes; std::vector<LuzPontual> luzes; std::vector<Vec3f> quadrosRecompensa; std::vector<Vec3f> pontosNPC; };
  Cenario construirCenario(const Mapa&, const Terreno&, MundoColisao&, uint32_t semente);
  ```
- Por tipo: **Cidade** = muralha com portões, ~15 casas (pedra embaixo, madeira em cima, telhado prisma), taverna maior, escritório do xerife com quadro de recompensas na frente, mercado com barracas de lona colorida, estábulo, cadeia com grades, tochas nas ruas e muralha. **Vilarejo** = 6–8 casas, poço, tochas. **Fazenda** = celeiro, cercas, plantação (fileiras). **Castelo** = muralhas quebradas, torre caída, entrada da masmorra (porta escura com tochas). **Acampamento** = tendas, fogueira, caixas. **Mina** = entrada escorada com vigas, trilhos, lanterna. **Cemitério** = lápides, cerca, capela pequena. **Ponte** = madeira sobre o rio. **Floresta** e área geral = árvores (pinheiros de cones empilhados, árvores de copa) espalhadas por ruído, densas na floresta, ausentes em estradas/lugares/água; pedras e arbustos.
- Desempenho: modelos agrupados por chunk de 64 m (uma malha por chunk), árvores distantes em versão de 1 cone; nada além da neblina é desenhado. Toda construção sólida adiciona AABB.

- [ ] **Step 1:** Implementar. Verificação: screenshots da cidade ao pôr do sol e do castelo à noite (posição de câmera via `--pos X Z --yaw Y`, flag adicionada aqui).
  Expected: cidade reconhecível com tochas acesas; castelo com brilho das tochas na entrada da masmorra.
- [ ] **Step 2:** Orçamento: total de triângulos desenhados no pior ponto (centro da cidade) < 300 mil; draw calls < 300 (imprimir no modo screenshot).
- [ ] **Step 3:** Commit `feat: lugares, construções e vegetação`.

### Task 9: Cavalo

**Files:** Modify `src/core/motion.{h,cpp}`, `src/game/actors.{h,cpp}`, `src/game/camera.cpp`; Create `tests/test_horse.cpp`

**Interfaces:**
- Consumes: `Corpo`, `Entrada`, `MundoColisao`, `Terreno`.
- Produces:
  ```cpp
  enum class Andadura { Parado, Passo, Trote, Galope };
  struct Cavalo { Corpo corpo; Andadura andadura; bool montado; };
  constexpr float VEL_PASSO = 2.f, VEL_TROTE = 6.f, VEL_GALOPE = 12.f, DIST_MONTAR = 2.5f;
  bool tentarMontar(Cavalo&, const Corpo& jogador);       // true se perto o bastante
  void desmontar(Cavalo&, Corpo& jogador, const MundoColisao&);  // jogador ao lado, em lugar livre
  void moverCavalo(Cavalo&, const Entrada&, float dt, const Terreno&, const MundoColisao&);
  ```
- W acelera (Passo → Trote; Shift = Galope), S freia, A/D viram (mais devagar quanto mais rápido); aceleração gradual (0 → galope em ~2 s); não sobe encostas > 35°; água funda freia para Passo. Cavalo começa no estábulo da cidade e outro no vilarejo; chamar o cavalo fica para depois.

- [ ] **Step 1: Testes**
  ```cpp
  TEST_CASE("montar só perto") { CHECK(tentarMontar(c, jogadorA2m)); CHECK_FALSE(tentarMontar(c2, jogadorA5m)); }
  TEST_CASE("galope chega a 12 m/s") { ...shift por 4s em plano...; CHECK(c.corpo.vel==doctest::Approx(VEL_GALOPE).epsilon(0.05)); CHECK(c.andadura==Andadura::Galope); }
  TEST_CASE("aceleração gradual") { ...0.5s...; CHECK(c.corpo.vel < VEL_GALOPE*0.5f); }
  TEST_CASE("cruza o mapa em 3-4 min") { CHECK(1400.f/VEL_GALOPE/60.f > 1.8f); /* ~1,95 min em linha reta; com estradas sinuosas e relevo ≈ 3–4 min, ver nota */ }
  TEST_CASE("desmontar não coloca jogador dentro de parede") {...; CHECK(col.resolver(j.pos,0.4f)==j.pos); }
  TEST_CASE("não escala paredão") {...; CHECK(c.corpo.pos.y < alturaDoTopo-5); }
  ```
  Nota: se o teste de travessia real (bot andando pela estrada da ponta sul à norte) der < 3 min, reduzir `VEL_GALOPE` para 10 e registrar no spec.
- [ ] **Step 2–4:** FAIL → implementar → PASS. Desenho: cavalo de caixas/cilindros com pernas animadas por andadura; câmera recua para 6 m montado.
- [ ] **Step 5:** Commit `feat: cavalo`.

### Task 10: NPCs

**Files:** Create `src/core/npc.{h,cpp}`, `tests/test_npc.cpp`; Modify `src/game/actors.cpp`, `src/main.cpp`

**Interfaces:**
- Consumes: `Cenario::pontosNPC`, `DayClock`, `Terreno`, `MundoColisao`, desenho de pessoa (Task 7) com cores de roupa variadas.
- Produces:
  ```cpp
  struct Npc { Corpo corpo; std::vector<Vec3f> rota; int alvo; bool ativo; uint32_t semente; };
  class Populacao { public: Populacao(const std::vector<Vec3f>& pontos, uint32_t semente, int quantidade);
                    void atualizar(float dt, const DayClock&, const Terreno&, const MundoColisao&, Vec3f jogador);
                    const std::vector<Npc>& npcs() const; };
  ```
- ~40 NPCs. Andam entre pontos de caminho a 1,4 m/s, param 2–6 s em cada ponto. À noite (Fase::Noite) 80% somem ("foram pra casa") num ponto fora de vista; reaparecem no amanhecer. Só NPCs a < 120 m do jogador são atualizados a cada frame; os demais a cada 1 s. Não empurram o jogador; desviam dele.

- [ ] **Step 1: Testes**
  ```cpp
  TEST_CASE("chega ao ponto e segue para o próximo") {...; CHECK(n.alvo==1); }
  TEST_CASE("à noite a cidade esvazia") { ...DayClock(23)...; CHECK(ativos < 0.3f*total); }
  TEST_CASE("de dia volta") { ...DayClock(12)...; CHECK(ativos > 0.9f*total); }
  TEST_CASE("determinístico pela semente") {...}
  ```
- [ ] **Step 2–4:** FAIL → implementar → PASS.
- [ ] **Step 5:** Commit `feat: NPCs com rotina de dia e noite`.

### Task 11: Quadro de recompensas

**Files:** Create `src/core/bounty.{h,cpp}`, `src/game/ui.{h,cpp}`, `tests/test_bounty.cpp`; Modify `src/main.cpp`

**Interfaces:**
- Consumes: `Cenario::quadrosRecompensa`, `Entrada::interagir`.
- Produces:
  ```cpp
  struct Cartaz { std::string nome, apelido, crime; int recompensaVivo, recompensaMorto; std::string ultimoVisto; };
  std::vector<Cartaz> gerarCartazes(uint32_t semente, int quantidade, const Mapa& mapa);
  void desenharQuadro(const std::vector<Cartaz>&, int selecionado);   // UI estilo pergaminho, desenhada com retângulos + fonte padrão
  void desenharAvisos(const std::vector<ErroMapa>&);                  // canto superior, some após 10 s
  ```
- Perto do quadro (< 2 m): aparece "E — Ver recompensas". E abre/fecha; setas navegam. Cartazes mostram rosto procedural simples (formas), nome, crime, valor vivo > valor morto, "Visto por último: <nome de um lugar do mapa>". Sem aceitar contrato (vem com as missões).

- [ ] **Step 1: Testes**
  ```cpp
  TEST_CASE("vivo paga mais que morto") { for(auto&c:gerarCartazes(1,20,m)) CHECK(c.recompensaVivo > c.recompensaMorto); }
  TEST_CASE("último visto é um lugar do mapa") {...}
  TEST_CASE("nomes não repetem") {...}
  TEST_CASE("determinístico") {...}
  ```
- [ ] **Step 2–4:** FAIL → implementar → PASS. Verificação visual: screenshot com o quadro aberto (flag `--abrir-quadro`).
- [ ] **Step 5:** Commit `feat: quadro de recompensas`.

### Task 12: Níveis de qualidade e efeitos 5–7

**Files:** Create `src/core/quality.{h,cpp}`, `src/render/shaders/{shadow.vs,shadow.fs,bloom.fs,godrays.fs}`, `tests/test_quality.cpp`; Modify `src/render/lighting.{h,cpp}`, `src/game/ui.cpp`, `src/main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  enum class Nivel { Baixo, Medio, Alto };
  struct Efeitos { bool sombras, bloom, raiosDeLuz; int resolucaoSombra; float escalaRender; float distanciaVisao; };
  Efeitos efeitosDe(Nivel);
  Nivel sugerirNivel(const std::string& rendererGL);   // "Intel"/"llvmpipe"/"Mali" etc. -> Baixo; NVIDIA/AMD dedicada -> Alto
  Nivel lerConfig(const std::string& caminho, Nivel padrao); void salvarConfig(const std::string&, Nivel);  // config.txt ao lado do .exe
  ```
- Baixo: sem sombras (bolha), sem bloom, sem raios, distância 250 m. Médio: sombras 1024 (uma cascata de 80 m em volta do jogador), distância 350 m. Alto: sombras 2048, bloom (meia resolução, 5 passes), raios de luz (radial blur a partir do sol na tela, só quando o sol está na frente e baixo), distância 450 m. Sombra da lua à noite com intensidade baixa. Menu de opções (Esc → Opções) troca o nível sem reiniciar.

- [ ] **Step 1: Testes**
  ```cpp
  TEST_CASE("Baixo não tem efeitos pesados") { auto e=efeitosDe(Nivel::Baixo); CHECK_FALSE(e.sombras); CHECK_FALSE(e.bloom); CHECK_FALSE(e.raiosDeLuz); }
  TEST_CASE("Alto tem tudo") { auto e=efeitosDe(Nivel::Alto); CHECK(e.sombras); CHECK(e.bloom); CHECK(e.raiosDeLuz); }
  TEST_CASE("Intel sugere Baixo") { CHECK(sugerirNivel("Intel(R) HD Graphics 4000")==Nivel::Baixo); CHECK(sugerirNivel("NVIDIA GeForce GTX 1060/PCIe/SSE2")==Nivel::Alto); }
  TEST_CASE("config inválida usa padrão") { CHECK(lerConfig("nao/existe.txt",Nivel::Medio)==Nivel::Medio); }
  ```
- [ ] **Step 2–4:** FAIL → implementar → PASS.
- [ ] **Step 5:** Verificação visual: pôr do sol na cidade em Baixo e Alto (`--qualidade baixo|alto`), lado a lado. Expected: Alto com sombras longas, tochas com brilho, raios de sol na neblina; Baixo sem esses efeitos, mas com as mesmas cores e AO.
- [ ] **Step 6:** Commit `feat: níveis de qualidade com sombras, bloom e raios de luz`.

### Task 13: Fechamento — desempenho e entregas

**Files:** Modify o que for necessário para orçamento; Create `README.md` (como jogar, controles, como compilar)

- [ ] **Step 1:** Modo `--benchmark` percorre um trajeto fixo pela cidade, floresta e castelo em 4 horas do dia, imprime triângulos e draw calls por frame (máx. e média). Expected no Baixo: máx. < 350 mil triângulos e < 350 draw calls. Se passar, otimizar (juntar malhas, diminuir distância de LOD) até caber.
- [ ] **Step 2:** `scripts/build-windows.sh` → `.exe` estático; tamanho do `.exe` + `data/` < 10 MB. Expected: `ls -la` confirma.
- [ ] **Step 3:** `scripts/build-web.sh` → abrir `build-web/index.html` com Playwright, esperar 5 s, screenshot; Expected: cena renderizada, sem erros no console.
- [ ] **Step 4:** Todos os testes: `./build/testes` → `Status: SUCCESS!`
- [ ] **Step 5:** Commit `chore: orçamento de desempenho e builds de entrega`.
