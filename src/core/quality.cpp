#include "core/quality.h"

Efeitos efeitosDe(Nivel n) {
    switch (n) {
        case Nivel::Alto: return {true, true, true, 2048, 1.f, 450.f};
        case Nivel::Medio: return {true, false, false, 1024, 1.f, 350.f};
        default: return {false, false, false, 0, 1.f, 250.f};
    }
}
