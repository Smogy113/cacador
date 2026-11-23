# Desempenho do Caçador

Compile em `Release` e execute na raiz do projeto, com a mesma resolução, qualidade gráfica,
mapa e driver nas duas versões. O benchmark imprime o renderizador OpenGL e a resolução.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/testes
./build/cacador --benchmark --qualidade alto
./build/cacador --benchmark --qualidade alto --pos 320 345 --yaw 90 --hora 12 --frames 120
```

No Windows, rode o executável na pasta que contém `data/`. O PowerShell pode gravar o resultado em um arquivo.

```powershell
Start-Process .\cacador.exe -ArgumentList '--benchmark --qualidade alto' -Wait -NoNewWindow -RedirectStandardOutput benchmark-alto.txt
```

- O trajeto padrão passa por 12 pontos em quatro horários e mede o tiroteio em separado.
  `--pos X Z` ou `--tiroteio` mede uma cena específica. `--frames N` define o número de amostras
  por cena. O padrão é 12, com três quadros de aquecimento antes de cada ponto do trajeto.
- Compare a média e o **p95 do tempo de quadro**. O p95 é o tempo que 95% das amostras cumprem.
  Para referência, 60 FPS correspondem a 16,7 ms por quadro e 20 FPS a 50 ms. O benchmark
  desliga VSync e o limite de FPS e avança a simulação em passos de 1/60 s.
- Rode pelo menos três vezes na mesma máquina para avaliar a variação. O Mesa llvmpipe
  renderiza na CPU. Use a GPU do jogador para medir o ganho em jogo.
- Use **F1** em outra execução para localizar os passes mais caros. Os contadores de geometria
  do benchmark cobrem o passe principal. O painel mede sombras, luz das paredes e efeitos
  de tela separadamente.

## Medição de referência

A comparação da etapa de otimização até o commit `80ccaec` usou
`--benchmark --frames 3 --qualidade alto`, Mesa llvmpipe, 1280×720 e 144 amostras por versão.
A base contém o mesmo código de medição aplicado à versão 0.26.0.

| Trajeto no Alto | Base | Otimizado |
| --- | ---: | ---: |
| Tempo de quadro médio | 291,03 ms | 260,47 ms |
| Tempo de quadro p95 | 398,40 ms | 366,58 ms |
| Triângulos por quadro, média | 289.239 | 216.786 |
| Chamadas de desenho por quadro, média | 212 | 149 |

## Verificar o visual

Use os mesmos argumentos nas duas versões e compare cidade de dia, vilarejo à noite, vegetação
e interior com uma porta aberta. `--screenshot` fixa a hora e usa o quadro simulado para
animar vento e granulação.

```bash
./build/cacador --screenshot cidade.png --qualidade alto --pos 320 345 --yaw 90 --hora 12 --frames 16
./build/cacador --screenshot interior.png --qualidade alto --pos -214.38 122.61 --yaw -15 --hora 22 --portas-abertas --frames 16
```

## Onde atuar nos gargalos

- Geometria em `src/render/terrain_mesh.cpp`, `props.cpp` e `grama.cpp`. Reaproveite os
  vértices quando posição, normal, cor e material coincidirem. O descarte por volume de visão
  usa as matrizes do passe atual. Inclua o deslocamento máximo do shader nos limites de malhas
  que se movem, como a grama.
- Luz das paredes em `src/render/lighting.cpp`. A geometria estática alimenta o atlas por face.
  Ao mudar o cache, confira a primeira visita, o retorno à área e as trocas de qualidade,
  com sol e à noite. Os quadros logo após um teleporte mostram o custo de atualizar o atlas.
- Sombras e materiais em `src/render/shaders.cpp`. Preserve as amostras, distâncias e fórmulas
  dos efeitos. Procure primeiro operações repetidas e cálculos cujo peso na imagem seja zero.

## Profundidade das construções

No Alto do PC, a profundidade das construções visíveis é preenchida antes do terreno.
O renderizador pode rejeitar os fragmentos encobertos antes do cálculo de iluminação e texturas.
Os passes de profundidade e cor usam a mesma seleção de malhas e o mesmo nível de detalhe.
A posição de cada vértice precisa coincidir entre os shaders para o teste `GL_LEQUAL` aceitar
o desenho de cor. As chamas são desenhadas no passe de cor quando estão acesas.

O mapa de sombras usa uma variante de profundidade para terreno e vegetação opacos. Ao adicionar
um material com recorte a essas malhas, mantenha sua condição de recorte no passe de profundidade.
No F1, o pré-passe entra no tempo de **Terreno**. Os triângulos e as chamadas impressos pelo
benchmark continuam contando os desenhos de cor do passe principal.

Comparação com `9ac6c568` em 1280×720, nível Alto, cabelo ondulado e Mesa llvmpipe
(LLVM 19.1.7, duas threads). Os valores são as medianas de três rodadas de 24 quadros
por cena. A cidade usa `--pos 320 345 --yaw 90 --hora 12`. O interior usa
`--pos -219.94 143.38 --yaw 165 --hora 22 --portas-abertas`.

| Cena | Média antes → depois | p95 antes → depois |
| --- | ---: | ---: |
| Cidade | 482,24 → 394,58 ms | 611,79 → 426,29 ms |
| Interior | 434,23 → 296,60 ms | 494,53 → 330,50 ms |

Na floresta (`--pos -382 -249 --yaw 85 --hora 7.25 --pitch 6`), as médias de rodada
variaram de 327 a 344 ms na base e de 313 a 395 ms com a mudança. Em duas rodadas adicionais
de 36 quadros, com o processo fixado a um núcleo e `LP_NUM_THREADS=1`, a mediana dos tempos
médios foi de 667,47 para 641,14 ms e a dos p95 foi de 752,72 para 720,08 ms.

## Iluminação do nível Alto

- O mapa de sombras é compartilhado por terreno, construções, personagens e grama. As
  leituras de profundidade localizam os bloqueadores e o filtro de comparação da GPU suaviza
  as bordas. O custo da grama fica em uma leitura filtrada por fragmento. O atlas de paredes
  fornece oclusão e luz rebatida.
- Os fachos reaproveitam o mapa de sombras e marcham em meia resolução até a profundidade da
  primeira superfície. A oclusão em tela também usa a profundidade para preservar silhuetas.
  Use as seções **Sombras**, **Pós: raios de sol** e **Pós: oclusão** do F1 ao comparar custos.
- Para conferir a contraluz na floresta no Alto, use
  `--pos -382 -249 --yaw 85 --hora 7.25 --pitch 6`. Na cidade, use
  `--pos 320 345 --yaw 90 --hora 12` para ver sombras de bancas e personagens na grama.
  Na margem do rio, use `--pos 83 -100 --yaw -120 --hora 18.25 --pitch 14`.

Com `--benchmark --frames 40 --qualidade alto`, em 1280×720 no llvmpipe, a cidade passou de
303,82 para 305,70 ms em média por quadro. A floresta passou de 259,24 para 238,69 ms.

## Grama

- `src/render/grama.cpp` monta uma malha indexada por bloco de 8 m. A ordem dos índices mantém
  as folhas principais primeiro, para desenhar o mesmo bloco à distância com menos triângulos.
  A semente de cada tufo conserva as folhas principais quando o bloco recebe folhas curvas.
- As folhas extras desaparecem gradualmente até 12 m no `FS_LIT`. Ao alterar esse raio,
  ajuste também `RAIO_DETALHE` no C++. Se mudar o vento, revise a folga da caixa da malha.
- Para comparar silhueta e cobertura no Alto, use
  `--pos 315 346 --yaw 90 --hora 12 --pitch 39 --dist-camera 2.6`. Confira também a margem
  do rio, as bordas das trilhas e a seção **Grama** do F1.

Em 32 quadros por cena no Alto, a 1280×720 no llvmpipe, o tempo médio passou de 262,04 para
280,99 ms na cidade de perto e de 273,65 para 259,39 ms na floresta.

## Chão

- No Médio e no Alto do PC, `SOLO_DETALHADO` usa mapas de cor de 2048×2048 e normais de
  1024×1024. Mipmaps e filtro anisotrópico ajudam a manter o detalhe em ângulos rasantes.
  O shader só amostra os materiais presentes no fragmento e reduz o detalhe entre 45 e 100 m.
- O LOD de 1 m fica nos pedaços mais próximos no Alto, fora da linha da água. A altura vem da grade
  existente, compartilhada com o restante do jogo. Ao ajustar os raios em `terrain_mesh.cpp`, confira
  o custo dos passes **Terreno** e **Sombras** no F1 e caminhe entre pedaços para ver a transição.
- Para comparar texturas e relevo, use `--pos -40 10 --yaw 160 --hora 12 --pitch 36 --dist-camera 3`
  na estrada e `--pos -420 -230 --yaw 120 --hora 8 --pitch 24 --dist-camera 3.2` na encosta.

Com 32 quadros por cena no Alto, a 1280×720 no llvmpipe, o tempo médio passou de 209,65
para 229,02 ms na estrada e de 194,60 para 214,37 ms na encosta. A contagem de chamadas de
desenho permaneceu igual nas duas cenas.

## Física do cabelo

O penteado ondulado usa três guias, um em cada lateral e outro na nuca. `hair_physics.cpp`
simula a dobra e a ponta de cada guia, com a raiz presa à cabeça. Os subpassos de até 1/120 s
limitam a variação com o FPS. O shader interpola os deslocamentos entre os guias e reaproveita
a mesma deformação no passe principal e nas sombras, com a malha original na GPU.

Ao ajustar rigidez, amortecimento ou colisão, use `--cabelo ondulado --retrato .65 --girar-camera 75 --direto`
para observar uma corrida, uma parada e uma virada de perto. Confira também o movimento a cavalo.
