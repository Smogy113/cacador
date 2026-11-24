# Combate reativo, engenharia reversa e rebote: design curto

> Data: 2026-11-24 (0.29). Continua o protótipo dos Circuitos de Queima (0.28).

## Intenção

Os Circuitos de Queima precisam de alvos que reajam, de um motivo para recolher os vestígios e de
risco no uso da magia. Tudo continua provisório, feito com primitivas, sem depender do modelo 3D
final do personagem. A regra fica no núcleo e é testada.

## O que entra

1. **Inimigos de teste:** quatro manequins no pátio de patrulha. Têm vida, patrulha e reação física
   aos golpes:
   - Grade + Enxofre: empurra e atordoa no raio.
   - Onda + Pó de Prata: a zona deixa lento quem pisa.
   - Espiral + Resina: o ricochete na parede lateral pega o Sentinela atrás da cobertura.
2. **Vestígios:** quem patrulha e vê uma cápsula marcada no chão passa a "Alerta" e vai investigar.
3. **Engenharia reversa:** o caçador começa só com Espiral e Pólvora Fina. As pistas (equipamentos
   entalhados) ensinam Grade, Onda, Resina, Enxofre e Pó de Prata.
4. **Rebote:** pode vir do Preparo Rápido na fogueira ou de forçar o disparo com o pavio úmido. A
   bala e o doador se perdem, o caçador leva dano e a mira dá um tranco e treme.
5. **Depuração:** os estados (Patrulha, Alerta, Congelado, Atordoado, Derrotado) e os desbloqueios
   aparecem na tela (F3).

## Mapa do código

| Peça | Onde | Por quê |
| --- | --- | --- |
| Cabeça do inimigo: patrulha, percepção e investigação | `core/inimigo_teste.*` | lógica pura e testável |
| Corpo: vida, empurrão, voo, derrubada, atordoamento, gelo e lentidão | `core/fisica_alvo.*` (`AlvoTreino`) | o mesmo dos bonecos e dos bandidos |
| Receitas conhecidas, pistas e `gravar()` | `core/receitas.*` | o Arsenal continua só com slots e doadores |
| Decisão do gatilho e tremor da mira | `core/rebote.*` | o sorteio vem de fora, então o teste é determinístico |
| Cobertura baixa e paredes a 45° | `core/baia_cobertura.*` | as mesmas caixas na colisão e nos testes |
| Normal exata do contato | `MundoColisao::primeiroContato` | a sondagem antiga errava até ~20° em paredes giradas |
| Montagem, ligação aos golpes e desenho | `game/combate_reativo.cpp` | só o jogo conhece a raylib |
| Painel da fogueira, gatilho e HUD | `game/circuitos_jogo.cpp` | onde já estavam |
| Planta da arena | `game/arena_teste.h` | um lugar só para as posições |

## Decisões

- **Estado visível = física + conduta.** `estado()` decide pelo mais forte: Derrotado, Atordoado
  (zonzo, caído ou no ar), Congelado (gelo ou lentidão), Alerta (investigando) e Patrulha. Golpes não
  mudam a conduta. O corpo impedido (`FisicaAlvo::ocupado`) só pausa a cabeça.
- **A zona congelante deixa lento em vez de parar.** O congelamento total continua com a Onda, o Pó
  de Prata e a Pólvora Fina. Os bandidos seguem a mesma regra: na zona, andam a 20% e ainda atiram.
- **A Grade atordoa mais do que derruba** (2,5 s contra 1,6 s): o alvo levanta zonzo.
- **Cada cápsula é percebida uma vez** (`CapsulaVestigio::descoberta`), então duas patrulhas não
  disputam a mesma. A percepção pede o cone de visão e linha livre, ou proximidade de 2,5 m. A
  cobertura esconde a cápsula do Sentinela agachado.
- **O Sentinela é de posto.** Investigar só o faz virar, porque ele não sai da cobertura que é o
  teste do ricochete.
- **O espólio cai uma vez.** Ao reaparecer (20 s), o inimigo não carrega outro. Se a pista não for
  inspecionada, continua no chão.
- **A bancada ensaia antes da barra.** O Preparo Cuidadoso grava numa cópia do Arsenal para avisar
  na hora se não vai dar, sem esperar 3 s.
- **Forçar exige o segundo clique.** O primeiro clique com o pavio úmido falha e abre uma janela de
  1,5 s. Na água a bala falha sempre, como antes.
- **O tremor devolve a mira.** O jogo soma à câmera só a diferença do deslocamento entre quadros, e
  no fim a mira volta ao lugar.

## Números de ajuste

- `ParametrosInimigo` (`core/inimigo_teste.h`): velocidades, visão (8 m, ±60°), percepção próxima,
  inspeção (3,5 s), alturas e reaparecimento.
- `ParametrosCircuito` (`core/circuito.h`), na seção de reação dos alvos: `tempoAtordoado`,
  `fatorLentidaoGelo`, `tempoLentidaoGelo`.
- `ParametrosCircuito`, na seção de preparo e rebote: `tempoPreparoCuidadoso`, `chanceReboteRapido`,
  `chanceReboteUmidoMin/Max`, `janelaForcar`, `danoRebote`, `travaRebote` e o tremor.
- `ParametrosBaia` (`core/baia_cobertura.h`): as medidas da cobertura e das paredes.

## Verificação

- `tests/test_combate_reativo.cpp` cobre:
  - receitas e pistas;
  - gatilho e rebote, com sorteio fixo;
  - tremor da mira;
  - patrulha, percepção e investigação;
  - Grade atordoando dois alvos;
  - lentidão na zona (20% medidos);
  - o ricochete na baia em quatro orientações, com e sem Resina.
- O teste da colisão giratória confere a normal exata.
- No jogo, `--roteiro-circuito 6..10` reproduz cada comportamento. Ver `docs/DESEMPENHO.md`.

## Fora do escopo (próximos passos)

- Os inimigos de teste não atiram no caçador. Para isso, a cabeça deles pode virar a máquina de
  estados do Bando (`core/bando`) quando o combate real entrar.
- Ouvir tiros e rebotes. O `tiroOuvido_` já existe e pode virar outro `MotivoAlerta`.
- Trocar os manequins pelo modelo final sem mexer na regra: o desenho lê só `InimigoTeste`
  (posição, yaw, passada e física).
