# Caçador (nome provisório)

Mundo aberto 3D low-poly, medieval, com clima de faroeste: você é um caçador de recompensas
numa fronteira com cidade murada, vilarejo, fazendas, floresta, rio, ruínas de castelo,
acampamento de bandidos, mina e cemitério. Feito em C++ com raylib. A geometria é gerada
ou decodificada pelo próprio código.

O protagonista usa uma base humana real quantizada no executável (26,7 mil triângulos, cerca de
250 KB), em A-pose e pronta para receber cabelo e roupas como camadas. Pessoas distantes e cavalos
mantêm modelos leves com esqueleto animado na placa de vídeo: andar, correr, respirar parado e
cavalgar (passo, trote e galope).

**v2 (jogável):** pegue uma recompensa no quadro, siga a bússola até o esconderijo, enfrente o
procurado e o bando dele no revólver e traga o alvo vivo (vale mais) ou morto ao xerife, ao lado do
quadro. Cada procurado tem um temperamento — uns se rendem, outros lutam até o fim, outros fogem —
e o cartaz dá a pista. Se você cair, acorda na cidade e a recompensa continua.
Missões de história e loja vêm depois.

## Como jogar (Windows)

Abra `cacador.exe` (a pasta `data/` precisa estar ao lado dele).
No menu inicial, use o mouse ou as setas e Enter para jogar ou escolher a qualidade gráfica.
Em **Personagem**, escolha **Ondulado** ou **Sem cabelo**. A seleção é guardada em `config.txt` no PC.

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

O nível **Baixo** atende PCs modestos. O **Médio** usa sombras suaves e o **Alto** usa
sombras filtradas, oclusão de contato em tela e fachos de sol recortados pela geometria. No
Alto a sombra começa mais firme junto ao objeto e ganha penumbra conforme se afasta dele.
A água tem ondulações no reflexo do céu, do sol e da lua. Em todos os níveis há luz rebatida
do chão e preenchimento do céu. Na primeira vez o jogo escolhe um nível pela placa de vídeo e
salva em `config.txt`.

No **Médio** e no **Alto** do PC, o chão usa texturas de 2048×2048 e mapas de relevo para terra,
grama e pedra. No Alto, a malha próxima também usa o passo de 1 m da grade de alturas do mundo.

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
janela acesa clareando a parede. No Alto, as sombras diretas seguem o mapa do cenário e o
atlas guarda a oclusão de ambiente e a luz que uma parede ensolarada rebate na casa em frente.
As tábuas e pedras também fazem sombra no próprio relevo.

**Casas e disparos (0.20):** use E perto de uma porta para entrar nos prédios. A câmera se aproxima
em cômodos fechados e a lanterna continua útil à noite. Os passos mudam sobre o piso das casas,
os tiros reverberam nos interiores e as paredes abafam disparos do outro lado. O clarão e a fumaça
saem do cano, e as pontas do cabelo do caçador reagem ao movimento e ao vento.

**Telhados (0.26):** cada telhado tem espessura e uma cobertura desenhada no shader: palha penteada
em camadas, com beiral irregular e rolo na cumeeira (vilarejo); telhas capa e canal em fileiras,
com a sombra de cada fileira (cidade); e tábuas sobrepostas nas casas de madeira e no faroeste. A
palha e as telhas são desenhadas uma vez numa textura de 1024² ao abrir o jogo; as tábuas usam a
textura de madeira das paredes. O custo por quadro fica praticamente igual ao de antes.

**Andar de gente (0.27):** o caçador anda e corre com base em medidas de pessoas reais. O pé
pisa com o calcanhar, fica chato no chão e sai pela ponta sem escorregar. O quadril sobe e desce
uns 4 cm a cada passo, sem o tranco que havia logo depois de cada pisada, e o joelho de apoio
quase se estica no meio do passo. Correndo, há uma fase de voo e o ritmo caiu de ~275 para
~215 passos por minuto. O caçador acelera e freia em vez de mudar de velocidade de uma vez,
inclina o tronco ao arrancar e ao frear e tomba para dentro nas curvas. Para dar meia-volta,
ele primeiro diminui o passo. As pessoas da cidade e os bandidos usam o mesmo andar. O cálculo
da pose ficou ~25% mais barato.

O capim cresce em tufos irregulares, com folhas finas, alturas variadas e algumas pontas
secas. No Médio e no Alto, perto da câmera aparecem também folhas curvas que balançam com o
vento. A cobertura fica mais baixa junto às trilhas e aos poços, e muda de tom na margem do rio.

**Painel de depuração (F1):** em "Gráficos" dá para ligar e desligar a cabeça por raymarching (com
o tempo do quadro ao lado, para comparar o custo), a luz de personagem, a profundidade de campo e a
granulação/aberração.

