#pragma once
// Identidade visual "Velho Oeste Sombrio" da interface: paleta, painéis chanfrados com borda dupla de
// latão, cards, botões, prompts de tecla, banner de descoberta e ícones procedurais (só primitivas 2D
// da raylib, sem texturas).
//
// Quem usa:
//   - game/circuitos_jogo.cpp: HUD de cartuchos (canto inferior direito) e Bancada da Fogueira.
//   - game/combate_reativo.cpp: marcadores sobre os inimigos, prompts flutuantes e quadro de depuração.
//   - game/ui.cpp: barra de vida e dica de interação na base da tela.
// Para mudar o visual de tudo de uma vez, mexa só aqui (cores, espessuras, desenho dos ícones).
#include <string>
#include "core/circuito.h"
#include "raylib.h"

namespace oeste {

// ---------------------------------------------------------------- paleta
const Color FUNDO{22, 18, 16, 230};         // couro escuro / madeira queimada (painéis)
const Color CARD{36, 29, 25, 240};          // fundo secundário (cards)
const Color LATAO{184, 146, 74, 220};       // bordas e detalhes (latão envelhecido)
const Color PERGAMINHO{234, 224, 208, 255}; // texto principal
const Color CINZA{165, 152, 136, 255};      // texto secundário (cinza quente)
const Color BRASA{224, 90, 43, 255};        // destaque ativo / fogo
const Color GELO{118, 199, 208, 255};       // gelo / pó de prata
const Color FERRO{108, 122, 134, 255};      // bloqueio / ferro frio
const Color ALERTA{232, 196, 72, 255};      // inimigo investigando (amarelo)
const Color PERIGO{206, 52, 40, 255};       // inimigo em combate (vermelho)

constexpr float MARGEM = 18.f;  // distância dos widgets até a borda da tela
constexpr float PAD = 12.f;     // espaçamento interno dos painéis

// ---------------------------------------------------------------- animação
float suave(float t01);                            // ease-out cúbico, t fora de [0,1] é preso
Rectangle escalar(Rectangle r, float s);           // escala em volta do centro
Color misturar(Color a, Color b, float t);         // a -> b

// ---------------------------------------------------------------- mouse
bool sobre(Rectangle r);
bool clicou(Rectangle r);  // botão esquerdo pressionado sobre r neste quadro

// ---------------------------------------------------------------- painéis e controles
// Painel principal: sombra suave, fundo de couro, cantos chanfrados e borda dupla de latão.
void painel(Rectangle r, float alfa = 1.f, float chanfro = 10.f);
// Card: fundo secundário, borda simples. hover clareia; ativo = borda de latão forte;
// foco = borda de brasa (coluna/linha que as setas estão mexendo); bloqueado = ferro frio.
void card(Rectangle r, bool hover, bool ativo, bool foco = false, bool bloqueado = false, float alfa = 1.f);
// Botão com tecla de atalho opcional ("Enter", "X"...). Só desenha: o clique é lido na atualização.
void botao(Rectangle r, const std::string& tecla, const std::string& rotulo, bool destaque = false, float alfa = 1.f);
// Barra de progresso com moldura de latão.
void barra(Rectangle r, float fracao, Color cor, float alfa = 1.f);
// Tecla desenhada como plaquinha de latão; retorna a largura.
float tecla(const std::string& t, float x, float y, float tam, float alfa = 1.f);
// Prompt "[E] Recolher Cápsula" centrado em (cx, y): faixa escura com a tecla destacada.
// tecla vazia = só o texto.
void prompt(const std::string& tecla, const std::string& acao, float cx, float y, float alfa = 1.f, float tam = 20.f);
// Cabeçalho de seção: número da etapa num círculo e título em versalete.
void cabecalho(const std::string& numero, const std::string& titulo, float x, float y, float largura, bool foco);

// ---------------------------------------------------------------- ícones procedurais
// Todos recebem o centro e o "raio" (meia largura) do ícone.
void iconeEntalhe(Entalhe e, Vector2 c, float r, Color cor);  // espiral, grade entrelaçada, ondas
void iconeCadeado(Vector2 c, float r, Color cor);
Color corDaPasta(Pasta p);  // chumbo, âmbar, ocre, azul-prateado
void iconeFrasco(Pasta p, Vector2 c, float r, float alfa = 1.f);
void iconeDoador(Doador d, Vector2 c, float r, float alfa = 1.f);  // pena, pedra, carvão em brasa
// Seco: chama viva. Úmido: gota com anel de secagem (fracaoUmidade 1 = encharcado, 0 = seco).
// Molhado: gota cheia pulsando. Ferro Frio: elos de corrente. t = tempo para a animação.
void iconePavio(EstadoPavio e, float fracaoUmidade, Vector2 c, float r, float t);
Color corDoPavio(EstadoPavio e);
// Cartucho de latão em pé (bala para cima). slot nulo ou vazio = silhueta apagada.
// brilho 0..1 = halo de brasa do slot selecionado.
void cartucho(Rectangle r, const SlotMunicao* slot, float brilho);
// Marcadores sobre inimigos: losango com olho (alerta/combate) e badges de estado.
void losangoAlerta(Vector2 c, float r, Color cor);
void badgeGelo(Vector2 c, float r);
void badgeAtordoado(Vector2 c, float r, float t);

// ---------------------------------------------------------------- texto
// Maiúsculas que respeitam acentos em UTF-8 ("Pó de Prata" -> "PÓ DE PRATA").
std::string maiusculas(const std::string& s);

// ---------------------------------------------------------------- banner de descoberta
// "NOVO ENTALHE DESCOBERTO: GRADE" no topo da tela, com fade de entrada e saída.
// Usa o relógio do jogo (sem atualização por quadro): mostrar(..., agora) e desenhar(agora).
struct Banner {
    std::string titulo, detalhe;
    float inicio = -1e9f;
    static constexpr float DURACAO = 4.5f;
    void mostrar(const std::string& titulo, const std::string& detalhe, float agora);
    void desenhar(float agora) const;
};

}  // namespace oeste
