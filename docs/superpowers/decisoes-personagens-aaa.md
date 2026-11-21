# Decisões — personagem "mais AAA" (pedido do usuário)

- Ruling: o salto veio de materiais (não de mais polígonos): pele com subsuperfície (difusa "wrap" + espalhamento avermelhado), poros; tecido com trama e veludo; couro granulado com brilho e desgaste; metal com reflexo; cabelo em fios; olhos molhados; feltro no chapéu. Tudo calculado no shader a partir da posição de repouso (o detalhe acompanha o corpo) — custo só nos pixels dos personagens.
- Ruling: material por vértice (MAT_* 10–16) no lugar do "tipo"; janela/chama/água limitadas às faixas 1–3 para não confundir com materiais de personagem.
- Ruling: globos oculares de verdade (esclera, íris, pupila) dentro de órbitas mais fundas, com pálpebras e sulco nariz-boca esculpidos; costeletas removidas; barba e linha do cabelo com transição suave.
- Ruling: dobras no sobretudo (mais fortes na barra) e rugas na manga do cotovelo, deslocando a geometria.
- Ruling: teste de contagem do HD subiu de ≤14 mil para ≤16 mil triângulos (globos oculares + dobras).
- Limite conhecido: sem texturas, cores ficam nos vértices — o rosto ainda tem ar de ilustração. Próximo passo possível: textura de pele/roupa gerada por código (poucas centenas de KB).

## Rosto real (pedido: "rosto real, barba real, olhos reais")
- Ruling: cabeça refeita do zero (src/core/head_hd.cpp) a partir de medidas antropométricas de homem adulto: distância entre pupilas 63 mm, nariz 54 mm (raiz→subnasal) com ~20 mm de projeção, boca 53 mm, lábio superior 22 mm, queixo 45 mm abaixo da boca; perfil lateral em tabela (testa→queixo) + curvatura lateral por altura + relevos (asas do nariz, órbitas, arco da sobrancelha, maçãs, olheira, sulco nariz-boca, filtro, cantos da boca). Testes fixam pupilas 60–66 mm, nariz 15–45 mm à frente da bochecha, boca 45–60 mm.
- Ruling: malha da cabeça com anéis a cada 2,7 mm no rosto e 56 lados 3,6× mais densos na frente (tuboA com ângulos não uniformes) — detalhe onde a câmera olha.
- Ruling: olhos com pálpebras de verdade (faixas de pele sobre o globo, com borda arredondada e vinco), cílios superiores e inferiores, e íris desenhada por pixel a partir da normal do globo (fibras radiais, colarete, anel limbar, pupila, veias e sombra da pálpebra na esclera).
- Ruling: barba, sobrancelhas, cílios e cabelo por cascas de pelo (4 cascas na barba de 5 mm, 2 no resto), recortadas em fios no shader; máscara de barba "fechada" curta (linha da bochecha, bigode, queixo, pescoço) sem cobrir os lábios; cabelo escondido pelo chapéu não gera cascas.
- Ruling: teste de triângulos do HD subiu para ≤24 mil (cabeça + cascas); benchmark Baixo máx. 207 mil / 113 chamadas.

## Ajustes depois da revisão visual (moiré, manchas, pescoço)
- Ruling: fios das cascas desenhados com duas grades giradas (escalas 1150 e 1530/m, 30° e 93°) no plano da pele (eixo dominante da normal), inclinados para baixo — uma grade regular fazia moiré nas laterais.
- Ruling: segundo mapa de sombra só do jogador (1024², 3 m → 3 mm por texel, Médio/Alto); no personagem, o mapa grande ignora bloqueadores a menos de 0,8 m (o próprio corpo, mal resolvido a 8 cm/texel) — eram as manchas do rosto.
- Ruling: a cabeça HD desce até 1,53 m por dentro do pescoço do tronco (o tronco HD termina em 1,54) — acabou o degrau no pescoço; a pele só escurece sob pelo denso.
- Ruling: um material por triângulo (separarMateriais duplica vértices de borda) — o código do material era interpolado dentro do triângulo e a borda crina/pelo (14→16) passava por 15 (olho) e ficava branca; cópias das cascas separadas por número de camadas. Teste: nenhum triângulo com materiais diferentes.
- Ruling: cavalo com malha suavizada (~4,3 mil triângulos), pelo curto aveludado, crina e cauda com cascas de fios, olhos escuros úmidos, cascos e sela de couro, estribos de metal; teste do cavalo sobe para ≤9 mil.
- Ruling: caçador completo (barba + equipamento) ~33 mil triângulos (teste ≤36 mil); só ele usa sempre o HD; benchmark Baixo segue no orçamento.
- Ruling: brilho especular de cabelo/crina reduzido (0,4 → 0,15) — fazia uma faixa clara na crista lisa da crina.

