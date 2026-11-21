# Herói realista — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Desenhar a cabeça do herói por raymarching num volume de distâncias (resolução sem limite, sombra/oclusão/translucidez no rosto) e somar luz de personagem, olhos e câmera de cinema.

**Architecture:** A escultura `sdf::cabeca` é amostrada ao carregar num volume (distâncias + cor), empacotado em atlas 2D de fatias. Uma caixa presa ao osso da cabeça é desenhada com o **mesmo FS_LIT** (com `#define PERSONAGEM` e `#define RAYMARCH`): no começo do `main()` o raio anda pelo volume, acha a superfície e preenche as variáveis que antes vinham do vertex shader (posição, normal, cor, coordenadas do rosto), e todo o resto da luz/pele/névoa é reaproveitado. A malha do herói ganha uma versão sem a pele da cabeça para o passe principal; as sombras continuam com a malha completa.

**Tech Stack:** C++17, raylib 5.5, GLSL 330 (o raymarching não roda na web/GLSL 100: lá fica a malha), doctest.

**Spec:** `docs/superpowers/specs/2026-11-21-heroi-realista-design.md`

## Global Constraints
- Tudo gerado por código; sem arquivos de modelo/textura novos.
- Raymarching só no Alto e no Médio, só no herói, só com a câmera a < 15 m dele; Baixo e web usam a malha atual.
- Médio: versão econômica (menos passos, sem translucidez e sem sombra própria); DOF/granulação/aberração só no Alto.
- Benchmark Baixo ≤ 350 mil triângulos e ≤ 350 chamadas (inalterado).
- Herói completo (malha de sombra/fallback) ≤ 95 mil triângulos e < 65535 vértices (testes atuais).
- Cor do rosto igual à de hoje (mesma função para malha e volume).
- Meta de custo: < 1,5 ms de GPU a mais no close no Alto (medido pelo usuário no F1).
- Ao final: versão 0.18.0 em `src/versao.h`.

## Review Focus
1. Cabeça girada nos extremos (mirar para cima/baixo, galope) → sem emenda nem atraso no pescoço. Teste na Task 3 (rotações de 0,4 rad).
2. Câmera dentro da caixa da cabeça (muito perto/atravessando) → o raio começa na câmera, nunca fica preto nem some. Task 5: `t0 = max(entrada, 0)` + screenshot `--retrato 0.12`.
3. Silhueta: raio que roça e não acha superfície → descartado (nunca aparecem as bordas da caixa). Task 5: screenshot de perfil contra o céu.
4. Noite com lanterna e tochas → o rosto recebe as luzes pontuais como o corpo. Task 5: screenshot `--hora 22`.
5. Trocar Alto→Baixo→Alto em jogo → sem vazar texturas/shader, cabeça volta à malha no Baixo. Task 5: teste manual pelo F1 + contagem de texturas.

---

### Task 1: Cor da pele do rosto numa função única

**Files:** Create `src/core/head_cor.h`; Modify `src/core/head_hd.cpp`; Test `tests/test_head_cor.cpp`

**Interfaces — Produces:**
```cpp
// p e n em coordenadas da escultura (sdf); a mesma cor que os vertices recebem hoje
struct CorPele { Rgb cor; bool labio = false; float pelos = 0; float escurecer = 0; /* fundo das narinas: 0..1 */ };
CorPele corDaPele(Vec3f p, Vec3f n, const Visual& vis);
```
- [ ] **Step 1: Testes** `cor da pele: igual a malha` (para 500 vértices da cabeça HD do herói, `corDaPele(pos/escala, normal)` dá a mesma `cor` (±0,002) e o mesmo `material==MAT_LABIO ⇔ labio`); `cor da pele: labio no meio da boca` (p = (0, 1.6366, 0.0984) → `labio`); `cor da pele: sobrancelha tem pelos` (p na sobrancelha (0.032, 1.719, superfície) → `pelos > 0.3`); `cor da pele: sem barba a bochecha nao tem pelos` (barba=false, (0.045, 1.66, superfície) → `pelos < 0.02`). Para achar "superfície" use a marcha em z de `tests/test_sdf_head.cpp`.
- [ ] **Step 2:** `./build/testes -tc="cor da pele*"` → falha (não compila).
- [ ] **Step 3:** mover o cálculo de cor/material/pelos de `cabecaHD` (linhas do laço `for (const auto& e : esc.v)`) para `corDaPele`; `cabecaHD` passa a chamá-la (o vértice continua recebendo `dirPelo/compPelo` como hoje).
- [ ] **Step 4:** testes novos e suíte inteira passam; screenshot `--estudio --retrato 0.5` idêntico ao de antes (comparar visualmente).
- [ ] **Step 5:** commit `refactor: cor do rosto numa funcao unica`.

