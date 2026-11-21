# v2 "um pouco jogável": recompensas, tiroteio e captura — Design

> Data: 2026-11-21. Continua o mundo aberto v1 e os personagens AAA.

## 1. Intenção

Hoje o jogo é um mundo para passear: dá para andar, cavalgar, ver o dia e a noite e abrir o quadro de
recompensas, mas não há objetivo, desafio nem consequência. O usuário quer o primeiro **ciclo completo
de jogo**, o menor pacote em que dá para jogar uns 15 minutos e ter uma experiência inteira:

1. Aceitar uma recompensa no quadro.
2. Ir até o esconderijo do procurado (bússola com marcador).
3. Enfrentar o alvo e 2–3 capangas com revólver, mira livre.
4. Levar dano; se morrer, volta na cidade.
5. Trazer o alvo vivo (vale mais) ou morto.
6. Entregar ao xerife e receber o dinheiro.

Decisões do usuário: itens 1–7 da lista (recompensa, bandido no mapa, combate, vida, capturar/matar,
dinheiro, marcador + bússola); **revólver**; **mira livre**; captura do jeito **A** (o alvo se rende com
pouca vida), com **"talvez ele se renda"** — nem todo alvo se entrega. Tiro em parte do corpo (B) e laço
(C) ficam para depois. Arquitetura escolhida: **sistema de combate próprio**, separado dos moradores,
com acerto simples (corpo + cabeça).

## 2. Objetivos, não-objetivos, anti-objetivos

Objetivos
- Ciclo completo jogável do começo ao fim, sem travar em nenhum passo.
- Tiroteio que pareça justo: o jogador entende de onde vem o tiro, os bandidos erram quando você se
  mexe ou está longe, e acertar dá retorno claro (marca na mira, reação do corpo).
- Alvos com temperamento: alguns se rendem, outros lutam até morrer, outros fogem.
- Interface mínima e clara: vida, munição, dinheiro, bússola com o objetivo.

Não-objetivos (v2)
- Sons (ficam para depois; o tiro tem clarão e fumaça).
- Atirar montado; laço; tiro na perna/braço; trava de mira.
- Loja, gastar dinheiro, salvar progresso, mapa em tela cheia.
- Moradores reagindo a tiros, atirar em inocentes, sistema de "procurado".
- Perseguição a cavalo (alvo fugindo montado).

Anti-objetivos
- Ficar pesado: Baixo continua ≤ 350 mil triângulos / 350 chamadas no benchmark, com um tiroteio em tela.
- Bandidos "aimbot" (acertam sempre) ou "bonecos" (ficam parados levando tiro).
- Jogador perder o progresso por um bug: alvo preso dentro de construção, rendido que some, cavalo
  inalcançável — qualquer um desses torna a recompensa impossível.
- Quebrar a rotina dos moradores ou o que já funciona no mundo (quadros, cavalos, dia/noite).

## 3. Restrições

- C++17 + raylib 5.5, tudo gerado por código (malha do revólver, poses, efeitos). Sem arquivos de modelo.
- Lógica pura em `src/core/` com testes doctest; `src/game/` só lê entrada e desenha (padrão do projeto).
- Web (WebGL1/GLSL 100) continua rodando no Baixo; mouse esquerdo/direito funcionam no navegador.
- Moradores (`Populacao`) mantêm a rotina; a única mudança é uma **zona de exclusão** (ver Esconderijo).

## 4. Abordagens consideradas

| | Abordagem | Troca |
|---|---|---|
| **1** | **Combate próprio** (escolhida): "combatentes" com vida, arma, estado e IA simples, num módulo separado e testado; acerto por cilindro do corpo + esfera da cabeça | Mais código novo, mas não mexe na rotina dos moradores |
| 2 | Moradores com bandeira "hostil" | A rotina casa/trabalho/taverna não combina com tiroteio; viraria código cheio de exceções |
| 3 | Combate próprio com acerto por osso | Só compensa quando vier o tiro na perna (B); hoje é custo à toa |

## 5. O que foi decidido (pelo usuário)

- Ciclo: quadro → esconderijo → tiroteio → vivo/morto → xerife → dinheiro.
- Revólver, mira livre.
- Captura: o alvo **pode** se render com pouca vida; ao se render, o jogador amarra com E e leva no cavalo.
- Combate num sistema próprio, acerto corpo + cabeça.

## 6. Escolhas do executor (propostas; podem ser corrigidas)

### Recompensa
- Uma recompensa ativa por vez. No quadro, Enter (ou clique) aceita o cartaz selecionado; o cartaz
  aparece marcado "ACEITO". Aceitar outro troca a ativa (a anterior volta ao quadro).
- O cartaz ganha **temperamento** (sorteado com a mesma semente): covarde ~40% (se rende com vida < 35%),
  teimoso ~35% (luta até morrer), fujão ~25% (com vida < 35% foge a pé; se o jogador chegar a 4 m dele,
  se rende). O fujão corre mais devagar que o jogador a pé, chegar perto a cavalo também conta, e depois
  que o tiroteio começa ele nunca some — a bússola aponta para ele. O cartaz traz uma pista em texto ("Perigoso: não se entrega", "Costuma fugir").
- **Esconderijo**: o "último visto" passa a ser só lugar fora de povoados (castelo, acampamento, mina,
  floresta, cemitério, ponte). O ponto exato é um local livre de colisão perto do centro do lugar,
  calculado ao aceitar (fogueira pequena no meio, como marca).
- **Moradores longe do bando** (decisão do usuário, opção A): esses lugares têm moradores passeando.
  Enquanto a recompensa estiver ativa, moradores a menos de ~80 m do esconderijo somem (como se
  tivessem fugido do bando) e voltam quando ela termina. Os demais lugares não mudam.
