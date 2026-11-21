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
