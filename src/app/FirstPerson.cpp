// =============================================================================
// FirstPerson.cpp — Dựng cảnh 3D qua mắt sinh vật (xem FirstPerson.h).
//
// Đồ hoạ "low-poly" nhẹ nhưng có hồn:
//   * Shader riêng: ánh sáng mặt trời theo pháp tuyến + sương mù theo khoảng cách hoà vào
//     màu chân trời (không còn cảnh "cắt cụt" ở xa), GLSL 330 (PC) / GLSL 100 (điện thoại).
//   * Địa hình: màu đỉnh chuyển sắc mượt giữa các ô (cỏ, rừng, cát, tuyết trên đỉnh núi...),
//     cỏ lay động trong gió, mặt nước gợn sóng, cây có tán nhiều lớp đung đưa.
//   * Sinh vật: vị trí & hướng được NỘI SUY giữa các tick mô phỏng → chuyển động mượt;
//     chân đánh theo khớp hông (pha bước tỉ lệ quãng đường → không trượt chân), tay vung,
//     đầu gật, đuôi vẫy, chim vỗ cánh/lượn, cá quẫy đuôi; ngủ thì nằm, ăn thì cúi đầu,
//     đi săn/chiến đấu thì lao tới, thợ đốn gỗ vung tay.
//   * Cơ thể của chính mình: hai tay + công cụ đang cầm (người), mõm + chân trước (thú),
//     đung đưa theo bước đi và vung lên khi tấn công / thu thập.
// =============================================================================
#include "FirstPerson.h"
#include "Renderer.h"
#include "../sim/Simulation.h"
#include "../ai/Goap.h"
#include "../ai/Invention.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>

namespace wb {

namespace {
constexpr int kR = 26;               // bán kính lưới địa hình (ô)
constexpr int kN = 2 * kR + 1;       // số ô mỗi cạnh
constexpr int kObjR = 20;            // bán kính vẽ cây/công trình/sinh vật (sương mù che phần xa hơn)
constexpr int kQuadsPerTile = 5;     // mặt trên + 4 vách (16-bit index: 53²·5·4 < 65536 đỉnh)

// ---------------------------------------------------------------------------
// Shader: ánh sáng + sương mù
// ---------------------------------------------------------------------------
const char* kVs330 = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord; out vec4 fragColor; out vec3 fragNormal; out float fragDepth;
void main() {
    fragTexCoord = vertexTexCoord; fragColor = vertexColor; fragNormal = vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
    fragDepth = gl_Position.w;
})";
const char* kFs330 = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor; in vec3 fragNormal; in float fragDepth;
uniform sampler2D texture0; uniform vec4 colDiffuse;
uniform vec3 sunDir; uniform vec3 fogColor; uniform vec2 fogRange; uniform float lightAmount; uniform vec3 ambient;
out vec4 finalColor;
void main() {
    vec4 t = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    float nl = length(fragNormal) > 0.01 ? max(dot(normalize(fragNormal), sunDir), 0.0) : 0.6;
    float l = mix(1.0, 0.5 + 0.65 * nl, lightAmount);
    vec3 c = t.rgb * l * ambient;
    float f = clamp((fragDepth - fogRange.x) / (fogRange.y - fogRange.x), 0.0, 1.0);
    finalColor = vec4(mix(c, fogColor, f * f * (3.0 - 2.0 * f)), t.a);
})";
const char* kVs100 = R"(#version 100
attribute vec3 vertexPosition; attribute vec2 vertexTexCoord; attribute vec3 vertexNormal; attribute vec4 vertexColor;
uniform mat4 mvp;
varying vec2 fragTexCoord; varying vec4 fragColor; varying vec3 fragNormal; varying float fragDepth;
void main() {
    fragTexCoord = vertexTexCoord; fragColor = vertexColor; fragNormal = vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
    fragDepth = gl_Position.w;
})";
const char* kFs100 = R"(#version 100
precision mediump float;
varying vec2 fragTexCoord; varying vec4 fragColor; varying vec3 fragNormal; varying float fragDepth;
uniform sampler2D texture0; uniform vec4 colDiffuse;
uniform vec3 sunDir; uniform vec3 fogColor; uniform vec2 fogRange; uniform float lightAmount; uniform vec3 ambient;
void main() {
    vec4 t = texture2D(texture0, fragTexCoord) * fragColor * colDiffuse;
    float nl = length(fragNormal) > 0.01 ? max(dot(normalize(fragNormal), sunDir), 0.0) : 0.6;
    float l = mix(1.0, 0.5 + 0.65 * nl, lightAmount);
    vec3 c = t.rgb * l * ambient;
    float f = clamp((fragDepth - fogRange.x) / (fogRange.y - fogRange.x), 0.0, 1.0);
    gl_FragColor = vec4(mix(c, fogColor, f * f * (3.0 - 2.0 * f)), t.a);
})";

void ensureShader(FirstPerson& fp) {
    if (fp.shaderTried) return;
    fp.shaderTried = true;
    bool es = rlGetVersion() == RL_OPENGL_ES_20 || rlGetVersion() == RL_OPENGL_ES_30 || rlGetVersion() == RL_OPENGL_21 || rlGetVersion() == RL_OPENGL_11;
    std::string vs = es ? kVs100 : kVs330, fs = es ? kFs100 : kFs330;
    if (rlGetVersion() == RL_OPENGL_21) {   // GLSL 1.20: không có "precision", không có #version 100
        vs.replace(0, 12, "#version 120");
        fs.replace(0, 12, "#version 120");
        size_t p = fs.find("precision mediump float;");
        if (p != std::string::npos) fs.erase(p, 24);
    }
    fp.shader = LoadShaderFromMemory(vs.c_str(), fs.c_str());
    fp.shaderReady = fp.shader.id != 0 && fp.shader.id != rlGetShaderIdDefault();
    if (!fp.shaderReady) { TraceLog(LOG_WARNING, "WorldBox: không dựng được shader góc nhìn thứ nhất — dùng shader mặc định"); return; }
    fp.locSun = GetShaderLocation(fp.shader, "sunDir");
    fp.locFog = GetShaderLocation(fp.shader, "fogColor");
    fp.locFogRange = GetShaderLocation(fp.shader, "fogRange");
    fp.locLight = GetShaderLocation(fp.shader, "lightAmount");
    fp.locAmbient = GetShaderLocation(fp.shader, "ambient");
}

void setLight(FirstPerson& fp, float amount) {
    if (!fp.shaderReady) return;
    rlDrawRenderBatchActive();   // các khối đã xếp hàng phải vẽ với giá trị cũ
    SetShaderValue(fp.shader, fp.locLight, &amount, SHADER_UNIFORM_FLOAT);
}

int modeOf(Simulation& sim, Entity e) {
    Habitat h = sim.species[sim.reg.get<Body>(e).species].habitat;
    return h == Habitat::Water ? 1 : (h == Habitat::Cave ? 2 : 0);
}

// Độ cao (đơn vị 3D; 1 ô = 1 đơn vị) của ô theo "thế giới" đang nhìn
float elevOf(const World& w, int x, int y, int mode) {
    if (!w.inBounds(x, y)) return mode == 2 ? 2.6f : -0.2f;
    if (mode == 2) {
        switch (w.under(x, y).kind) {
        case Under::Rock: return 2.6f;     // vách đá
        case Under::Water: return -0.15f;
        default: return 0.0f;              // sàn hang / đường hầm / magma
        }
    }
    const Tile& t = w.at(x, y);
    if (t.terrain == Terrain::DeepWater) return mode == 1 ? -2.2f : -0.9f;
    if (t.terrain == Terrain::ShallowWater) return mode == 1 ? -1.0f : -0.35f;
    if (t.terrain == Terrain::Ice) return 0.04f;
    return 0.05f + (t.height - 0.42f) * 14.0f;
}

Color shadeC(Color c, float k) {
    return Color{static_cast<unsigned char>(std::clamp(c.r * k, 0.0f, 255.0f)), static_cast<unsigned char>(std::clamp(c.g * k, 0.0f, 255.0f)),
                 static_cast<unsigned char>(std::clamp(c.b * k, 0.0f, 255.0f)), c.a};
}
Color mixC(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{static_cast<unsigned char>(a.r + (b.r - a.r) * t), static_cast<unsigned char>(a.g + (b.g - a.g) * t),
                 static_cast<unsigned char>(a.b + (b.b - a.b) * t), 255};
}

