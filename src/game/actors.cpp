#include "game/actors.h"
#include <chrono>
#include <cmath>
#include "core/meshbuilder.h"
#include "raymath.h"
#include "render/meshconv.h"
#include "rlgl.h"

namespace {
void adicionarBracoDireito(MalhaPele& vista, const MalhaPele& corpo) {
    auto bracoDireito = [&](uint16_t i) {
        int osso = corpo.v[i].osso[0];
        return osso == BRACO_D || osso == ANTEBRACO_D || osso == MAO_D;
    };
    std::vector<int> remapeado(corpo.v.size(), -1);
    // Reutiliza a pele e o rig do braço, com a mesma iluminação da terceira pessoa.
    for (size_t i = 0; i < corpo.idx.size(); i += 3) {
        uint16_t a = corpo.idx[i], b = corpo.idx[i + 1], c = corpo.idx[i + 2];
        if (!bracoDireito(a) || !bracoDireito(b) || !bracoDireito(c)) continue;
        for (uint16_t j : {a, b, c}) {
            if (remapeado[j] < 0) {
                remapeado[j] = int(vista.v.size());
                vista.v.push_back(corpo.v[j]);
            }
            vista.idx.push_back(uint16_t(remapeado[j]));
        }
    }
}
}  // namespace

void ModeloHumano::carregar(const Proporcoes& p, const Visual& v) {
    baseHumana_ = v.corpoBase;
    esq_ = v.corpoBase ? esqueletoHumanoBase(p) : esqueletoHumano(p);
    const bool armaSeparada = baseHumana_ && v.revolver;
    auto corpo = corpoHumano(p, v);
    malha_.carregar(corpo, armaSeparada);
    if (!baseHumana_) hd_.carregar(corpoHumanoHD(p, v));
    if (armaSeparada) {
        float escala = p.altura / 1.8f;
        const auto [inicio, fim] = fecharMaoDoRevolver(corpo, esq_, escala);
        maoInicio_ = inicio;
        maoPosicoes_.reserve((fim - inicio) * 3);
        maoNormais_.reserve((fim - inicio) * 3);
        for (size_t i = inicio; i < fim; ++i) {
            const auto& v = corpo.v[i];
            maoPosicoes_.insert(maoPosicoes_.end(), {v.pos.x, v.pos.y, v.pos.z});
            maoNormais_.insert(maoNormais_.end(), {v.normal.x, v.normal.y, v.normal.z});
        }
        auto arma = malhaRevolver(esq_, escala);
        revolver_.carregar(arma);
        adicionarBracoDireito(arma, corpo);
        bracoArmado_.carregar(arma);
    }
}

void ModeloHumano::carregarHeroi(const Proporcoes& p, const Visual& v) {
    carregar(p, v);
    if (v.corpoBase) return;  // a mesma base compacta atende perto e longe; nao duplica na GPU
    if (v.cacadorDeFronteira) return;  // a silhueta vem da malha animada
#if !defined(PLATFORM_WEB)
    Visual sp = v;
    sp.semPeleCabeca = true;
    sp.ladosExtras = 2.f;
    MalhaPele m = corpoHumanoHD(p, sp);
    trisSemPele_ = int(m.triangulos());
    hdSemPele_.carregar(m);
    escY_ = p.altura / 1.8f;
    escXZ_ = escY_ * p.massa;
    volume_ = std::async(std::launch::async, [v] { return gerarVolume(0.0013f, 0.002f, v); });
    volumePendente_ = true;
#endif
}

void ModeloHumano::descarregar() {
    malha_.descarregar();
    hd_.descarregar();
    hdSemPele_.descarregar();
    revolver_.descarregar();
    bracoArmado_.descarregar();
    maoPosicoes_.clear();
    maoNormais_.clear();
    armaEmpunhada_ = false;
    if (volumePendente_) volume_.wait();
    volumePendente_ = false;
    cabeca_.descarregar();
}

