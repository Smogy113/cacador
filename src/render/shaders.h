#pragma once
#include "raylib.h"

// Shaders embutidos no executavel. Escritos uma vez; um cabecalho por plataforma
// (GLSL 330 no PC, GLSL 100 na web) troca as palavras-chave.
Shader carregarShader(const char* vs, const char* fs);

Shader carregarShaderPos(const char* fs);  // usa o vertex shader padrao da raylib (tela cheia)

extern const char* VS_PROF;
extern const char* VS_PELE;
extern const char* VS_PROF_PELE;
extern const char* FS_PROF;
extern const char* FS_BRILHO;
extern const char* FS_BLUR;
extern const char* FS_RAIOS;
extern const char* FS_JUNTA;
extern const char* FS_SSAO;
extern const char* VS_LIT;
extern const char* FS_LIT;
extern const char* VS_CEU;
extern const char* FS_CEU;
