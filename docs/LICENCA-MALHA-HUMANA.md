# Proveniência da malha humana

`src/core/human_base_data.inc` é uma transformação mecânica de assets do projeto MakeHuman:

- `makehuman/data/3dobjs/base.obj` (basemesh hm08)
- `makehuman/data/targets/macrodetails/caucasian-male-young.target`
- `makehuman/data/targets/macrodetails/universal-male-young-maxmuscle-averageweight.target`
- `makehuman/data/targets/armslegs/{l,r}-upperarm-muscle-incr.target`
- `makehuman/data/targets/armslegs/{l,r}-upperarm-shoulder-muscle-incr.target`
- `makehuman/data/targets/armslegs/{l,r}-lowerarm-muscle-incr.target`
- `makehuman/data/targets/armslegs/{l,r}-upperleg-muscle-incr.target`
- `makehuman/data/targets/torso/torso-muscle-dorsi-incr.target`
- `makehuman/data/targets/torso/torso-muscle-pectoral-incr.target`
- `makehuman/data/targets/torso/torso-vshape-incr.target`
- `makehuman/data/targets/stomach/stomach-tone-incr.target`
- commit de origem: `a8bc2d54ff0ac92e78ff71431b1023eda42bf482`
- repositório: <https://github.com/makehumancommunity/makehuman>

O próprio repositório de origem declara a malha-base, targets e demais assets sob **CC0 1.0
Universal**. O código AGPL do aplicativo MakeHuman não foi copiado nem utilizado no jogo; o
gerador deste repositório apenas lê os arquivos de dados CC0.

Referências da licença na origem:

- <https://github.com/makehumancommunity/makehuman/blob/master/LICENSE.md>
- <https://github.com/makehumancommunity/makehuman/blob/master/LICENSE.ASSETS.md>

O arquivo gerado contém somente coordenadas quantizadas e índices triangulados. Para reproduzi-lo,
baixe os assets na revisão acima e execute, no diretório com os arquivos:

```bash
python3 tools/generate_human_base.py base.obj caucasian-male-young.target \
  src/core/human_base_data.inc \
  --target-extra universal-male-young-maxmuscle-averageweight.target:1.0 \
  --target-extra l-upperarm-muscle-incr.target:0.7 \
  --target-extra r-upperarm-muscle-incr.target:0.7 \
  --target-extra l-upperarm-shoulder-muscle-incr.target:0.7 \
  --target-extra r-upperarm-shoulder-muscle-incr.target:0.7 \
  --target-extra l-lowerarm-muscle-incr.target:0.4 \
  --target-extra r-lowerarm-muscle-incr.target:0.4 \
  --target-extra l-upperleg-muscle-incr.target:0.4 \
  --target-extra r-upperleg-muscle-incr.target:0.4 \
  --target-extra torso-muscle-dorsi-incr.target:0.8 \
  --target-extra torso-muscle-pectoral-incr.target:0.8 \
  --target-extra torso-vshape-incr.target:0.5 \
  --target-extra stomach-tone-incr.target:0.8
```
