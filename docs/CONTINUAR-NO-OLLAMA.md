# Continuar o projeto "Caçador" com um modelo local (Ollama)

Este arquivo tem tudo que um assistente precisa para continuar o projeto: como preparar o
ambiente, o prompt de sistema para colar, o contexto do jogo, as regras do usuário, o jeito de
trabalhar (as "skills") e as armadilhas técnicas já descobertas.

---

## 1. Antes de tudo: o Ollama sozinho não mexe em arquivos

O Ollama só conversa. Para o modelo ler o código, editar arquivos, compilar e rodar os testes,
use um **agente de código** que aceite modelos do Ollama, por exemplo:

| Ferramenta | Como usar com Ollama |
|---|---|
| **Aider** (terminal) | `aider --model ollama_chat/qwen2.5-coder:32b` dentro da pasta do projeto |
| **OpenCode** (terminal) | configurar o provedor Ollama no `opencode.json` |
| **Continue** (VS Code) | adicionar o modelo do Ollama nas configurações e usar o modo "Agent" |
| **Goose** (terminal) | `goose configure` → provedor Ollama |

Dicas:
- Modelo sugerido: o maior modelo de **código** que a sua placa aguentar (ex.: `qwen2.5-coder:32b`,
  `qwen3-coder`, `deepseek-coder-v2`). Uma RTX 5070 (12 GB) roda bem modelos de ~14B; 32B fica lento.
- Aumente a janela de contexto: `OLLAMA_CONTEXT_LENGTH=32768` (ou `num_ctx` no Modelfile).
  Este projeto tem ~9 mil linhas; o modelo não consegue ler tudo de uma vez: peça tarefas pequenas
  e diga quais arquivos abrir.
- Modelos locais erram mais que modelos grandes na nuvem: mantenha os testes rodando a cada mudança.

O código está no GitHub (repositório privado do usuário): `https://github.com/Smogy113/cacador`
(ramo `master`). Clone no PC: `git clone https://github.com/Smogy113/cacador`.

---

## 2. Prompt de sistema (cole no agente)

```
Você é um programador de jogos experiente trabalhando com o usuário no jogo "Caçador"
(C++17 + raylib 5.5). Responda sempre em português do Brasil, de forma direta e clara.

Regras do usuário (obrigatórias):
- O jogo tem que ser LEVE e BONITO. Tudo é gerado por código: sem modelos 3D, sem texturas de
  arquivo, sem sons de arquivo. A única exceção são as fontes em data/fontes (licença OFL).
- Nível gráfico "Baixo" precisa ficar em no máximo 350 mil triângulos e 350 chamadas de desenho
  no benchmark (./build/cacador --benchmark --qualidade baixo). A versão web (WebGL1/GLSL 100)
  precisa continuar compilando.
- Não mande o .exe/zip a cada mudança; só quando o usuário pedir. Imagens do resultado são bem-vindas.
- Se o usuário escrever em inglês com erros, corrija o inglês dele.
- A cada entrega, suba a versão em src/versao.h (MAIOR.MENOR.CORRECAO) e use esse número no
  nome do pacote (cacador-X.Y.Z.zip).
- Antes de afirmar que algo funciona, rode os testes (./build/testes) e mostre o resultado.
- Commits pequenos e frequentes, mensagem em português no estilo "feat: ...", "fix: ...".

Jeito de trabalhar:
1. Entender o pedido: se algo importante estiver aberto, faça UMA pergunta por vez.
2. Para mudanças grandes: escrever um design curto (docs/superpowers/specs/), depois um plano em
   tarefas pequenas (docs/superpowers/plans/), e só então codar.
3. TDD: escrever o teste que falha (tests/*.cpp, doctest), ver falhar, implementar, ver passar,
   rodar a suíte inteira.
4. Lógica pura vai em src/core/ (testável, sem raylib); desenho e entrada em src/game/ e src/render/.
5. Conferir visualmente com screenshots (scripts/screenshot.sh) quando mexer em visual.
6. Ao terminar: resumir em português o que mudou, o que foi verificado e o que ficou pendente.
```

---

## 3. O jogo (estado atual: versão 0.17.0)

Mundo aberto 3D medieval com clima de faroeste ("um Red Dead medieval"): você é um caçador de
recompensas. Câmera em 3ª pessoa atrás do ombro. Roda em PC fraco (Windows) e no navegador.

O que já existe:
- **Mundo**: terreno por ruído com 3 níveis de detalhe, rio, estradas, montanhas, lugares lidos de
  `data/lugares.txt` (cidade Valdoro, vilarejo Pedrabranca, fazendas, ruínas, acampamento, mina,
  cemitério, ponte, floresta); dia/noite de 20 min; tochas; moradores com rotina; cavalos
  (passo/trote/galope); grama densa com vento (Médio/Alto) e tufos (Baixo); nuvens volumétricas.
