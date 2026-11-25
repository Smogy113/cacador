# Tela inicial como cartaz pregado

**Por quê:** o menu inicial (faixa escura, título, lista com barrinha dourada) tem cara de
interface genérica. A tela deve parecer um objeto do mundo do jogo.

**Objetivo:** o menu vira um cartaz de papel envelhecido pregado sobre a paisagem da entrada.
**Fora do escopo:** HUD, bancada e outras telas (ficam para depois).
**Anti-objetivo:** perder legibilidade ou mudar o comportamento (opções, teclado, mouse, config).

## Abordagens
- **Cartaz de papel (escolhida):** papel procedural com borda rasgada, manchas, prego e carimbo;
  a opção em foco ganha um círculo de tinta desenhado à mão. Tem identidade e conversa com o tema.
- **Cinema minimalista:** letreiro grande e opções como créditos. Descartada: é o visual
  minimalista genérico que queremos evitar.

## Design
- Fundo: a foto atual (`data/menu/entrada.jpg`) com sépia leve, vinheta e grão.
- Cartaz à esquerda, levemente girado, balançando devagar; tudo desenhado por código
  (sem arquivos novos), com as fontes do jogo (Rye e EB Garamond) em tinta escura.
- Cabeçalho "AVISO", título "Caçador", subtítulo "de recompensas", filetes de tipografia antiga.
- Opções em tinta; a focada recebe um círculo de tinta que se desenha em ~0,2 s.
  A escolha salva (penteado, qualidade) leva um tique carimbado.
- Submenus (Personagem, Configurações) trocam a folha do mesmo cartaz, com título próprio.
- Carimbo vermelho torto no rodapé; dica de teclas fora do cartaz, discreta.
- Escala com a tela (720p a 4K); cliques usam a mesma geometria do desenho.

## Decisões do construtor
Paleta exata, formato dos rasgos e das manchas, tempo das animações.
