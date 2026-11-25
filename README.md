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
| Rodinha do mouse | para cima aproxima até a primeira pessoa e para baixo afasta |
| E | montar, desmontar, abrir e fechar portas, ver o quadro de recompensas, puxar o preso pela corda, entregar ao xerife |
| Segurar E | amarrar o procurado rendido |
| Enter (no quadro) | aceitar a recompensa do cartaz |
| Botão direito | mirar |
| Botão esquerdo | atirar |
| R | recarregar o revólver (6 balas) |
| 1 / 2 / 3 / 4 | munição comum / balas gravadas dos slots 1 a 3 (Circuitos de Queima) |
| Tab | passar para a próxima munição gravada |
| E (na fogueira) | gravar munição: entalhe + pasta + objeto doador + preparo (cuidadoso ou rápido) |
| E (numa pista) | inspecionar o equipamento entalhado e aprender o entalhe ou a pasta dele |
| E (numa cápsula) | recolher a cápsula marcada e apagar o rastro |
| Clicar 2× com o pavio úmido | forçar a bala gravada logo depois de sair da água (risco de rebote) |
| F3 | mostrar/esconder a depuração do combate reativo (estados dos inimigos, receitas, rebote) |
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

**Circuitos de Queima (0.28, protótipo):** a magia passa a propriedade de um objeto para a bala.
Na fogueira (E), você grava até três balas, cada uma com três partes:
- **Entalhe:** Espiral é velocidade sem gravidade, Grade é peso esmagador e Onda é calor.
- **Pasta:** Pólvora Fina concentra tudo no ponto do impacto e Resina e Cera faz a bala ricochetear
  até 3 vezes mantendo o efeito. Enxofre e Sal espalha o efeito em área. Pó de Prata inverte o
  entalhe: Onda vira gelo, Grade vira um impulso que arremessa para cima e Espiral vira um voo
  lento em arco.
- **Objeto doador:** a Espiral exige Pena, a Grade exige Pedra Pesada e a Onda exige Carvão em
  Brasa. O doador queima na boca do cano no disparo.

Escolha a munição com 1 (comum) e 2–4 (gravadas), ou passe para a próxima com Tab. Cada disparo de
bala gravada larga uma cápsula marcada no chão, e E recolhe a cápsula e apaga o rastro. Com o pavio
molhado (dentro da água ou da chuva), a bala gravada falha e não sai nada do cano. Por 5 s depois de
sair, o pavio fica úmido: o primeiro clique falha e avisa, e um segundo clique em até 1,5 s força o
disparo, com risco de rebote. No Ferro Frio, nenhum circuito acende, e o projétil que atravessa a
barreira vira chumbo comum.

A entrada de Pedrabranca tem uma arena de teste com a fogueira, um corredor de paredes para o
ricochete, quatro bonecos de treino, a zona de chuva e a barreira de Ferro Frio, com um boneco
atrás dela. Os bandidos também levam os golpes: são empurrados, derrubados, arremessados e
congelados. Tudo é provisório, feito com primitivas e sprites, sem depender do modelo final do
caçador. Os números de ajuste ficam em `ParametrosCircuito` ([src/core/circuito.h](src/core/circuito.h)):
slots, ricochetes, raio do Enxofre, duração das zonas, secagem do pavio e outros. A regra fica em
`src/core/circuito.cpp`, `projetil_encantado.cpp` e `fisica_alvo.cpp`, testada em
`tests/test_circuitos.cpp`. A interface e o desenho ficam em `src/game/circuitos_jogo.cpp`.

**Combate reativo (0.29, protótipo):** ao lado da arena há um pátio de patrulha com quatro inimigos
de teste, manequins de pano azul-ardósia que reagem aos circuitos:
- **Patrulheiro:** faz um laço que passa perto da fogueira.
- **Dupla A e Dupla B:** vão e voltam lado a lado. A Grade com Enxofre e Sal empurra os dois para
  fora do impacto, derruba e deixa os dois **Atordoados** (estrelas sobre a cabeça, sem andar).
- **Sentinela:** fica agachado atrás de uma cobertura baixa, entre duas paredes laterais viradas a
  45°. O tiro reto para na cobertura. A Espiral com Resina e Cera, mirada na parede lateral ao lado
  da cobertura, ricocheteia de lado e acerta o Sentinela.