// Màu "chất liệu" (albedo) của một ô cho địa hình low-poly — ánh sáng do shader tính
Color tileColor(const World& w, int x, int y, int mode) {
    uint32_t h = hash32(static_cast<uint32_t>(w.wrapX(x) * 92821 + y * 68917));
    float jit = 0.92f + (h % 17) * 0.01f;
    if (mode == 2) {
        if (!w.inBounds(x, y)) return Color{70, 64, 60, 255};
        const UnderTile& u = w.under(x, y);
        Color c;
        switch (u.kind) {
        case Under::Rock: c = Color{92, 82, 74, 255}; break;
        case Under::Tunnel: c = Color{110, 85, 58, 255}; break;
        case Under::Water: c = Color{45, 80, 140, 255}; break;
        case Under::Magma: c = Color{255, 120, 35, 255}; break;
        default: c = Color{70, 64, 68, 255}; break;
        }
        if (u.oreAmt > 40 && u.oreMat != 0xFF && u.kind == Under::Rock) {
            const MaterialDef& od = w.materials[static_cast<MatId>(u.oreMat)];
            c = mixC(c, Color{od.r, od.g, od.b, 255}, 0.45f);   // mạch quặng lộ trên vách
        }
        if (u.kind == Under::Cave && u.fungus > 50) c = mixC(c, Color{60, 220, 180, 255}, 0.35f);   // thảm nấm phát quang
        return shadeC(c, jit);
    }
    if (!w.inBounds(x, y)) return Color{230, 235, 245, 255};
    const Tile& t = w.at(x, y);
    Color c;
    switch (t.terrain) {
    case Terrain::DeepWater: c = mode == 1 ? Color{70, 90, 110, 255} : Color{150, 140, 110, 255}; break;   // đáy biển
    case Terrain::ShallowWater: c = Color{190, 175, 125, 255}; break;
    case Terrain::Sand: c = Color{226, 208, 150, 255}; break;
    case Terrain::Grass: c = Color{104, 168, 70, 255}; break;
    case Terrain::Forest: c = Color{78, 135, 58, 255}; break;
    case Terrain::Jungle: c = Color{62, 125, 52, 255}; break;
    case Terrain::Desert: c = Color{232, 196, 128, 255}; break;
    case Terrain::Savanna: c = Color{186, 176, 92, 255}; break;
    case Terrain::Hill: c = Color{128, 140, 88, 255}; break;
    case Terrain::Mountain: c = Color{132, 124, 118, 255}; break;
    case Terrain::Snow: c = Color{238, 242, 250, 255}; break;
    case Terrain::Tundra: c = Color{158, 172, 150, 255}; break;
    case Terrain::Swamp: c = Color{84, 104, 70, 255}; break;
    case Terrain::Lava: c = Color{255, 110, 30, 255}; break;
    case Terrain::Ash: c = Color{96, 90, 86, 255}; break;
    case Terrain::Farmland: c = t.food > 60 ? Color{214, 192, 82, 255} : Color{140, 104, 62, 255}; break;
    case Terrain::Road: c = Color{170, 145, 105, 255}; break;
    case Terrain::Ice: c = Color{205, 230, 245, 255}; break;
    case Terrain::Basalt: c = Color{72, 72, 80, 255}; break;
    case Terrain::Glass: c = Color{175, 220, 225, 255}; break;
    default: c = Color{120, 160, 80, 255}; break;
    }
    if (mode == 0 && t.height > 0.82f && t.terrain == Terrain::Mountain) c = mixC(c, Color{240, 244, 250, 255}, (t.height - 0.82f) * 8);  // tuyết phủ đỉnh
    if (t.building != Building::None && t.building != Building::ScentMark) c = mixC(c, Color{150, 128, 96, 255}, 0.5f);           // nền đất quanh nhà
    if (t.fire) c = mixC(c, Color{40, 30, 25, 255}, 0.6f);
    return shadeC(c, jit);
}

// Dựng lại lưới địa hình quanh ô (cx, cy)
void buildMesh(FirstPerson& fp, Simulation& sim, int cx, int cy, int mode) {
    const World& w = sim.world;
    if (!fp.meshReady) {
        Mesh mesh{};
        mesh.vertexCount = kN * kN * 4 * kQuadsPerTile;
        mesh.triangleCount = kN * kN * 2 * kQuadsPerTile;
        mesh.vertices = static_cast<float*>(MemAlloc(mesh.vertexCount * 3 * sizeof(float)));
        mesh.normals = static_cast<float*>(MemAlloc(mesh.vertexCount * 3 * sizeof(float)));
        mesh.texcoords = static_cast<float*>(MemAlloc(mesh.vertexCount * 2 * sizeof(float)));
        mesh.colors = static_cast<unsigned char*>(MemAlloc(mesh.vertexCount * 4));
        mesh.indices = static_cast<unsigned short*>(MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short)));
        for (int q = 0; q < kN * kN * kQuadsPerTile; ++q) {
            unsigned short b = static_cast<unsigned short>(q * 4);   // TL, TR, BL, BR
            unsigned short* id = mesh.indices + q * 6;
            id[0] = b; id[1] = b + 2; id[2] = b + 1;          // TL → BL → TR (mặt trước hướng lên)
            id[3] = b + 1; id[4] = b + 2; id[5] = b + 3;      // TR → BL → BR
        }
        UploadMesh(&mesh, true);
        fp.model = LoadModelFromMesh(mesh);
        // Địa hình tô bằng màu đỉnh trên texture trắng 1×1 (không kéo giãn ảnh pixel nữa)
        fp.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        fp.meshReady = true;
    }
    if (fp.shaderReady) fp.model.materials[0].shader = fp.shader;
    Mesh& mesh = fp.model.meshes[0];
    // Độ cao & màu các ô (thêm viền 1 ô để tính góc)
    constexpr int E2 = kN + 2;
    static float E[E2 * E2];
    static Color Ccol[E2 * E2];
    for (int b = -1; b <= kN; ++b)
        for (int a = -1; a <= kN; ++a) {
            E[(b + 1) * E2 + (a + 1)] = elevOf(w, cx - kR + a, cy - kR + b, mode);
            Ccol[(b + 1) * E2 + (a + 1)] = tileColor(w, cx - kR + a, cy - kR + b, mode);
        }
    // Độ cao, màu & pháp tuyến tại các GÓC (dùng chung giữa các ô → chuyển sắc mượt)
    constexpr int K = kN + 1;
    static float CH[K * K];
    static Color CC[K * K];
    static Vector3 CN[K * K];
    for (int b = 0; b <= kN; ++b)
        for (int a = 0; a <= kN; ++a) {
            const int i00 = b * E2 + a, i10 = i00 + 1, i01 = i00 + E2, i11 = i01 + 1;
            CH[b * K + a] = 0.25f * (E[i00] + E[i10] + E[i01] + E[i11]);
            Color c0 = Ccol[i00], c1 = Ccol[i10], c2 = Ccol[i01], c3 = Ccol[i11];
            CC[b * K + a] = Color{static_cast<unsigned char>((c0.r + c1.r + c2.r + c3.r) / 4), static_cast<unsigned char>((c0.g + c1.g + c2.g + c3.g) / 4),
                                  static_cast<unsigned char>((c0.b + c1.b + c2.b + c3.b) / 4), 255};
        }
    for (int b = 0; b <= kN; ++b)
        for (int a = 0; a <= kN; ++a) {
            float hl = CH[b * K + std::max(0, a - 1)], hr = CH[b * K + std::min(kN, a + 1)];
            float hu = CH[std::max(0, b - 1) * K + a], hd = CH[std::min(kN, b + 1) * K + a];
            float nx = hl - hr, ny = 2.0f, nz = hu - hd, L = std::sqrt(nx * nx + ny * ny + nz * nz);
            CN[b * K + a] = {nx / L, ny / L, nz / L};
        }
    auto putVertex = [&](int vi, float x, float y, float z, Vector3 n, Color c) {
        mesh.vertices[vi * 3] = x; mesh.vertices[vi * 3 + 1] = y; mesh.vertices[vi * 3 + 2] = z;
        mesh.normals[vi * 3] = n.x; mesh.normals[vi * 3 + 1] = n.y; mesh.normals[vi * 3 + 2] = n.z;
        mesh.texcoords[vi * 2] = 0.5f; mesh.texcoords[vi * 2 + 1] = 0.5f;
        mesh.colors[vi * 4] = c.r; mesh.colors[vi * 4 + 1] = c.g; mesh.colors[vi * 4 + 2] = c.b; mesh.colors[vi * 4 + 3] = 255;
    };
    for (int b = 0; b < kN; ++b)
        for (int a = 0; a < kN; ++a) {
            int q = b * kN + a, v = q * 4;
            float x0 = static_cast<float>(a - kR), z0 = static_cast<float>(b - kR);
            if (mode == 2) {
                // trong hang: mỗi ô phẳng ở đúng độ cao → khối vuông vức (vách dựng ở dưới)
                float h = E[(b + 1) * E2 + (a + 1)];
                Color c = Ccol[(b + 1) * E2 + (a + 1)];
                Vector3 up{0, 1, 0};
                putVertex(v, x0, h, z0, up, c); putVertex(v + 1, x0 + 1, h, z0, up, c);
                putVertex(v + 2, x0, h, z0 + 1, up, c); putVertex(v + 3, x0 + 1, h, z0 + 1, up, c);
                continue;
            }
            putVertex(v, x0, CH[b * K + a], z0, CN[b * K + a], CC[b * K + a]);
            putVertex(v + 1, x0 + 1, CH[b * K + a + 1], z0, CN[b * K + a + 1], CC[b * K + a + 1]);
            putVertex(v + 2, x0, CH[(b + 1) * K + a], z0 + 1, CN[(b + 1) * K + a], CC[(b + 1) * K + a]);
            putVertex(v + 3, x0 + 1, CH[(b + 1) * K + a + 1], z0 + 1, CN[(b + 1) * K + a + 1], CC[(b + 1) * K + a + 1]);
        }
    // ---- Vách đứng (chỉ trong hang) ----
    int q = kN * kN;
    if (mode == 2) {
        const int da[4] = {1, -1, 0, 0}, db[4] = {0, 0, 1, -1};
        for (int b = 0; b < kN && q < kN * kN * kQuadsPerTile; ++b)
            for (int a = 0; a < kN && q < kN * kN * kQuadsPerTile; ++a) {
                float h = E[(b + 1) * E2 + (a + 1)];
                Color c = shadeC(Ccol[(b + 1) * E2 + (a + 1)], 0.9f);
                for (int k = 0; k < 4 && q < kN * kN * kQuadsPerTile; ++k) {
                    float hn = E[(b + 1 + db[k]) * E2 + (a + 1 + da[k])];
                    if (hn >= h - 0.01f) continue;
                    float x0 = static_cast<float>(a - kR), z0 = static_cast<float>(b - kR);
                    float ex1, ez1, ex2, ez2;
                    if (k == 0) { ex1 = x0 + 1; ez1 = z0; ex2 = x0 + 1; ez2 = z0 + 1; }
                    else if (k == 1) { ex1 = x0; ez1 = z0 + 1; ex2 = x0; ez2 = z0; }
                    else if (k == 2) { ex1 = x0 + 1; ez1 = z0 + 1; ex2 = x0; ez2 = z0 + 1; }
                    else { ex1 = x0; ez1 = z0; ex2 = x0 + 1; ez2 = z0; }
                    Vector3 n{static_cast<float>(da[k]), 0, static_cast<float>(db[k])};
                    int v = q * 4;
                    // thứ tự (ex2 → ex1): pháp tuyến hướng RA phía ô thấp (mặt trước, không bị cull)
                    putVertex(v, ex2, h, ez2, n, c); putVertex(v + 1, ex1, h, ez1, n, c);
                    putVertex(v + 2, ex2, hn, ez2, n, shadeC(c, 0.75f)); putVertex(v + 3, ex1, hn, ez1, n, shadeC(c, 0.75f));
                    ++q;
                }
            }
    }
    // phần dung lượng còn lại: tam giác suy biến (không vẽ gì)
    std::fill(mesh.vertices + q * 12, mesh.vertices + mesh.vertexCount * 3, 0.0f);
    UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);
    UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
    UpdateMeshBuffer(mesh, 2, mesh.normals, mesh.vertexCount * 3 * sizeof(float), 0);
    UpdateMeshBuffer(mesh, 3, mesh.colors, mesh.vertexCount * 4, 0);
    fp.cx = cx; fp.cy = cy; fp.mode = mode;
    fp.builtAt = GetTime();
}