### Task 2: Volume da cabeça (distâncias + cor)

**Files:** Create `src/core/head_volume.h`, `src/core/head_volume.cpp`; Test `tests/test_head_volume.cpp`

**Interfaces — Consumes:** `sdf::cabeca`, `corDaPele`. **Produces:**
```cpp
struct VolumeCabeca {
    Vec3f lo, hi;            // caixa (coordenadas da escultura)
    int nx, ny, nz; float h; // grade de distancias (voxel h)
    std::vector<float> dist; // nx*ny*nz, x mais rapido
    int cx, cy, cz; float hc;           // grade de cor
    std::vector<uint8_t> cor;           // RGBA8: rgb = albedo (como a cor dos vertices), a = labio ? 255 : pelos*230
    int colunas, linhas;     // atlas 2D: fatia z = k fica na coluna k%colunas, linha k/colunas
    float amostra(Vec3f p) const;       // trilinear (para testes e para a CPU)
};
VolumeCabeca gerarVolume(float h, float hCor, const Visual& vis);   // h ~0.0013, hCor ~0.003
std::vector<uint16_t> atlasMeiaPrecisao(const VolumeCabeca& v);       // dist em half float no layout do atlas
std::vector<uint8_t> atlasCor(const VolumeCabeca& v);                 // cor no layout do atlas
```
Caixa = a mesma de `sdf::malhar` (x −0,1..0,1; y 1,49..1,815; z −0,13..0,135) com 2 voxels de folga. Valores longe da superfície podem ser grossos (grade grossa como em `malhar`), mas sempre do lado certo e nunca maiores que a distância real (raymarching seguro). Cor só precisa estar certa a < 6 mm da superfície; fora disso, a cor de pele base.
- [ ] **Step 1: Testes:** `volume: reproduz a escultura perto da superficie` (para pontos a ±3 mm de 2000 vértices de `sdf::malhar(0.0026)`: `|amostra(p) − sdf::cabeca(p)| < 0.0004`); `volume: nunca superestima longe` (1000 pontos aleatórios na caixa: `amostra(p) <= sdf::cabeca(p) + 0.0005`); `volume: caixa contem a escultura` (todos os vértices da malha dentro de lo..hi com 2 voxels de folga); `volume: atlas no layout certo` (valor do atlas na fatia k, coluna k%colunas == dist[...]); `volume: gera em menos de 2 s` (cronômetro, `-O2`); `volume: cor do labio marcada no alfa`.
- [ ] **Step 2:** falham.
- [ ] **Step 3:** implementar (grade grossa + fina perto da superfície; `std::thread` por fatias; conversão float→half sem bibliotecas).
- [ ] **Step 4:** passam.
- [ ] **Step 5:** commit `feat: volume de distancias e cor da cabeca`.

### Task 3: Pescoço sem emenda (pele inversa) e inversa rígida

**Files:** Modify `src/core/mat4.h`; Create `src/core/pescoco.h`, `src/core/pescoco.cpp`; Test `tests/test_pescoco.cpp`

**Interfaces — Produces:**
```cpp
Mat4 inversaRigida(const Mat4& m);   // rotacao + translacao
// peso da cabeca pela altura de repouso (o mesmo de head_hd: smoothstep(1.53, 1.585, y) abaixo de 1.585, 1 acima)
float pesoCabeca(float yRepouso);
// ponto do modelo (ja deformado) -> ponto de repouso, misturando as inversas de cabeca e pescoco
Vec3f paraRepouso(Vec3f p, const Mat4& invCabeca, const Mat4& invPescoco);
```
- [ ] **Step 1: Testes:** `pescoco: pose de repouso nao muda nada`; `pescoco: volta ao repouso com a cabeca girada` (para 300 vértices da cabeça/pescoço, deformar como a GPU (mistura das matrizes por `pesoCabeca`) com rotações de cabeça de 0,4 rad em x e y e pescoço 0,2 rad; `|paraRepouso(deformado) − original| < 0.0015`); `inversa rigida: M * inv(M) = identidade`.
- [ ] **Step 2:** falham. **Step 3:** implementar (2–3 iterações de ponto fixo usando o peso do ponto estimado). **Step 4:** passam. **Step 5:** commit `feat: pele inversa do pescoco`.

