#pragma once
// Cronometro da placa de video (consultas GL_TIME_ELAPSED) por parte do quadro, para o painel F1.
// Le o resultado de 2 quadros atras (sem travar a placa). So no PC; na web nao mede nada.
enum ParteQuadro { P_LUZ_PAREDES, P_SOMBRAS, P_TERRENO, P_CONSTRUCOES, P_GRAMA, P_ATORES, P_CEU, P_EFEITOS, P_POS, N_PARTES };

class PerfilGPU {
public:
    void ligar(bool sim) { ligado_ = sim; }
    bool ligado() const { return ligado_; }
    void comecarQuadro();       // le os resultados antigos
    void comecar(ParteQuadro p);
    void terminar();
    float ms(ParteQuadro p) const { return ms_[p]; }
    static const char* nome(ParteQuadro p);

private:
    bool ligado_ = false, criado_ = false;
    unsigned int q_[3][N_PARTES] = {};
    bool usado_[3][N_PARTES] = {};
    int quadro_ = 0, atual_ = -1;
    float ms_[N_PARTES] = {};
};