// Độ cao mặt đất tại điểm thực (nội suy song tuyến từ các ô) — để đặt camera, sinh vật, cây
float groundAt(const World& w, float x, float y, int mode) {
    float fx = x - 0.5f, fy = y - 0.5f;
    int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
    float tx = fx - x0, ty = fy - y0;
    float a = elevOf(w, x0, y0, mode), b = elevOf(w, x0 + 1, y0, mode), c = elevOf(w, x0, y0 + 1, mode), d = elevOf(w, x0 + 1, y0 + 1, mode);
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
}

void box(float x, float y, float z, float sx, float sy, float sz, Color c) { DrawCube({x, y, z}, sx, sy, sz, c); }

// Chi (chân / tay) quay quanh khớp ở (x, y, z): góc deg quanh trục ngang (z cục bộ) → đánh trước/sau
void limb(float x, float y, float z, float len, float thick, float deg, Color c) {
    rlPushMatrix();
    rlTranslatef(x, y, z);
    rlRotatef(deg, 0, 0, 1);
    box(0, -len * 0.5f, 0, thick, len, thick, c);
    rlPopMatrix();
}

// Hai con mắt nhỏ trên đầu (ở toạ độ cục bộ của đầu: x hướng mặt)
void eyes(float x, float y, float z, float spread, float sz) {
    box(x, y, z - spread, sz * 0.6f, sz, sz, Color{20, 18, 18, 255});
    box(x, y, z + spread, sz * 0.6f, sz, sz, Color{20, 18, 18, 255});
}