void ModeloHumano::atualizarCabelo(float dt, Vec3f pos, float yaw, const Pose& pose, Vec3f vento, Vec3f camera) {
    calcularMatrizes(esq_, pose, ossos_);
    Mat4 mundo = Mat4::trans(pos) * Mat4::rotY(yaw);
    Mat4 cabeca = mundo * ossos_[CABECA], pescoco = mundo * ossos_[PESCOCO], peito = mundo * ossos_[PEITO];
    Vec3f distancia = camera - cabeca.ponto(esq_.ossos[CABECA].inicio);
    // De 5 a 12 m o movimento próprio das pontas cede lugar ao dos guias principais.
    float detalhe = 1.f - smoothstep(5.f * 5.f, 12.f * 12.f, dot(distancia, distancia));
    for (auto* malha : {&malha_, &hd_, &hdSemPele_})
        malha->atualizarCabelo(dt, cabeca, pescoco, peito, vento, detalhe);
}

void ModeloHumano::reiniciarCabelo() {
    for (auto* malha : {&malha_, &hd_, &hdSemPele_}) malha->reiniciarCabelo();
}

void ModeloHumano::empunharArma(bool empunhada) {
    if (armaEmpunhada_ == empunhada || maoPosicoes_.empty()) return;
    malha_.alternarForma(maoInicio_, maoPosicoes_, maoNormais_);
    armaEmpunhada_ = empunhada;
}

int ModeloHumano::triangulos(bool hd, VistaArma vista) const {
    if (vista == VistaArma::PrimeiraPessoa) return bracoArmado_.triangulos();
    int corpo = hd && !baseHumana_ ? hd_.triangulos() : malha_.triangulos();
    return corpo + (vista == VistaArma::NaMao ? revolver_.triangulos() : 0);
}

void ModeloHumano::desenhar(Material& mat, Vec3f p, float yaw, const Pose& pose, bool hd, Material* matCabeca,
                           VistaArma vista) const {
    calcularMatrizes(esq_, pose, ossos_);
    Matrix modelo = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z));
    if (vista == VistaArma::PrimeiraPessoa) {
        bracoArmado_.desenhar(mat, ossos_, modelo);
        return;
    }
    if (volumePendente_ && volume_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cabeca_.carregar(volume_.get(), escXZ_, escY_);  // envia os atlas a placa (na thread do desenho)
        volumePendente_ = false;
    }
    if (hd && matCabeca && cabeca_.ok()) {
        hdSemPele_.desenhar(mat, ossos_, modelo);
        cabeca_.desenhar(*matCabeca, ossos_, modelo);
        return;
    }
    (hd && !baseHumana_ ? hd_ : malha_).desenhar(mat, ossos_, modelo);
    if (vista == VistaArma::NaMao) revolver_.desenhar(mat, ossos_, modelo);
}

void ModeloCavaloAnat::carregar(const VisualCavalo& v) {
    esq_ = esqueletoCavalo();
    detalhada_.carregar(corpoCavalo(v, true));
    leve_.carregar(corpoCavalo(v, false));
}

void ModeloCavaloAnat::descarregar() {
    detalhada_.descarregar();
    leve_.descarregar();
}

void ModeloCavaloAnat::desenhar(Material& mat, Vec3f p, float yaw, const Pose& pose, bool perto) const {
    calcularMatrizes(esq_, pose, ossos_);
    (perto ? detalhada_ : leve_).desenhar(mat, ossos_, MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(p.x, p.y, p.z)));
}

Vec3f ModeloCavaloAnat::assento(Vec3f p, float yaw, const Pose& pose) const {
    std::vector<Mat4> m;
    calcularMatrizes(esq_, pose, m);
    return p + rotY(m[C_CORPO].ponto(assentoCavalo()), yaw);
}

void SombraBolha::carregar() {
    disco_ = LoadModelFromMesh(GenMeshPoly(14, 1.f));
    disco_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {0, 0, 0, 80};
}

void SombraBolha::descarregar() { UnloadModel(disco_); }

void SombraBolha::desenhar(Vec3f p, Vec3f n, float raio) const {
    Vector3 eixo = Vector3CrossProduct({0, 1, 0}, {n.x, n.y, n.z});
    float ang = std::acos(std::fmin(1.f, n.y)) * RAD2DEG;
    if (Vector3Length(eixo) < 1e-4f) eixo = {1, 0, 0};
    rlDisableDepthMask();
    DrawModelEx(disco_, {p.x, p.y + 0.04f, p.z}, eixo, ang, {raio, 1, raio}, WHITE);
    rlEnableDepthMask();
}