### Task 4: Malha do herói sem a pele da cabeça (e corpo mais denso)

**Files:** Modify `src/core/body.h`, `src/core/head_hd.cpp`, `src/core/body_hd.cpp`; Test `tests/test_body.cpp`

**Interfaces — Produces:** `Visual::semPeleCabeca = false` (a pele esculpida não entra; olhos, cílios, cabelo e lenço entram) e `Visual::ladosExtras = 1.f` (multiplica os lados dos tubos do herói; 2,0 nesta versão).
- [ ] **Step 1: Testes:** `heroi sem pele da cabeca: nenhum vertice de pele acima de 1,55 m` (material MAT_PELE com osso CABECA e y > 1,55 → 0); `heroi sem pele: olhos, cilios e cabelo continuam` (contagem de MAT_OLHO, MAT_CARTAO > 0); `heroi sem pele e corpo denso: <= 60 mil triangulos e < 65535 vertices`.
- [ ] **Step 2:** falham. **Step 3:** implementar. **Step 4:** passam (inclui os testes antigos do herói ≤ 95 mil). **Step 5:** commit `feat: malha do heroi sem a pele da cabeca`.

### Task 5: Raymarching da cabeça no jogo

**Files:** Create `src/render/cabeca_raio.h`, `src/render/cabeca_raio.cpp`; Modify `src/render/shaders.{h,cpp}`, `src/render/lighting.{h,cpp}`, `src/game/actors.{h,cpp}`, `src/game/jogo.cpp`, `src/game/depuracao.cpp`

**Interfaces — Consumes:** Tasks 2–4. **Produces:**
```cpp
class CabecaRaio {
public:
    void carregar(const VolumeCabeca& v, float escalaXZ, float escalaY);  // envia os atlas (R16 e RGBA8)
    void descarregar();
    // desenha a caixa da cabeca; ossos = matrizes de pele da pose; modelo = matriz do personagem
    void desenhar(Shader sh, const Camera3D& cam, const std::vector<Mat4>& ossos, Matrix modelo) const;
};
// lighting.h: Shader shaderCabeca();  usar o mesmo conjunto de uniformes de pele_ (loop {lit_, pele_, cabeca_})
// lighting.h: bool usaCabecaRaio() const;  // Alto/Medio, nao web, e dbg.semCabecaRaio == false
// actors.h (ModeloHumano): carregarHeroi(...) carrega tambem a malha sem pele e a CabecaRaio
```
Shader: `VS_CABECA` (posição da caixa no mundo) + `FS_LIT` com `#define RAYMARCH`: as entradas do FS viram variáveis globais preenchidas por `marchar()` no começo do `main()` (`posMundo, normalMundo, cor, extra, posRep, nRep, tanRep, tanMundo`), `tanRep = vec4(q.x, q.y, 1, 0)` (coordenadas do rosto como na malha), `extra.y = labio ? 18.0 : 10.0`, `cor.a = pelos`. Distância lida do atlas com interpolação entre duas fatias; passos ≤ 64 (Médio ≤ 32); refinamento final por bisseção; `discard` se não achar; `gl_FragDepth` pela matriz vista×projeção (uniform). Cada ponto do raio vai ao repouso com a mesma regra da Task 3 (portada ao GLSL).
- [ ] **Step 1:** implementar; desenho do herói: passe principal usa a malha sem pele + `CabecaRaio` quando `usaCabecaRaio()` e câmera < 15 m; sombras e o resto usam a malha completa. Estúdio (`--estudio`) também.
- [ ] **Step 2:** F1: caixa "Cabeça por raymarching" e o tempo de quadro ao lado.
- [ ] **Step 3: Verificação visual** (screenshots, comparar com a malha): `--estudio --retrato 0.5 --girar-camera 0/35/80` (forma e cor iguais, sem facetas); `--retrato 0.12` (câmera dentro da caixa: não fica preto); perfil contra o céu (sem bordas da caixa); `--hora 22` com lanterna (luz pontual no rosto); andando/mirando/montado (`--montado`, `--mirar`, `--tiroteio`): sem emenda nem atraso no pescoço; Alto→Baixo→Alto pelo F1 sem erro.
- [ ] **Step 4:** `./build/testes` verde; benchmark Baixo inalterado.
- [ ] **Step 5:** commit `feat: cabeca do heroi por raymarching`.