## Barba real (pedido: "essa barba tá horrível")
- Ruling: cascas trocadas por cartões de pelo (hair cards, como em jogos AAA): fitas de ~3–6 mm com 9 fios desenhados no shader (afinam na ponta, raiz escura, ondulação, alguns grisalhos), nascendo de raízes espalhadas por ÁREA (densidade por m², cobertura ~2,6× na barba e 1,3× no cabelo) — por vértice formava colunas. Direção de crescimento por região (barba para baixo, bigode para baixo e para fora, sob o queixo para trás, sobrancelha para os lados, cabelo para trás); fios quase deitados e quase paralelos (barba penteada).
- Ruling: luz de cabelo Kajiya-Kay (difusa e brilho ao longo do fio, com segundo brilho tingido); cartões não projetam sombra (seriam quadrados).
- Ruling: rugas por pixel nas coordenadas do rosto (3 linhas na testa, pés de galinha, bolsas sob os olhos), poros maiores no nariz, folículos azulados sob a barba; abertura do olho reduzida para ~10 mm; barba desce pelo pescoço e vai ficando rala na bochecha.
- Ruling: caçador completo agora ~26,8 mil triângulos; cavalo ~7,7 mil (crina e cauda em cartões longos). Proteção contra passar de 65 mil vértices (índices de 16 bits).
- Bug corrigido: ponteiros para vértices invalidados quando a lista crescia (cartoesDePelo copia os 3 vértices do triângulo).

## Rosto de homem-modelo e barba curta (pedido do usuário)
- Ruling: barba curta (~3 mm) e cabelo raspado desenhados só na pele, por pixel (sombra da barba feita + traços finos de ~0,4 mm em duas grades giradas apontando no sentido do crescimento); cartões ficam só nas sobrancelhas e na crina/cauda do cavalo — cartões curtos viravam pontinhos soltos ("sujeira").
- Ruling: formato masculino: curvatura lateral menor embaixo (maxilar quadrado), crânio mais largo na mandíbula, ângulo da mandíbula, queixo largo com covinha, maçãs altas com bochecha cavada abaixo, arco da sobrancelha forte, órbitas mais fundas, nariz reto e mais fino na ponta, lábios mais finos, testa levemente inclinada, topo do crânio mais redondo, pescoço mais grosso. Teste: largura da mandíbula ≥ 88% da largura nas maçãs (medida na frente do rosto) — ficou ~94%.
- Testes de barba reescritos: a barba é marcada na pele (pelos e direção de crescimento), não em cartões.

## Cabeça esculpida (pedido: "quero um modelo assim" + 3 esculturas de referência)
- Ruling: a cabeça deixou de ser relevo sobre um tubo e virou uma escultura por volumes (SDF, src/core/sdf_head.cpp): soma suave de ~40 formas anatômicas (crânio, testa, arcos, glabela, zigomáticos, maxila, lateral do rosto, mandíbula, ramo, masseter, queixo, boca, lábios com arco do cupido, nariz com dorso/ponta/asas/columela/narinas, órbitas, pálpebras com vinco, orelhas com hélice/concha/lóbulo, pescoço com esternocleidomastoideos e pomo de Adão). Malha por "surface nets" com banda estreita, vértices projetados na superfície e normais do gradiente (sombreado liso de escultura); oclusão calculada pela própria forma (narinas, concha, cantos).
- Ruling: o herói usa células de 2,6 mm (~37 mil vértices, ~74 mil triângulos só na cabeça); moradores 3,8 mm. Malhas geradas uma vez e reaproveitadas (só escala). Carregamento +1 s. Benchmark Baixo máx. 267 mil / 113 chamadas.
- Ruling: modo estúdio nos prints (--estudio, --argila): só o personagem, fundo escuro, luz de estúdio, material de argila cinza — para comparar a forma com esculturas.
- Ruling: barba e sobrancelhas desenhadas na pele por pixel (sobrancelha com traços mais longos para os lados); cílios continuam em cartões; máscaras de barba e cabelo recalibradas para a nova cabeça.
- Ruling: teste do maxilar trocado de ≥88% para 70–85% da largura das maçãs — as referências do usuário afinam em V e a anatomia real fica entre ~70% e 80%; o limite de 88% era da cabeça antiga.
- Ruling: teste de sobrancelha verifica a marcação na pele (direção lateral), não mais cartões.

