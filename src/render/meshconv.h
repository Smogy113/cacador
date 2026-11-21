#pragma once
#include "raylib.h"
#include "core/meshbuilder.h"

// Converte para Mesh da raylib (envia para a GPU). Cor final = cor * ao (em vertex colors);
// ao tambem vai em texcoord.x para o shader.
Mesh paraMesh(const MeshBuilder& b);