// Vẽ một sinh vật khác dạng khối 3D có hoạt ảnh. (x, y, z) là điểm chạm đất; yaw = hướng mặt.
void drawCreature3D(Simulation& sim, Entity o, float x, float y, float z, float yaw, float phase, float speed, float time) {
    const Body& b = sim.reg.get<Body>(o);
    const SpeciesDef& sd = sim.species[b.species];
    const Social& so = sim.reg.get<Social>(o);
    const Mind& m = sim.reg.get<Mind>(o);
    Color c{sd.r, sd.g, sd.b, 255};
    float s = std::clamp(sd.size, 0.3f, 2.4f) * (b.ageTicks < sd.adultYears * kTicksPerYear ? 0.7f : 1.0f);
    ActionId act = m.plan.done() ? ActionId::Count : m.plan.current();
    bool moving = speed > 0.05f;
    float swing = moving ? std::sin(phase) : 0.0f;                    // dao động bước chân
    float idle = std::sin(time * 2.0f + entityIndex(o)) * 0.5f + 0.5f; // nhịp thở
    float bob = moving ? std::fabs(std::sin(phase)) * 0.04f * s : idle * 0.01f * s;
    bool eating = act == ActionId::EatHere || act == ActionId::Forage || act == ActionId::Scavenge;
    bool fighting = (act == ActionId::Hunt || act == ActionId::Fight) && !moving;
    bool working = act == ActionId::GatherWood || act == ActionId::GatherStone || act == ActionId::GatherOre || act == ActionId::Build || act == ActionId::Farm;
    float lunge = fighting ? std::max(0.0f, std::sin(time * 9 + entityIndex(o))) * 0.15f * s : 0.0f;
    rlPushMatrix();
    rlTranslatef(x, y, z);
    rlRotatef(-yaw * RAD2DEG, 0, 1, 0);   // x cục bộ = hướng mặt, z cục bộ = bên phải
    if (b.sleeping && sd.plan != BodyPlan::Bird && sd.plan != BodyPlan::Fish && sd.plan != BodyPlan::Bat) {
        rlRotatef(85, 1, 0, 0);            // ngủ: nằm nghiêng
        rlTranslatef(0, -0.15f * s, 0);
    }
    rlTranslatef(lunge, bob, 0);
    switch (sd.plan) {
    case BodyPlan::Humanoid: {
        Color cloth = so.group >= 0 ? Color{sim.groups[so.group].colR, sim.groups[so.group].colG, sim.groups[so.group].colB, 255} : shadeC(c, 0.6f);
        Color hair = so.group >= 0 ? Color{sim.groups[so.group].col2R, sim.groups[so.group].col2G, sim.groups[so.group].col2B, 255} : Color{60, 45, 35, 255};
        float h = 0.95f * s;
        float leg = h * 0.45f;
        limb(0, leg, 0.08f * s, leg, 0.11f * s, swing * 35, shadeC(cloth, 0.6f));                  // chân trái/phải đánh ngược pha
        limb(0, leg, -0.08f * s, leg, 0.11f * s, -swing * 35, shadeC(cloth, 0.6f));
        box(0, leg + h * 0.19f, 0, 0.2f * s, h * 0.38f, 0.32f * s, cloth);                            // thân
        float armR = working ? -60 + std::sin(time * 11 + entityIndex(o)) * 55 : (fighting ? -80 + lunge * 400 : -swing * 30);
        limb(0, leg + h * 0.36f, 0.21f * s, h * 0.36f, 0.08f * s, armR, c);                          // tay phải (vung khi làm việc)
        limb(0, leg + h * 0.36f, -0.21f * s, h * 0.36f, 0.08f * s, swing * 30, c);                   // tay trái
        float headY = leg + h * 0.47f + (eating ? -0.05f : 0.0f);
        box(0, headY, 0, 0.2f * s, 0.2f * s, 0.2f * s, c);                                            // đầu
        box(-0.02f * s, headY + 0.09f * s, 0, 0.22f * s, 0.06f * s, 0.22f * s, hair);                 // tóc
        eyes(0.1f * s, headY + 0.02f * s, 0, 0.05f * s, 0.035f * s);
        if (heldValue(sim, o, Purpose::Pierce) > 0) {   // giáo cầm tay phải
            rlPushMatrix(); rlTranslatef(0.05f, leg + h * 0.36f, 0.26f * s); rlRotatef(armR * 0.6f - 15, 0, 0, 1);
            box(0.1f, -0.05f, 0, 0.03f, h * 1.0f, 0.03f, Color{150, 110, 70, 255}); box(0.1f, h * 0.48f, 0, 0.05f, 0.12f, 0.05f, Color{200, 200, 210, 255});
            rlPopMatrix();
        }
        break;
    }
    case BodyPlan::Primate: {
        float leg = 0.22f * s;
        limb(0.08f * s, leg, 0.1f * s, leg, 0.07f * s, swing * 30, shadeC(c, 0.75f));
        limb(0.08f * s, leg, -0.1f * s, leg, 0.07f * s, -swing * 30, shadeC(c, 0.75f));
        rlPushMatrix(); rlTranslatef(0, leg + 0.15f * s, 0); rlRotatef(-25, 0, 0, 1);               // lưng khom
        box(0, 0, 0, 0.32f * s, 0.3f * s, 0.26f * s, c);
        rlPopMatrix();
        limb(0.12f * s, leg + 0.28f * s, 0.17f * s, 0.34f * s, 0.06f * s, -swing * 40 - 20, shadeC(c, 0.85f));   // tay dài chống đất
        limb(0.12f * s, leg + 0.28f * s, -0.17f * s, 0.34f * s, 0.06f * s, swing * 40 - 20, shadeC(c, 0.85f));
        float headY = leg + 0.36f * s - (eating ? 0.08f * s : 0);
        box(0.2f * s, headY, 0, 0.18f * s, 0.17f * s, 0.18f * s, shadeC(c, 1.15f));
        box(0.29f * s, headY - 0.02f * s, 0, 0.05f * s, 0.08f * s, 0.12f * s, Color{215, 180, 150, 255});         // mặt
        eyes(0.31f * s, headY + 0.03f * s, 0, 0.035f * s, 0.03f * s);
        rlPushMatrix(); rlTranslatef(-0.15f * s, leg + 0.15f * s, 0); rlRotatef(30 + std::sin(time * 3) * 15, 0, 0, 1);  // đuôi cong
        box(-0.15f * s, 0, 0, 0.3f * s, 0.035f, 0.035f, shadeC(c, 0.7f));
        rlPopMatrix();
        break;
    }
    case BodyPlan::Bird: {
        // Bay: vỗ cánh nhanh khi lên cao/đổi hướng, dang cánh lượn khi bay đều
        float glide = std::sin(time * 0.7f + entityIndex(o)) > 0.3f ? 0.25f : 1.0f;
        float flap = std::sin(time * 16 + entityIndex(o)) * 55 * glide;
        float wl = 0.42f * s + 0.12f;
        box(0, 0, 0, 0.3f * s + 0.08f, 0.13f * s + 0.04f, 0.13f * s + 0.04f, c);
        box(0.2f * s + 0.06f, 0.04f, 0, 0.1f * s + 0.04f, 0.1f * s + 0.04f, 0.1f * s + 0.04f, shadeC(c, 1.1f));  // đầu
        box(0.29f * s + 0.08f, 0.03f, 0, 0.06f, 0.025f, 0.025f, Color{230, 190, 60, 255});                       // mỏ
        box(-0.22f * s - 0.06f, 0.0f, 0, 0.14f * s + 0.04f, 0.02f, 0.14f * s + 0.06f, shadeC(c, 0.8f));          // đuôi
        rlPushMatrix(); rlRotatef(flap, 1, 0, 0); box(0, 0, wl * 0.5f, 0.16f * s + 0.06f, 0.02f, wl, shadeC(c, 0.8f)); rlPopMatrix();
        rlPushMatrix(); rlRotatef(-flap, 1, 0, 0); box(0, 0, -wl * 0.5f, 0.16f * s + 0.06f, 0.02f, wl, shadeC(c, 0.8f)); rlPopMatrix();
        break;
    }
    case BodyPlan::Fish: {
        float wig = std::sin(time * (moving ? 10.0f : 4.0f) + entityIndex(o)) * 25;
        rlPushMatrix(); rlRotatef(wig * 0.2f, 0, 1, 0);
        box(0, 0, 0, 0.55f * s, 0.22f * s, 0.14f * s, c);
        box(0.12f * s, -0.05f * s, 0, 0.3f * s, 0.08f * s, 0.12f * s, shadeC(c, 1.25f));   // bụng sáng
        eyes(0.22f * s, 0.04f * s, 0, 0.07f * s, 0.04f * s);
        rlPushMatrix(); rlTranslatef(-0.27f * s, 0, 0); rlRotatef(wig, 0, 1, 0); box(-0.1f * s, 0, 0, 0.2f * s, 0.26f * s, 0.03f, shadeC(c, 0.8f)); rlPopMatrix();
        if (sd.size > 1.0f) box(0, 0.18f * s, 0, 0.15f * s, 0.22f * s, 0.03f, shadeC(c, 0.7f));   // vây lưng cá mập
        rlPopMatrix();
        break;
    }
    case BodyPlan::Bat: {
        float flap = std::sin(time * 22 + entityIndex(o)) * 65;
        box(0, 0, 0, 0.13f, 0.11f, 0.1f, c);
        box(0.08f, 0.06f, 0.03f, 0.03f, 0.06f, 0.02f, c); box(0.08f, 0.06f, -0.03f, 0.03f, 0.06f, 0.02f, c);   // tai
        box(0.07f, 0.02f, 0.025f, 0.02f, 0.02f, 0.02f, Color{255, 70, 50, 255}); box(0.07f, 0.02f, -0.025f, 0.02f, 0.02f, 0.02f, Color{255, 70, 50, 255});
        rlPushMatrix(); rlRotatef(flap, 1, 0, 0); box(0, 0, 0.16f, 0.14f, 0.015f, 0.28f, shadeC(c, 0.7f)); rlPopMatrix();
        rlPushMatrix(); rlRotatef(-flap, 1, 0, 0); box(0, 0, -0.16f, 0.14f, 0.015f, 0.28f, shadeC(c, 0.7f)); rlPopMatrix();
        break;
    }
    case BodyPlan::Spider: {
        box(-0.1f * s, 0.16f * s, 0, 0.3f * s, 0.2f * s, 0.26f * s, c);            // bụng
        box(0.1f * s, 0.14f * s, 0, 0.16f * s, 0.13f * s, 0.16f * s, shadeC(c, 0.9f));
        eyes(0.18f * s, 0.18f * s, 0, 0.04f * s, 0.03f * s);
        for (int k = -1; k <= 1; k += 2)
            for (int l = 0; l < 4; ++l) {
                float sw = moving ? std::sin(phase * 1.5f + l * 1.6f + (k > 0 ? 3.14f : 0.0f)) * 25 : 0.0f;
                rlPushMatrix(); rlTranslatef((l - 1.5f) * 0.08f * s, 0.18f * s, k * 0.08f * s); rlRotatef(sw, 0, 1, 0); rlRotatef(k * 55.0f, 1, 0, 0);
                box(0, 0, k * 0.16f * s, 0.03f, 0.03f, 0.32f * s, shadeC(c, 0.6f));
                rlPopMatrix();
            }
        break;
    }
    case BodyPlan::Reptile: {
        float sway = moving ? std::sin(phase) * 12 : 0;
        rlPushMatrix(); rlRotatef(sway, 0, 1, 0);
        box(0, 0.13f * s, 0, 1.0f * s, 0.18f * s, 0.32f * s, c);
        for (int k = -1; k <= 1; k += 2)
            for (int l = -1; l <= 1; l += 2) limb(l * 0.3f * s, 0.12f * s, k * 0.2f * s, 0.12f * s, 0.07f * s, swing * 30 * k * l, shadeC(c, 0.8f));
        float jaw = std::max(0.0f, std::sin(time * 1.3f + entityIndex(o))) * (fighting ? 30.0f : 8.0f);
        box(0.65f * s, 0.15f * s, 0, 0.35f * s, 0.08f * s, 0.18f * s, shadeC(c, 1.1f));                 // hàm trên
        rlPushMatrix(); rlTranslatef(0.5f * s, 0.1f * s, 0); rlRotatef(-jaw, 0, 0, 1); box(0.17f * s, 0, 0, 0.35f * s, 0.05f * s, 0.16f * s, shadeC(c, 0.9f)); rlPopMatrix();
        eyes(0.55f * s, 0.22f * s, 0, 0.07f * s, 0.04f * s);
        rlPushMatrix(); rlTranslatef(-0.5f * s, 0.12f * s, 0); rlRotatef(-sway * 2 + std::sin(time * 2) * 10, 0, 1, 0); box(-0.35f * s, 0, 0, 0.7f * s, 0.1f * s, 0.14f * s, shadeC(c, 0.8f)); rlPopMatrix();
        rlPopMatrix();
        break;
    }
    case BodyPlan::Giant: {
        float leg = 0.45f * s;
        for (int k = -1; k <= 1; k += 2)
            for (int l = -1; l <= 1; l += 2) limb(l * 0.33f * s, leg, k * 0.2f * s, leg, 0.17f * s, swing * 22 * k * l, shadeC(c, 0.7f));
        box(0, leg + 0.3f * s, 0, 1.0f * s, 0.65f * s, 0.62f * s, shadeC(c, 0.85f));
        box(0, leg + 0.62f * s, 0, 0.7f * s, 0.15f * s, 0.5f * s, shadeC(c, 0.75f));                       // bướu lông
        float headY = leg + 0.42f * s - (eating ? 0.15f * s : 0);
        box(0.6f * s, headY, 0, 0.36f * s, 0.42f * s, 0.42f * s, c);
        eyes(0.78f * s, headY + 0.08f * s, 0, 0.13f * s, 0.05f * s);
        rlPushMatrix(); rlTranslatef(0.78f * s, headY - 0.12f * s, 0); rlRotatef(-70 + std::sin(time * 1.5f + entityIndex(o)) * 20, 0, 0, 1);   // vòi đung đưa
        box(0, -0.25f * s, 0, 0.11f * s, 0.5f * s, 0.11f * s, shadeC(c, 0.95f));
        rlPopMatrix();
        for (int k = -1; k <= 1; k += 2) {   // ngà cong
            rlPushMatrix(); rlTranslatef(0.82f * s, headY - 0.18f * s, k * 0.13f * s); rlRotatef(-35, 0, 0, 1);
            box(0.2f * s, 0, 0, 0.42f * s, 0.05f, 0.05f, Color{245, 240, 225, 255});
            rlPopMatrix();
        }
        box(-0.55f * s, leg + 0.3f * s + std::sin(time * 3) * 0.03f, 0, 0.12f * s, 0.05f, 0.05f, shadeC(c, 0.6f));
        break;
    }
    default: {   // thú bốn chân (sói, hươu, gấu, lợn rừng, cáo, thỏ...)
        float leg = 0.26f * s;
        for (int k = -1; k <= 1; k += 2)
            for (int l = -1; l <= 1; l += 2) limb(l * 0.24f * s, leg, k * 0.1f * s, leg, 0.07f * s, swing * 38 * k * l, shadeC(c, 0.7f));
        box(0, leg + 0.13f * s, 0, 0.68f * s, 0.28f * s, 0.27f * s, shadeC(c, 0.88f));
        float headY = leg + (eating ? 0.02f : 0.3f) * s, headX = 0.42f * s;
        rlPushMatrix(); rlTranslatef(headX, headY, 0); rlRotatef(eating ? -35.0f + std::sin(time * 6) * 8 : std::sin(phase * 0.5f) * 4, 0, 0, 1);
        box(0, 0, 0, 0.22f * s, 0.2f * s, 0.18f * s, c);
        box(0.13f * s, -0.04f * s, 0, 0.12f * s, 0.1f * s, 0.11f * s, shadeC(c, 1.1f));     // mõm
        box(0.19f * s, -0.02f * s, 0, 0.03f, 0.03f, 0.04f, Color{30, 25, 25, 255});         // mũi
        eyes(0.09f * s, 0.05f * s, 0, 0.06f * s, 0.03f * s);
        if (b.species == sp::Deer) {   // gạc hươu
            box(-0.02f * s, 0.2f * s, 0.06f * s, 0.03f, 0.25f * s, 0.03f, Color{200, 180, 140, 255});
            box(-0.02f * s, 0.2f * s, -0.06f * s, 0.03f, 0.25f * s, 0.03f, Color{200, 180, 140, 255});
        } else if (b.species == sp::Rabbit) {   // tai thỏ dài
            box(-0.03f * s, 0.2f * s, 0.04f * s, 0.04f * s, 0.25f * s, 0.05f * s, c);
            box(-0.03f * s, 0.2f * s, -0.04f * s, 0.04f * s, 0.25f * s, 0.05f * s, c);
        } else {
            box(-0.03f * s, 0.12f * s, 0.06f * s, 0.05f * s, 0.08f * s, 0.05f * s, shadeC(c, 0.8f));   // tai
            box(-0.03f * s, 0.12f * s, -0.06f * s, 0.05f * s, 0.08f * s, 0.05f * s, shadeC(c, 0.8f));
        }
        if (b.species == sp::Boar) { box(0.2f * s, -0.08f * s, 0.05f * s, 0.08f * s, 0.02f, 0.02f, Color{245, 240, 225, 255}); box(0.2f * s, -0.08f * s, -0.05f * s, 0.08f * s, 0.02f, 0.02f, Color{245, 240, 225, 255}); }
        rlPopMatrix();
        // đuôi vẫy (sói, cáo vẫy mạnh hơn khi chạy)
        rlPushMatrix(); rlTranslatef(-0.34f * s, leg + 0.2f * s, 0); rlRotatef(std::sin(time * (moving ? 12.0f : 4.0f) + entityIndex(o)) * 25, 0, 1, 0); rlRotatef(-25, 0, 0, 1);
        bool bushy = b.species == sp::Fox || b.species == sp::Wolf;
        box(-0.13f * s, 0, 0, 0.26f * s, bushy ? 0.09f * s : 0.04f * s, bushy ? 0.09f * s : 0.04f * s, b.species == sp::Fox ? Color{240, 150, 70, 255} : shadeC(c, 0.8f));
        rlPopMatrix();
        break;
    }
    }
    rlPopMatrix();
}

