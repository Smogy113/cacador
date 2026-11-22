#include "render/perfil.h"
#include "raylib.h"
#include "rlgl.h"
#if !defined(PLATFORM_WEB)
#include "external/glad.h"
#endif

const char* PerfilGPU::nome(ParteQuadro p) {
    static const char* n[N_PARTES] = {"Luz das paredes", "Sombras", "Terreno", "Construções", "Árvores", "Grama",
                                      "Personagens", "Céu e nuvens", "Efeitos", "Pós: brilho (recorte)", "Pós: raios de sol", "Pós: brilho (desfoque)",
                                      "Pós: oclusão (SSAO)", "Pós: foco", "Pós: junção final"};
    return n[p];
}

void PerfilGPU::comecarQuadro() {
#if !defined(PLATFORM_WEB)
    if (!ligado_) return;
    if (!criado_) {
        glGenQueries(3 * N_PARTES, &q_[0][0]);
        criado_ = true;
    }
    quadro_ = (quadro_ + 1) % 3;
    int velho = (quadro_ + 1) % 3;  // de 2 quadros atras
    for (int p = 0; p < N_PARTES; p++) {
        if (!usado_[velho][p]) continue;
        GLuint pronto = 0;
        glGetQueryObjectuiv(q_[velho][p], GL_QUERY_RESULT_AVAILABLE, &pronto);
        if (!pronto) continue;
        GLuint64 ns = 0;
        glGetQueryObjectui64v(q_[velho][p], GL_QUERY_RESULT, &ns);
        ms_[p] = ms_[p] * 0.9f + float(ns) * 1e-6f * 0.1f;  // media suave
        usado_[velho][p] = false;
    }
#endif
}

void PerfilGPU::comecar(ParteQuadro p) {
#if !defined(PLATFORM_WEB)
    if (!ligado_ || !criado_ || atual_ >= 0) return;
    rlDrawRenderBatchActive();  // o que ja estava na fila nao conta para esta parte
    glBeginQuery(GL_TIME_ELAPSED, q_[quadro_][p]);
    usado_[quadro_][p] = true;
    atual_ = p;
#else
    (void)p;
#endif
}

void PerfilGPU::terminar() {
#if !defined(PLATFORM_WEB)
    if (atual_ < 0) return;
    rlDrawRenderBatchActive();
    glEndQuery(GL_TIME_ELAPSED);
    atual_ = -1;
#endif
}
