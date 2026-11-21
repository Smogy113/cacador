# Paredes com "raytracing" — Design

> Data: 2026-11-21. Primeiro passo da direção "realista" para o mundo (o caçador já tem a cabeça por
> raymarching, `docs/superpowers/specs/2026-11-21-heroi-realista-design.md`).

## 1. Intenção

O usuário acha que o jogo ainda parece "um jogo Flash". Escolheu puxar o visual para o **realista** e
começar **apenas pelas paredes das casas**, "como se tivesse um raytracing fortíssimo ali". O
diferencial do jogo é parecer caro sem ser pesado e sem nenhum arquivo de modelo/textura.

Hoje as paredes são caixas (`Gerador::caixa` em `src/render/props.cpp`) pintadas de uma cor só
(REBOCO, REBOCO2, MADEIRA, PEDRA), sem coordenadas de textura, desenhadas pelo `FS_LIT` do mundo
(`lit_`), com luz hemisférica, sol com mapa de sombras PCSS e oclusão em tela (SSAO) no Alto.

## 2. Objetivos, não-objetivos, anti-objetivos

Objetivos
- **Luz grande, traçada contra as casas reais** (escolha do usuário: "fórmula de distância" do vilarejo):
  - sombra do sol suave de verdade nas paredes (nítida junto ao que a projeta, abrindo com a distância);
  - oclusão real: becos entre casas, cantos, sob o beiral e a base da parede junto ao chão escurecem;
  - luz rebatida: a parede ensolarada de uma casa clareia e tinge a parede em frente (um rebote);
  - tudo acompanha o sol ao longo do dia, nada pré-calculado.
- **Detalhe pequeno com profundidade, desenhado pixel a pixel** (relevo, como a cabeça):
  - reboco com textura, rachaduras e pedaços caídos que mostram a pedra por baixo;
  - madeira em tábuas com frestas e veios;
  - pedra em blocos irregulares com argamassa funda;
  - cada relevo projeta sombra própria suave conforme o sol (tábua sobre tábua, pedra sobre argamassa).
- À noite, a luz da janela acesa clareia a parede em volta.

Não-objetivos (ficam para depois)
- Telhados, muralha da cidade, torres, cercas, chão, árvores.
- Sombras das casas no **chão** com a técnica nova (o chão continua com o mapa de sombras).
- Reflexos especulares (vidro/água).

Anti-objetivos
- Ruído/granulado visível (raytracing "barato" pontilhado) ou faixas/serrilhado.
- Queda de FPS perceptível no Médio; qualquer mudança no Baixo e na web.
- Paredes "plastificadas" ou cores mais saturadas (o objetivo é parecer real, não colorido).
- Relevo "nadando" ao mover a câmera, ou relevo nas bordas da caixa entregando que é falso de longe.

## 3. Restrições

- C++17 + raylib 5.5; tudo gerado por código; GLSL 330 (no PC); web sem mudança.
- Baixo: sem mudança. Benchmark Baixo ≤ 350 mil triângulos / 350 chamadas (inalterado).
- Sem triângulos novos nas paredes (o detalhe é por pixel).
- Testes atuais passando.

## 4. Abordagens consideradas

| | Abordagem | Troca |
|---|---|---|
| 1 | Relevo desenhado pixel a pixel nas paredes (raio dentro de um campo de alturas por material) + luz analítica da própria casa | Barato; de perto parece geometria; sozinho não sabe das casas vizinhas |
| 2 | Geometria real (cada tábua/pedra em triângulos) | Silhueta perfeita, mas multiplica os triângulos de todos os vilarejos e ainda precisa da luz |
| 3 | Raytracing em tela para a cena toda (luz rebatida e reflexos) | Afeta tudo, caro e granulado |
| **4** | **Cena como fórmula de distância (as construções são formas conhecidas) + raios de sombra/oclusão/rebote contra ela, combinada com a 1 para o detalhe** (escolhida) | Raytracing de verdade contra as casas reais, liso e barato; exige manter a lista de formas perto da câmera |

## 5. Decidido pelo usuário
- Direção realista; começar só pelas paredes das casas.
- Abordagem 4 (fórmula de distância do vilarejo para a luz grande) + 1 (relevo para o detalhe).

## 6. Escolhas do executor (propostas)

### Escopo das "paredes"
- As faces da caixa de parede de `casa()` (reboco/madeira/pedra), o alicerce de pedra, as vigas do
  enxaimel e as paredes dos prédios especiais (taverna, xerife, cadeia, estábulo, capela) que usam
  `casa()` ou caixas de parede. Telhado, chaminé, porta e janelas: sem mudança de material (mas
  entram na fórmula de distância como bloqueadores de luz).

