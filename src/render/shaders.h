#pragma once
#include "raylib.h"

// Shaders embutidos no executavel. Escritos uma vez; um cabecalho por plataforma
// (GLSL 330 no PC, GLSL 100 na web) troca as palavras-chave.
Shader carregarShader(const char* vs, const char* fs);

extern const char* VS_LIT;
extern const char* FS_LIT;
extern const char* VS_CEU;
extern const char* FS_CEU;
