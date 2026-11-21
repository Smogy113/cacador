# Caçador de Recompensas Medieval — Design do Mundo Aberto (v1)

> Nome do jogo: provisório ("Caçador"). Data: 2026-11-21.

## 1. Intenção e o porquê

Um jogo 3D de **mundo aberto com clima de "Red Dead medieval"**: o jogador é um
**caçador de recompensas** que cavalga por uma fronteira selvagem, pega contratos,
rastreia alvos, persegue, captura ou elimina. O visual segue o estilo **low-poly
com iluminação dramática e colorida** das imagens de referência (estilo "Simple
Dungeons"): formas simples, cores chapadas, bonito pela luz e não pela textura.

A prioridade absoluta é **ser o mais leve possível** e rodar até em PC fraco,
sem abrir mão de ser bonito.

Esta primeira etapa é **só o mundo aberto**. A história ("foda", com muitas
missões legais) vem depois, construída em cima dele.

## 2. Objetivos, não-objetivos e anti-objetivos

### Objetivos (v1 — mundo aberto)
- Terreno 3D natural com morros, rio, floresta, estradas.
- Lugares fixos do mapa (seção 5) visíveis e visitáveis por fora.
- Personagem em 3ª pessoa andando/correndo, câmera atrás das costas.
- **Cavalo**: montar, desmontar, cavalgar (trote/galope).
- **Ciclo dia/noite** completo (~20 min reais), com céu, luz, neblina e
  tochas/fogueiras mudando de acordo.
- **Pessoas circulando** (NPCs simples andando entre pontos) nos vilarejos/estradas.
- **Quadro de recompensas** no xerife: mostra cartazes, ainda sem missões jogáveis.
- 60 FPS em notebook antigo com vídeo integrado (Intel HD).

### Não-objetivos da v1 (vêm depois)
- Combate (besta, arco, faca, punhos, "foco do caçador").
- Captura (laço, boleadeira, amarrar, carregar no cavalo).
- Perseguições, sistema de procurado, missões, história.
- Entrar em construções e masmorras (interiores).
- Lobos/animais, carroças, som/música, salvar o jogo, menus completos.
- Celular, navegador e controle (gamepad pode entrar se for barato).

### Anti-objetivos (seria fracasso mesmo funcionando)
- Ficar pesado: travar em PC fraco ou exigir download grande.
- Mapa grande e vazio. Melhor pequeno e cheio.
- Visual "programador": cinza, sem cor, sem clima. A luz e as cores são o charme.
- Copiar o Red Dead (nomes, personagens, arte). Inspiração de gênero, não cópia.

## 3. Restrições

- **Linguagem: C++** com **raylib** (decisão do usuário).
- **Tudo em 3D**, câmera **atrás das costas** como GTA/Red Dead (decisão do usuário).
- **Plataforma: computador com Windows**, teclado + mouse (decisão do usuário).
  Entrega como `.exe` para Windows, compilado de forma cruzada (MinGW) a partir de Linux.
- Modelos 3D **gerados no código** a partir de primitivas (caixas, cilindros,
  cones, rampas), no estilo low-poly — sem arquivos de modelo pesados.
- Alvo de hardware: notebook antigo com vídeo integrado a 60 FPS (minha proposta).
- Tamanho final pequeno (meta: executável + dados em poucos MB).

## 4. Abordagens consideradas

| | Abordagem | Troca |
|---|---|---|
| A | Mapa todo feito à mão | Controle total, mas muito trabalho e terreno "duro" |
| B | Mapa todo gerado por semente | Levíssimo e grande, mas sem garantia de lugares fixos pra história |
| **C** | **Híbrido** (escolhida) | Terreno/floresta gerados por código; lugares importantes num arquivo de texto editável à mão |

**Escolhida: C**, pelo usuário. Junta terreno natural, mapa em poucos KB e
controle total sobre onde a história acontece; fácil adicionar lugares depois.

## 5. O que foi decidido

### Mundo
- Ambientação **medieval de fronteira**, vibe Red Dead.
- Mapa **compacto e cheio**: cruzar a cavalo leva ~3–4 min. Cercado de montanhas.
- Lugares:
  - **Cidade murada**: taverna, xerife com quadro de recompensas, mercado, estábulo, cadeia.
  - **Vilarejo** pequeno com fazendas em volta.
  - **Floresta escura** (densa; perigosa à noite — perigo em si vem depois).
  - **Acampamento de bandidos**.
  - **Ruínas de castelo** num morro (masmorra embaixo, entrada só visual na v1).
  - **Mina** e **cemitério**.
  - **Rio com ponte**.
  - **Estradas de terra** ligando os lugares.

### Dia e noite
- Dia completo ≈ 20 min reais.
- Dia: céu claro, neblina leve. Pôr do sol: céu laranja, neblina dourada,
  tochas acendem. Noite: escuro de verdade, luz só de tochas, fogueiras e
  lanterna do jogador.

### Câmera e desempenho
- Câmera em 3ª pessoa atrás do ombro, controlada pelo mouse.
- **Neblina na distância** esconde o fim da visão (leveza + clima, cor muda com a hora).
- Objetos distantes em versão mais simples (LOD) ou não desenhados.

### Para depois (já combinado, fora da v1)
- Armas: besta (principal), arco, faca/espada curta, punhos.
- Captura: laço, boleadeira, amarrar e levar no cavalo; vivo paga mais.
- Alguns alvos se rendem, outros fogem ou chamam o bando.
- "Foco do caçador": câmera lenta curta pra marcar tiros.
- Cartazes de procurado; fazer besteira te torna o procurado.
- Perseguições (cavalo, depois carroças).

## 6. Escolhas minhas (podem ser corrigidas)
- Escala: mapa de ~1,5 km × 1,5 km; galope ~12 m/s.
- Terreno em blocos (chunks) com altura gerada por ruído; o código aplaina
  onde há construção e "pinta" as estradas entre lugares.
- Iluminação: shader próprio simples (luz do sol/lua + até ~8 tochas mais
  próximas por vez), cores por vértice, sem texturas. Sem sombras reais na v1:
  sombra "bolha" escura embaixo de personagens e cavalo.
- Animação dos personagens e do cavalo feita no código (pernas/braços balançando).
- Construções sólidas (colisão por caixas); sem interiores na v1.
- Build com CMake; raylib baixada pelo próprio build.
- Além do `.exe`, uma versão de teste no navegador (WebAssembly) a cada etapa,
  só pro usuário experimentar rápido. Se atrapalhar a leveza ou o ritmo, é descartável.
- Arquivo de lugares: texto simples, uma linha por objeto (tipo, posição, rotação).
- Controles: WASD andar, Shift correr, mouse câmera, E montar/desmontar/interagir.

## 7. Deixado para quem constrói
- Organização do código, nomes de arquivos, formato exato do arquivo de lugares.
- Algoritmo de ruído, tamanho dos chunks, técnica de LOD/culling.
- Paleta exata de cores por hora do dia (seguindo as referências).
- Como os NPCs escolhem caminhos (pontos fixos já bastam).
