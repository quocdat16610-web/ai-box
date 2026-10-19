// =============================================================================
// FirstPerson.cpp — Dựng cảnh 3D qua mắt sinh vật (xem FirstPerson.h).
// =============================================================================
#include "FirstPerson.h"
#include "Renderer.h"
#include "../sim/Simulation.h"
#include "rlgl.h"
#include <cmath>
#include <algorithm>

namespace wb {

namespace {
constexpr int kR = 28;               // bán kính lưới địa hình (ô)
constexpr int kN = 2 * kR + 1;       // số ô mỗi cạnh
constexpr int kObjR = 22;            // bán kính vẽ cây/công trình/sinh vật
constexpr int kQuadsPerTile = 5;     // mặt trên + 4 vách (16-bit index: 57²·5·4 = 64980 đỉnh < 65536)

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
    if (t.terrain == Terrain::DeepWater) return mode == 1 ? -2.2f : -0.12f;
    if (t.terrain == Terrain::ShallowWater) return mode == 1 ? -1.0f : -0.08f;
    if (t.terrain == Terrain::Ice) return 0.04f;
    return 0.05f + (t.height - 0.42f) * 14.0f;
}

Color shadeC(Color c, float k) {
    return Color{static_cast<unsigned char>(std::clamp(c.r * k, 0.0f, 255.0f)), static_cast<unsigned char>(std::clamp(c.g * k, 0.0f, 255.0f)),
                 static_cast<unsigned char>(std::clamp(c.b * k, 0.0f, 255.0f)), c.a};
}

// Dựng lại lưới địa hình quanh ô (cx, cy)
void buildMesh(FirstPerson& fp, Simulation& sim, Renderer& r, int cx, int cy, int mode) {
    const World& w = sim.world;
    if (!fp.meshReady) {
        Mesh mesh{};
        // dung lượng: 1 mặt trên + tối đa 4 vách đứng mỗi ô (vách chỉ dùng trong hang)
        mesh.vertexCount = kN * kN * 4 * kQuadsPerTile;
        mesh.triangleCount = kN * kN * 2 * kQuadsPerTile;
        mesh.vertices = static_cast<float*>(MemAlloc(mesh.vertexCount * 3 * sizeof(float)));
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
        fp.meshReady = true;
    }
    Mesh& mesh = fp.model.meshes[0];
    // Độ cao các ô (thêm viền 1 ô để tính góc)
    static float E[(kN + 2) * (kN + 2)];
    for (int b = -1; b <= kN; ++b)
        for (int a = -1; a <= kN; ++a) E[(b + 1) * (kN + 2) + (a + 1)] = elevOf(w, cx - kR + a, cy - kR + b, mode);
    auto corner = [&](int a, int b) {   // góc trên-trái của ô (a, b)
        return 0.25f * (E[b * (kN + 2) + a] + E[b * (kN + 2) + a + 1] + E[(b + 1) * (kN + 2) + a] + E[(b + 1) * (kN + 2) + a + 1]);
    };
    const float W = static_cast<float>(w.width()), H = static_cast<float>(w.height());
    const float sx = 0.45f, sy = 0.8f, sz = -0.35f;   // hướng nắng (chuẩn hoá xấp xỉ)
    for (int b = 0; b < kN; ++b)
        for (int a = 0; a < kN; ++a) {
            int q = b * kN + a;
            float h00 = corner(a, b), h10 = corner(a + 1, b), h01 = corner(a, b + 1), h11 = corner(a + 1, b + 1);
            float x0 = static_cast<float>(a - kR), z0 = static_cast<float>(b - kR);
            float* v = mesh.vertices + q * 12;
            v[0] = x0;     v[1] = h00; v[2] = z0;
            v[3] = x0 + 1; v[4] = h10; v[5] = z0;
            v[6] = x0;     v[7] = h01; v[8] = z0 + 1;
            v[9] = x0 + 1; v[10] = h11; v[11] = z0 + 1;
            // Texture: lấy đúng điểm ảnh của ô trên bản đồ (x quấn vòng theo từng ô → không có đường nối)
            int tx = w.wrapX(cx - kR + a), ty = std::clamp(cy - kR + b, 0, w.height() - 1);
            float u0 = tx / W, u1 = (tx + 1) / W, t0 = ty / H, t1 = (ty + 1) / H;
            float* t = mesh.texcoords + q * 8;
            t[0] = u0; t[1] = t0; t[2] = u1; t[3] = t0; t[4] = u0; t[5] = t1; t[6] = u1; t[7] = t1;
            // Đổ bóng theo độ dốc (pháp tuyến xấp xỉ) — núi đồi có khối
            float nx = (h00 + h01 - h10 - h11) * 0.5f, nz = (h00 + h10 - h01 - h11) * 0.5f;
            float inv = 1.0f / std::sqrt(nx * nx + 1 + nz * nz);
            float lit = 0.62f + 0.48f * std::max(0.0f, (nx * sx + sy + nz * sz) * inv);
            if (mode == 2) {
                // Trong hang: chỉ có ánh sáng yếu quanh sinh vật, tối dần theo khoảng cách
                float d = std::sqrt(static_cast<float>((a - kR) * (a - kR) + (b - kR) * (b - kR)));
                lit *= std::clamp(1.45f - d / 11.0f, 0.1f, 1.0f);
                if (w.under(tx, ty).fungus > 50 && w.under(tx, ty).kind == Under::Cave) lit = std::max(lit, 0.55f);   // nấm phát quang
            }
            if (mode == 1 && (w.isWater(tx, ty))) lit *= 0.75f;
            unsigned char L = static_cast<unsigned char>(std::clamp(lit * 225.0f, 0.0f, 255.0f));
            unsigned char* c = mesh.colors + q * 16;
            for (int k = 0; k < 4; ++k) { c[k * 4] = L; c[k * 4 + 1] = L; c[k * 4 + 2] = mode == 1 ? static_cast<unsigned char>(std::min(255, L + 30)) : L; c[k * 4 + 3] = 255; }
        }
    // ---- Vách đứng (chỉ trong hang): ô đá cao hơn ô bên cạnh → dựng mặt tường thẳng đứng ----
    int q = kN * kN;
    if (mode == 2) {
        // mặt trên của hang: mỗi ô phẳng ở đúng độ cao của nó (không lấy trung bình) → khối vuông vức
        for (int b = 0; b < kN; ++b)
            for (int a = 0; a < kN; ++a) {
                float h = E[(b + 1) * (kN + 2) + (a + 1)];
                float* v = mesh.vertices + (b * kN + a) * 12;
                v[1] = v[4] = v[7] = v[10] = h;
            }
        const int da[4] = {1, -1, 0, 0}, db[4] = {0, 0, 1, -1};
        for (int b = 0; b < kN; ++b)
            for (int a = 0; a < kN; ++a) {
                float h = E[(b + 1) * (kN + 2) + (a + 1)];
                int tx = w.wrapX(cx - kR + a), ty = std::clamp(cy - kR + b, 0, w.height() - 1);
                float d = std::sqrt(static_cast<float>((a - kR) * (a - kR) + (b - kR) * (b - kR)));
                for (int k = 0; k < 4; ++k) {
                    float hn = E[(b + 1 + db[k]) * (kN + 2) + (a + 1 + da[k])];
                    if (hn >= h - 0.01f) continue;
                    float x0 = static_cast<float>(a - kR), z0 = static_cast<float>(b - kR);
                    // cạnh chung giữa ô và hàng xóm (p1 → p2 nhìn từ phía hàng xóm thấp hơn)
                    float ex1, ez1, ex2, ez2;
                    if (k == 0) { ex1 = x0 + 1; ez1 = z0; ex2 = x0 + 1; ez2 = z0 + 1; }
                    else if (k == 1) { ex1 = x0; ez1 = z0 + 1; ex2 = x0; ez2 = z0; }
                    else if (k == 2) { ex1 = x0 + 1; ez1 = z0 + 1; ex2 = x0; ez2 = z0 + 1; }
                    else { ex1 = x0; ez1 = z0; ex2 = x0 + 1; ez2 = z0; }
                    float* v = mesh.vertices + q * 12;   // TL, TR, BL, BR (đỉnh trên, đáy dưới)
                    // thứ tự (ex2 → ex1) để pháp tuyến hướng RA phía ô thấp (mặt trước, không bị cull)
                    v[0] = ex2; v[1] = h;  v[2] = ez2;
                    v[3] = ex1; v[4] = h;  v[5] = ez1;
                    v[6] = ex2; v[7] = hn; v[8] = ez2;
                    v[9] = ex1; v[10] = hn; v[11] = ez1;
                    float u0 = tx / W, u1 = (tx + 1) / W, t0 = ty / H, t1 = (ty + 1) / H;
                    float* t = mesh.texcoords + q * 8;
                    t[0] = u0; t[1] = t0; t[2] = u1; t[3] = t0; t[4] = u0; t[5] = t1; t[6] = u1; t[7] = t1;
                    float lit = (k < 2 ? 0.75f : 0.55f) * std::clamp(1.45f - d / 11.0f, 0.1f, 1.0f);
                    unsigned char L = static_cast<unsigned char>(std::clamp(lit * 225.0f, 0.0f, 255.0f));
                    unsigned char* c = mesh.colors + q * 16;
                    for (int j = 0; j < 4; ++j) { c[j * 4] = L; c[j * 4 + 1] = L; c[j * 4 + 2] = L; c[j * 4 + 3] = 255; }
                    if (++q >= kN * kN * kQuadsPerTile) break;
                }
            }
    }
    // phần dung lượng còn lại: tam giác suy biến (không vẽ gì)
    std::fill(mesh.vertices + q * 12, mesh.vertices + mesh.vertexCount * 3, 0.0f);
    UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);
    UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
    UpdateMeshBuffer(mesh, 3, mesh.colors, mesh.vertexCount * 4, 0);
    fp.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = mode == 2 ? r.undergroundTexture(sim) : r.terrainTexture();
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

// Vẽ một sinh vật khác dạng khối 3D theo hình dáng cơ thể. (x, y, z) là chân; yaw theo hướng đi.
void drawCreature3D(Simulation& sim, Entity o, float x, float y, float z, float yaw, float time) {
    const Body& b = sim.reg.get<Body>(o);
    const SpeciesDef& sd = sim.species[b.species];
    const Social& so = sim.reg.get<Social>(o);
    const Transform& tr = sim.reg.get<Transform>(o);
    Color c{sd.r, sd.g, sd.b, 255};
    float s = std::clamp(sd.size, 0.3f, 2.4f) * (b.ageTicks < sd.adultYears * kTicksPerYear ? 0.7f : 1.0f);
    float walk = tr.moving ? std::sin(time * 10 + entityIndex(o)) : 0.0f;
    rlPushMatrix();
    rlTranslatef(x, y, z);
    rlRotatef(-yaw * RAD2DEG, 0, 1, 0);   // x cục bộ = hướng mặt
    switch (sd.plan) {
    case BodyPlan::Humanoid: {
        Color cloth = so.group >= 0 ? Color{sim.groups[so.group].colR, sim.groups[so.group].colG, sim.groups[so.group].colB, 255} : shadeC(c, 0.6f);
        float h = 0.9f * s;
        box(0, h * 0.22f, 0.09f * s, 0.12f * s, h * 0.44f, 0.1f * s, shadeC(cloth, 0.7f));     // chân
        box(0, h * 0.22f, -0.09f * s, 0.12f * s, h * 0.44f, 0.1f * s, shadeC(cloth, 0.7f));
        box(0, h * 0.62f, 0, 0.22f * s, h * 0.38f, 0.32f * s, cloth);                         // thân
        box(walk * 0.08f, h * 0.62f, 0.21f * s, 0.09f * s, h * 0.34f, 0.08f * s, c);          // tay vung
        box(-walk * 0.08f, h * 0.62f, -0.21f * s, 0.09f * s, h * 0.34f, 0.08f * s, c);
        box(0, h * 0.92f, 0, 0.2f * s, 0.2f * s, 0.2f * s, c);                                // đầu
        if (heldValue(sim, o, Purpose::Pierce) > 0) box(0.15f * s, h * 0.7f, 0.26f * s, 0.03f, h * 0.9f, 0.03f, Color{200, 200, 210, 255}); // giáo
        break;
    }
    case BodyPlan::Primate:
        box(0, 0.25f * s, 0, 0.35f * s, 0.3f * s, 0.25f * s, c);
        box(0.15f * s, 0.48f * s, 0, 0.18f * s, 0.18f * s, 0.18f * s, shadeC(c, 1.2f));
        box(0.05f, 0.18f * s, 0.18f * s, 0.06f, 0.36f * s, 0.06f, shadeC(c, 0.8f));
        box(0.05f, 0.18f * s, -0.18f * s, 0.06f, 0.36f * s, 0.06f, shadeC(c, 0.8f));
        box(-0.25f * s, 0.4f * s, 0, 0.3f * s, 0.04f, 0.04f, shadeC(c, 0.7f));                // đuôi
        break;
    case BodyPlan::Bird: {
        float flap = std::sin(time * 18 + entityIndex(o)) * 0.5f;
        box(0, 0, 0, 0.28f * s + 0.08f, 0.12f * s + 0.04f, 0.12f * s + 0.04f, c);
        rlPushMatrix(); rlRotatef(flap * 50, 1, 0, 0); box(0, 0, 0.22f * s + 0.08f, 0.16f * s + 0.06f, 0.02f, 0.4f * s + 0.1f, shadeC(c, 0.8f)); rlPopMatrix();
        rlPushMatrix(); rlRotatef(-flap * 50, 1, 0, 0); box(0, 0, -0.22f * s - 0.08f, 0.16f * s + 0.06f, 0.02f, 0.4f * s + 0.1f, shadeC(c, 0.8f)); rlPopMatrix();
        break;
    }
    case BodyPlan::Fish: {
        float wig = std::sin(time * 8 + entityIndex(o)) * 20;
        box(0, 0, 0, 0.6f * s, 0.22f * s, 0.14f * s, c);
        rlPushMatrix(); rlTranslatef(-0.32f * s, 0, 0); rlRotatef(wig, 0, 1, 0); box(-0.1f * s, 0, 0, 0.2f * s, 0.24f * s, 0.03f, shadeC(c, 0.8f)); rlPopMatrix();
        if (sd.size > 1.0f) box(0, 0.18f * s, 0, 0.15f * s, 0.2f * s, 0.03f, shadeC(c, 0.7f));   // vây lưng cá mập
        break;
    }
    case BodyPlan::Bat: {
        float flap = std::sin(time * 22 + entityIndex(o)) * 0.6f;
        box(0, 0, 0, 0.12f, 0.1f, 0.1f, c);
        rlPushMatrix(); rlRotatef(flap * 60, 1, 0, 0); box(0, 0, 0.15f, 0.12f, 0.02f, 0.25f, shadeC(c, 0.7f)); rlPopMatrix();
        rlPushMatrix(); rlRotatef(-flap * 60, 1, 0, 0); box(0, 0, -0.15f, 0.12f, 0.02f, 0.25f, shadeC(c, 0.7f)); rlPopMatrix();
        break;
    }
    case BodyPlan::Spider:
        box(0, 0.12f * s, 0, 0.3f * s, 0.18f * s, 0.25f * s, c);
        for (int k = -1; k <= 1; k += 2)
            for (int l = 0; l < 4; ++l) box((l - 1.5f) * 0.09f * s, 0.08f * s, k * 0.25f * s, 0.03f, 0.03f, 0.3f * s, shadeC(c, 0.6f));
        break;
    case BodyPlan::Reptile:
        box(0, 0.12f * s, 0, 1.1f * s, 0.18f * s, 0.3f * s, c);
        box(0.7f * s, 0.12f * s, 0, 0.35f * s, 0.12f * s, 0.18f * s, shadeC(c, 1.1f));
        box(-0.8f * s, 0.08f * s, 0, 0.6f * s, 0.08f * s, 0.12f * s, shadeC(c, 0.8f));
        break;
    case BodyPlan::Giant:
        box(0, 0.75f * s, 0, 1.0f * s, 0.7f * s, 0.6f * s, shadeC(c, 0.85f));
        for (int k = -1; k <= 1; k += 2)
            for (int l = -1; l <= 1; l += 2) box(l * 0.35f * s, 0.2f * s, k * 0.2f * s, 0.16f * s, 0.4f * s + walk * 0.02f, 0.16f * s, shadeC(c, 0.7f));
        box(0.6f * s, 0.85f * s, 0, 0.35f * s, 0.4f * s, 0.4f * s, c);
        box(0.85f * s, 0.55f * s, 0.12f * s, 0.35f * s, 0.04f, 0.04f, Color{245, 240, 225, 255});    // ngà
        box(0.85f * s, 0.55f * s, -0.12f * s, 0.35f * s, 0.04f, 0.04f, Color{245, 240, 225, 255});
        break;
    default: {   // thú bốn chân
        box(0, 0.38f * s, 0, 0.7f * s, 0.3f * s, 0.28f * s, shadeC(c, 0.85f));
        for (int k = -1; k <= 1; k += 2)
            for (int l = -1; l <= 1; l += 2) box(l * 0.25f * s + walk * 0.04f * k, 0.12f * s, k * 0.1f * s, 0.07f * s, 0.25f * s, 0.07f * s, shadeC(c, 0.7f));
        box(0.42f * s, 0.55f * s, 0, 0.22f * s, 0.2f * s, 0.18f * s, c);
        if (b.species == sp::Fox) box(-0.45f * s, 0.42f * s, 0, 0.3f * s, 0.08f * s, 0.08f * s, Color{240, 150, 70, 255});
        break;
    }
    }
    rlPopMatrix();
}

void drawBuilding3D(Building bd, float x, float y, float z, Color owner, float time) {
    switch (bd) {
    case Building::Hut:
        box(x, y + 0.3f, z, 0.7f, 0.6f, 0.7f, Color{150, 105, 60, 255});
        DrawCylinderEx({x, y + 0.6f, z}, {x, y + 1.05f, z}, 0.55f, 0.0f, 4, shadeC(owner, 0.8f));
        break;
    case Building::House: case Building::StoneHouse: case Building::Granary: case Building::Workshop: case Building::TownHall: {
        bool stone = bd == Building::StoneHouse;
        float hh = bd == Building::TownHall ? 1.3f : 0.9f;
        box(x, y + hh * 0.5f, z, 0.85f, hh, 0.85f, stone ? Color{165, 165, 160, 255} : Color{170, 125, 75, 255});
        DrawCylinderEx({x, y + hh, z}, {x, y + hh + 0.5f, z}, 0.66f, 0.0f, 4, owner);
        box(x + 0.43f, y + 0.25f, z, 0.02f, 0.45f, 0.22f, Color{60, 40, 25, 255});   // cửa
        break;
    }
    case Building::Wall: box(x, y + 0.6f, z, 1.0f, 1.2f, 1.0f, Color{150, 150, 145, 255}); break;
    case Building::Tower:
        box(x, y + 1.2f, z, 0.8f, 2.4f, 0.8f, Color{150, 150, 145, 255});
        box(x, y + 2.55f, z, 0.3f, 0.3f, 0.05f, owner);
        break;
    case Building::Castle:
        box(x, y + 1.0f, z, 1.0f, 2.0f, 1.0f, Color{140, 140, 138, 255});
        box(x, y + 2.2f, z, 0.25f, 0.4f, 0.04f, owner);
        break;
    case Building::Temple:
        box(x, y + 0.5f, z, 0.9f, 1.0f, 0.9f, Color{220, 210, 180, 255});
        DrawCylinderEx({x, y + 1.0f, z}, {x, y + 1.7f, z}, 0.6f, 0.0f, 4, Color{230, 190, 60, 255});
        break;
    case Building::Forge:
        box(x, y + 0.45f, z, 0.85f, 0.9f, 0.85f, Color{95, 90, 88, 255});
        box(x, y + 0.92f, z, 0.4f, 0.06f, 0.4f, Color{255, static_cast<unsigned char>(130 + 60 * std::sin(time * 9)), 30, 255});
        break;
    case Building::Hearth:
        box(x, y + 0.06f, z, 0.6f, 0.12f, 0.6f, Color{110, 110, 105, 255});
        box(x, y + 0.2f + 0.04f * std::sin(time * 11), z, 0.25f, 0.25f, 0.25f, Color{255, 150, 40, 230});
        break;
    case Building::Bridge: box(x, y + 0.06f, z, 1.0f, 0.12f, 1.0f, Color{120, 80, 45, 255}); break;
    case Building::Mine:
        box(x - 0.3f, y + 0.4f, z, 0.08f, 0.8f, 0.08f, Color{120, 80, 45, 255});
        box(x + 0.3f, y + 0.4f, z, 0.08f, 0.8f, 0.08f, Color{120, 80, 45, 255});
        box(x, y + 0.82f, z, 0.7f, 0.08f, 0.1f, Color{120, 80, 45, 255});
        box(x, y + 0.3f, z, 0.5f, 0.6f, 0.05f, Color{15, 12, 12, 255});
        break;
    case Building::Den: case Building::Nest: box(x, y + 0.15f, z, 0.7f, 0.3f, 0.7f, Color{100, 75, 50, 255}); break;
    case Building::Totem: box(x, y + 0.6f, z, 0.18f, 1.2f, 0.18f, Color{120, 80, 45, 255}); box(x, y + 1.1f, z, 0.35f, 0.15f, 0.06f, owner); break;
    default: break;
    }
}
} // namespace

