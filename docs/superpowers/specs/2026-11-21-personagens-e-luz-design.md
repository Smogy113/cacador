# Personagens anatômicos e luz "cara de ray tracing" — Design

> Data: 2026-11-21. Continuação do mundo aberto v1 (`2026-11-21-mundo-aberto-design.md`).

## 1. Intenção

O usuário quer personagens com aparência de jogo AAA (referência: malha low-poly anatômica de
~1.100 triângulos), sem perder a leveza, e uma iluminação que pareça ray tracing.
Decisão do usuário: **meio-termo** — corpos anatômicos no nível da referência, **gerados por
código**, **animados com esqueleto**, no mesmo clima de cores e luz do mundo atual, com luz
"parecendo ray tracing".

## 2. Objetivos, não-objetivos, anti-objetivos

Objetivos
- Pessoas (jogador e NPCs) com corpo humano anatômico: tronco, ombros, cotovelos, joelhos,
  mãos e pés com forma, cabeça com nariz, queixo e sobrancelha. ~1.000–1.600 triângulos.
- Roupas como regiões de cor e volumes extras (sobretudo, chapéu de aba, botas, lenço).
- Animação por esqueleto (17 ossos): andar, correr, parado (respiração), montado.
- Cavalo anatômico no mesmo sistema (esqueleto próprio), passo/trote/galope.
- Cada personagem = **1 chamada de desenho** (deformação na placa de vídeo).
- Luz:
  - todos os níveis: luz rebatida do chão (GI falsa) e oclusão de ambiente nos personagens;
  - Médio: sombras suaves (penumbra que cresce com a distância do objeto que faz sombra);
  - Alto: + oclusão de ambiente em tela (SSAO), sombras de contato, reflexo do céu e do sol na água.

Não-objetivos
- Texturas, normal maps, captura de movimento, rosto com expressões, dedos animados.
- Ray tracing de verdade (não roda em PC fraco).

Anti-objetivos
- Ficar pesado: nível Baixo continua dentro de 350 mil triângulos / 350 chamadas no benchmark.
- Personagem "de borracha": juntas que dobram feio (cotovelo/joelho achatando).
- Contraste de estilo: pessoas realistas, mas com a mesma paleta e luz do mundo.

## 3. Restrições

- C++17 + raylib 5.5, sem arquivos de modelo; tudo gerado por código.
- Web (WebGL1) continua funcionando no nível Baixo: no máximo 128 vetores de uniform no
  vertex shader → até 17 ossos como `mat4` (68 vec4) cabe.
- Baixo: 60 FPS em Intel HD.

## 4. Abordagens consideradas

| | Abordagem | Troca |
|---|---|---|
| A | Partes rígidas por osso (como hoje, mas com formas anatômicas) | Simples, mas ~12 chamadas por pessoa e emendas visíveis nas juntas |
| B | Malha única deformada na CPU a cada quadro | Juntas suaves, mas reenviar vértices de 60 pessoas por quadro pesa |
| **C** | **Malha única com pesos por osso, deformada na placa de vídeo** (escolhida) | Juntas suaves, 1 chamada por pessoa, custo de CPU só para calcular 17 matrizes |

## 5. O que foi decidido

- Corpo por "loft": anéis elípticos ao longo de cada osso, com raios tirados de uma tabela de
  perfil anatômico (frente/lado); anéis vizinhos ligados por faixas; tampas na cabeça, mãos e pés.
  Normais suaves (não facetadas). Cada vértice tem até 2 ossos com pesos (mistura perto das juntas).
- Esqueleto humano: quadril(raiz), coluna, peito, pescoço, cabeça, braço/antebraço/mão (E/D),
  coxa/canela/pé (E/D) = 17 ossos.
- Animações procedurais (sem arquivos): ciclo de passada com amplitude pela velocidade;
  respiração parado; pose montada.
- Variação entre NPCs: altura, largura de ombro/quadril, cor de pele, roupas, chapéu ou não.
- Luz rebatida: termo extra = cor do sol × cor média do chão × (1 − n.y) × fator; age em paredes e corpos.
- Sombras suaves: PCF com raio variável estimado pela busca de bloqueadores (PCSS simplificado).
- SSAO: profundidade da cena em textura, 8–12 amostras, meia resolução, desfocada.

## 6. Escolhas do executor (podem ser corrigidas)

- ~1.200 triângulos por humano, ~1.600 por cavalo.
- Até 2 ossos por vértice; índices e pesos guardados em `texcoords2`.
- Distância > 35 m: mesma malha (sem LOD extra) — 1 chamada já basta.
- SSAO e sombra de contato só no Alto; sombra suave no Médio e Alto.
