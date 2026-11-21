# Herói realista: cabeça de resolução infinita + câmera e luz de cinema — Design

> Data: 2026-11-21. Continua os personagens AAA (cabeça esculpida por SDF, `src/core/sdf_head.cpp`).

## 1. Intenção

O usuário quer que o caçador (o personagem principal), de perto, pareça **realista**, não "de
jogo", e que isso seja **o diferencial do jogo**, sem custar caro. Pediu para "pensar fora da caixa"
e usar alguma ilusão de ótica. Escolheu: **(1) cabeça de resolução infinita desenhada pixel a pixel**
mais **(3) truques de câmera e luz de cinema**.

Hoje a cabeça é uma soma de ~40 formas (SDF) que vira ~74 mil triângulos ao carregar; mais detalhe
exigiria mais triângulos (o herói já está perto do teto de 95 mil). A ideia é não transformar a cabeça
em triângulos: guardar a escultura como um bloco 3D de distâncias na placa de vídeo e, a cada pixel
do rosto, procurar a superfície dentro dele (raymarching). Sem triângulos não há facetas; e o mesmo
bloco dá, quase de graça, sombra suave no próprio rosto, oclusão real e luz atravessando orelha/nariz.

## 2. Objetivos, não-objetivos, anti-objetivos

Objetivos
- Rosto e pescoço do herói sem facetas nem silhueta poligonal em qualquer distância (inclusive no
  `--retrato 0.5`).
- Luz que parece "ray tracing" no rosto: sombra suave do nariz/sobrancelha/maçã no próprio rosto,
  oclusão nos cantos dos olhos/narinas/orelhas, **translucidez** (orelha e nariz avermelham contra o sol).
- Pele com cara de pele: poros e micro-rugas por pixel estáveis (presos na cabeça), especular em duas
  camadas (oleosidade + pele), cor do rosto (lábios, maçãs, nariz, barba por fazer, sobrancelhas)
  igual à de hoje.
- Olhos vivos: reflexo do céu/sol na córnea, linha úmida na pálpebra de baixo, sombra da pálpebra.
- Luz de cinema só no herói: contraluz (borda clara) e preenchimento suave, como em cenas de jogos AAA.
- Câmera de cinema: profundidade de campo leve (fundo distante desfocado, mais forte com a câmera
  perto), granulação fina de filme, aberração cromática sutil nas bordas.
- Corpo do herói com mais lados no Alto (silhueta mais redonda).
- Custo: no Alto, numa placa média, o rosto em close não pode custar mais que ~1,5 ms; em jogo
  normal (câmera a 4 m) bem menos.

Não-objetivos
- Animação facial (falar, piscar, expressões).
- Corpo inteiro por raymarching (o corpo deforma muito; fica em triângulos).
- Moradores e bandidos com a técnica nova (só o herói; eles continuam com malha).

Anti-objetivos
- Emenda visível no pescoço (linha, buraco ou degrau onde a cabeça "pixel a pixel" encontra o corpo).
- Cabeça "flutuando" ou atrasada em relação ao corpo quando ele anda, corre, mira ou cavalga.
- Rosto mais feio que o atual (perder lábios, sobrancelhas, olhos, cor) ou com cor diferente do corpo.
- Queda de FPS perceptível no Médio/Baixo; tela preta, artefatos ou rosto sumindo em placas sem suporte.
- Quebrar a versão web (WebGL1/GLSL 100).

## 3. Restrições

- C++17 + raylib 5.5; tudo gerado por código (a escultura continua sendo `sdf::cabeca`).
- Baixo e web: cabeça de triângulos atual (sem mudança). Benchmark Baixo ≤ 350 mil tri / 350 chamadas.
- O herói continua 1 chamada de desenho para o corpo + o que a cabeça nova precisar (poucas).
- Os testes atuais continuam passando.

## 4. Abordagens consideradas

| | Abordagem | Troca |
|---|---|---|
| **1** | **Cabeça por raymarching num bloco 3D de distâncias** (escolhida) | Resolução sem limite e luz "de ray tracing" no rosto; exige emenda cuidadosa no pescoço e refazer a cor do rosto no novo desenho |
| 2 | Detalhe assado em texturas (normal/AO) sobre uma malha mais leve | Pipeline AAA clássico, barato; a cabeça gerada por código não tem mapeamento UV (costuras, muito trabalho) e não é diferencial |
| **3** | **Câmera e luz de cinema** (escolhida junto com a 1) | Rápido e barato, melhora a percepção de realismo; sozinho não aumenta a resolução |

## 5. O que foi decidido (pelo usuário)

- Abordagem 1 + 3: cabeça do herói desenhada por raymarching, mais luz de personagem, olhos,
  profundidade de campo, granulação e aberração cromática, e mais lados no corpo no Alto.

## 6. Escolhas do executor (propostas; podem ser corrigidas)

### Onde vale
- Raymarching da cabeça no **Alto e no Médio**, só para o herói e só quando a câmera está a menos de
  ~15 m dele (longe, a malha atual basta e é mais barata). Baixo/web: malha atual. Um interruptor no
  painel F1 alterna malha/raymarching para comparar.

### O bloco 3D (gerado ao carregar)
- **Distâncias**: `sdf::cabeca` amostrada numa grade em volta da cabeça+pescoço (caixa da escultura),
  com voxels de ~1,2–1,5 mm, guardada como **atlas 2D de fatias** (uma textura comum com as fatias z
  lado a lado; interpolação entre duas fatias no shader). Evita texturas 3D (não existem no WebGL1 e a
  raylib não expõe) — o mesmo caminho poderá servir à web no futuro. Formato float de 16 ou 32 bits.