void fpEnter(FirstPerson& fp, Simulation& sim, Entity e) {
    if (!sim.reg.valid(e)) return;
    fp.active = true;
    fp.control = false;
    fp.who = e;
    fp.cx = INT_MIN;
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
    if (fp.meshReady) { UnloadModel(fp.model); fp.meshReady = false; }
    fp.cx = INT_MIN;
}

Vec2 fpFacing(const FirstPerson& fp) { return {std::cos(fp.yaw), std::sin(fp.yaw)}; }

void fpRender(FirstPerson& fp, Simulation& sim, Renderer& r, float time) {
    if (!sim.reg.valid(fp.who)) return;
    const World& w = sim.world;
    const Transform& me = sim.reg.get<Transform>(fp.who);
    const Body& body = sim.reg.get<Body>(fp.who);
    const SpeciesDef& sd = sim.species[body.species];
    int mode = modeOf(sim, fp.who);
    int tx = static_cast<int>(std::floor(me.pos.x)), ty = static_cast<int>(std::floor(me.pos.y));
    if (tx != fp.cx || ty != fp.cy || mode != fp.mode || GetTime() - fp.builtAt > 2.0) buildMesh(fp, sim, r, tx, ty, mode);
    // Nếu không điều khiển: quay đầu theo hướng sinh vật đang đi (mượt)
    if (!fp.control && me.moving) {
        Vec2 d = wdiff(me.target, me.pos);
        if (d.len2() > 1e-3f) {
            float want = std::atan2(d.y, d.x), diff = std::remainder(want - fp.yaw, 6.2831853f);
            fp.yaw += diff * std::min(1.0f, GetFrameTime() * 3.0f);
        }
    }
    // ---- Camera ở vị trí mắt ----
    Vec2 lp = wdiff(me.pos, {static_cast<float>(fp.cx), static_cast<float>(fp.cy)});
    float ground = groundAt(w, me.pos.x, me.pos.y, mode);
    float eye;
    switch (sd.plan) {
    case BodyPlan::Bird: eye = std::max(ground, 0.0f) + 3.5f; break;
    case BodyPlan::Fish: eye = -0.55f; break;
    case BodyPlan::Bat: eye = 0.9f; break;
    case BodyPlan::Spider: case BodyPlan::Reptile: eye = ground + 0.25f * sd.size + 0.1f; break;
    case BodyPlan::Humanoid: eye = ground + 0.8f * std::max(0.6f, sd.size) + 0.05f; break;
    case BodyPlan::Giant: eye = ground + 1.6f; break;
    default: eye = ground + 0.45f * sd.size + 0.2f; break;
    }
    if (me.moving) { fp.bobT += GetFrameTime() * 9; if (sd.plan != BodyPlan::Bird && sd.plan != BodyPlan::Fish) eye += std::sin(fp.bobT) * 0.03f; }
    Camera3D cam{};
    cam.position = {lp.x, eye, lp.y};
    float cp = std::cos(fp.pitch);
    cam.target = {lp.x + std::cos(fp.yaw) * cp, eye + std::sin(fp.pitch), lp.y + std::sin(fp.yaw) * cp};
    cam.up = {0, 1, 0};
    cam.fovy = 72;
    cam.projection = CAMERA_PERSPECTIVE;

    // ---- Bầu trời ----
    float season = w.weather().seasonTemp;
    Color sky = mode == 2 ? Color{8, 7, 12, 255} : mode == 1 ? Color{20, 70, 120, 255}
              : Color{static_cast<unsigned char>(120 + season * 15), static_cast<unsigned char>(175 + season * 15), 235, 255};
    bool raining = w.raining(tx, ty);
    if (raining && mode == 0) sky = Color{110, 120, 140, 255};
    ClearBackground(sky);
    if (mode == 0) {
        // dải chân trời sáng hơn cho cảm giác xa
        DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight() / 2, shadeC(sky, 0.85f), shadeC(sky, 1.12f));
    }

    BeginMode3D(cam);
    DrawModel(fp.model, {0, 0, 0}, 1.0f, WHITE);
    if (mode == 0) {
        // Mặt nước phản chiếu trời: tấm phẳng bán trong suốt phủ biển/hồ
        for (int b = -kObjR; b <= kObjR; ++b)
            for (int a = -kObjR; a <= kObjR; ++a) {
                int x = fp.cx + a, y = fp.cy + b;
                if (!w.inBounds(x, y) || !w.isWater(x, y)) continue;
                float wave = 0.02f * std::sin(time * 2 + a * 0.7f + b * 0.9f);
                DrawPlane({a + 0.5f, -0.02f + wave, b + 0.5f}, {1, 1}, Color{90, 150, 220, 70});
            }
    }
    // ---- Cây, đá, công trình, lửa trong bán kính ----
    for (int b = -kObjR; b <= kObjR; ++b)
        for (int a = -kObjR; a <= kObjR; ++a) {
            if (a * a + b * b > kObjR * kObjR) continue;
            int x = fp.cx + a, y = fp.cy + b;
            if (!w.inBounds(x, y)) continue;
            float px = a + 0.5f, pz = b + 0.5f;
            uint32_t h = hash32(static_cast<uint32_t>(w.wrapX(x) * 7349 + y * 1931));
            if (mode == 2) {
                const UnderTile& u = w.under(x, y);
                if (u.kind == Under::Cave && u.fungus > 50 && h % 3 == 0)
                    box(px - 0.2f + (h % 7) * 0.06f, 0.08f, pz - 0.2f + (h % 5) * 0.08f, 0.08f, 0.16f, 0.08f, Color{70, 230, 190, 255});   // nấm
                if (u.kind == Under::Cave && h % 11 == 0) DrawCylinderEx({px, 0, pz}, {px, 0.5f, pz}, 0.12f, 0.0f, 5, Color{95, 88, 82, 255}); // măng đá
                if (u.kind == Under::Magma) box(px, 0.02f + 0.03f * std::sin(time * 3 + a), pz, 1.0f, 0.06f, 1.0f, Color{255, 120, 30, 255});
                if (u.kind == Under::Tunnel && (x + y) % 3 == 0) {   // cột chống & đèn trong hầm mỏ
                    box(px - 0.4f, 0.5f, pz, 0.08f, 1.0f, 0.08f, Color{125, 88, 50, 255});
                    box(px - 0.32f, 0.85f, pz, 0.08f, 0.08f, 0.08f, Color{255, 210, 100, 255});
                }
                continue;
            }
            const Tile& t = w.at(x, y);
            float g = groundAt(w, x + 0.5f, y + 0.5f, mode);
            if (t.wood > 40 && (t.terrain == Terrain::Forest || t.terrain == Terrain::Jungle || t.terrain == Terrain::Snow || t.terrain == Terrain::Tundra)) {
                float th = 0.6f + (h % 5) * 0.12f;
                float cw = 0.6f + (h % 3) * 0.12f;
                Color leaf = t.terrain == Terrain::Jungle ? Color{35, 115, 45, 255} : t.terrain == Terrain::Snow ? Color{60, 100, 80, 255} : Color{45, 125, 55, 255};
                leaf = shadeC(leaf, 0.85f + (h % 4) * 0.08f);
                float ox = ((h >> 4) % 5 - 2) * 0.07f, oz = ((h >> 8) % 5 - 2) * 0.07f;
                box(px + ox, g + th * 0.5f, pz + oz, 0.12f, th, 0.12f, Color{100, 70, 40, 255});
                if (t.terrain == Terrain::Snow || t.terrain == Terrain::Tundra) {
                    DrawCylinderEx({px + ox, g + th * 0.6f, pz + oz}, {px + ox, g + th + 1.0f, pz + oz}, cw * 0.55f, 0.0f, 6, leaf);   // cây lá kim
                } else {
                    box(px + ox, g + th + cw * 0.4f, pz + oz, cw, cw * 0.8f, cw, leaf);
                }
            }
            if (t.stone > 40 && t.terrain != Terrain::Mountain && t.terrain != Terrain::Hill && h % 3 == 0)
                box(px, g + 0.08f, pz, 0.25f, 0.16f, 0.2f, Color{140, 138, 132, 255});
            if (t.food > 140 && t.terrain != Terrain::Farmland && !w.isWater(x, y) && h % 4 == 0)
                box(px + 0.2f, g + 0.12f, pz - 0.1f, 0.22f, 0.22f, 0.22f, Color{50, 120, 50, 255}), box(px + 0.2f, g + 0.26f, pz - 0.1f, 0.06f, 0.06f, 0.06f, Color{200, 40, 60, 255});
            if (t.building != Building::None) {
                Color owner = t.owner >= 0 && t.owner < static_cast<int>(sim.groups.size())
                                  ? Color{sim.groups[t.owner].colR, sim.groups[t.owner].colG, sim.groups[t.owner].colB, 255} : Color{150, 60, 50, 255};
                drawBuilding3D(t.building, px, g, pz, owner, time);
            }
            if (t.fire) {
                float f = std::sin(time * 13 + a * 3 + b * 5) * 0.5f + 0.5f;
                box(px, g + 0.35f + f * 0.15f, pz, 0.6f, 0.7f + f * 0.3f, 0.6f, Color{255, static_cast<unsigned char>(110 + f * 100), 30, 200});
            }
        }
    // ---- Sinh vật khác (cùng tầng sống) ----
    int myLayer = sim.layerOf(body.species);
    sim.grid.query(me.pos, static_cast<float>(kObjR), [&](Entity o, Vec2 op) {
        if (o == fp.who) return;
        const Body* ob = sim.reg.pool<Body>().tryGet(o);
        if (!ob || ob->dead || sim.layerOf(ob->species) != myLayer) return;
        const Transform& otr = sim.reg.get<Transform>(o);
        const SpeciesDef& od = sim.species[ob->species];
        Vec2 lo = wdiff(op, {static_cast<float>(fp.cx), static_cast<float>(fp.cy)});
        float gy = groundAt(w, op.x, op.y, mode);
        if (od.plan == BodyPlan::Bird) gy = std::max(gy, 0.0f) + 3.0f + 0.3f * std::sin(time + entityIndex(o));
        else if (od.plan == BodyPlan::Fish) gy = -0.5f;
        else if (od.plan == BodyPlan::Bat) gy = 0.7f + 0.15f * std::sin(time * 3 + entityIndex(o));
        else if (od.habitat == Habitat::Amphibious && w.isWater(static_cast<int>(op.x), static_cast<int>(op.y))) gy = -0.12f;
        Vec2 d = wdiff(otr.target, otr.pos);
        float oyaw = d.len2() > 1e-4f ? std::atan2(d.y, d.x) : static_cast<float>(entityIndex(o) % 6);
        drawCreature3D(sim, o, lo.x, gy, lo.y, oyaw, time);
    });
    // ---- Mũi tên, đá bay ----
    for (const PhysBody& pb : sim.physics.bodies) {
        Vec2 lo = wdiff(pb.pos, {static_cast<float>(fp.cx), static_cast<float>(fp.cy)});
        if (std::fabs(lo.x) > kObjR || std::fabs(lo.y) > kObjR) continue;
        const MaterialDef& md = w.materials[pb.mat];
        DrawSphereEx({lo.x, groundAt(w, pb.pos.x, pb.pos.y, mode) + pb.z * 0.5f + 0.1f, lo.y}, std::max(0.06f, pb.radius * 0.6f), 4, 6, Color{md.r, md.g, md.b, 255});
    }
    // ---- Mưa ----
    if (raining && mode == 0)
        for (int i = 0; i < 160; ++i) {
            uint32_t h = hash32(i * 977);
            float rx = lp.x + (h % 2000) / 100.0f - 10, rz = lp.y + ((h >> 11) % 2000) / 100.0f - 10;
            float ry = eye + 5 - std::fmod(time * 9 + (h >> 22) % 10, 8.0f);
            DrawLine3D({rx, ry, rz}, {rx, ry - 0.35f, rz}, Color{200, 210, 240, 150});
        }
    EndMode3D();
    if (mode == 1) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{20, 80, 140, 70});   // dưới nước
}

} // namespace wb