## Rosto menos exagerado ("tá muito chad"), sobrancelha mais grossa, barba sem bochecha
- Ruling: queixo e mandíbula subiram ~1,2 cm (terço inferior / terço médio de 1,42 para ≤1,25, como num rosto comum), queixo menor e atrás do lábio de baixo, arco da sobrancelha e glabela mais suaves, ângulo da mandíbula suavizado, lábios mais finos, pomo de Adão discreto. Testes novos fixam essas proporções.
- Ruling: barba só no bigode, cavanhaque e faixa embaixo da mandíbula — o usuário pediu "nem existe pelo aí" nas bochechas; teste: nenhum pelo na bochecha.
- Ruling: sobrancelha com ~8 mm de altura no meio (antes ~5 mm); teste conta pelos acima de 30% (como aparece desenhado), pois os vértices ficam a ~2,5 mm.
- Ruling: teste do maxilar mede agora na altura do novo ângulo da mandíbula (1,612–1,622 m).

## Correções marcadas pelo usuário (nariz/boca, atrás da orelha, bochecha) e sem barba
- Ruling: o caçador fica sem barba (`Visual.barba = false` no jogador); a barba continua disponível para moradores e testes.
- Ruling: linha da boca virou um corte limitado (elipse no plano xz com espessura em y, afinando até a comissura). O elipsoide achatado de 0,6 mm subestimava a distância e o corte "vazava" pela bochecha como um vinco horizontal — era a mancha ao lado da boca.
- Ruling: lábios curvados pela arcada dentária (cantos ~7 mm atrás do meio), com volume de novo; a cor do vermelhão sai da própria escultura (`sdf::labios`) no meio e de uma faixa que afina e desbota até a comissura nos cantos (onde o lábio some sob a pele). Os furinhos dos cantos saíram (AO escuro demais).
- Ruling: sob o lábio inferior havia uma cavidade de ~14 mm (sulco do queixo fundo + frente da mandíbula somando misturas); agora o sulco é raso (~2,5 mm atrás da linha lábio–queixo) e a frente da mandíbula recuou para o queixo dominar.
- Ruling: nariz com base triangular contínua, asas maiores e mais fundidas, sem o sulco duro da asa; narinas em gota inclinada 20°, abertas para baixo e rasas na malha (a grade de 2,6 mm não resolve uma cavidade fina); o fundo escuro é pintado na pele (`sdf::narina`).
- Ruling: bochecha baixa preenchida e emendas da lateral do rosto e de sob os olhos mais macias — sem o "focinho" em volta da boca e sem vinco ao lado do nariz.
- Ruling: esternocleidomastoideo nasce dentro da cabeça (sem calombo atrás da orelha) e a nuca ganhou volume do trapézio até o occipital; pomo de Adão mais discreto.
- Ruling: sombra de perto do personagem com PCF bilinear numa grade 3×3, sem ruído — a sombra do nariz ficava pontilhada (parecia sujeira/barba). Sardas redondas.
- Ferramenta: visualizador por raymarching da SDF (fora do repositório) com vistas de frente/perfil/baixo e mapa de cavidade para achar vincos — iterar a escultura sem abrir o jogo.
- Benchmark Baixo: máx. 264 mil triângulos / 113 chamadas.
