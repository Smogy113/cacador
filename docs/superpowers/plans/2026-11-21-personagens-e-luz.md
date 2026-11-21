# Personagens anatômicos e luz — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Trocar os bonecos de caixas por corpos anatômicos com esqueleto (pessoas e cavalo) e dar à luz um aspecto de ray tracing, mantendo o jogo leve.

**Architecture:** Esqueleto, pose e geração de malha são lógica pura em `src/core/` (testada com doctest). A deformação acontece no vertex shader (`ossos[17]`), então cada personagem é 1 chamada de desenho. Os efeitos de luz entram no shader principal (luz rebatida, sombra suave) e no pós-processamento (SSAO, Alto).

**Tech Stack:** C++17, raylib 5.5, GLSL 330/100, doctest.

**Spec:** `docs/superpowers/specs/2026-11-21-personagens-e-luz-design.md`

## Global Constraints
- Sem arquivos de modelo; tudo gerado por código. Sem texturas.
- 1 chamada de desenho por personagem; até 17 ossos (`mat4`) por malha; até 2 ossos por vértice.
- Humano ~1.000–1.600 triângulos; cavalo ≤ 2.000.
- Benchmark no Baixo ≤ 350 mil triângulos e ≤ 350 chamadas.
- Web (GLSL 100) continua compilando e rodando no Baixo.

## Review Focus
1. Juntas dobradas ao máximo (joelho montado, cotovelo) não podem achatar nem atravessar a malha → mistura de 2 ossos perto das juntas.
2. NPC com proporções extremas (mais baixo/alto) continua com os pés no chão.
3. Troca de qualidade em tempo de jogo não vaza texturas de SSAO/sombra.
4. Personagem bem longe ou fora da tela não custa matrizes (pula cálculo de pose).
5. Shader com esqueleto compila no GLSL 100 (web).

---

### Task 1: Matriz 4×4 e esqueleto
**Files:** Create `src/core/mat4.h`, `src/core/skeleton.{h,cpp}`, `tests/test_skeleton.cpp`
**Interfaces (Produces):**
```cpp
struct Mat4 { float m[16]; static Mat4 id(); static Mat4 trans(Vec3f); static Mat4 rotX(float), rotY(float), rotZ(float); Vec3f ponto(Vec3f) const; };
Mat4 operator*(const Mat4& a, const Mat4& b);   // a depois b (mesma convencao da raymath: MatrixMultiply(a,b))
struct Osso { int pai; Vec3f inicio; Vec3f fim; };          // em pose de repouso, coordenadas do modelo
struct Esqueleto { std::vector<Osso> ossos; };
struct Pose { std::vector<Vec3f> rot; Vec3f raiz; };       // rotacao local (x,y,z em radianos) por osso + deslocamento da raiz
void calcularMatrizes(const Esqueleto&, const Pose&, std::vector<Mat4>& saida);  // matriz de skinning por osso
```
- [ ] Testes: pose de repouso → todas as matrizes identidade; girar o braço 90° leva o ponto da mão para a frente; filho herda rotação do pai; deslocamento da raiz move tudo.
- [ ] Implementar; testes passam; commit `feat: esqueleto e matrizes de pose`.

### Task 2: Corpo humano por loft com pesos
**Files:** Create `src/core/body.{h,cpp}`, `tests/test_body.cpp`
**Interfaces (Produces):**
```cpp
enum OssoHumano { QUADRIL, COLUNA, PEITO, PESCOCO, CABECA, BRACO_E, ANTEBRACO_E, MAO_E, BRACO_D, ANTEBRACO_D, MAO_D, COXA_E, CANELA_E, PE_E, COXA_D, CANELA_D, PE_D, N_OSSOS_HUMANO };
struct Proporcoes { float altura = 1.8f, ombros = 1.f, quadril = 1.f, massa = 1.f; };
struct Visual { Rgb pele, camisa, casaco, calca, botas, chapeu, lenco, cabelo; bool sobretudo, chapeuAba, barba; };
struct VertPele { Vec3f pos, normal; Rgb cor; float ao; uint8_t osso[2]; float peso; };  // peso do osso[0]; osso[1] recebe 1-peso
struct MalhaPele { std::vector<VertPele> v; std::vector<uint16_t> idx; };
Esqueleto esqueletoHumano(const Proporcoes&);
MalhaPele corpoHumano(const Proporcoes&, const Visual&);
```
- [ ] Testes: 1.000–1.600 triângulos; índices de osso < 17 e pesos em [0,1]; topo da cabeça ≈ altura (±3%); pés em y≈0; normais apontam para fora (dot com vetor do eixo do osso > 0 na maioria ≥ 95%); vértices perto do cotovelo misturam braço e antebraço.
- [ ] Implementar; commit `feat: corpo humano anatômico gerado por código`.

