#pragma once
#include <cmath>

struct Vec3f {
    float x = 0, y = 0, z = 0;
};
inline Vec3f operator+(Vec3f a, Vec3f b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3f operator-(Vec3f a, Vec3f b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f operator*(Vec3f a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline bool operator==(Vec3f a, Vec3f b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline float dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3f cross(Vec3f a, Vec3f b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(Vec3f a) { return std::sqrt(dot(a, a)); }
inline Vec3f normalizar(Vec3f a) { float l = len(a); return l > 1e-8f ? a * (1.f / l) : Vec3f{0, 1, 0}; }

struct Rgb {
    float r = 0, g = 0, b = 0;
};
inline Rgb operator*(Rgb a, float s) { return {a.r * s, a.g * s, a.b * s}; }
inline Rgb operator+(Rgb a, Rgb b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
inline Rgb lerp(Rgb a, Rgb b, float t) { return a * (1 - t) + b * t; }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float smoothstep(float e0, float e1, float x) { float t = clamp01((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }

// Gira em torno do eixo Y (mesma convencao do yaw: 0 = +z, positivo gira +z para +x).
inline Vec3f rotY(Vec3f p, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
}
