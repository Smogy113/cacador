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

Uma comparação do trajeto em `--benchmark --frames 3 --qualidade alto` usou Mesa llvmpipe,
1280×720 e 144 amostras por versão. A base contém o mesmo código de medição aplicado à versão 0.26.0.

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
./build/cacador --screenshot interior.png --qualidade alto --pos -219.94 143.38 --yaw 165 --hora 22 --portas-abertas --frames 16
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
