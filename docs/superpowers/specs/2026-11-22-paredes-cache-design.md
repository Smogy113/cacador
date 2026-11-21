# Paredes com raytracing baratas (texturas geradas + luz guardada) — Design curto

> Data: 2026-11-22. Reforma de desempenho de `2026-11-21-paredes-raytracing-design.md`.

## Intenção
No Alto, olhar para as paredes derruba o jogo para ~20 FPS numa RTX 5070. Cada pixel de parede
inventa a textura (centenas de ruídos) e traça >100 passos de raio a cada quadro, embora a casa
não se mova e o sol ande devagar. O usuário aprovou fazer como os jogos grandes, **sem mudar o
visual** e sem arquivos: calcular uma vez e reaproveitar.

## Objetivos / anti-objetivos
- Paredes no Alto praticamente de graça (meta: olhar para paredes custa < 1 ms a mais).
- Mesmo visual (mesmo desenho de tábua/pedra/reboco, mesma luz); a textura pode repetir a cada
  4 m (imperceptível com a variação de cor das peças).
- Anti: costuras visíveis na repetição ou entre faces; luz "atrasada" visível quando o sol anda;
  Baixo/web mudando.

## Abordagens
1. **(escolhida, aprovada)** (a) Material assado em texturas na abertura (GPU, a partir das mesmas
   funções), lidas pelo relevo; (b) luz traçada (sombra suave, oclusão, rebote) guardada numa imagem
   por face de parede (≈ 25 cm por texel), recalculada aos poucos (algumas faces por quadro).
2. Menos passos/meia resolução: mais simples, mas muda o visual e ganha pouco.

## Decisões do executor
- **Material:** 3 materiais × 2 texturas 2048² cobrindo 4 m × 4 m (≈ 2 mm/texel), repetindo:
  A = (fator de cor RGB, altura), B = (cor absoluta RGB, peso) → `albedo = mix(base·A.rgb, B.rgb, B.a)`
  (a dessaturação da madeira continua no shader, depende da cor do vértice). As funções de ruído
  passam a ser periódicas em 4 m (frequências arredondadas para caber inteiras: mesmo aspecto).
  Mipmaps ligados (sem cintilar de longe).
- **Luz guardada:** só nas caixas de parede/alicerce registradas (flag `parede` na forma). Cada
  face vertical ganha um retângulo num atlas 2048² (25 cm/texel, borda de 1 texel). Duas imagens:
  L1 = (sombra suave, oclusão), L2 = rebote RGB/2. Atualização: as faces a < 70 m da câmera, em
  rodízio, 48 faces por quadro (todas em ~1 s); face que entra no raio é feita na hora. À noite a
  luz é a da lua (como hoje).
- **Ligação vértice → face:** o vértice de parede leva o índice da forma (+1) em `texcoord2.x`;
  o shader acha a face pela normal no espaço da caixa e lê uma tabela (textura float) com retângulo
  do atlas e geometria da face.
- Vigas (sem forma própria) e paredes sem face no atlas continuam com o caminho atual por pixel.
- Janelas acesas e sombra própria do relevo continuam por pixel (baratas com a textura).
- Médio: lê a oclusão guardada (como hoje, sem sombra/rebote traçados).

## Verificação
- Testes (core): alocação do atlas (sem sobreposição, tamanho e borda), geometria das faces bate
  com os vértices da caixa, índice da forma nos vértices de parede.
- Capturas antes/depois (vilarejo 10 h/16 h, cidade 17 h, noite): mesmo aspecto.
- Medida de custo: contador de "avaliações pesadas" não dá na GPU; o usuário mede no F1.