// Cây: thân + tán nhiều lớp, đung đưa trong gió
void drawTree(float px, float g, float pz, uint32_t h, Terrain terr, float time, float wind) {
    float th = 0.7f + (h % 5) * 0.14f;
    float sway = std::sin(time * 1.3f + (h % 100) * 0.1f) * 0.04f * wind;
    float ox = ((h >> 4) % 5 - 2) * 0.07f, oz = ((h >> 8) % 5 - 2) * 0.07f;
    box(px + ox, g + th * 0.5f, pz + oz, 0.13f, th, 0.13f, Color{105, 74, 44, 255});
    bool conifer = terr == Terrain::Snow || terr == Terrain::Tundra;
    Color leaf = terr == Terrain::Jungle ? Color{38, 122, 48, 255} : conifer ? Color{46, 96, 70, 255} : Color{58, 138, 60, 255};
    leaf = shadeC(leaf, 0.85f + (h % 4) * 0.07f);
    if (conifer) {
        for (int k = 0; k < 3; ++k) {   // tán lá kim nhiều tầng
            float w = 0.85f - k * 0.22f, yy = g + th * 0.6f + k * 0.38f;
            box(px + ox + sway * k, yy, pz + oz, w, 0.3f, w, k == 2 && terr == Terrain::Snow ? Color{235, 240, 245, 255} : shadeC(leaf, 1.0f + k * 0.06f));
        }
    } else {
        float cw = 0.65f + (h % 3) * 0.12f;
        rlPushMatrix();
        rlTranslatef(px + ox + sway, g + th + cw * 0.35f, pz + oz);
        rlRotatef(static_cast<float>(h % 90), 0, 1, 0);
        box(0, 0, 0, cw, cw * 0.75f, cw, leaf);
        box(0.15f, cw * 0.35f, -0.1f, cw * 0.7f, cw * 0.55f, cw * 0.7f, shadeC(leaf, 1.12f));
        box(-0.18f, -0.05f, 0.14f, cw * 0.6f, cw * 0.5f, cw * 0.6f, shadeC(leaf, 0.92f));
        if (terr == Terrain::Jungle) box(0, cw * 0.1f, 0.3f, cw * 0.5f, cw * 0.4f, cw * 0.5f, shadeC(leaf, 1.05f));
        rlPopMatrix();
    }
}

void drawBuilding3D(Building bd, float x, float y, float z, Color owner, float time) {
    Color wood{168, 122, 74, 255}, woodD{120, 84, 50, 255}, stone{168, 166, 160, 255};
    switch (bd) {
    case Building::Hut:
        for (int k = 0; k < 6; ++k) {   // lều hình nón ghép từ cọc gỗ
            float a = k * 1.047f;
            rlPushMatrix(); rlTranslatef(x + std::cos(a) * 0.3f, y, z + std::sin(a) * 0.3f); rlRotatef(-a * RAD2DEG, 0, 1, 0); rlRotatef(20, 0, 0, 1);
            box(0, 0.45f, 0, 0.05f, 0.95f, 0.05f, woodD); rlPopMatrix();
        }
        DrawCylinderEx({x, y, z}, {x, y + 0.85f, z}, 0.42f, 0.04f, 6, shadeC(owner, 0.85f));
        break;
    case Building::House: case Building::StoneHouse: case Building::Granary: case Building::Workshop: case Building::TownHall: {
        bool st = bd == Building::StoneHouse;
        float hh = bd == Building::TownHall ? 1.3f : 0.85f;
        box(x, y + hh * 0.5f, z, 0.86f, hh, 0.86f, st ? stone : wood);
        if (!st) for (int k = 0; k < 4; ++k) box(x + (k < 2 ? -0.42f : 0.42f), y + hh * 0.5f, z + (k % 2 ? -0.42f : 0.42f), 0.07f, hh, 0.07f, woodD);  // cột góc
        DrawCylinderEx({x, y + hh, z}, {x, y + hh + 0.55f, z}, 0.68f, 0.0f, 4, owner);                                  // mái
        box(x + 0.44f, y + 0.26f, z, 0.02f, 0.5f, 0.24f, Color{60, 40, 25, 255});                                       // cửa
        box(x, y + hh * 0.6f, z + 0.44f, 0.22f, 0.18f, 0.02f, Color{255, 220, 140, 255});                              // cửa sổ có ánh đèn
        if (bd == Building::TownHall) box(x, y + hh + 0.8f, z, 0.03f, 0.6f, 0.03f, woodD), box(x + 0.12f, y + hh + 1.0f + std::sin(time * 3) * 0.02f, z, 0.22f, 0.14f, 0.02f, owner);
        break;
    }
    case Building::Wall: box(x, y + 0.6f, z, 1.0f, 1.2f, 1.0f, stone); box(x, y + 1.27f, z + 0.3f, 0.3f, 0.15f, 0.3f, stone); break;
    case Building::Tower:
        box(x, y + 1.2f, z, 0.8f, 2.4f, 0.8f, stone);
        box(x, y + 2.5f, z, 0.95f, 0.2f, 0.95f, shadeC(stone, 0.9f));
        box(x, y + 2.9f, z, 0.03f, 0.6f, 0.03f, woodD);
        box(x + 0.15f, y + 3.05f, z + std::sin(time * 4) * 0.02f, 0.3f, 0.18f, 0.02f, owner);   // cờ bay
        break;
    case Building::Castle:
        box(x, y + 1.0f, z, 1.0f, 2.0f, 1.0f, shadeC(stone, 0.9f));
        for (int k = 0; k < 4; ++k) box(x + (k < 2 ? -0.38f : 0.38f), y + 2.1f, z + (k % 2 ? -0.38f : 0.38f), 0.22f, 0.25f, 0.22f, stone);
        box(x + 0.15f, y + 2.6f, z, 0.3f, 0.18f, 0.02f, owner);
        break;
    case Building::Temple:
        box(x, y + 0.15f, z, 1.0f, 0.3f, 1.0f, stone);
        box(x, y + 0.65f, z, 0.75f, 0.7f, 0.75f, Color{225, 215, 185, 255});
        DrawCylinderEx({x, y + 1.0f, z}, {x, y + 1.7f, z}, 0.6f, 0.0f, 4, Color{230, 190, 60, 255});
        break;
    case Building::Forge: {
        box(x, y + 0.45f, z, 0.85f, 0.9f, 0.85f, Color{95, 90, 88, 255});
        box(x + 0.3f, y + 1.1f, z + 0.3f, 0.2f, 0.5f, 0.2f, Color{80, 76, 74, 255});           // ống khói
        float f = std::sin(time * 9) * 0.5f + 0.5f;
        box(x + 0.43f, y + 0.35f, z, 0.03f, 0.3f, 0.35f, Color{255, static_cast<unsigned char>(120 + 80 * f), 30, 255});   // miệng lò rực sáng
        for (int k = 0; k < 3; ++k) {   // khói bốc lên
            float t = std::fmod(time * 0.6f + k * 0.33f, 1.0f);
            box(x + 0.3f + t * 0.3f, y + 1.4f + t * 1.4f, z + 0.3f, 0.15f + t * 0.25f, 0.15f + t * 0.25f, 0.15f + t * 0.25f, Color{120, 120, 125, static_cast<unsigned char>(150 * (1 - t))});
        }
        break;
    }
    case Building::Hearth: {
        for (int k = 0; k < 8; ++k) box(x + std::cos(k * 0.785f) * 0.28f, y + 0.06f, z + std::sin(k * 0.785f) * 0.28f, 0.12f, 0.12f, 0.12f, Color{120, 118, 112, 255});
        float f = std::sin(time * 13) * 0.5f + 0.5f;
        DrawCylinderEx({x, y, z}, {x, y + 0.35f + f * 0.12f, z}, 0.18f, 0.0f, 5, Color{255, static_cast<unsigned char>(140 + f * 80), 40, 230});
        break;
    }
    case Building::Bridge: box(x, y + 0.06f, z, 1.0f, 0.12f, 1.0f, woodD); box(x, y + 0.3f, z + 0.45f, 1.0f, 0.05f, 0.05f, woodD); box(x, y + 0.3f, z - 0.45f, 1.0f, 0.05f, 0.05f, woodD); break;
    case Building::Mine:   // ụ đá có khung gỗ chống, cửa hầm tối nhỏ
        box(x, y + 0.15f, z, 0.9f, 0.3f, 0.9f, Color{128, 120, 112, 255});
        box(x - 0.1f, y + 0.32f, z + 0.1f, 0.55f, 0.2f, 0.6f, Color{116, 108, 100, 255});
        box(x + 0.46f, y + 0.18f, z, 0.03f, 0.3f, 0.3f, Color{18, 14, 12, 255});
        box(x + 0.47f, y + 0.18f, z + 0.17f, 0.05f, 0.36f, 0.05f, woodD);
        box(x + 0.47f, y + 0.18f, z - 0.17f, 0.05f, 0.36f, 0.05f, woodD);
        box(x + 0.47f, y + 0.37f, z, 0.06f, 0.05f, 0.42f, woodD);
        break;
    case Building::Den: case Building::Nest: box(x, y + 0.15f, z, 0.7f, 0.3f, 0.7f, Color{100, 75, 50, 255}); break;
    case Building::Totem: box(x, y + 0.6f, z, 0.18f, 1.2f, 0.18f, woodD); box(x, y + 1.1f, z, 0.35f, 0.15f, 0.06f, owner); box(x, y + 0.75f, z, 0.24f, 0.18f, 0.24f, owner); break;
    default: break;
    }
}