A Onda com Pó de Prata deixa uma zona congelante no chão, e quem pisa nela fica **Congelado**, andando
a 20% da velocidade. Uma cápsula marcada esquecida no chão chama atenção: o inimigo em patrulha que a
vê (até 8 m no cone de visão, ou 2,5 m em volta) passa a **Alerta**, vai até ela, olha e volta à
rota. Quem leva um golpe também fica em alerta e vai até de onde veio o tiro.

**Engenharia reversa:** o caçador começa sabendo só a Espiral e a Pólvora Fina. A fogueira mostra o
resto como "desconhecido" e não grava. Cada inimigo derrotado larga uma vez um equipamento entalhado:
Resina e Cera (Patrulheiro), Enxofre e Sal (Dupla A), Pó de Prata (Dupla B) e o entalhe Onda
(Sentinela). A cartucheira com a Grade está no chão, perto dos bonecos. Chegue perto e use E para
inspecionar e aprender. Os inimigos voltam ao começo da rota 20 s depois de derrotados.

**Rebote:** na fogueira, a última linha escolhe o preparo. O **Cuidadoso** leva 3 s e não tem risco.
O **Rápido / Imperfeito** grava na hora, mas a bala tem 35% de chance de estourar na câmara. O pavio
úmido também arrisca, de 30% a 85% conforme a umidade, e os dois riscos se somam. No rebote, a bala e o
doador se perdem, o caçador leva 12 de dano, a mira dá um tranco para cima e treme por 1,4 s, e o
revólver fica travado por 0,9 s.

A depuração (F3, ligada por padrão) mostra o estado sobre cada inimigo (Patrulha, Alerta, Congelado,
Atordoado, Derrotado), o cone de visão, a rota e um quadro com vida, receitas desbloqueadas,
vestígios descobertos e o risco de rebote da bala na vez. A regra fica no núcleo, sem raylib e sem a
malha do personagem:
- `src/core/inimigo_teste.*`: patrulha, percepção e investigação.
- `src/core/receitas.*`: o que o caçador sabe e as pistas.
- `src/core/rebote.*`: a decisão do gatilho e o tremor da mira.
- `src/core/baia_cobertura.*`: a geometria da cobertura.

O corpo dos inimigos reusa `core/fisica_alvo`, agora com atordoamento e lentidão. Os testes ficam em
`tests/test_combate_reativo.cpp`, e o jogo e o desenho em `src/game/combate_reativo.cpp`. A planta
da arena está em `src/game/arena_teste.h`. Os números ficam em `ParametrosInimigo` e em
`ParametrosCircuito`, nas seções de reação dos alvos e de preparo e rebote.

**Painel de depuração (F1):** em "Gráficos" dá para ligar e desligar a cabeça por raymarching (com
o tempo do quadro ao lado, para comparar o custo), a luz de personagem, a profundidade de campo e a
granulação/aberração.

Um dia completo dura 20 minutos. À noite as tochas acendem, a maioria dos moradores vai para casa
e a sua lanterna ilumina o caminho.

## O mapa

Os lugares ficam em `data/lugares.txt` (dá para editar). Cada linha é
`lugar <tipo> <nome> <x> <z> <rotação> <raio>` ou `estrada <lugarA> <lugarB>`. Se uma linha tiver
erro, o jogo abre mesmo assim e mostra o problema na tela.

### Escala do mundo

Uma unidade equivale a um metro nos três eixos. O terreno tem 1,5 km de lado e a área
jogável tem 1,4 km de lado. O mapa aberto com **M** inclui uma régua de 100 m.
As medidas compartilhadas pelo cenário e pelas colisões ficam em `src/core/medidas_mundo.h`.

| Elemento | Medida usada no jogo |
|---|---|
| Caçador | 1,82 m de altura |
| Porta residencial | 1,04 m de largura por 2,12 m de altura |
| Estrada de terra | 4 m no leito, com transição nas bordas |
| Canal do rio | 18 m de largura nominal |
| Ponte | tabuleiro de 4,20 m e guarda-corpo de 1,05 m |
| Árvores | pinheiros em torno de 10–17 m e carvalhos de 8–14 m |

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
| Pedrabranca | `gerarVilarejo` | 6,8–9,2 × 5,8–7,8 m |
| Fazendas | `gerarFazenda`, no lote `casa` | 9,5 × 7,5 m |

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

### Cabelo do caçador

Três penteados (`src/core/penteado.h`): sem cabelo, ondulado (`hair.cpp`, na altura do queixo) e
longo (`cabelo_longo.cpp`, até os ombros). Eles aparecem no menu Personagem, no `config.txt`
(`cabelo=longo`) e na linha de comando (`--cabelo longo`).