### Task 6: Luz "de ray tracing" no rosto

**Files:** Modify `src/render/shaders.cpp` (bloco `RAYMARCH`), `src/render/lighting.cpp`

- [ ] **Step 1:** implementar no shader da cabeça: oclusão por 5 amostras ao longo da normal (multiplica `ao`); sombra própria suave até o sol (≤ 16 passos, penumbra pelo menor afastamento; `sombra = min(sombra, propria)`); translucidez: espessura por 4 amostras andando para dentro contra a luz, `+= corSol * vec3(1.0, 0.25, 0.15) * exp(-espessura*k) * ...`; especular em dois lóbulos (macio + oleoso na zona T da testa/nariz). Médio: uniform `economico=1` desliga sombra própria e translucidez e usa ≤ 32 passos.
- [ ] **Step 2: Verificação visual:** contraluz ao pôr do sol (`--hora 18.3`, sol atrás da cabeça): orelhas e borda do nariz avermelhadas; sol lateral: sombra suave do nariz na bochecha; comparar com/sem pelo F1.
- [ ] **Step 3:** commit `feat: sombra, oclusao e translucidez no rosto do heroi`.

### Task 7: Luz de personagem e olhos

**Files:** Modify `src/render/shaders.cpp`, `src/render/lighting.{h,cpp}`, `src/game/jogo.cpp`, `src/game/depuracao.cpp`

**Interfaces — Produces:** `Iluminacao::definirHeroi(bool)` (uniform `heroi` em pele_ e cabeca_: 1 ao desenhar o herói, 0 depois).
- [ ] **Step 1:** shader (PERSONAGEM, só com `heroi > 0.5`): contraluz `pow(1−max(dot(n,v),0), 3)` na cor do céu/sol (mais forte com o sol atrás), preenchimento suave vindo da câmera; olhos (material 15): reflexo do céu (gradiente) e do sol na córnea, linha úmida na borda de baixo (faixa clara fina), sombra da pálpebra de cima no topo do globo.
- [ ] **Step 2:** F1: caixa "Luz de personagem".
- [ ] **Step 3: Verificação visual** com/sem (`--retrato 0.5`, dia e pôr do sol).
- [ ] **Step 4:** commit `feat: luz de personagem e olhos vivos`.

### Task 8: Câmera de cinema (Alto) e fechamento

**Files:** Modify `src/render/shaders.cpp` (`FS_JUNTA`), `src/render/lighting.{h,cpp}`, `src/game/jogo.cpp`, `src/game/depuracao.cpp`, `src/core/body_hd.cpp` (lados), `src/versao.h`, `README.md`

- [ ] **Step 1:** pós-processamento no Alto: desfoque do fundo (uma cópia da cena em ¼ de resolução desfocada; mistura pela profundidade: foco = distância câmera→herói, desfoque cresce de foco+25 m a foco+150 m, mais forte com a câmera < 2 m do herói); granulação ~2% (ruído por pixel e tempo); aberração cromática nas bordas (R e B deslocados ∝ distância do centro², máx. ~1,5 px). Uniforms `distFoco`, `forcaFoco`, `tempo`.
- [ ] **Step 2:** F1: caixas "Profundidade de campo" e "Granulação e aberração".
- [ ] **Step 3: Verificação:** screenshots Alto (jogo normal, mira e close); o herói nunca desfocado; benchmark Baixo inalterado; `./build/testes` verde; build Windows e web compilam (`scripts/build-windows.sh`, `scripts/build-web.sh` — a web deve continuar com a malha).
- [ ] **Step 4:** versão 0.18.0; README: o que mudou no F1.
- [ ] **Step 5:** commit `feat: camera de cinema no Alto e versao 0.18.0`.