- **Personagens**: corpo anatômico por "loft" com esqueleto de 17 ossos deformado na GPU; cabeça
  esculpida por SDF (src/core/sdf_head.cpp) e transformada em malha; olhos, cílios, sobrancelhas;
  cabelo médio penteado para trás em mechas com física (mola no CPU, deslocamento no shader).
  Caminhar/correr a partir de curvas de marcha humana (Perry/Winter/Novacheck) com pé travado
  no chão por cinemática inversa.
- **Luz**: shader próprio com sombras PCSS, sombra de perto para o herói, luz rebatida, SSAO,
  bloom, raios de sol volumétricos recortados, névoa dourada, neblina nos vales, reflexos de
  lente, ACES + correção de cor. Níveis Baixo / Médio / Alto.
- **Jogo (ciclo de recompensas)**: quadro de recompensas (Enter aceita), procurados com
  temperamento (covarde se rende, teimoso luta, fujão foge), bando com IA (cobertura, rajadas,
  contorna obstáculos), revólver com mira livre (botão direito mira, esquerdo atira, R recarrega),
  vida que regenera, morte e volta na cidade, amarrar (segurar E), levar na garupa ou puxando,
  entregar ao xerife, pratas.
- **Interface**: bússola, minimapa redondo com rota tipo GPS (A* que prefere estradas e cruza o
  rio pela ponte), mapa grande (M), HUD (vida em arco no minimapa, cartuchos, pratas), quadro de
  cartazes, pausa (Esc), painel de depuração (F1).
- **Som**: tudo sintetizado em código (tiro com eco, passos, cascos, recarga, vento, pássaros, grilos).

---

## 4. Como compilar, testar e ver

```bash
cmake -B build && cmake --build build -j8     # jogo (Linux) e testes
./build/testes                                 # ~192 testes (doctest)
./build/cacador                                # jogar
./build/cacador --benchmark --qualidade baixo  # orçamento: <= 350 mil tri / 350 chamadas
scripts/build-windows.sh                       # build-win/cacador.exe + data/ (MinGW)
scripts/build-web.sh                           # build-web/ (precisa do emsdk)
scripts/screenshot.sh saida.png --hora 15 --qualidade alto   # print sem janela (Xvfb)
```

Opções úteis (prints e testes): `--hora H`, `--pos X Z`, `--yaw graus`, `--qualidade baixo|medio|alto`,
`--montado`, `--abrir-quadro`, `--abrir-mapa`, `--depuracao`, `--recompensa N`, `--tiroteio`,
`--mirar`, `--pitch graus`, `--girar-camera graus`, `--retrato distancia`, `--estudio`, `--argila`.

No Windows: o jogo precisa da pasta `data/` ao lado do `cacador.exe`.

---

## 5. Mapa do código

```
src/main.cpp            argumentos, janela, benchmark, laço principal
src/versao.h            versão do jogo
src/core/   (lógica pura, testada; sem raylib)
  math.h mat4.h noise.*     vetores, matrizes, ruído
  terrain.*                 altura, normal, máscaras de estrada e rio
  places.*                  leitura de data/lugares.txt
  collision.*               caixas do mundo, pisos (ponte), raioLivre, caixasPerto
  motion.* camrig.*         andar a pé, câmera de ombro
  horse.*                   cavalo (andaduras, inclinação, piso da ponte)
  npc.*                     moradores (rotas, noite, zona de exclusão do bando)
  skeleton.* anim.*         esqueleto, poses (andar/correr por dados de marcha, mirar, cair, rendido...)
  body.* body_hd.* loft.h   corpo leve e HD (loft de anéis), revólver na mão
  sdf_head.* head_hd.*      cabeça esculpida (SDF), rosto, olhos, cabelo em mechas
  bounty.*                  cartazes (nome, crime, valores, temperamento, pista)
  combat.*                  revólver, vida, tiro (lancarTiro, linhaLivre)
  bando.*                   IA do bando
  cacada.*                  andamento da recompensa (aceitar → combate → coletar → levar → entregar)
  rota.*                    navegador A* (GPS)
  bussola.*                 matemática da bússola
  quality.*                 níveis gráficos e config.txt
src/render/  (raylib + shaders)
  shaders.*                 todos os shaders (GLSL 330 no PC, 100 na web)
  lighting.*                iluminação, sombras, pós-processamento, céu e nuvens, depuração
  terrain_mesh.*            malha do terreno em pedaços de 64 m (passo 1,5/4/8 m)
  props.*                   construções, árvores, tufos de grama, quadros
  grama.*                   gramado denso em blocos de 8 m
  skinned.*                 envio da malha com ossos para a GPU
  mapa_tela.*               minimapa e mapa grande
  estilo.*                  fontes e peças da interface
src/game/
  jogo.*                    o jogo (atualizar, desenhar, interface)
  combate.cpp               tiroteio, captura, entrega, morte (membros de Jogo)
  depuracao.cpp             painel F1
  actors.*                  modelos de pessoa e cavalo
  sons.*                    sons sintetizados
  ui.*                      HUD, bússola, quadro, pausa, mensagens
tests/                      doctest (um arquivo por módulo do core)
docs/superpowers/           designs, planos e decisões já tomadas
data/                       lugares.txt e fontes
```