Cada penteado é feito de fitas presas à cabeça. A física (`hair_physics.cpp`) move alguns guias e
o vertex shader (`FISICA_MECHA` em `src/render/shaders.cpp`) curva as fitas a partir deles. Para o
cabelo não atravessar o corpo, há três camadas:

1. **Montagem** (`cabelo_corpo.h`, `superficie.h`): as fitas são montadas fora da pele de verdade
   e das orelhas, que entram como volumes sólidos. O cabelo longo cai até assentar nos ombros.
2. **Apoio por vértice**: cada vértice guarda o plano da pele mais próxima e o osso que a move
   (`VertPele::apoio*`). Em movimento, ele não atravessa esse plano.
3. **Volumes justos**: orelhas, pescoço e alto do tronco (`ColisoresCabelo`, medidas em
   `cabelo_corpo.h`) seguram os cantos côncavos. A física e o shader usam os mesmos volumes.

`posicaoDaFita` (core) repete em C++ a conta do shader. Os testes em `tests/test_cabelo_corpo.cpp`
usam essa função para conferir o cabelo em repouso e em movimento: corrida, meia-volta e cabeça
girando. Se mexer no shader, mexa nela também.

```bash
./build/testes --test-case='cabelo*,superficie*,base humana*'
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
caçador, de perto), `--sem-raio` (cabeça de triângulos), `--sem-luz-personagem`, `--sem-foco`, `--sem-paredes`,
`--resolucao L A` (abre a janela nesse tamanho, por exemplo `--resolucao 1920 1080` para conferir a
interface em Full HD), `--menu [principal|personagem|configuracoes]` (abre no menu inicial, nessa tela;
com `--screenshot`, tira o print do cartaz).

No Windows sem WSL, o MinGW-w64 do WinLibs já traz GCC, CMake e Ninja (`winget install BrechtSanders.WinLibs.POSIX.UCRT`).
Com a pasta `mingw64/bin` dele no `PATH`, `cmake -B build -G Ninja && cmake --build build` gera o jogo e os testes.
O `testes.exe` precisa dessa pasta no `PATH` para achar as DLLs do GCC.

Para os Circuitos de Queima, `--roteiro-circuito 1..10` simula a entrada:

| Roteiro | O que testa |
| --- | --- |
| 1 | Espiral com Resina quicando no corredor |
| 2 | Grade com Enxofre nos bonecos |
| 3 | Onda com Pó de Prata (zona de gelo) |
| 4 | Pavio molhado na chuva |
| 5 | Tiro atravessando o Ferro Frio |
| 6 | Grade com Enxofre atordoando a Dupla |
| 7 | Onda com Prata deixando o Patrulheiro lento |
| 8 | Espiral com Resina ricocheteando no Sentinela atrás da cobertura |
| 9 | Rebote da bala do Preparo Rápido |
| 10 | Patrulheiro achando a cápsula no chão e indo investigar |

Os roteiros liberam todas as receitas. Outras opções:
- `--abrir-fogueira` começa com o painel da fogueira aberto.
- `--receitas-todas` começa sabendo todos os entalhes e pastas.
- `--sem-entrada` roda o roteiro sem tirar print.
- `--congelar-no-quadro N` para a lógica no quadro N e continua desenhando.

As duas últimas servem para a versão web.

**Windows com o Smart App Control ligado:** o Windows bloqueia executáveis recém-compilados, como
`cacador.exe` e `testes.exe`, e algumas ferramentas do Emscripten. Para testar assim:
1. Compile os testes e o jogo para a web com
   `emcmake cmake -B build-web -G Ninja -DCACADOR_WEB_ASYNCIFY=OFF`.
2. Rode a lógica com `node build-web/testes.js`. Para os testes da cabeça, que usam threads, configure
   com `-DCMAKE_CXX_FLAGS=-pthread -DCMAKE_EXE_LINKER_FLAGS="-pthread -sPTHREAD_POOL_SIZE=16"`.
3. Abra o jogo servindo `build-web/` com `python -m http.server`.

Os argumentos vão na URL, separados por `&` ([web/argumentos.js](web/argumentos.js)):
`index.html?--direto&--sem-entrada&--roteiro-circuito&6&--congelar-no-quadro&45`.

Para repetir medições e comparar capturas entre versões, veja o [guia de desempenho](docs/DESEMPENHO.md).

Use `--direto` para abrir uma partida sem passar pelo menu inicial e
`--cabelo sem`, `--cabelo ondulado` ou `--cabelo longo` para escolher o visual daquela execução.
