# Casas realistas e ocas (gerador por gramática) — Design

> Data: 2026-11-22. Continua a direção "realista" (paredes com relevo e luz traçada, 0.19).

## 1. Intenção
O usuário quer as casas **modeladas de verdade**: realistas, **ocas e grandes o suficiente para entrar**,
vazias por enquanto. Não quer cada uma feita à mão: um **algoritmo** monta todas. Hoje cada casa é uma
caixa maciça (`Gerador::casa` em `src/render/props.cpp`) com telhado, vigas, porta e janelas pintadas
por fora e colisão de bloco.

## 2. Objetivos, não-objetivos, anti-objetivos
Objetivos
- Todas as construções que hoje usam `casa()` e os prédios especiais (taverna, xerife, cadeia, estábulo,
  capela, casa da fazenda) passam a sair de um gerador único.
- **Estilos por lugar (decisão do usuário):** medieval (enxaimel, reboco, pedra, palha/telha, beiral fundo,
  chaminé de pedra) na cidade murada e nos vilarejos; **faroeste** (tábuas sobrepostas, varanda coberta
  com postes, fachada falsa na rua principal, telhado de tabuinhas) nas fazendas e lugares afastados
  (fazenda, mina, acampamento quando houver construção).
- **Ocas e entráveis:** paredes com espessura (~0,2 m madeira, ~0,4 m pedra), vãos reais de porta e
  janela, piso, forro, telhado visto por dentro; pé-direito ≥ 2,6 m; porta ≥ 1,0 × 2,1 m.
- **Porta que abre e fecha com E** (decisão do usuário); aberta deixa a luz entrar e o caminho livre.
- **Só o térreo acessível** (decisão do usuário): casas de 2 andares mostram o andar de cima pelas janelas,
  o forro do térreo fecha; sem escada por enquanto.
- **Câmera** (decisão do usuário): dentro de casa aproxima do ombro e não atravessa paredes.
- Detalhes que vendem realismo: molduras e vergas, venezianas, peitoris, vigas aparentes com volume,
  beiral com caibros, cumeeira, chaminé, degrau/soleira, varanda (faroeste), placa (taverna/xerife).
- Interior mais escuro que o exterior (luz entra pela porta/janelas), sem ficar preto.

Não-objetivos
- Móveis, objetos e moradores dentro; escadas e andares de cima; telhados/paredes destrutíveis.
- Muralha, torres, ruínas do castelo, tendas do acampamento (seguem como estão).

Anti-objetivos
- Casas "montadas"/repetidas (todas iguais), janelas desalinhadas, portas no ar ou dando para parede.
- Cair/atravessar parede, ficar preso na porta, câmera dentro da parede.
- Queda de FPS: a cidade inteira detalhada é cara — longe vira casca simples.
- Interior claro como o lado de fora (parece cenário de papelão) ou preto absoluto.
- Mudar o Baixo/web além do necessário (no Baixo as casas também são ocas, mas com menos detalhe).

## 3. Restrições
- C++17 + raylib 5.5; tudo por código; regras de desempenho do jogo (Baixo ≤ 350 mil triângulos/350
  chamadas no benchmark).
- Colisão só por AABB (paredes giradas viram faixas de AABBs pequenos, como o estábulo hoje) + pisos
  andáveis (`adicionarPiso`, altura sobe no máximo 0,7 m por passo).
- A luz traçada das paredes (formas/atlas) deve continuar funcionando: os painéis de parede viram
  formas `parede` (faces de dentro e de fora no atlas).

## 4. Abordagens
| | Abordagem | Troca |
|---|---|---|
| **1** | **Gramática de formas** (lote → volume → andares → fachadas → vãos → peças), regras por estilo e tipo, semente por lote (**escolhida pelo usuário**) | Muita variedade coerente com poucas regras |
| 2 | Kit de módulos encaixados numa grade | Rápido, mas repetitivo |
| 3 | Colapso de funções de onda (WFC) | Formas inesperadas, difícil garantir realismo |
(As peças finais da gramática são parametrizadas, como num kit.)

## 5. Escolhas do executor
- **Gerador no core** (`src/core/predio.{h,cpp}`, lógica pura e testável): entra lote (largura,
  profundidade, frente), estilo, tipo, andares, semente; sai uma **planta de peças** em coordenadas do
  lote: caixas/prismas com material e cor, quais registram forma (luz traçada), lista de colisão
  (segmentos de parede com vão da porta livre), porta (dobradiça, largura, altura, sentido),
  janelas (para a luz das janelas), volume interior (para escurecer), piso (altura), rota de entrada
  (ponto na frente da porta, para os moradores). O render só converte peças em `MeshBuilder`.
