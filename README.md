# Caçador (nome provisório)

Mundo aberto 3D low-poly, medieval, com clima de faroeste: você é um caçador de recompensas
numa fronteira com cidade murada, vilarejo, fazendas, floresta, rio, ruínas de castelo,
acampamento de bandidos, mina e cemitério. Feito em C++ com raylib, sem texturas nem arquivos de
modelo: tudo é gerado pelo código.

Pessoas e cavalos têm corpo anatômico (~1.200 e ~1.500 triângulos) com esqueleto animado na placa
de vídeo: andar, correr, respirar parado e cavalgar (passo, trote e galope).

**v2 (jogável):** pegue uma recompensa no quadro, siga a bússola até o esconderijo, enfrente o
procurado e o bando dele no revólver e traga o alvo vivo (vale mais) ou morto ao xerife, ao lado do
quadro. Cada procurado tem um temperamento — uns se rendem, outros lutam até o fim, outros fogem —
e o cartaz dá a pista. Se você cair, acorda na cidade e a recompensa continua.
Missões de história e loja vêm depois.

## Como jogar (Windows)

Abra `cacador.exe` (a pasta `data/` precisa estar ao lado dele).

| Tecla | Ação |
|---|---|
| W A S D | andar (no cavalo: W anda, A/D viram, S freia) |
| Shift | correr / galopar |
| Mouse | girar a câmera |
| E | montar, desmontar, abrir e fechar portas, ver o quadro de recompensas, puxar o preso pela corda, entregar ao xerife |
| Segurar E | amarrar o procurado rendido |
| Enter (no quadro) | aceitar a recompensa do cartaz |
| Botão direito | mirar (câmera sobre o ombro) |
| Botão esquerdo | atirar |
| R | recarregar o revólver (6 balas) |
| Esc | pausa (lá dentro: 1/2/3 trocam a qualidade gráfica, Q sai) |

Qualidade gráfica: **Baixo** (PC fraco), **Médio** (sombras suaves) e **Alto** (sombras suaves,
oclusão de ambiente em tela, brilho em volta das luzes e raios de sol). Em todos os níveis há luz
rebatida do chão e contorno de luz do céu. Na primeira vez o jogo escolhe um nível pela placa de vídeo e
salva em `config.txt`.

**O caçador de perto (0.18):** no Médio e no Alto, a cabeça do caçador não é feita de triângulos:
é desenhada pixel a pixel (raymarching) a partir da escultura guardada num bloco 3D. Não tem facetas
em nenhuma distância e ganha luz "de cinema": sombra suave do nariz e da sobrancelha no próprio
rosto, orelhas que avermelham contra o sol e brilho oleoso na testa e no nariz (estes três só no
Alto). O caçador também tem luz de personagem (contraluz e preenchimento) e olhos com reflexo. No
Alto a câmera desfoca de leve o fundo distante (mais forte na mira) e tem granulação de filme e
aberração cromática sutil nas bordas.

**Paredes (0.19):** no Médio e no Alto, as paredes das casas têm relevo desenhado pixel a pixel
(tábuas com frestas, pedra com argamassa funda, reboco rachado e descascado mostrando a pedra) e luz
traçada contra as construções de verdade: escuro sob o beiral e junto ao alicerce e, à noite, a
janela acesa clareando a parede. No Alto ainda há sombra suave do sol traçada contra casas e
telhados, sombra de cada tábua e pedra, e a luz que uma parede ensolarada rebate na casa em frente.

**Casas e disparos (0.20):** use E perto de uma porta para entrar nos prédios. A câmera se aproxima
em cômodos fechados e a lanterna continua útil à noite. Os passos mudam sobre o piso das casas,
os tiros reverberam nos interiores e as paredes abafam disparos do outro lado. O clarão e a fumaça
saem do cano, e as pontas do cabelo do caçador reagem ao movimento e ao vento.

**Painel de depuração (F1):** em "Gráficos" dá para ligar e desligar a cabeça por raymarching (com
o tempo do quadro ao lado, para comparar o custo), a luz de personagem, a profundidade de campo e a
granulação/aberração.

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
`--montado`, `--abrir-quadro`, `--mapa arquivo.txt`, `--recompensa N` (aceita o cartaz N),
`--tiroteio` (começa perto do bando, já em combate), `--mirar`, `--estudio --retrato 0.5` (só o
caçador, de perto), `--sem-raio` (cabeça de triângulos), `--sem-luz-personagem`, `--sem-foco`, `--sem-paredes`.
