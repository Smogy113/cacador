# Decisões — personagem "mais AAA" (pedido do usuário)

- Ruling: o salto veio de materiais (não de mais polígonos): pele com subsuperfície (difusa "wrap" + espalhamento avermelhado), poros; tecido com trama e veludo; couro granulado com brilho e desgaste; metal com reflexo; cabelo em fios; olhos molhados; feltro no chapéu. Tudo calculado no shader a partir da posição de repouso (o detalhe acompanha o corpo) — custo só nos pixels dos personagens.
- Ruling: material por vértice (MAT_* 10–16) no lugar do "tipo"; janela/chama/água limitadas às faixas 1–3 para não confundir com materiais de personagem.
- Ruling: globos oculares de verdade (esclera, íris, pupila) dentro de órbitas mais fundas, com pálpebras e sulco nariz-boca esculpidos; costeletas removidas; barba e linha do cabelo com transição suave.
- Ruling: dobras no sobretudo (mais fortes na barra) e rugas na manga do cotovelo, deslocando a geometria.
- Ruling: teste de contagem do HD subiu de ≤14 mil para ≤16 mil triângulos (globos oculares + dobras).
- Limite conhecido: sem texturas, cores ficam nos vértices — o rosto ainda tem ar de ilustração. Próximo passo possível: textura de pele/roupa gerada por código (poucas centenas de KB).