- **Gramática (resumo):**
  - Volume: retângulo; às vezes "L" (anexo) nas casas grandes medievais; faroeste: retângulo + varanda.
  - Fachadas divididas em vãos de ~1,6–2,2 m; frente: porta num vão (perto do meio ou do lado),
    janelas nos outros; laterais/fundos: janelas esparsas e parede cheia; 2º andar: janelas alinhadas
    sobre as de baixo.
  - Medieval: alicerce de pedra baixo, enxaimel (postes nos cantos e nas divisões dos vãos, frechal,
    travessas, diagonais), reboco entre vigas ou pedra (capela/xerife/cadeia), telhado íngreme de
    palha/telha/ardósia com beiral fundo e caibros, chaminé de pedra numa empena.
  - Faroeste: base de pedra/estacas, paredes de tábuas sobrepostas (o relevo de madeira horizontal já
    existe), varanda com piso de tábuas, postes e cobertura, telhado de tabuinhas menos inclinado,
    fachada falsa (taverna/saloon/xerife na rua principal), venezianas.
- **Porta:** peça própria desenhada separada (gira na dobradiça); colisão da porta dinâmica (testada à
  parte no movimento do jogador). E abre/fecha quando o caçador está a < 1,5 m da porta e olhando para
  ela; ícone "E Abrir/Fechar" na interface. Começam fechadas; moradores ignoram (não entram por enquanto).
- **Piso e degrau:** piso de tábuas (medieval: tábuas/pedra) a 0,35 m do chão no ponto mais alto do lote,
  degrau na porta; ambos como pisos andáveis. Terreno dentro da casa não aparece (o piso cobre).
- **Interior mais escuro:** lista dos ~16 interiores mais próximos (caixa girada + abertura) enviada a
  todos os shaders do mundo e personagens; dentro de um interior a luz do céu/ambiente cai para ~25%
  (mais perto da porta/janelas abertas, menos ao fundo); sol e lua entram só pelo que o mapa de sombras
  deixa (o telhado já bloqueia). Lanterna do caçador ilumina normalmente.
- **Câmera:** dentro de um interior, distância máxima ~1,8 m e ombro mais perto; colisão com as
  paredes já existente (`raioLivre`).
- **Desempenho:** malha detalhada por pedaço até ~90 m; além disso uma "casca" (volume + telhado +
  vãos escuros) por pedaço. Luz traçada: limite de formas por face sobe de 32 para 64.
- **Baixo/web:** mesma planta, sem peças de detalhe pequeno (caibros, venezianas, travessas).

### Decisões do executor após a revisão do design
- **Onde aparece o faroeste:** hoje só a fazenda tem construções afastadas. O faroeste vale para a casa
  e o celeiro da fazenda e para um **barracão novo na mina** (minerador). A cidade murada (taverna,
  xerife, cadeia, casas) e os vilarejos ficam medievais; a "fachada falsa" fica pronta na gramática
  para quando houver uma rua faroeste. Acampamento continua com tendas.
- **Inimigos e cavalos não entram** nas casas por enquanto (os tiroteios são nos esconderijos, longe das
  casas); moradores continuam do lado de fora. Cavalo é barrado pela porta (colisão).
- **Prédios cobertos:** `casa()` (vilarejo, cidade, fazenda), taverna, xerife, cadeia, capela (a torre vai
  para os fundos, a porta fica na frente), celeiro da fazenda (portão largo de celeiro). O **estábulo
  continua aberto** (é um galpão).
- **Colisão de paredes giradas:** o `MundoColisao` ganha **caixas giradas** (retângulo girado no plano +
  altura), usadas pelas paredes; `resolver` e `raioLivre` passam a considerá-las. Sem escadinha de AABBs.
- **Lotes:** as pegadas atuais (5,5–12 m) se mantêm; a varanda/anexo entra dentro da folga que `livre()`
  já exige. Rotas dos moradores, quadros e cavalos passam a usar o ponto de entrada que o gerador devolve.
- **Desempenho:** no Médio/Alto, o vilarejo e a cidade não podem ficar mais de 10% mais caros (triângulos
  e chamadas) que hoje com a casca longe; orçamento de ~6 mil triângulos por casa detalhada.

## 6. Verificação
- Testes (core): para centenas de sementes de cada estilo/tipo — porta no térreo e dando para fora,
  vão livre ≥ 1,0 × 2,1 m sem colisão; janelas não se sobrepõem nem cruzam vigas/porta; interior
  livre ≥ 2,6 m de altura e ≥ 2,5 m de lado; caminho da porta ao centro sem colisão; peças dentro do
  lote; formas de parede batem com as peças; contagem de triângulos por casa dentro de um orçamento.
- Capturas: vilarejo, cidade, fazenda (faroeste), dentro de uma casa de dia e à noite, porta abrindo.
- Benchmark Baixo dentro dos limites; build Windows e web.

## 7. Deixado para o executor
- Números exatos das regras, cores, quantidade de variações; desenho dos detalhes.