// Cơ thể của chính mình trong khung hình (vẽ trong toạ độ camera: x trước, y lên, z phải)
void drawSelf(FirstPerson& fp, Simulation& sim, const Camera3D& cam, float time, bool moving) {
    const Body& b = sim.reg.get<Body>(fp.who);
    const SpeciesDef& sd = sim.species[b.species];
    const Social& so = sim.reg.get<Social>(fp.who);
    Color skin{sd.r, sd.g, sd.b, 255};
    float walk = moving ? std::sin(fp.walkPhase) : 0.0f;
    float breathe = std::sin(time * 2.0f) * 0.004f;
    float act = fp.actAnim;                                   // 1 → 0
    float strike = std::sin(std::min(1.0f, (1 - act) * 1.4f) * 3.14159f) * (act > 0 ? 1.0f : 0.0f);
    rlPushMatrix();
    rlTranslatef(cam.position.x, cam.position.y, cam.position.z);
    rlRotatef(-fp.yaw * RAD2DEG, 0, 1, 0);
    rlRotatef(fp.pitch * RAD2DEG, 0, 0, 1);
    rlScalef(0.6f, 0.6f, 0.6f);   // thu nhỏ & kéo sát → tay chỉ chiếm góc dưới màn hình
    rlTranslatef(0.05f, -0.08f, 0);
    switch (sd.plan) {
    case BodyPlan::Humanoid: case BodyPlan::Primate: {
        Color cloth = so.group >= 0 ? Color{sim.groups[so.group].colR, sim.groups[so.group].colG, sim.groups[so.group].colB, 255} : shadeC(skin, 0.6f);
        // tay trái: đung đưa theo bước
        box(0.38f, -0.3f + walk * 0.025f + breathe, -0.24f, 0.26f, 0.07f, 0.08f, cloth);
        box(0.53f, -0.28f + walk * 0.025f + breathe, -0.22f, 0.1f, 0.075f, 0.085f, skin);
        // tay phải: vung tới trước khi tấn công / thu thập
        float fx = 0.38f + strike * 0.22f, fy = -0.3f - walk * 0.025f + breathe + strike * (fp.actKind == 2 ? 0.12f : 0.04f);
        box(fx, fy, 0.24f, 0.26f, 0.07f, 0.08f, cloth);
        box(fx + 0.15f, fy + 0.02f, 0.22f, 0.1f, 0.075f, 0.085f, skin);
        // công cụ đang cầm
        float pierce = heldValue(sim, fp.who, Purpose::Pierce), cut = heldValue(sim, fp.who, Purpose::Cut);
        if (pierce > 0 || cut > 0) {
            rlPushMatrix();
            rlTranslatef(fx + 0.17f, fy + 0.03f, 0.21f);
            rlRotatef(pierce > 0 ? -70 + strike * 50 : 25 - strike * 80, 0, 0, 1);
            box(0, 0.18f, 0, 0.025f, pierce > 0 ? 0.6f : 0.32f, 0.025f, Color{150, 110, 70, 255});                         // cán
            if (pierce > 0) box(0, 0.5f, 0, 0.04f, 0.09f, 0.04f, Color{200, 200, 210, 255});                               // mũi giáo
            else box(0.04f, 0.32f, 0, 0.1f, 0.08f, 0.02f, Color{170, 170, 175, 255});                                       // lưỡi rìu
            rlPopMatrix();
        }
        break;
    }
    case BodyPlan::Bird: {   // mép cánh hai bên khung hình, vỗ lên xuống, lông vũ ở đầu cánh
        float flap = std::sin(time * 9) * 14;
        for (int k = -1; k <= 1; k += 2) {
            rlPushMatrix();
            rlTranslatef(0.3f, -0.28f, k * 0.32f);
            rlRotatef(k * (flap - 8), 1, 0, 0);
            box(0, 0, k * 0.25f, 0.2f, 0.025f, 0.5f, skin);
            for (int f = 0; f < 4; ++f) box(-0.06f - f * 0.03f, -0.005f, k * (0.42f + f * 0.07f), 0.14f - f * 0.02f, 0.02f, 0.06f, shadeC(skin, 0.7f + f * 0.05f));
            rlPopMatrix();
        }
        box(0.42f, -0.3f, 0, 0.12f, 0.04f, 0.05f, Color{230, 190, 60, 255});   // mỏ
        break;
    }
    case BodyPlan::Fish: case BodyPlan::Bat: break;
    default: {
        // thú: mõm ở dưới tầm mắt, chân trước bước luân phiên, lao tới khi cắn
        box(0.32f + strike * 0.12f, -0.2f + breathe, 0, 0.18f, 0.1f, 0.14f, shadeC(skin, 1.05f));
        box(0.42f + strike * 0.12f, -0.19f, 0, 0.03f, 0.03f, 0.05f, Color{30, 25, 25, 255});
        box(0.42f + walk * 0.06f, -0.4f + std::max(0.0f, walk) * 0.06f, -0.13f, 0.08f, 0.16f, 0.07f, shadeC(skin, 0.75f));
        box(0.42f - walk * 0.06f, -0.4f + std::max(0.0f, -walk) * 0.06f, 0.13f, 0.08f, 0.16f, 0.07f, shadeC(skin, 0.75f));
        break;
    }
    }
    rlPopMatrix();
}

void drawCloud(Vector2 c, float r, unsigned char a) {
    DrawCircleV(c, r, Color{255, 255, 255, a});
    DrawCircleV({c.x - r * 0.9f, c.y + r * 0.25f}, r * 0.7f, Color{250, 250, 255, a});
    DrawCircleV({c.x + r * 0.95f, c.y + r * 0.2f}, r * 0.75f, Color{250, 250, 255, a});
    DrawCircleV({c.x + r * 0.3f, c.y - r * 0.35f}, r * 0.6f, Color{255, 255, 255, a});
}
} // namespace

void fpEnter(FirstPerson& fp, Simulation& sim, Entity e) {
    if (!sim.reg.valid(e)) return;
    fp.active = true;
    fp.control = false;
    fp.who = e;
    fp.cx = INT_MIN;
    fp.camInit = false;
    fp.smooth.clear();
    const Transform& tr = sim.reg.get<Transform>(e);
    Vec2 d = wdiff(tr.target, tr.pos);
    fp.yaw = d.len2() > 1e-4f ? std::atan2(d.y, d.x) : 0.0f;
    fp.pitch = sim.species[sim.reg.get<Body>(e).species].plan == BodyPlan::Bird ? -0.35f : -0.1f;
    fp.msg.clear();
}

void fpExit(FirstPerson& fp, Simulation& sim) {
    if (sim.reg.valid(fp.who)) {
        Mind& m = sim.reg.get<Mind>(fp.who);
        m.possessed = false;
        m.needReplan = true;
        m.nextThink = sim.tick;
    }
    fp.active = false;
    fp.control = false;
    fp.who = NullEntity;
}

