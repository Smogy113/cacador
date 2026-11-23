# Gravações do Caçador

As gravações e os efeitos de foley abaixo foram disponibilizados sob Creative Commons Zero. Os arquivos no jogo são recortes editados, com ajustes de frequência, volume e fades. [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

| Arquivos | Gravações no Freesound |
| --- | --- |
| `passo_terra_*`, `passo_capim_*`, `passo_pedra_*` e `passo_madeira_*` | Fission9 — [Gravel](https://freesound.org/people/Fission9/sounds/521588/), [Grass](https://freesound.org/people/Fission9/sounds/521587/), [Stone](https://freesound.org/people/Fission9/sounds/521590/) e [Wooden Planks](https://freesound.org/people/Fission9/sounds/521589/) |
| `passo_agua_*` e respingos em `casco_agua_*` | EricsSoundschmiede — [Walking in a puddle](https://freesound.org/people/EricsSoundschmiede/sounds/454623/) |
| `casco_terra_*`, `casco_firme_*` e `casco_agua_*` | gbnelso — [HorseGallop1](https://freesound.org/people/gbnelso/sounds/684446/), Luisa_Sanchez — [Horse steps on grass](https://freesound.org/people/Luisa_Sanchez/sounds/813383/) e Max_Headroom — [Horse Galloping](https://freesound.org/people/Max_Headroom/sounds/175356/) |
| `tiro_*` | C-V — [Revolver Gunshots](https://freesound.org/people/C-V/sounds/835200/) e mahecic — [Pocket Pistol Gunshot 1](https://freesound.org/people/mahecic/sounds/803496/). As reflexões e o abafamento são editados nos próprios arquivos. |
| `recarga_*` | Dredile — [Clean Revolver Reload](https://freesound.org/people/Dredile/sounds/177863/) e serøutōnin//deprivəd — [Reload Foley](https://freesound.org/s/881371/) |
| `porta_movimento_*` | Sandwichbill — [Wooden Creaking Door](https://freesound.org/people/Sandwichbill/sounds/842232/) e HyperFoley — [door creak](https://freesound.org/people/HyperFoley/sounds/776515/) |
| `porta_batente_*` | felix.blume — [Wooden door, opening and closing](https://freesound.org/people/felix.blume/sounds/779881/) e Mariayolanda511 — [Puerta de madera abriendo y cerrando](https://freesound.org/people/Mariayolanda511/sounds/850566/) |
| `ave_*` | jasonlee3071 — [bird chirp](https://freesound.org/people/jasonlee3071/sounds/693113/) e F1tz — [bird song](https://freesound.org/people/F1tz/sounds/795995/) |
| `vento.ogg` | Nox_Sound — [Ambiance Wind Forest Trees](https://freesound.org/people/Nox_Sound/sounds/530908/) |
| `insetos.ogg` | Speziax — [Crickets – Warm Summer Night](https://freesound.org/people/Speziax/sounds/645329/) |

## Edição e reprodução

O jogo carrega os efeitos curtos em mono e aplica volume e direção na hora de tocar. O vento e os insetos são estéreo e lidos em fluxo. As emendas dos ambientes e as quatro acústicas de tiro são preparadas fora do jogo.

Para refazer os cortes, reúna as prévias MP3 de alta qualidade em uma pasta. Use o número da gravação como nome de cada arquivo, como `521588.mp3`. Execute o comando com Python, NumPy, SciPy e FFmpeg com libvorbis instalados. O build do jogo usa os Ogg já versionados.

```sh
python3 tools/preparar_sons.py caminho/das/gravacoes
```

`src/game/sons.cpp` controla os bancos e os ganhos. O piso é escolhido em `src/game/jogo.cpp`. As batidas do cavalo seguem a fase da passada em `src/core/horse.cpp`.