### Task 3: Desenho com esqueleto na placa de vídeo
**Files:** Create `src/render/skinned.{h,cpp}`; Modify `src/render/shaders.cpp`, `src/game/actors.{h,cpp}`, `src/game/jogo.cpp`
**Interfaces:** `class MalhaEsqueleto { void carregar(const MalhaPele&); void desenhar(Shader, const std::vector<Mat4>& ossos, Matrix modelo); }`; `Iluminacao::shaderPele()` (mesmo FS do lit, VS com `uniform mat4 ossos[17]`, índices/pesos em `vertexTexCoord2`).
- [ ] Shader compila em GLSL 330 e 100 (rodar build web). Screenshot: pessoa em repouso de perto e de longe.
- [ ] Commit `feat: personagens deformados na placa de video`.

### Task 4: Animações humanas
**Files:** Create `src/core/anim.{h,cpp}`, `tests/test_anim.cpp`; Modify `src/game/actors.cpp`, `src/game/jogo.cpp`
**Interfaces:** `Pose poseAndar(float fase, float velocidade); Pose poseParado(float tempo); Pose poseMontado(float fase, int andadura);`
- [ ] Testes: parado → pernas paradas (rot ≈ 0 exceto respiração); andando → pernas em oposição de fase; correndo → amplitude maior que andando; montado → coxas para a frente (> 60°); pé mais baixo fica em y≈0 (sem afundar no chão) em todas as fases.
- [ ] Commit `feat: animacoes do corpo humano`.

### Task 5: Cavalo anatômico
**Files:** Modify `src/core/body.{h,cpp}`, `src/core/anim.{h,cpp}`, `src/game/actors.{h,cpp}`; tests in `tests/test_body.cpp`, `tests/test_anim.cpp`
**Interfaces:** `Esqueleto esqueletoCavalo(); MalhaPele corpoCavalo(Rgb pelo, Rgb crina, bool selado); Pose poseCavalo(float fase, int andadura, float tempo); Vec3f assentoCavalo(...)`.
- [ ] Testes: ≤ 2.000 triângulos; cascos em y≈0 parado; trote com diagonais em fase; galope com amplitude maior.
- [ ] Commit `feat: cavalo anatomico`.

### Task 6: Luz rebatida e sombras suaves
**Files:** Modify `src/render/shaders.cpp`, `src/render/lighting.cpp`
- [ ] Luz rebatida em todos os níveis; PCSS simplificado no Médio/Alto. Screenshots ao pôr do sol: sombra de um poste nítida perto do pé e borrada na ponta.
- [ ] Commit `feat: luz rebatida e sombras suaves`.

### Task 7: SSAO, sombra de contato e reflexo na água (Alto)
**Files:** Modify `src/render/lighting.{h,cpp}`, `src/render/shaders.cpp`
- [ ] A cena do Alto passa a ter textura de profundidade; SSAO em meia resolução, desfocado, aplicado na composição. Trocar Baixo→Alto→Baixo 10× sem vazar (contar texturas vivas).
- [ ] Commit `feat: SSAO e reflexos`.

### Task 8: Fechamento
- [ ] `./build/cacador --benchmark` no Baixo e Alto dentro do orçamento; `.exe` e web compilam; README atualizado.
- [ ] Commit `chore: benchmark e builds com personagens novos`.