void fpRelease(FirstPerson& fp) {
    if (fp.meshReady) {
        // texture trắng mặc định không thuộc quyền model → tách ra trước khi giải phóng
        fp.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{};
        fp.model.materials[0].shader = Shader{rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
        UnloadModel(fp.model);
        fp.meshReady = false;
    }
    if (fp.shaderReady) { UnloadShader(fp.shader); fp.shaderReady = false; }
    fp.shaderTried = false;
    fp.cx = INT_MIN;
    fp.smooth.clear();
}

Vec2 fpFacing(const FirstPerson& fp) { return {std::cos(fp.yaw), std::sin(fp.yaw)}; }

void fpRender(FirstPerson& fp, Simulation& sim, Renderer& r, float time) {
    (void)r;
    if (!sim.reg.valid(fp.who)) return;
    ensureShader(fp);
    const World& w = sim.world;
    const Transform& me = sim.reg.get<Transform>(fp.who);
    const Body& body = sim.reg.get<Body>(fp.who);
    const SpeciesDef& sd = sim.species[body.species];
    float dt = std::min(0.1f, GetFrameTime());
    float follow = 1.0f - std::exp(-dt * 14.0f);   // hệ số đuổi theo vị trí mô phỏng (mượt, độc lập FPS)
    static long frameNo = 0;
    ++frameNo;
    fp.actAnim = std::max(0.0f, fp.actAnim - dt * 2.5f);
    // ---- Vị trí camera mượt ----
    if (!fp.camInit || dist2(fp.camPos, me.pos) > 9) { fp.camPos = me.pos; fp.camInit = true; }
    Vec2 step = wdiff(me.pos, fp.camPos) * follow;
    fp.camPos = wrapPos(fp.camPos + step);
    bool moving = step.len2() > 1e-7f || me.moving;
    fp.walkPhase += step.len() * 9.0f;
    int mode = modeOf(sim, fp.who);
    int tx = static_cast<int>(std::floor(fp.camPos.x)), ty = static_cast<int>(std::floor(fp.camPos.y));
    if (tx != fp.cx || ty != fp.cy || mode != fp.mode || GetTime() - fp.builtAt > 2.0) buildMesh(fp, sim, tx, ty, mode);
    // Nếu không điều khiển: quay đầu theo hướng sinh vật đang đi (mượt)
    if (!fp.control && me.moving) {
        Vec2 d = wdiff(me.target, me.pos);
        if (d.len2() > 1e-3f) {
            float want = std::atan2(d.y, d.x), diff = std::remainder(want - fp.yaw, 6.2831853f);
            fp.yaw += diff * std::min(1.0f, dt * 3.0f);
        }
    }
    // ---- Camera ở vị trí mắt ----
    Vec2 origin{static_cast<float>(fp.cx), static_cast<float>(fp.cy)};
    Vec2 lp = wdiff(fp.camPos, origin);
    float ground = groundAt(w, fp.camPos.x, fp.camPos.y, mode);
    float eye;
    switch (sd.plan) {
    case BodyPlan::Bird: eye = std::max(ground, 0.0f) + 3.5f + std::sin(time * 0.8f) * 0.15f; break;
    case BodyPlan::Fish: eye = std::min(-0.4f, ground + 0.5f); break;
    case BodyPlan::Bat: eye = 0.9f + std::sin(time * 3) * 0.05f; break;
    case BodyPlan::Spider: case BodyPlan::Reptile: eye = ground + 0.25f * sd.size + 0.1f; break;
    case BodyPlan::Humanoid: eye = ground + 0.8f * std::max(0.6f, sd.size) + 0.05f; break;
    case BodyPlan::Giant: eye = ground + 1.6f; break;
    default: eye = ground + 0.45f * sd.size + 0.2f; break;
    }
    if (moving && sd.plan != BodyPlan::Bird && sd.plan != BodyPlan::Fish && sd.plan != BodyPlan::Bat) eye += std::fabs(std::sin(fp.walkPhase)) * 0.035f;
    Camera3D cam{};
    cam.position = {lp.x, eye, lp.y};
    float cp = std::cos(fp.pitch);
    cam.target = {lp.x + std::cos(fp.yaw) * cp, eye + std::sin(fp.pitch), lp.y + std::sin(fp.yaw) * cp};
    cam.up = {0, 1, 0};
    cam.fovy = 70;
    cam.projection = CAMERA_PERSPECTIVE;

    // ---- Bầu trời, mặt trời, mây ----
    int SW = GetScreenWidth(), SH = GetScreenHeight();
    float season = w.weather().seasonTemp;
    bool raining = w.raining(tx, ty);
    Color skyTop, horizon;
    if (mode == 2) { skyTop = Color{5, 4, 8, 255}; horizon = Color{10, 9, 14, 255}; }
    else if (mode == 1) { skyTop = Color{30, 110, 160, 255}; horizon = Color{18, 70, 110, 255}; }
    else if (raining) { skyTop = Color{90, 98, 115, 255}; horizon = Color{150, 156, 166, 255}; }
    else { skyTop = Color{static_cast<unsigned char>(70 + season * 10), static_cast<unsigned char>(130 + season * 12), 215, 255}; horizon = Color{200, 222, 238, 255}; }
    ClearBackground(horizon);
    // vị trí đường chân trời trên màn hình phụ thuộc góc ngẩng/cúi
    float horizonY = SH * 0.5f + std::tan(fp.pitch) / std::tan(cam.fovy * 0.5f * DEG2RAD) * SH * 0.5f;
    DrawRectangleGradientV(0, 0, SW, static_cast<int>(std::max(1.0f, horizonY)), skyTop, horizon);
    Vector3 sunDir{0.45f, 0.78f, -0.43f};
    if (mode == 0) {
        Vector3 sp{cam.position.x + sunDir.x * 200, cam.position.y + sunDir.y * 200, cam.position.z + sunDir.z * 200};
        float facing = (sunDir.x * std::cos(fp.yaw) + sunDir.z * std::sin(fp.yaw));
        if (facing > -0.2f && !raining) {
            Vector2 s2 = GetWorldToScreen(sp, cam);
            DrawCircleGradient(static_cast<int>(s2.x), static_cast<int>(s2.y), SH * 0.16f, Color{255, 250, 210, 120}, Color{255, 250, 210, 0});
            DrawCircleV(s2, SH * 0.035f, Color{255, 252, 230, 255});
        }
        for (int i = 0; i < 16; ++i) {   // mây trôi theo gió
            uint32_t h = hash32(i * 7919 + 3);
            float az = (h % 6283) / 1000.0f + time * 0.008f * (1 + (h >> 20) % 3), el = 0.1f + ((h >> 8) % 100) / 400.0f;
            Vector3 d{std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)};
            if (d.x * std::cos(fp.yaw) * cp + d.z * std::sin(fp.yaw) * cp + d.y * std::sin(fp.pitch) < 0.2f) continue;
            Vector2 c2 = GetWorldToScreen({cam.position.x + d.x * 150, cam.position.y + d.y * 150, cam.position.z + d.z * 150}, cam);
            drawCloud(c2, SH * (0.03f + ((h >> 4) % 5) * 0.008f), raining ? 200 : 170);
        }
    }

    // ---- Uniform shader: hướng nắng, sương mù hoà vào chân trời ----
    if (fp.shaderReady) {
        float fogC[3] = {horizon.r / 255.0f, horizon.g / 255.0f, horizon.b / 255.0f};
        float fogR[2] = {mode == 0 ? (raining ? 6.0f : 10.0f) : (mode == 1 ? 1.5f : 1.0f), mode == 0 ? (raining ? 19.0f : 24.0f) : (mode == 1 ? 11.0f : 9.0f)};
        float amb[3] = {1.0f, 1.0f, 1.0f};
        if (mode == 1) { amb[0] = 0.7f; amb[1] = 0.9f; amb[2] = 1.05f; }
        if (raining && mode == 0) { amb[0] = amb[1] = 0.8f; amb[2] = 0.85f; }
        float sun[3] = {sunDir.x, sunDir.y, sunDir.z};
        float light = mode == 2 ? 0.5f : 1.0f;
        SetShaderValue(fp.shader, fp.locSun, sun, SHADER_UNIFORM_VEC3);
        SetShaderValue(fp.shader, fp.locFog, fogC, SHADER_UNIFORM_VEC3);
        SetShaderValue(fp.shader, fp.locFogRange, fogR, SHADER_UNIFORM_VEC2);
        SetShaderValue(fp.shader, fp.locAmbient, amb, SHADER_UNIFORM_VEC3);
        SetShaderValue(fp.shader, fp.locLight, &light, SHADER_UNIFORM_FLOAT);
    }

    BeginMode3D(cam);
    if (fp.shaderReady) BeginShaderMode(fp.shader);
    DrawModel(fp.model, {0, 0, 0}, 1.0f, WHITE);
    float wind = 1.0f + std::fabs(w.matter.wind.x + w.matter.wind.y) * 8 + (raining ? 1.5f : 0.0f);
    // ---- Cây, cỏ, đá, công trình, lửa trong bán kính ----
    for (int b = -kObjR; b <= kObjR; ++b)
        for (int a = -kObjR; a <= kObjR; ++a) {
            int d2 = a * a + b * b;
            if (d2 > kObjR * kObjR) continue;
            int x = fp.cx + a, y = fp.cy + b;
            if (!w.inBounds(x, y)) continue;
            float px = a + 0.5f, pz = b + 0.5f;
            uint32_t h = hash32(static_cast<uint32_t>(w.wrapX(x) * 7349 + y * 1931));
            if (mode == 2) {
                const UnderTile& u = w.under(x, y);
                if (u.kind == Under::Cave && u.fungus > 50)
                    for (int k = 0; k < 3; ++k) {   // cụm nấm phát quang
                        uint32_t hk = hash32(h + k * 31);
                        float fx = px - 0.35f + (hk % 70) * 0.01f, fz = pz - 0.35f + ((hk >> 8) % 70) * 0.01f, fh = 0.1f + (hk % 5) * 0.03f;
                        box(fx, fh * 0.5f, fz, 0.03f, fh, 0.03f, Color{200, 230, 210, 255});
                        box(fx, fh, fz, 0.12f, 0.04f, 0.12f, Color{70, 240, 200, 255});
                    }
                if (u.kind == Under::Cave && h % 9 == 0) DrawCylinderEx({px, 0, pz}, {px, 0.55f, pz}, 0.12f, 0.0f, 5, Color{120, 110, 100, 255}); // măng đá
                if (u.kind == Under::Magma) box(px, 0.02f + 0.03f * std::sin(time * 3 + a), pz, 1.0f, 0.06f, 1.0f, Color{255, 140, 40, 255});
                if (u.kind == Under::Tunnel && (x + y) % 3 == 0) {   // cột chống & đèn trong hầm mỏ
                    box(px - 0.4f, 0.5f, pz, 0.08f, 1.0f, 0.08f, Color{125, 88, 50, 255});
                    box(px - 0.32f, 0.85f + std::sin(time * 7 + x) * 0.01f, pz, 0.08f, 0.08f, 0.08f, Color{255, 220, 110, 255});
                }
                continue;
            }
            const Tile& t = w.at(x, y);
            if (w.isWater(x, y)) continue;
            float g = groundAt(w, x + 0.5f, y + 0.5f, mode);
            float ddx = px - lp.x, ddz = pz - lp.y;
            bool atEye = ddx * ddx + ddz * ddz < 0.6f;   // cây/nhà trùng chỗ đứng → ẩn để không che kín camera
            if (!atEye && t.wood > 40 && (t.terrain == Terrain::Forest || t.terrain == Terrain::Jungle || t.terrain == Terrain::Snow || t.terrain == Terrain::Tundra))
                drawTree(px, g, pz, h, t.terrain, time, wind);
            // Cỏ lay trong gió gần người xem
            if (d2 < 64 && (t.terrain == Terrain::Grass || t.terrain == Terrain::Savanna || t.terrain == Terrain::Forest || t.terrain == Terrain::Jungle)) {
                Color gc = t.terrain == Terrain::Savanna ? Color{196, 186, 96, 255} : Color{92, 170, 66, 255};
                for (int k = 0; k < 5; ++k) {
                    uint32_t hk = hash32(h * 13 + k);
                    float gx = px - 0.45f + (hk % 90) * 0.01f, gz = pz - 0.45f + ((hk >> 7) % 90) * 0.01f, gh = 0.08f + (hk % 7) * 0.02f;
                    float bend = std::sin(time * 2.2f + gx * 1.7f + gz * 1.3f) * 0.025f * wind;
                    box(gx + bend, groundAt(w, x + 0.5f, y + 0.5f, mode) + gh * 0.5f, gz, 0.025f, gh, 0.025f, shadeC(gc, 0.85f + (hk % 5) * 0.06f));
                }
            }
            if (t.stone > 40 && t.terrain != Terrain::Mountain && t.terrain != Terrain::Hill && h % 3 == 0)
                box(px, g + 0.08f, pz, 0.25f, 0.16f, 0.2f, Color{140, 138, 132, 255}), box(px + 0.12f, g + 0.05f, pz + 0.1f, 0.12f, 0.1f, 0.12f, Color{125, 122, 118, 255});
            if (t.food > 140 && t.terrain != Terrain::Farmland && h % 4 == 0) {
                box(px + 0.2f, g + 0.13f, pz - 0.1f, 0.26f, 0.24f, 0.26f, Color{56, 128, 54, 255});   // bụi quả mọng
                box(px + 0.27f, g + 0.24f, pz - 0.05f, 0.05f, 0.05f, 0.05f, Color{210, 40, 60, 255});
                box(px + 0.14f, g + 0.2f, pz - 0.15f, 0.05f, 0.05f, 0.05f, Color{210, 40, 60, 255});
            }
            if (t.terrain == Terrain::Farmland && t.food > 40 && d2 < 144)   // lúa
                for (int k = 0; k < 4; ++k) box(px - 0.3f + k * 0.2f, g + 0.12f + std::sin(time * 2 + k + a) * 0.01f, pz, 0.04f, 0.25f, 0.8f, Color{220, 196, 80, 255});
            if (t.building != Building::None && !atEye) {
                Color owner = t.owner >= 0 && t.owner < static_cast<int>(sim.groups.size())
                                  ? Color{sim.groups[t.owner].colR, sim.groups[t.owner].colG, sim.groups[t.owner].colB, 255} : Color{150, 60, 50, 255};
                drawBuilding3D(t.building, px, g, pz, owner, time);
            }
            if (t.fire) {   // ngọn lửa nhảy múa + khói
                setLight(fp, 0.0f);
                for (int k = 0; k < 3; ++k) {
                    float f = std::sin(time * 13 + a * 3 + b * 5 + k * 2.1f) * 0.5f + 0.5f;
                    DrawCylinderEx({px - 0.2f + k * 0.2f, g, pz}, {px - 0.2f + k * 0.2f, g + 0.5f + f * 0.5f, pz}, 0.22f, 0.0f, 5,
                                   Color{255, static_cast<unsigned char>(110 + f * 120), 30, 220});
                }
                float t2 = std::fmod(time * 0.8f + a * 0.3f, 1.0f);
                box(px, g + 1.0f + t2 * 2, pz, 0.3f + t2 * 0.5f, 0.3f + t2 * 0.5f, 0.3f + t2 * 0.5f, Color{70, 65, 65, static_cast<unsigned char>(140 * (1 - t2))});
                setLight(fp, 1.0f);
            }
        }
    // ---- Sinh vật khác (cùng tầng sống), vị trí & hướng nội suy mượt ----
    int myLayer = sim.layerOf(body.species);
    sim.grid.query(fp.camPos, static_cast<float>(kObjR), [&](Entity o, Vec2) {
        if (o == fp.who) return;
        const Body* ob = sim.reg.pool<Body>().tryGet(o);
        if (!ob || ob->dead || sim.layerOf(ob->species) != myLayer) return;
        const Transform& otr = sim.reg.get<Transform>(o);
        const SpeciesDef& od = sim.species[ob->species];
        SmoothState& st = fp.smooth[o];
        float speed = 0;
        if (st.seen == 0 || frameNo - st.seen > 30 || dist2(st.pos, otr.pos) > 9) {
            st.pos = otr.pos;
            Vec2 d = wdiff(otr.target, otr.pos);
            st.yaw = d.len2() > 1e-4f ? std::atan2(d.y, d.x) : static_cast<float>(entityIndex(o) % 6);
        } else {
            Vec2 mv = wdiff(otr.pos, st.pos) * follow;
            st.pos = wrapPos(st.pos + mv);
            speed = mv.len() / std::max(1e-4f, dt);
            st.phase += mv.len() * (14.0f / std::max(0.4f, od.size));   // bước chân tỉ lệ quãng đường
            if (mv.len2() > 1e-7f) {
                float want = std::atan2(mv.y, mv.x);
                st.yaw += std::remainder(want - st.yaw, 6.2831853f) * std::min(1.0f, dt * 8.0f);
            }
        }
        st.seen = frameNo;
        if (dist2(st.pos, fp.camPos) < 0.36f) return;   // đứng sát mắt → không vẽ (tránh che kín màn hình)
        Vec2 lo = wdiff(st.pos, origin);
        float gy = groundAt(w, st.pos.x, st.pos.y, mode);
        if (od.plan == BodyPlan::Bird) gy = std::max(gy, 0.0f) + 2.6f + 0.4f * std::sin(time * 0.9f + entityIndex(o));
        else if (od.plan == BodyPlan::Fish) gy = std::min(-0.35f, gy + 0.3f + 0.1f * std::sin(time * 2 + entityIndex(o)));
        else if (od.plan == BodyPlan::Bat) gy = 0.7f + 0.25f * std::sin(time * 3 + entityIndex(o));
        else if (w.isWater(static_cast<int>(st.pos.x), static_cast<int>(st.pos.y))) gy = mode == 1 ? -0.1f : -0.18f;   // bơi: chìm nửa người
        drawCreature3D(sim, o, lo.x, gy, lo.y, st.yaw, st.phase, speed, time);
    });
    if (frameNo % 120 == 0)
        for (auto it = fp.smooth.begin(); it != fp.smooth.end();) it = frameNo - it->second.seen > 120 ? fp.smooth.erase(it) : std::next(it);
    // ---- Mũi tên, đá bay ----
    for (const PhysBody& pb : sim.physics.bodies) {
        Vec2 lo = wdiff(pb.pos, origin);
        if (std::fabs(lo.x) > kObjR || std::fabs(lo.y) > kObjR) continue;
        const MaterialDef& md = w.materials[pb.mat];
        float yy = groundAt(w, pb.pos.x, pb.pos.y, mode) + pb.z * 0.5f + 0.1f;
        if (pb.kind == BodyKind::Projectile) {   // mũi tên định hướng theo vận tốc
            Vec2 v = pb.vel.normalized();
            DrawLine3D({lo.x - v.x * 0.3f, yy, lo.y - v.y * 0.3f}, {lo.x + v.x * 0.3f, yy, lo.y + v.y * 0.3f}, Color{150, 110, 70, 255});
        } else {
            box(lo.x, yy, lo.y, std::max(0.1f, pb.radius), std::max(0.1f, pb.radius), std::max(0.1f, pb.radius), Color{md.r, md.g, md.b, 255});
        }
    }
    // ---- Mặt nước gợn sóng (bán trong suốt, vẽ sau cùng) ----
    if (mode != 2) {
        rlDisableBackfaceCulling();   // dưới nước nhìn lên vẫn thấy mặt nước
        // Sóng tính tại GÓC ô (dùng chung giữa các ô kề nhau) → mặt nước liền, không hở khe
        auto waveAt = [&](int a, int b) { return -0.02f + 0.03f * std::sin(time * 1.8f + a * 0.9f + b * 0.7f) + 0.015f * std::sin(time * 2.9f - a * 1.7f + b * 1.3f); };
        auto shimmer = [&](int a, int b) { return static_cast<unsigned char>(150 + 50 * std::sin(time * 2.5f + a * 1.3f - b)); };
        rlBegin(RL_TRIANGLES);
        rlNormal3f(0, 1, 0);
        for (int b = -kObjR; b <= kObjR; ++b)
            for (int a = -kObjR; a <= kObjR; ++a) {
                int x = fp.cx + a, y = fp.cy + b;
                if (a * a + b * b > kObjR * kObjR || !w.inBounds(x, y) || !w.isWater(x, y)) continue;
                const int ca[4] = {a, a + 1, a, a + 1}, cb[4] = {b, b, b + 1, b + 1};
                auto vtx = [&](int k) {
                    unsigned char sh = shimmer(ca[k], cb[k]);
                    if (mode == 1) rlColor4ub(120, 190, 230, 90);
                    else rlColor4ub(40, static_cast<unsigned char>(sh / 2 + 45), sh, 160);
                    rlVertex3f(static_cast<float>(ca[k]), waveAt(ca[k], cb[k]), static_cast<float>(cb[k]));
                };
                vtx(0); vtx(2); vtx(1);
                vtx(1); vtx(2); vtx(3);
            }
        rlEnd();
        rlEnableBackfaceCulling();
    }
    // ---- Mưa ----
    if (raining && mode == 0)
        for (int i = 0; i < 220; ++i) {
            uint32_t h = hash32(i * 977);
            float rx = lp.x + (h % 2000) / 100.0f - 10, rz = lp.y + ((h >> 11) % 2000) / 100.0f - 10;
            float ry = eye + 5 - std::fmod(time * 11 + (h >> 22) % 10, 8.0f);
            DrawLine3D({rx, ry, rz}, {rx - 0.03f, ry - 0.4f, rz}, Color{200, 210, 240, 150});
        }
    // ---- Cơ thể của chính mình ----
    setLight(fp, 0.7f);
    drawSelf(fp, sim, cam, time, moving);
    if (fp.shaderReady) EndShaderMode();
    EndMode3D();
    if (mode == 1) {   // dưới nước: ánh sáng xuyên mặt nước chập chờn
        DrawRectangle(0, 0, SW, SH, Color{20, 80, 140, 60});
        for (int i = 0; i < 6; ++i) {
            float xx = SW * (0.1f + i * 0.17f) + std::sin(time * 0.7f + i) * SW * 0.03f;
            DrawTriangle({xx, 0}, {xx - SW * 0.08f, static_cast<float>(SH)}, {xx + SW * 0.04f, static_cast<float>(SH)}, Color{180, 230, 255, 12});
        }
    }
}

} // namespace wb