- Capangas: 2 (recompensa < 100) ou 3 (≥ 100). Roupas escuras, lenço no rosto; o alvo é reconhecível
  (chapéu/cor do cartaz).
- O bando só é criado quando o jogador chega a ~150 m do esconderijo e é removido se ele se afastar
  mais de ~400 m sem ter lutado (custo zero no resto do mapa).
- Ao entregar, o cartaz sai do quadro e um novo cartaz é gerado no lugar.

### Combate
- **Revólver**: 6 balas, R recarrega (~2 s, interrompível), munição de reserva infinita na v2.
- **Mirar**: botão direito segura a mira — a câmera aproxima sobre o ombro, aparece uma cruz no centro,
  o personagem anda devagar e vira para onde a câmera olha. Botão esquerdo atira. Atirar sem mirar é
  permitido com espalhamento maior. Cadência mínima ~0,35 s entre tiros.
- **Acerto**: raio instantâneo do cano até a mira; para no terreno e nas caixas de colisão; contra cada
  combatente testa cilindro do corpo e esfera da cabeça (cabeça = morte na hora, corpo = 34 de dano;
  três tiros no corpo matam).
- **Retorno**: marca na mira quando acerta (vermelha se matou), o atingido dá um tranco, clarão no cano
  (luz pontual de ~60 ms) e fumacinha; poeira onde a bala bate no chão.
- **Vida do jogador**: 100. Tiro de bandido tira ~18. Regenera após 5 s sem levar dano (~12/s).
  Tela avermelha nas bordas quando a vida está baixa.
- **Morte do jogador**: tela escurece, volta em pé junto ao quadro mais próximo com vida cheia.
  A recompensa continua ativa e o bando é recriado com vida cheia (quem estava morto continua morto).
- **IA dos bandidos** (estados): parado/vigiando perto da fogueira → alerta (viu o jogador a ≤ 35 m
  com linha de visão, ou ouviu tiro a ≤ 80 m) → combate (procura cobertura numa caixa de colisão próxima
  ou mantém 10–25 m, anda de lado, atira em rajadas de 1–3) → fuga/rendição/morte.
  Precisão cai com a distância e com a velocidade do jogador; reação de ~0,6 s antes do primeiro tiro;
  no máximo 2 bandidos atirando ao mesmo tempo.
- Capangas fogem (e somem a 120 m) quando o alvo é rendido ou morto.

### Captura e entrega
- **Rendido**: o alvo joga a arma, ajoelha com as mãos para cima. Atirar nele de novo o mata (vale o
  preço de morto). Fica parado esperando.
- **Amarrar**: a 2 m, segurar E por ~1,5 s → amarrado (mãos para trás, sentado).
- **Levar**: com o alvo amarrado (ou o corpo, se morto) a ≤ 8 m, ao montar num cavalo o alvo vai junto,
  atravessado na garupa. A pé, E perto do amarrado faz ele seguir o jogador andando (o jogador fica no
  passo de andar). O corpo morto só vai no cavalo.
- **Xerife**: um homem parado ao lado de cada quadro (estrela no peito). Chegar a 4 m dele com o alvo
  (na garupa ou seguindo) e apertar E → paga recompensa de vivo ou de morto; o alvo é entregue e some.
- Se o cavalo for perdido: os cavalos dos estábulos continuam disponíveis; não há outra regra.

### Interface
- Canto inferior: vida (barra), balas (6 ícones), dinheiro ("$ 120").
- **Bússola** no topo: faixa com N/L/S/O e um marcador do objetivo com distância em metros:
  esconderijo → (perto) o alvo → (alvo capturado ou morto) o xerife mais próximo.
- Mensagens curtas no centro por ~3 s: "Recompensa aceita", "Fulano se rendeu!", "Recompensa: $ 80".
- Dica de tecla contextual (já existe `desenharDica`): "E - Amarrar", "Segure E", "E - Entregar".

### Animações (procedurais, no esqueleto existente)
- Mirar (braço direito estendido), recuo do tiro, recarregar, levar tiro (tranco), cair morto
  (queda de ~0,6 s e fica deitado), ajoelhado com as mãos para cima, amarrado sentado, deitado na garupa.
- Revólver: malha pequena gerada por código presa à mão direita (jogador e bandidos).

### Benchmark
- `--benchmark` ganha uma cena fixa de tiroteio no acampamento: alvo + 3 capangas atirando, clarão do
  tiro como luz. Precisa ficar dentro de 350 mil triângulos / 350 chamadas no Baixo; a versão web é
  conferida na mesma cena.

### Testes (core)
- Raio acerta cabeça/corpo/nada; parede bloqueia o tiro.
- Dano, morte, regeneração com atraso.
- Temperamentos: covarde se rende abaixo do limite, teimoso nunca, fujão foge e se rende quando alcançado.
- Máquina de estados do bandido: vê com linha de visão, não vê atrás de parede; no máximo 2 atirando.
- Precisão cai com distância e velocidade (estatística com semente fixa).
- Recompensa: vivo paga `recompensaVivo`, morto paga `recompensaMorto`; só uma ativa; quadro repõe cartaz.
- Esconderijo sempre num ponto sem colisão e dentro do mapa.
- Zona de exclusão: com recompensa ativa nenhum morador fica a menos de 80 m do esconderijo; sem ela, todos voltam.

## 7. Deixado para o executor
- Estrutura dos módulos, nomes e divisão em tarefas.
- Valores finos (dano, distâncias, tempos) — ajustar jogando; os números acima são o ponto de partida.
- Forma da malha do revólver e dos efeitos.