- Cálculo rápido: grade grossa primeiro e grade fina só perto da superfície (como `sdf::malhar`),
  em várias threads; meta < 1,5 s no carregamento.
- **Cor e material**: um segundo atlas (RGBA8, resolução menor, ~3 mm) com a cor da pele já pintada
  (lábios, maçãs, ponta do nariz, orelhas, fundo das narinas — a mesma lógica de `head_hd.cpp`,
  refatorada para uma função que recebe um ponto e devolve cor), e no alfa a densidade de pelo
  (sobrancelha/barba por fazer/linha do cabelo) para os traços por pixel que já existem.

### Desenho
- Uma caixa (8 vértices) em volta da cabeça, presa ao osso da cabeça, com um shader próprio:
  o raio entra na caixa, é levado para o espaço de repouso da cabeça e anda pelo bloco até a superfície
  (passos guiados pela distância; refinamento final). Escreve a profundidade (`gl_FragDepth`) para
  olhos, cílios, cabelo e corpo se cruzarem certo com o rosto.
- **Pescoço sem emenda**: abaixo do queixo a escultura segue o pescoço como hoje (mistura dos ossos
  pescoço/cabeça); no shader, cada ponto do raio é levado ao repouso pela mesma mistura de ossos pela
  altura (aproximação de "pele inversa", boa para os ângulos pequenos do pescoço). A parte de baixo do
  pescoço fica escondida pela gola e pelo lenço; a malha do corpo continua dona do tronco.
- A malha do herói deixa de desenhar a pele da cabeça no passe principal quando o raymarching está
  ligado (olhos, cílios, cabelo e lenço continuam na malha). As **sombras** (mapas de sombra) seguem
  usando a cabeça de triângulos (barata e suficiente).

### Luz do rosto (no shader da cabeça)
- Normal pela diferença de distâncias no bloco + detalhe de poros/micro-rugas por ruído 3D no espaço
  da cabeça (estável).
- Oclusão por 4–5 amostras ao longo da normal; sombra própria suave por marcha curta em direção ao sol
  (penumbra pelo menor afastamento); sombra do mundo pelo mapa de sombras existente.
- Translucidez: espessura medida andando para dentro na direção oposta à luz (poucas amostras); pouca
  espessura + luz atrás → vermelho de sangue (orelha, narinas, borda do nariz).
- Especular em dois lóbulos (pele macia + brilho oleoso na testa/nariz) e Fresnel; mesma névoa,
  exposição e correção de cor do resto da cena (sem diferença de tom entre rosto e corpo).

### Cinema (ajustável no F1)
- Médio recebe só o que não precisa de passe de tela: luz de personagem, olhos e a cabeça por
  raymarching em versão econômica (menos passos; sem translucidez e sem sombra própria). Profundidade
  de campo, granulação e aberração cromática só no Alto, que já tem pós-processamento. (Escolha do
  executor: o Médio não ganha passe de tela novo para não perder FPS.)
- **Luz de personagem**: contraluz oposta à câmera (borda clara, cor do céu/sol) e preenchimento suave
  vindo da câmera, só no shader dos personagens do herói.
- **Olhos**: reflexo do céu e do sol na córnea, linha úmida, sombra da pálpebra de cima no globo.
- **Profundidade de campo**: foco na distância do herói; fundo muito distante levemente desfocado
  (mais forte com a câmera perto, ex.: close/mira); nunca desfoca o herói nem o que está perto dele.
  Usa a profundidade da cena quando houver (Alto); no Médio, só desfoque leve pela distância.
- **Granulação** fina (~2%) e **aberração cromática** sutil só nas bordas da tela.
- **Corpo**: no Alto, mais lados nos tubos do herói (o teto de triângulos do herói sobe só para o Alto
  se precisar; o Baixo usa a malha leve de sempre).

### Verificação
- Custo na GPU: esta máquina não tem placa de vídeo; a medição é feita no PC do usuário (RTX 5070)
  pelo painel F1, que ganha o interruptor malha × raymarching e mostra o tempo de quadro dos dois.
  Meta: diferença < 1,5 ms no close no Alto. (Escolha do executor.)
- Comparações `--estudio --retrato` mostram só a cabeça (sem desfoque/granulação), para comparar a forma.
- Testes de lógica pura (no core): bloco 3D reproduz `sdf::cabeca` (erro pequeno perto da superfície);
  função de cor do rosto igual à cor dos vértices de hoje nos mesmos pontos; mistura de ossos do pescoço
  contínua; caixa da cabeça contém toda a escultura.
- Screenshots lado a lado malha × raymarching (`--retrato 0.5`, frente/perfil/contraluz ao pôr do sol),
  em pé, andando, mirando e montado (sem emenda no pescoço, sem atraso).
- Benchmark Baixo inalterado; tempo de carregamento medido; tempo de GPU do rosto estimado.

## 7. Deixado para o executor
- Tamanho exato dos voxels, formato (16/32 bits), número de passos e amostras.
- Forma de portar a escultura/cor ao shader (bloco vs. avaliar fórmulas no shader para o refinamento final).
- Intensidades de luz de personagem, desfoque, granulação e aberração (ajustar olhando).