---

## 6. Armadilhas já descobertas (leia antes de mexer)

- **Índices de 16 bits**: cada malha de personagem tem limite de 65 mil vértices (o herói usa ~50 mil).
  O herói tem teto de 95 mil triângulos (teste em tests/test_body.cpp).
- **Ossos na GPU**: `uniform mat4 ossos[17]`; na web o limite de uniforms impede muitos mais ossos.
  Osso A/B e peso vão em texcoord2 (y = osso B + 0,99 × peso de A).
- **Dados por vértice dos personagens**: alfa da cor = densidade de pelo; texcoord.y = material
  (10 pele, 11 tecido, 12 couro, 13 metal, 15 olho, 18 lábio, 24 cartão de fio, 24,5 cabelo com
  física); tangente xyz = direção do fio e o COMPRIMENTO dela = peso da física do cabelo; tangente w
  = posição ao longo do fio.
- **Terreno**: o passo de cada nível tem que dividir 64 m exatamente (senão aparece fenda).
- **Céu/nuvens**: o céu é desenhado DEPOIS da cena, com profundidade no fundo (só calcula onde aparece);
  o ruído das nuvens vem de uma textura (uma leitura) — não volte para hash por pixel (derruba o FPS).
- **Grama**: usa 1 amostra de sombra (são muitas folhas sobrepostas); material tipo 4 no shader.
- **Pós-processamento**: fazer clamp antes da curva de contraste (senão o céu fica com manchas azuis).
- **Pés**: andar/correr usam pé travado por IK; o teste "o pé de apoio não escorrega" mede isso.
- **Ponte**: rota e cavalo tratam a ponte como piso/caminho (o relevo embaixo não conta).
- **Testes que leem o mundo** usam o Terreno real (semente 1337) e caixas de colisão sintéticas.

---

## 7. Pendências conhecidas e próximos passos sugeridos

Pequenos problemas registrados (não corrigidos ainda):
- Entregar/garupa não conferem onde está o preso que vem seguindo.
- Uma cobertura de bandido pode cair no rio (ele fica parado até reavaliar).
- Perto do rendido (2 m) o E não monta no cavalo.
- Mirando colado numa parede, a direção do tiro pode inverter.
- Não testado recentemente: versão web; FPS real no Alto em placas fracas.

Ideias para deixar mais jogável (em ordem sugerida):
1. Loja (munição limitada, armas melhores, comida, roupas), estábulo, salvar o progresso.
2. Marcar um ponto no mapa grande e traçar rota até ele.
3. Perseguição a cavalo, alvo escondido na cidade, emboscadas, bando grande.
4. Trava de mira, tiro na perna (rende o alvo), cobertura do jogador, atirar a cavalo, laço.
5. Sistema de "procurado" (atirar em inocentes), moradores reagindo ao tiroteio, eventos aleatórios,
   animais e caça.
6. Menu inicial, diário, missões de história.

---

## 8. As "skills" (processo de trabalho) em resumo

1. **Brainstorming**: entender o que o usuário quer e por quê; uma pergunta por vez; propor 2–3
   abordagens com prós e contras e recomendar uma; escrever o design aprovado em
   docs/superpowers/specs/AAAA-MM-DD-tema-design.md.
2. **Plano**: dividir em tarefas pequenas, cada uma com arquivos, assinaturas, testes e commit
   (docs/superpowers/plans/AAAA-MM-DD-tema.md).
3. **Execução com TDD**: teste que falha → código mínimo → teste passa → suíte inteira → commit.
   Decisões tomadas no caminho vão para um registro ("Ruling: o que decidi — por quê — custo se errado").
4. **Depuração sistemática**: reproduzir com um teste ou um print/log, achar a causa (não remendar
   o sintoma), corrigir e provar com o teste.
5. **Verificação antes de concluir**: rodar testes, benchmark e screenshot; só então dizer que funciona.
6. **Revisão final**: reler o diff inteiro procurando bugs que tornariam o jogo impossível de jogar
   (recompensa que não termina, alvo preso, etc.), corrigir os graves com teste.