Um dia completo dura 20 minutos. À noite as tochas acendem, a maioria dos moradores vai para casa
e a sua lanterna ilumina o caminho.

## O mapa

Os lugares ficam em `data/lugares.txt` (dá para editar). Cada linha é
`lugar <tipo> <nome> <x> <z> <rotação> <raio>` ou `estrada <lugarA> <lugarB>`. Se uma linha tiver
erro, o jogo abre mesmo assim e mostra o problema na tela.

## Onde mexer

- **Prédios** (`src/core/predio.cpp`): gramática lote → volume → fachadas → peças. A cobertura do
  telhado vem de `Lote::cobertura` (`Auto`, `Palha`, `Telha`, `Tabua`) e define espessura, beiral e
  cumeeira. As peças marcadas `detalhe` (pontas do beiral, vigas...) só aparecem de perto.
- **Materiais das construções** (`src/render/shaders.cpp`): `FS_ASSAR_MATERIAL` (paredes) e
  `FS_ASSAR_TELHADO` (telhados) desenham os padrões uma vez em texturas que repetem a cada 4 m.
  `relevoParede` e `relevoTelhado` só leem essas texturas. Para criar uma cobertura nova, some um
  `TIPO_*` em `src/core/meshbuilder.h`, um caso em `matTelhado` e, se precisar de padrão próprio, um
  canal na textura assada.

### Manutenção das casas

O tamanho e a posição das casas no mapa ficam em `src/render/props.cpp`. As medidas vão de
parede a parede, em metros. [Abrir o gerador do cenário](src/render/props.cpp)

| Lugar | Função | Largura × profundidade |
| --- | --- | --- |
| Valdoro | `gerarCidade` | 9–12 × 9–12 m |
| Pedrabranca | `gerarVilarejo` | 9–12 × 8–10,5 m |
| Fazendas | `gerarFazenda`, no lote `casa` | 12 × 10,5 m |

`Gerador::livre` verifica a distância entre lotes pela margem e amostra estrada e rio na
pegada informada. Nas fazendas, essa pegada inclui o avanço da varanda e do degrau. Se uma
casa mudar de tamanho, confira também o espaço até a estrada, o vizinho e a muralha.

`gerarPredio` recebe o lote em coordenadas locais, com a frente em `+z`. É ali que ficam a
altura dos andares, os vãos e o telhado. A mesma planta fornece a malha detalhada, a casca de
longe e as colisões. [Abrir o gerador da planta](src/core/predio.cpp)

Em `props.cpp`, `Gerador::predio` coloca as peças, pisos e portas nas coordenadas do mapa
e devolve a posição da entrada usada na rota dos moradores.

Com o build Linux pronto, confira os vãos, a circulação e o custo das malhas usando o
comando abaixo. [Testes de prédios](tests/test_predio.cpp) · [Capturas de referência](docs/DESEMPENHO.md#verificar-o-visual)

```bash
./build/testes --test-case='predio:*,porta:*,interior:*,telhado:*'
```

## Como compilar

Precisa de CMake e de um compilador C++17. A raylib e o doctest são baixados pelo próprio build.

```bash
cmake -B build && cmake --build build -j8   # jogo (Linux) e testes
./build/testes                               # testes da lógica
scripts/build-windows.sh                     # build-win/cacador.exe (MinGW)
scripts/build-web.sh                         # build-web/ (versão de teste no navegador, precisa do emsdk)
scripts/screenshot.sh saida.png --hora 18.5  # tira um print sem abrir janela (Xvfb)
./build/cacador --benchmark --qualidade alto # mede tempo de quadro, triângulos e chamadas num trajeto fixo
```

Opções úteis para testes: `--hora H`, `--pos X Z`, `--yaw graus`, `--qualidade baixo|medio|alto`,
`--montado`, `--abrir-quadro`, `--mapa arquivo.txt`, `--recompensa N` (aceita o cartaz N),
`--tiroteio` (começa perto do bando, já em combate), `--mirar`, `--andar` e `--correr` (o caçador
anda sozinho para a frente, para capturar a marcha), `--estudio --retrato 0.5` (só o
caçador, de perto), `--sem-raio` (cabeça de triângulos), `--sem-luz-personagem`, `--sem-foco`, `--sem-paredes`.

Para repetir medições e comparar capturas entre versões, veja o [guia de desempenho](docs/DESEMPENHO.md).

Use `--direto` para abrir uma partida sem passar pelo menu inicial e
`--cabelo ondulado` ou `--cabelo sem` para escolher o visual daquela execução.
