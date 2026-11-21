# Caçador (nome provisório)

Mundo aberto 3D low-poly, medieval, com clima de faroeste: você é um caçador de recompensas
numa fronteira com cidade murada, vilarejo, fazendas, floresta, rio, ruínas de castelo,
acampamento de bandidos, mina e cemitério. Feito em C++ com raylib, sem texturas nem arquivos de
modelo: tudo é gerado pelo código.

Pessoas e cavalos têm corpo anatômico (~1.200 e ~1.500 triângulos) com esqueleto animado na placa
de vídeo: andar, correr, respirar parado e cavalgar (passo, trote e galope).

Esta é a **v1: só o mundo aberto**. Combate, captura, perseguições e missões vêm depois.

## Como jogar (Windows)

Abra `cacador.exe` (a pasta `data/` precisa estar ao lado dele).

| Tecla | Ação |
|---|---|
| W A S D | andar (no cavalo: W anda, A/D viram, S freia) |
| Shift | correr / galopar |
| Mouse | girar a câmera |
| E | montar, desmontar, ver o quadro de recompensas |
| Esc | pausa (lá dentro: 1/2/3 trocam a qualidade gráfica, Q sai) |

Qualidade gráfica: **Baixo** (PC fraco), **Médio** (sombras suaves) e **Alto** (sombras suaves,
oclusão de ambiente em tela, brilho em volta das luzes e raios de sol). Em todos os níveis há luz
rebatida do chão e contorno de luz do céu. Na primeira vez o jogo escolhe um nível pela placa de vídeo e
salva em `config.txt`.

Um dia completo dura 20 minutos. À noite as tochas acendem, a maioria dos moradores vai para casa
e a sua lanterna ilumina o caminho.

## O mapa

Os lugares ficam em `data/lugares.txt` (dá para editar). Cada linha é
`lugar <tipo> <nome> <x> <z> <rotação> <raio>` ou `estrada <lugarA> <lugarB>`. Se uma linha tiver
erro, o jogo abre mesmo assim e mostra o problema na tela.

## Como compilar

Precisa de CMake e de um compilador C++17. A raylib e o doctest são baixados pelo próprio build.

```bash
cmake -B build && cmake --build build -j8   # jogo (Linux) e testes
./build/testes                               # testes da lógica
scripts/build-windows.sh                     # build-win/cacador.exe (MinGW)
scripts/build-web.sh                         # build-web/ (versão de teste no navegador, precisa do emsdk)
scripts/screenshot.sh saida.png --hora 18.5  # tira um print sem abrir janela (Xvfb)
./build/cacador --benchmark                  # mede triângulos e chamadas de desenho num trajeto fixo
```

Opções úteis para testes: `--hora H`, `--pos X Z`, `--yaw graus`, `--qualidade baixo|medio|alto`,
`--montado`, `--abrir-quadro`, `--mapa arquivo.txt`.