### Marcação na malha
- Novos tipos de material no vértice (`TIPO_*`, como janela/chama): `TIPO_REBOCO`, `TIPO_MADEIRA`,
  `TIPO_PEDRA` (e vigas como madeira escura). As coordenadas do relevo vêm da posição no mundo
  projetada no plano da face (direção horizontal da face e altura): não precisa de UV, não "nada"
  com a câmera.

### Fórmula de distância do vilarejo
- Na construção do cenário, cada construção guarda suas formas: caixa das paredes, prisma do telhado,
  caixa da chaminé, muros/torres próximos como caixas/cilindros. Lista global em `Cenario`.
- A cada quadro, as ~32 formas mais próximas da câmera (dentro de ~70 m) vão ao shader como
  uniformes (centro, meia-extensão, rotação em y, tipo). Distância = mínimo das formas (exatas).
- Só fragmentos de parede (os tipos novos) usam isso:
  - **sombra suave do sol**: marcha até o sol (≤ 24 passos), penumbra pelo menor afastamento
    (como `sombraPropria` da cabeça); combinada com o mapa de sombras (mínimo), sem o serrilhado;
  - **oclusão**: 5 amostras ao longo da normal (como a cabeça) + o chão (plano/terreno aproximado
    pela altura no pé da casa);
  - **rebote**: 4–6 direções fixas no hemisfério da normal (sem ruído: direções fixas, giradas por
    face), raio até ~12 m; se acerta outra forma, soma a cor média da parede atingida × quanto o sol
    a ilumina (n·L, sem nova marcha de sombra) × fator; o chão já devolve luz pelo termo atual.
- Distância: efeito completo até ~40 m, some até ~70 m (lá fica o sombreado atual).

### Relevo (pixel a pixel)
- Campo de alturas procedural por material, em metros, função das coordenadas da face:
  - reboco: ondulação suave + rachaduras finas + manchas onde o reboco caiu (pedra por baixo, 1–2 cm
    mais funda, com cor de pedra);
  - madeira: tábuas verticais ou horizontais (~18 cm) com frestas de 5–8 mm, veios e nós;
  - pedra: blocos irregulares (Voronoi) com argamassa recuada 1,5–3 cm e faces abauladas.
- Raio dentro do relevo (≤ 24 passos + refino), cor/normal do ponto achado, sombra própria do relevo
  até o sol (≤ 10 passos). Some com a distância (sem relevo a partir de ~25 m; normal só até ~45 m).
- Sem mexer na silhueta (bordas da caixa continuam retas); o relevo nas bordas some aos poucos
  para não mostrar o "corte".
- Sujeira na base (respingo de lama até ~40 cm) e escorrido sob as janelas, pela cor.

### Janelas à noite
- Fragmentos de parede perto de uma janela acesa recebem a cor da janela com queda pela distância
  (as janelas já são conhecidas na geração; ficam na lista de formas como "emissoras").

### Luz que faz sombra (escolha do executor)
- Os raios usam a mesma luz do mapa de sombras (`dirSombra`): o sol de dia, a lua à noite (mais
  fraca, como hoje). À noite a sombra suave e o relevo seguem a lua; a janela acesa soma por cima.
- Nas paredes dentro do alcance do efeito, a sombra do sol vem **só** da fórmula de distância (casas,
  telhados, chaminés, muros): é ela que tira o serrilhado. Custo aceito: árvores, pessoas e carroças
  deixam de projetar sombra nas paredes perto da câmera (são pequenas e raramente tocam paredes).
  Fora do alcance e no chão, o mapa de sombras continua como hoje (o passe de sombra não muda).

### Níveis (escolha do executor)
- Alto (placas boas, como a RTX 5070): tudo.
- Médio (placas medianas/integradas boas): relevo com menos passos e sem sombra própria do relevo,
  oclusão pela fórmula (5 amostras) e **sem** marcha de sombra nem rebote — nas paredes, o Médio
  continua com o mapa de sombras. Custo estimado bem abaixo do Alto.
- Baixo e web: atual.
- Painel F1: "Paredes com raytracing" (liga/desliga) com o tempo de quadro ao lado.

### Verificação
- Testes de lógica pura: a lista de formas cobre as casas geradas (todo vértice de parede está a
  < 2 cm de alguma forma); distância das formas (caixa girada, prisma) exata em pontos conhecidos;
  campos de altura limitados ao intervalo declarado (o raio não atravessa).
- Capturas antes/depois (mesmo lugar e hora): de perto (2 m), rua do vilarejo (10 m), beco entre
  casas, pôr do sol, noite com janelas. Benchmark Baixo inalterado.
- Custo na GPU medido pelo usuário no F1 (RTX 5070).

## 7. Deixado para o executor
- Números exatos (passos, raios, quantas formas, distâncias de corte), desenho dos campos de altura,
  intensidades — ajustar olhando as capturas.
