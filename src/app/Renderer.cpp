// =============================================================================
// Renderer.cpp — Pixel art thủ tục (không cần file ảnh → APK nhỏ gọn).
// =============================================================================
#include "Renderer.h"
#include "Text.h"
#include "../sim/Simulation.h"
#include "../ai/Goap.h"
#include <cmath>
#include <algorithm>

namespace wb {

namespace {
Color rgb(uint8_t r, uint8_t g, uint8_t b) { return Color{r, g, b, 255}; }
Color shade(Color c, int d) {
    auto f = [&](int v) { return static_cast<unsigned char>(std::clamp(v + d, 0, 255)); };
    return Color{f(c.r), f(c.g), f(c.b), c.a};
}
Color mix(Color a, Color b, float t) {
    return Color{static_cast<unsigned char>(a.r + (b.r - a.r) * t), static_cast<unsigned char>(a.g + (b.g - a.g) * t),
                 static_cast<unsigned char>(a.b + (b.b - a.b) * t), 255};
}

// Sprite công trình 4×4. Ký tự: '.' giữ nền, R mái (màu nhóm), W gỗ, w gỗ tối, S đá, s đá tối,
// D cửa, F cờ (màu phụ của nhóm), Y lúa, O lửa lò, G vàng, K đen, T gỗ cột, y vàng nhạt.
const char* buildingSprite(Building b) {
    switch (b) {
    case Building::Den:        return "....""WwwW""wKKw""....";
    case Building::Nest:       return "....""TwwT""wYYw""....";
    case Building::Hut:        return ".RR.""RRRR""WDWW""WDWW";
    case Building::House:      return "RRRR""RRRR""WWDW""WWDW";
    case Building::StoneHouse: return "RRRR""SSSS""SDsS""SDsS";
    case Building::Wall:       return "SsSs""SSSS""sSsS""SSSS";
    case Building::Tower:      return ".FF.""SSSS""SKSs""SSSS";
    case Building::Bridge:     return "TTTT""wwww""TTTT""wwww";
    case Building::Granary:    return ".RR.""RYYR""WYYW""WDDW";
    case Building::Workshop:   return "RRRR""WWWW""WOWw""WDWw";
    case Building::Forge:      return "Ss.S""SOOS""SOOS""SDsS";
    case Building::Temple:     return ".GG.""SSSS""SDDS""SSSS";
    case Building::TownHall:   return "FRRF""RRRR""WWWW""WDDW";
    case Building::Castle:     return "SFFS""SSSS""SKKS""SDDS";
    case Building::ScentMark:  return "....""..y.""....""....";
    case Building::Totem:      return ".T..""FTF."".T.."".T..";
    case Building::Hearth:     return "....""sOOs""sOOs""....";
    case Building::Mine:       return "T..T""TKKT""wKKw""....";   // cửa hầm: khung gỗ + lối xuống tối
    default: return nullptr;
    }
}
} // namespace

Color speciesColor(const Simulation& sim, int species) {
    const SpeciesDef& s = sim.species[species];
    return rgb(s.r, s.g, s.b);
}

Color groupColor(const Simulation& sim, int gid) {
    if (gid < 0 || gid >= static_cast<int>(sim.groups.size())) return GRAY;
    const Group& g = sim.groups[gid];
    return rgb(g.colR, g.colG, g.colB);
}

void Renderer::init(Simulation& sim) {
    shutdown();
    w_ = sim.world.width(); h_ = sim.world.height();
    image_ = GenImageColor(w_ * kTilePx, h_ * kTilePx, BLACK);
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) paintTile(sim, x, y);
    tex_ = LoadTextureFromImage(image_);
    SetTextureFilter(tex_, TEXTURE_FILTER_POINT); // giữ nét pixel art
    sim.world.clearDirty();
    sim.world.fullRedraw = false;
    ready_ = true;
}

void Renderer::shutdown() {
    if (globeReady_) { UnloadModel(globe_); globeReady_ = false; }   // không gỡ texture (dùng chung)
    if (underReady_) { UnloadTexture(underTex_); UnloadImage(underImage_); underReady_ = false; underVersion_ = -1; }
    if (!ready_) return;
    UnloadTexture(tex_);
    UnloadImage(image_);
    ready_ = false;
}

void Renderer::paintTile(Simulation& sim, int x, int y) {
    const Tile& t = sim.world.at(x, y);
    const TerrainInfo& ti_ = terrainInfo(t.terrain);
    Color base = rgb(ti_.r, ti_.g, ti_.b);
    // Độ cao làm sáng/tối nhẹ → cảm giác địa hình nổi
    base = shade(base, static_cast<int>((t.height - 0.5f) * 40));
    Color* px = static_cast<Color*>(image_.data);
    int stride = image_.width;
    Color owner = t.owner >= 0 ? groupColor(sim, t.owner) : BLANK;
    bool border[4] = {false, false, false, false}; // trên, phải, dưới, trái
    if (t.owner >= 0) {
        const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        for (int k = 0; k < 4; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            border[k] = !sim.world.inBounds(nx, ny) || sim.world.at(nx, ny).owner != t.owner;
        }
    }
    const char* sprite = buildingSprite(t.building);
    Color roof = t.owner >= 0 ? shade(owner, -30) : rgb(150, 60, 50);
    Color flag = rgb(200, 40, 40);
    if (t.owner >= 0 && t.owner < static_cast<int>(sim.groups.size())) {
        const Group& g = sim.groups[t.owner];
        flag = rgb(g.col2R, g.col2G, g.col2B);
    }
    for (int py = 0; py < kTilePx; ++py)
        for (int pxl = 0; pxl < kTilePx; ++pxl) {
            uint32_t h = hash32(static_cast<uint32_t>((y * kTilePx + py) * 7919 + (x * kTilePx + pxl) * 104729));
            int jitter = static_cast<int>(h % 13) - 6;
            Color c = shade(base, jitter);
            switch (t.terrain) {
            case Terrain::Forest:
            case Terrain::Jungle:
                if (t.wood > 40) {
                    // tán cây + thân cây
                    bool canopy = (pxl >= 1 && pxl <= 2 && py <= 2) || (py == 1);
                    if (canopy) c = shade(base, -25 + (h % 3 == 0 ? 15 : 0));
                    if (pxl == 1 && py == 3) c = rgb(90, 60, 30);
                }
                break;
            case Terrain::Mountain:
                if (py == 0 || pxl == 0) c = shade(base, 25);
                if (t.height > 0.86f && py <= 1) c = rgb(240, 240, 245);
                break;
            case Terrain::DeepWater:
            case Terrain::ShallowWater:
                if (h % 17 == 0) c = shade(base, 30);
                break;
            case Terrain::Farmland:
                c = (py % 2 == 0) ? rgb(120, 85, 45) : rgb(150, 115, 65);
                if (t.food > 60 && (pxl + py) % 2 == 0) c = rgb(220, 200, 70);
                break;
            case Terrain::Lava:
                if (h % 5 == 0) c = rgb(255, 200, 60);
                break;
            case Terrain::Ice:
                if (h % 9 == 0) c = rgb(245, 252, 255);
                break;
            case Terrain::Glass:
                if (h % 4 == 0) c = rgb(230, 255, 255);
                break;
            case Terrain::Basalt:
                if (h % 6 == 0) c = rgb(95, 90, 100);
                break;
            case Terrain::Ash:
                if (h % 7 == 0) c = rgb(130, 125, 120);
                break;
            default:
                break;
            }
            // Cửa hang tự nhiên: hốc tối dẫn xuống lòng đất
            if ((t.flags & kTileCaveEntrance) && pxl >= 1 && pxl <= 2 && py >= 1 && py <= 2) c = (py == 1) ? rgb(45, 38, 35) : rgb(18, 14, 14);
            // Quả mọng, đá, quặng lộ thiên
            if (t.food > 140 && t.terrain != Terrain::Farmland && pxl == 2 && py == 2 && (h & 1)) c = rgb(200, 40, 60);
            if (t.ore > 40 && h % 6 == 0) {
                // màu mạch quặng theo đúng loại khoáng chất
                const MaterialDef& od = sim.world.materials[t.oreMat == kNoMat ? static_cast<MatId>(mat::CopperOre) : t.oreMat];
                c = rgb(od.r, od.g, od.b);
            }
            if (t.stone > 15 && t.terrain != Terrain::Mountain && t.terrain != Terrain::Hill && pxl == 1 && py == 2) {
                const MaterialDef& sd = sim.world.materials[t.stoneMat];
                c = rgb(sd.r, sd.g, sd.b);
            }
            // Lãnh thổ: phủ màu nhóm + viền
            if (t.owner >= 0) {
                c = mix(c, owner, 0.18f);
                if ((border[0] && py == 0) || (border[1] && pxl == kTilePx - 1) || (border[2] && py == kTilePx - 1) || (border[3] && pxl == 0))
                    c = mix(c, owner, 0.8f);
            }
            // Công trình
            if (sprite) {
                char s = sprite[py * 4 + pxl];
                switch (s) {
                case 'R': c = roof; break;
                case 'W': c = rgb(150, 105, 60); break;
                case 'w': c = rgb(110, 75, 40); break;
                case 'S': c = rgb(165, 165, 160); break;
                case 's': c = rgb(120, 120, 118); break;
                case 'D': c = rgb(60, 40, 25); break;
                case 'F': c = flag; break;
                case 'Y': c = rgb(230, 200, 80); break;
                case 'O': c = rgb(255, 120, 30); break;
                case 'G': c = rgb(240, 200, 60); break;
                case 'K': c = rgb(25, 20, 20); break;
                case 'T': c = rgb(120, 80, 45); break;
                case 'y': c = rgb(210, 200, 90); break;
                default: break;
                }
            }
            px[(y * kTilePx + py) * stride + x * kTilePx + pxl] = c;
        }
}

void Renderer::sync(Simulation& sim) {
    if (!ready_ || sim.world.width() != w_ || sim.world.height() != h_) { init(sim); return; }
    if (sim.world.fullRedraw) {
        for (int y = 0; y < h_; ++y) for (int x = 0; x < w_; ++x) paintTile(sim, x, y);
        sim.world.fullRedraw = false;
        sim.world.clearDirty();
        UpdateTexture(tex_, image_.data);
        return;
    }
    const auto& dirty = sim.world.dirtyTiles();
    if (dirty.empty()) return;
    for (int i : dirty) {
        int x = i % w_, y = i / w_;
        paintTile(sim, x, y);
        // Ô lân cận cần vẽ lại viền lãnh thổ
        paintTile(sim, (x + w_ - 1) % w_, y);   // quấn vòng kinh độ
        if (y > 0) paintTile(sim, x, y - 1);
        paintTile(sim, (x + 1) % w_, y);
        if (y + 1 < h_) paintTile(sim, x, y + 1);
    }
    sim.world.clearDirty();
    UpdateTexture(tex_, image_.data);
}

float nearestCopyX(float px, float camX, float worldW) {
    float d = px - camX;
    if (d > worldW * 0.5f) return px - worldW;
    if (d < -worldW * 0.5f) return px + worldW;
    return px;
}

Vector3 globePoint(float tileX, float tileY, int w, int h) {
    float lon = tileX / w * 6.2831853f;
    float lat = 1.5707963f - tileY / h * 3.14159265f;   // hàng 0 = cực bắc
    // z = −sin(kinh độ): nhìn từ ngoài vào, phía đông nằm bên PHẢI (không bị lật gương)
    return {std::cos(lat) * std::cos(lon), std::sin(lat), -std::cos(lat) * std::sin(lon)};
}

// ---------------------------------------------------------------------------
// Quả địa cầu 3D
// ---------------------------------------------------------------------------
namespace {
constexpr int kGlobeRings = 64, kGlobeSegs = 128;
}

void Renderer::buildGlobeMesh() {
    // Lưới cầu UV tự dựng: toạ độ texture (u, v) = (x/W, y/H) đúng phép chiếu trụ đẳng chữ nhật
    // mà bản đồ 2D đang dùng → dán thẳng texture địa hình lên, không cần ảnh riêng.
    Mesh mesh{};
    int vc = (kGlobeRings + 1) * (kGlobeSegs + 1);
    mesh.vertexCount = vc;
    mesh.triangleCount = kGlobeRings * kGlobeSegs * 2;
    mesh.vertices = static_cast<float*>(MemAlloc(vc * 3 * sizeof(float)));
    mesh.texcoords = static_cast<float*>(MemAlloc(vc * 2 * sizeof(float)));
    mesh.normals = static_cast<float*>(MemAlloc(vc * 3 * sizeof(float)));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(vc * 4));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short)));
    int k = 0;
    for (int i = 0; i <= kGlobeRings; ++i)
        for (int j = 0; j <= kGlobeSegs; ++j, ++k) {
            float u = j / static_cast<float>(kGlobeSegs), v = i / static_cast<float>(kGlobeRings);
            Vector3 p = globePoint(u * w_, v * h_, w_, h_);
            mesh.vertices[k * 3] = p.x; mesh.vertices[k * 3 + 1] = p.y; mesh.vertices[k * 3 + 2] = p.z;
            mesh.normals[k * 3] = p.x; mesh.normals[k * 3 + 1] = p.y; mesh.normals[k * 3 + 2] = p.z;
            mesh.texcoords[k * 2] = u; mesh.texcoords[k * 2 + 1] = v;
            for (int c = 0; c < 4; ++c) mesh.colors[k * 4 + c] = 255;
        }
    int t = 0;
    for (int i = 0; i < kGlobeRings; ++i)
        for (int j = 0; j < kGlobeSegs; ++j) {
            unsigned short a = static_cast<unsigned short>(i * (kGlobeSegs + 1) + j), b = a + 1;
            unsigned short c = static_cast<unsigned short>(a + kGlobeSegs + 1), d = c + 1;
            // ngược chiều kim đồng hồ khi nhìn từ ngoài (mặt trước, không bị cull)
            mesh.indices[t++] = a; mesh.indices[t++] = c; mesh.indices[t++] = b;
            mesh.indices[t++] = b; mesh.indices[t++] = c; mesh.indices[t++] = d;
        }
    UploadMesh(&mesh, true);   // dynamic: màu đỉnh (ánh sáng ngày/đêm) cập nhật mỗi khung
    globe_ = LoadModelFromMesh(mesh);
    globe_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex_;
    globeReady_ = true;
}

Camera3D globeCamera(const GlobeView& gv) {
    Vector3 dir = {std::cos(gv.lat) * std::cos(gv.lon), std::sin(gv.lat), -std::cos(gv.lat) * std::sin(gv.lon)};
    Camera3D cam{};
    cam.position = {dir.x * gv.dist, dir.y * gv.dist, dir.z * gv.dist};
    cam.target = {0, 0, 0};
    cam.up = {0, 1, 0};
    cam.fovy = 40;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

void Renderer::drawGlobe(Simulation& sim, const GlobeView& gv, const RenderOptions& opt, float time, float uiScale) {
    if (!ready_) return;
    if (!globeReady_) buildGlobeMesh();
    globe_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex_;
    int W = GetScreenWidth(), H = GetScreenHeight();
    ClearBackground(Color{4, 6, 14, 255});
    // Trời sao
    for (int i = 0; i < 260; ++i) {
        uint32_t h = hash32(i * 2654435761u);
        unsigned char a = static_cast<unsigned char>(90 + (h >> 24) % 140);
        float tw = 0.6f + 0.4f * std::sin(time * (1 + (h % 5)) + i);
        DrawPixel(static_cast<int>(h % W), static_cast<int>((h >> 11) % H), Color{220, 230, 255, static_cast<unsigned char>(a * tw)});
    }
    Vector3 dir = {std::cos(gv.lat) * std::cos(gv.lon), std::sin(gv.lat), -std::cos(gv.lat) * std::sin(gv.lon)};
    Camera3D cam = globeCamera(gv);
    // Quầng khí quyển phía sau quả cầu (bán kính trên màn hình tính từ góc nhìn)
    float ang = std::asin(std::min(0.999f, 1.0f / gv.dist));
    float rScreen = std::tan(ang) / std::tan(cam.fovy * 0.5f * DEG2RAD) * H * 0.5f;
    Vector2 c{W * 0.5f, H * 0.5f};
    DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y), rScreen * 1.12f, Color{90, 150, 255, 110}, Color{90, 150, 255, 0});
    // Ánh sáng: mặt trời chiếu từ phía trên-trái-trước so với người xem, + mùa làm nghiêng trục
    Vector3 right = {-dir.z, 0, dir.x};
    float rl = std::sqrt(right.x * right.x + right.z * right.z);
    if (rl > 1e-4f) { right.x /= rl; right.z /= rl; }
    float season = sim.world.weather().seasonTemp;
    Vector3 sun = {dir.x * 0.7f - right.x * 0.6f, dir.y * 0.7f + 0.35f + season * 0.25f, dir.z * 0.7f - right.z * 0.6f};
    float sl = std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    sun = {sun.x / sl, sun.y / sl, sun.z / sl};
    Mesh& mesh = globe_.meshes[0];
    for (int k = 0; k < mesh.vertexCount; ++k) {
        float d = mesh.normals[k * 3] * sun.x + mesh.normals[k * 3 + 1] * sun.y + mesh.normals[k * 3 + 2] * sun.z;
        float l = std::clamp(0.28f + std::max(0.0f, d) * 0.95f, 0.0f, 1.15f);   // mặt tối vẫn hơi sáng (ánh trăng)
        unsigned char v = static_cast<unsigned char>(std::min(255.0f, l * 230));
        mesh.colors[k * 4] = v; mesh.colors[k * 4 + 1] = v;
        mesh.colors[k * 4 + 2] = static_cast<unsigned char>(std::min(255.0f, v * 1.0f + (d < 0 ? 25 : 0)));
    }
    UpdateMeshBuffer(mesh, 3, mesh.colors, mesh.vertexCount * 4, 0);
    BeginMode3D(cam);
    DrawModel(globe_, {0, 0, 0}, 1.0f, WHITE);
    EndMode3D();
    // Nhãn các nền văn minh lớn nằm ở bán cầu đang nhìn thấy
    float fs = 13 * uiScale;
    for (const Group& g : sim.groups) {
        if (!g.alive || g.stage < (gv.dist > 2.2f ? Stage::Village : Stage::Tribe) || g.population() < 3) continue;
        Vector3 p = globePoint(g.homeX + 0.5f, g.homeY + 0.5f, w_, h_);
        float facing = p.x * dir.x + p.y * dir.y + p.z * dir.z;
        if (facing < 0.25f) continue;
        Vector2 sp = GetWorldToScreen({p.x * 1.01f, p.y * 1.01f, p.z * 1.01f}, cam);
        Color gc = groupColor(sim, g.id);
        DrawCircleV(sp, 3 * uiScale, gc);
        std::string label = g.displayName();
        float tw = ui::measureText(label, fs);
        unsigned char a = static_cast<unsigned char>(std::clamp((facing - 0.25f) * 600.0f, 0.0f, 230.0f));
        DrawRectangleRounded({sp.x + 5 * uiScale, sp.y - fs * 0.6f, tw + 8, fs + 4}, 0.3f, 4, Color{0, 0, 0, static_cast<unsigned char>(a * 0.7f)});
        ui::drawText(label, sp.x + 9 * uiScale, sp.y - fs * 0.5f, fs, Color{g.stage == Stage::Nation ? (unsigned char)255 : (unsigned char)235, 225, 160, a});
    }
    (void)opt;
}

// ---------------------------------------------------------------------------
// Lòng đất: hang động, đường hầm mỏ, mạch quặng, hồ ngầm, magma
// ---------------------------------------------------------------------------
void Renderer::paintUnder(Simulation& sim, int x, int y) {
    const World& w = sim.world;
    const UnderTile& u = w.under(x, y);
    const Tile& t = w.at(x, y);
    Color* px = static_cast<Color*>(underImage_.data);
    int stride = underImage_.width;
    bool highland = t.terrain == Terrain::Hill || t.terrain == Terrain::Mountain || t.terrain == Terrain::Snow;
    for (int py = 0; py < kTilePx; ++py)
        for (int pxl = 0; pxl < kTilePx; ++pxl) {
            uint32_t h = hash32(static_cast<uint32_t>((y * kTilePx + py) * 92821 + (x * kTilePx + pxl) * 68917));
            int j = static_cast<int>(h % 11) - 5;
            Color c;
            switch (u.kind) {
            case Under::Rock:
                // đá liền khối: phân tầng sáng/tối theo hàng (địa tầng), dưới đồi núi đá xám hơn
                c = highland ? rgb(78, 74, 76) : rgb(62, 52, 46);
                c = shade(c, j + ((y * kTilePx + py) / 3 % 2 ? 4 : -4));
                if (t.height < 0.42f) c = shade(c, -18);   // dưới đáy biển
                break;
            case Under::Cave:
                c = shade(rgb(26, 23, 30), j / 2);
                if (h % 23 == 0) c = rgb(92, 86, 80);                        // nhũ đá / măng đá
                if (u.fungus > 40 && h % 9 == 0) c = rgb(60, 210, 170);      // nấm phát quang
                break;
            case Under::Tunnel:
                c = shade(rgb(78, 60, 42), j);
                if ((pxl == 0 || pxl == kTilePx - 1) && (y + x) % 2 == 0) c = rgb(125, 88, 50);  // cột chống bằng gỗ
                if (pxl == 1 && py == 1 && (x * 7 + y * 3) % 5 == 0) c = rgb(255, 205, 90);     // đèn dầu
                break;
            case Under::Water:
                c = shade(rgb(28, 58, 118), j);
                if (h % 13 == 0) c = rgb(70, 110, 180);
                break;
            case Under::Magma:
                c = h % 4 == 0 ? rgb(255, 190, 60) : rgb(225, 80, 20);
                break;
            }
            // Mạch quặng: các tinh thể màu khoáng chất, càng giàu càng dày
            if (u.oreAmt > 0 && u.oreMat != 0xFF && u.kind != Under::Water && u.kind != Under::Magma &&
                static_cast<int>(h % 100) < 6 + u.oreAmt / 18) {
                const MaterialDef& od = w.materials[static_cast<MatId>(u.oreMat)];
                c = shade(rgb(od.r, od.g, od.b), 25);
            }
            // Cửa hang & cửa hầm mỏ: lối xuống từ mặt đất
            if ((t.flags & kTileCaveEntrance) && pxl >= 1 && pxl <= 2 && py >= 1 && py <= 2) c = rgb(150, 200, 255);
            if (t.building == Building::Mine && py == 0) c = rgb(150, 105, 60);
            px[(y * kTilePx + py) * stride + x * kTilePx + pxl] = c;
        }
}

void Renderer::syncUnder(Simulation& sim) {
    if (!underReady_) {
        underImage_ = GenImageColor(w_ * kTilePx, h_ * kTilePx, BLACK);
        underTex_ = LoadTextureFromImage(underImage_);
        SetTextureFilter(underTex_, TEXTURE_FILTER_POINT);
        underReady_ = true;
        underVersion_ = -1;
    }
    // Vẽ lại cả lớp khi lòng đất thay đổi (đào hầm), tối đa ~2 lần mỗi giây
    if (underVersion_ == sim.world.underVersion || GetTime() - underSyncTime_ < 0.5) return;
    for (int y = 0; y < h_; ++y) for (int x = 0; x < w_; ++x) paintUnder(sim, x, y);
    UpdateTexture(underTex_, underImage_.data);
    underVersion_ = sim.world.underVersion;
    underSyncTime_ = GetTime();
}

void Renderer::drawWorld(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float time) {
    const float S = kTileWorld;
    const float WS = w_ * S;
    // Khung nhìn (để culling)
    Vector2 tl = GetScreenToWorld2D({0, 0}, cam);
    Vector2 br = GetScreenToWorld2D({static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, cam);
    const bool underground = opt.layer == Layer::Underground;
    if (underground) syncUnder(sim);
    const Texture2D& T = underground ? underTex_ : tex_;
    // Hành tinh quấn vòng: vẽ các bản sao texture chạm khung nhìn (trái/giữa/phải)
    for (int k = static_cast<int>(std::floor(tl.x / WS)); k <= static_cast<int>(std::floor(br.x / WS)); ++k)
        DrawTexturePro(T, {0, 0, static_cast<float>(T.width), static_cast<float>(T.height)},
                       {k * WS, 0, WS, h_ * S}, {0, 0}, 0, WHITE);
    if (underground) {
        // Lòng đất: chỉ thấy sinh vật hang động + thợ mỏ đang đào trong hầm
        drawCreatures(sim, cam, opt, time, true);
        if (sim.reg.valid(opt.selected)) {
            const Transform& tr = sim.reg.get<Transform>(opt.selected);
            DrawCircleLinesV({nearestCopyX(tr.pos.x * S, cam.target.x, WS), tr.pos.y * S}, 6, WHITE);
        }
        return;
    }
    // Ô trong khung nhìn: x KHÔNG bị kẹp (world.at / matter.cell tự quấn), chỉ y bị kẹp ở hai cực
    int x0 = static_cast<int>(std::floor(tl.x / S)) - 1, y0 = std::max(0, static_cast<int>(tl.y / S) - 1);
    int x1 = static_cast<int>(std::floor(br.x / S)) + 1, y1 = std::min(h_ - 1, static_cast<int>(br.y / S) + 1);
    if (x1 - x0 > w_ + 2) { x0 = static_cast<int>(cam.target.x / S) - w_ / 2; x1 = x0 + w_; }
    auto nx = [&](float px) { return nearestCopyX(px, cam.target.x, WS); };

    // Lửa nhấp nháy
    if (sim.world.burningCount() > 0)
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                if (sim.world.at(x, y).fire) {
                    float f = std::sin(time * 12 + x * 3 + y * 7) * 0.5f + 0.5f;
                    DrawRectangleV({x * S + 1, y * S + 1}, {S - 2, S - 2}, Color{255, static_cast<unsigned char>(80 + f * 120), 20, 220});
                    DrawRectangleV({x * S + 3, y * S}, {2, 3}, Color{255, 230, 120, 200});
                }
    // Mưa
    const Weather& w = sim.world.weather();
    if (w.rainTicks > 0) {
        float rx = nx(w.rainX * S);
        DrawCircleV({rx, w.rainY * S}, w.rainRadius * S, Color{40, 60, 110, 50});
        for (int i = 0; i < 120; ++i) {
            float a = hash32(i * 31) % 6283 / 1000.0f, r = (hash32(i * 17) % 1000) / 1000.0f * w.rainRadius * S;
            float yy = std::fmod(time * 90 + i * 13, 16.0f);
            DrawLineV({rx + std::cos(a) * r, w.rainY * S + std::sin(a) * r + yy},
                      {rx + std::cos(a) * r - 1, w.rainY * S + std::sin(a) * r + yy + 3}, Color{180, 200, 255, 160});
        }
    }
    if (w.droughtTicks > 0) DrawRectangleV(tl, {br.x - tl.x, br.y - tl.y}, Color{255, 180, 60, 25});

    // ---- Lớp vật chất: chất lỏng, vật chất rời, khí (chỉ trong khung nhìn) ----
    const MatterGrid& G = sim.world.matter;
    const MaterialRegistry& R = sim.world.materials;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const MatterCell& c = G.cell(x, y);
            float T = G.temp(x, y);
            for (const MatterSlot& ms : c.s) {
                if (ms.empty()) continue;
                const MaterialDef& d = R[ms.mat];
                Phase ph = d.phaseAt(T);
                Color col{d.r, d.g, d.b, 255};
                if (T > 500) col = Color{255, static_cast<unsigned char>(std::clamp(T / 6.0f, 60.0f, 220.0f)), 40, 255}; // vật nóng đỏ rực
                if (ph == Phase::Liquid) {
                    float a = std::min(1.0f, ms.amount / 40.0f);
                    col.a = static_cast<unsigned char>(90 + 140 * a);
                    DrawRectangleV({x * S, y * S}, {S, S}, col);
                } else {
                    int dots = std::min(6, 1 + ms.amount / 6);
                    for (int k = 0; k < dots; ++k) {
                        uint32_t h = hash32(static_cast<uint32_t>(x * 977 + y * 131 + k * 7 + ms.mat));
                        DrawRectangleV({x * S + (h % 7), y * S + ((h >> 4) % 7)}, {1.5f, 1.5f}, col);
                    }
                }
            }
            if (!c.gas.empty() && (opt.layer == Layer::Gas || c.gas.amount > 30)) {
                const MaterialDef& d = R[c.gas.mat];
                unsigned char a = static_cast<unsigned char>(std::min(160, c.gas.amount * (opt.layer == Layer::Gas ? 4 : 1)));
                Color gc = c.gas.mat == mat::Water ? Color{240, 240, 250, a} : Color{d.r, d.g, d.b, a};
                DrawRectangleV({x * S, y * S}, {S, S}, gc);
            }
            if (c.heatTarget > 0) {   // lò / bếp đang cháy
                float f = std::sin(time * 9 + x) * 0.5f + 0.5f;
                DrawRectangleV({x * S + 2, y * S + 2}, {S - 4, S - 4}, Color{255, static_cast<unsigned char>(120 + f * 100), 30, 180});
            }
            // Lớp nhiệt độ: xanh (lạnh) → xanh lá (ôn hoà) → vàng → đỏ → trắng (cực nóng)
            if (opt.layer == Layer::Heat) {
                Color hc;
                if (T < 0) hc = Color{60, 120, 255, static_cast<unsigned char>(std::min(170.0f, 80 - T * 4))};
                else if (T < 30) hc = Color{60, 200, 120, 60};
                else if (T < 150) hc = Color{240, 220, 60, 110};
                else if (T < 700) hc = Color{240, 90, 30, 160};
                else hc = Color{255, 245, 220, 200};
                DrawRectangleV({x * S, y * S}, {S, S}, hc);
            }
            if (opt.layer == Layer::Gas && !c.gas.empty()) {
                float p = G.pressure(x, y);
                if (p > 1.5f) DrawRectangleLinesEx({x * S, y * S, S, S}, 1, Color{255, 60, 60, static_cast<unsigned char>(std::min(255.0f, (p - 1) * 80))});
            }
        }
    // ---- Vật thể vật lý: bóng đổ trên mặt đất + vật ở độ cao z ----
    for (const PhysBody& b : sim.physics.bodies) {
        float px = nx(b.pos.x * S), py = b.pos.y * S;
        if (px < tl.x - 8 || py < tl.y - 8 || px > br.x + 8 || py > br.y + 8) continue;
        float r = std::max(1.0f, b.radius * S);
        DrawEllipse(static_cast<int>(px), static_cast<int>(py), r, r * 0.5f, Color{0, 0, 0, 90}); // bóng
        const MaterialDef& d = R[b.mat];
        Color col = b.temp > 400 ? Color{255, 140, 40, 255} : Color{d.r, d.g, d.b, 255};
        float lift = b.z * S * 0.5f;   // độ cao hiển thị bằng cách dịch lên (giả 3D)
        if (b.kind == BodyKind::Projectile) {
            Vec2 v = b.vel.normalized();
            DrawLineEx({px - v.x * 4, py - lift - v.y * 4}, {px + v.x * 4, py - lift + v.y * 4}, 1.2f, col);
        } else {
            DrawCircleV({px, py - lift}, r, col);
            DrawCircleLinesV({px, py - lift}, r, Color{0, 0, 0, 120});
        }
    }

    drawCreatures(sim, cam, opt, time, false);
    // Cá thể được chọn: viền + đường đi tới mục tiêu
    if (sim.reg.valid(opt.selected)) {
        const Transform& tr = sim.reg.get<Transform>(opt.selected);
        Vector2 sp{nx(tr.pos.x * S), tr.pos.y * S};
        DrawCircleLinesV(sp, 6, WHITE);
        Vec2 td = wdiff(tr.target, tr.pos);
        if (tr.moving) DrawLineV(sp, {sp.x + td.x * S, sp.y + td.y * S}, Color{255, 255, 255, 120});
        const Mind& m = sim.reg.get<Mind>(opt.selected);
        float r = (5.0f + m.intellect * 6.0f + m.pers.openness * 3.0f) * S;
        DrawCircleLinesV(sp, r, Color{255, 255, 255, 50}); // tầm nhìn
    }
}

// Vẽ sinh vật theo HÌNH DÁNG CƠ THỂ (người, thú bốn chân, chim, cá, dơi, nhện, bò sát, voi ma mút, khỉ).
// underground = true: chỉ vẽ sinh vật trong hang + thợ mỏ; false: mọi sinh vật trên mặt đất / nước / trời.
void Renderer::drawCreatures(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float time, bool underground) {
    (void)opt;
    const float S = kTileWorld;
    Vector2 tl = GetScreenToWorld2D({0, 0}, cam);
    Vector2 br = GetScreenToWorld2D({static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, cam);
    for (Entity e : sim.agents) {
        const Transform& tr = sim.reg.get<Transform>(e);
        const Body& b = sim.reg.get<Body>(e);
        const SpeciesDef& sd = sim.species[b.species];
        bool caveDweller = sd.habitat == Habitat::Cave;
        bool miner = false;
        if (underground && !caveDweller) {
            // thợ mỏ: đang đào đá/quặng tại ô có hầm
            const Mind& m = sim.reg.get<Mind>(e);
            if (m.plan.done()) continue;
            ActionId a = m.plan.current();
            miner = (a == ActionId::GatherOre || a == ActionId::GatherStone) && sim.world.inBounds(m.tx, m.ty) &&
                    sim.world.under(m.tx, m.ty).kind == Under::Tunnel && dist2(tr.pos, {m.tx + 0.5f, m.ty + 0.5f}) < 4;
            if (!miner) continue;
        }
        if (!underground && caveDweller) continue;   // ở dưới lòng đất: không thấy từ trên mặt
        float px = nearestCopyX(tr.pos.x * S, cam.target.x, w_ * S), py = tr.pos.y * S;
        if (px < tl.x - 8 || py < tl.y - 8 || px > br.x + 8 || py > br.y + 8) continue;
        const Social& so = sim.reg.get<Social>(e);
        Color c = speciesColor(sim, b.species);
        float sz = std::clamp(sd.size, 0.4f, 2.4f);
        bool child = b.ageTicks < sd.adultYears * kTicksPerYear;
        if (child) sz *= 0.7f;
        // Nhịp bước chân / vỗ cánh
        float phase = time * 14 + entityIndex(e);
        float bob = tr.moving ? std::sin(phase) * 0.5f : 0;
        if (miner) DrawCircleV({px, py}, 5, Color{255, 200, 90, 60});   // đèn thợ mỏ
        if (!sd.customGenes.empty() && sd.geneBonus(GeneEffect::Glow) > 0)       // gen phát quang sinh học
            DrawCircleV({px, py - 1}, 4 + 2 * std::sin(time * 3 + entityIndex(e)), Color{c.r, c.g, c.b, 70});
        switch (sd.plan) {
        case BodyPlan::Humanoid: {
            // Dạng người: thân mặc "áo" màu nhóm, đầu màu da
            Color cloth = so.group >= 0 ? groupColor(sim, so.group) : shade(c, -50);
            DrawRectangleV({px - 1.5f * sz, py - 1.0f * sz + bob}, {3 * sz, 3 * sz}, cloth);
            DrawRectangleV({px - 1.0f * sz, py - 3.0f * sz + bob}, {2 * sz, 2 * sz}, c);
            if (heldValue(sim, e, Purpose::Pierce) > 0 || heldValue(sim, e, Purpose::Ranged) > 0) DrawRectangleV({px + 1.5f * sz, py - 3 * sz + bob}, {0.7f, 4 * sz}, Color{200, 200, 210, 255});
            if (miner) DrawRectangleV({px - 1.2f * sz, py - 3.6f * sz}, {2.4f * sz, 0.8f}, Color{240, 200, 60, 255});   // mũ thợ mỏ
            break;
        }
        case BodyPlan::Primate: {
            // Khỉ: thân khom, tay dài, đuôi cong
            DrawRectangleV({px - 1.2f * sz, py - 1.5f * sz + bob}, {2.4f * sz, 2.4f * sz}, shade(c, -10));
            DrawRectangleV({px - 0.9f * sz, py - 3.0f * sz + bob}, {1.8f * sz, 1.6f * sz}, shade(c, 20));
            DrawLineV({px - 1.2f * sz, py - 1.0f * sz + bob}, {px - 2.2f * sz, py + 0.8f * sz}, shade(c, -30));
            DrawLineV({px + 1.2f * sz, py - 1.0f * sz + bob}, {px + 2.2f * sz, py + 0.8f * sz}, shade(c, -30));
            DrawLineV({px + 1.2f * sz, py + 0.6f * sz}, {px + 2.6f * sz, py - 1.6f * sz}, shade(c, -40));
            break;
        }
        case BodyPlan::Bird: {
            // Chim bay trên cao: bóng dưới đất, thân nâng lên, đôi cánh vỗ
            float lift = 4.0f + std::sin(phase * 0.3f) * 0.8f;
            DrawEllipse(static_cast<int>(px), static_cast<int>(py), 1.6f * sz + 0.5f, 0.7f * sz + 0.3f, Color{0, 0, 0, 70});
            float flap = std::sin(phase * 1.6f) * 1.5f * sz + 0.4f;
            Color wing = shade(c, -20);
            DrawTriangle({px, py - lift}, {px - 2.8f * sz - 0.8f, py - lift - flap}, {px - 0.6f, py - lift + 0.5f}, wing);
            DrawTriangle({px, py - lift}, {px + 0.6f, py - lift + 0.5f}, {px + 2.8f * sz + 0.8f, py - lift - flap}, wing);
            DrawCircleV({px, py - lift}, 0.9f * sz + 0.4f, c);
            break;
        }
        case BodyPlan::Fish: {
            // Cá bơi dưới nước: thân bầu dục, đuôi đong đưa, hơi trong suốt
            float wig = std::sin(phase * 1.3f) * 0.8f;
            Color fc{c.r, c.g, c.b, 210};
            DrawEllipse(static_cast<int>(px), static_cast<int>(py), 2.0f * sz + 0.5f, 0.9f * sz + 0.3f, fc);
            DrawTriangle({px - 1.8f * sz, py}, {px - 3.2f * sz - 0.4f, py - 1.1f * sz + wig}, {px - 3.2f * sz - 0.4f, py + 1.1f * sz + wig}, fc);
            if (sd.size > 1.0f) DrawTriangle({px - 0.3f, py - 0.8f * sz}, {px + 0.6f * sz, py - 0.8f * sz}, {px, py - 2.0f * sz}, shade(c, -30)); // vây lưng cá mập
            break;
        }
        case BodyPlan::Bat: {
            float flap = std::sin(phase * 2.2f) * 1.6f;
            Color wing = shade(c, -15);
            DrawTriangle({px, py}, {px - 3.0f, py - 1.0f - flap}, {px - 0.5f, py + 0.8f}, wing);
            DrawTriangle({px, py}, {px + 0.5f, py + 0.8f}, {px + 3.0f, py - 1.0f - flap}, wing);
            DrawCircleV({px, py}, 1.0f, c);
            DrawPixelV({px - 0.4f, py - 0.4f}, Color{255, 80, 60, 255});
            break;
        }
        case BodyPlan::Spider: {
            for (int k = 0; k < 4; ++k) {
                float a = 0.5f + k * 0.45f + std::sin(phase + k) * 0.15f * tr.moving;
                DrawLineV({px, py}, {px - std::cos(a) * 2.6f * sz - 0.5f, py + std::sin(a - 1.2f) * 2.0f * sz}, shade(c, -20));
                DrawLineV({px, py}, {px + std::cos(a) * 2.6f * sz + 0.5f, py + std::sin(a - 1.2f) * 2.0f * sz}, shade(c, -20));
            }
            DrawCircleV({px, py}, 1.3f * sz + 0.3f, c);
            DrawPixelV({px - 0.3f, py - 0.8f}, Color{255, 60, 60, 255});
            break;
        }
        case BodyPlan::Reptile: {
            // Cá sấu: thân dài sát đất, đuôi, mõm dài
            DrawRectangleV({px - 3.0f * sz, py - 0.7f * sz}, {6.0f * sz, 1.4f * sz}, shade(c, -10));
            DrawTriangle({px - 3.0f * sz, py - 0.7f * sz}, {px - 5.0f * sz, py}, {px - 3.0f * sz, py + 0.7f * sz}, shade(c, -25));
            DrawRectangleV({px + 3.0f * sz, py - 0.4f * sz}, {1.6f * sz, 0.8f * sz}, shade(c, 10));
            DrawPixelV({px + 2.8f * sz, py - 0.8f * sz}, Color{230, 220, 60, 255});
            break;
        }
        case BodyPlan::Giant: {
            // Voi ma mút: thân to lông dày, ngà trắng, vòi
            DrawRectangleV({px - 2.6f * sz, py - 1.6f * sz + bob}, {5.2f * sz, 3.0f * sz}, shade(c, -15));
            DrawRectangleV({px + 2.0f * sz, py - 2.4f * sz + bob}, {1.8f * sz, 2.0f * sz}, c);
            DrawLineV({px + 3.6f * sz, py - 0.6f * sz + bob}, {px + 4.4f * sz, py - 1.6f * sz + bob}, Color{245, 240, 225, 255});
            DrawLineV({px + 3.0f * sz, py - 0.4f * sz + bob}, {px + 3.2f * sz, py + 1.2f * sz + bob}, shade(c, -30));
            break;
        }
        default: {
            // Dạng thú bốn chân: thân ngang + đầu
            DrawRectangleV({px - 2.2f * sz, py - 1.0f * sz + bob}, {4.4f * sz, 2.4f * sz}, shade(c, -15));
            DrawRectangleV({px + 1.4f * sz, py - 2.0f * sz + bob}, {1.8f * sz, 1.8f * sz}, c);
            if (b.species == sp::Boar) DrawPixelV({px + 3.2f * sz, py - 0.6f * sz + bob}, Color{245, 240, 225, 255}); // nanh
            if (b.species == sp::Fox) DrawLineV({px - 2.2f * sz, py}, {px - 4.0f * sz, py - 1.4f * sz}, Color{240, 150, 70, 255}); // đuôi xù
            break;
        }
        }
        if (so.group >= 0 && sd.plan != BodyPlan::Humanoid) DrawRectangleV({px - 0.5f, py - 2.8f * sz}, {1, 1}, groupColor(sim, so.group));
        if (b.sleeping) DrawRectangleV({px + 2, py - 5}, {1.2f, 1.2f}, Color{200, 200, 255, 200});
        if (so.role == Role::Leader) DrawRectangleV({px - 1, py - 5 * sz}, {2, 1}, GOLD);
    }
}

void Renderer::drawOverlay(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float uiScale) {
    const float S = kTileWorld;
    float fs = 13 * uiScale;
    // Nhãn nhóm / quốc gia
    if (opt.showLabels)
        for (const Group& g : sim.groups) {
            if (!g.alive || g.population() < 3) continue;
            if (g.stage == Stage::Pack && cam.zoom < 1.2f) continue;
            Vector2 sp = GetWorldToScreen2D({nearestCopyX((g.homeX + 0.5f) * S, cam.target.x, w_ * S), (g.homeY - 1.5f) * S}, cam);
            if (sp.x < -200 || sp.y < -50 || sp.x > GetScreenWidth() + 200 || sp.y > GetScreenHeight() + 50) continue;
            std::string label = g.displayName() + " · " + stageName(g.stage, g.animal) + " (" + std::to_string(g.population()) + ")";
            float tw = ui::measureText(label, fs);
            float bx = sp.x - tw / 2 - 4 - fs, by = sp.y - fs;
            Color gc = groupColor(sim, g.id);
            DrawRectangleRounded({bx, by, tw + 8 + fs * 1.2f, fs + 6}, 0.3f, 4, Color{0, 0, 0, static_cast<unsigned char>(g.id == opt.selectedGroup ? 220 : 150)});
            // Quốc kỳ nhỏ: 2 màu + biểu tượng
            DrawRectangle(static_cast<int>(bx + 3), static_cast<int>(by + 3), static_cast<int>(fs * 0.9f), static_cast<int>(fs * 0.6f), gc);
            if (g.stage >= Stage::Tribe)
                DrawRectangle(static_cast<int>(bx + 3), static_cast<int>(by + 3 + fs * 0.3f), static_cast<int>(fs * 0.9f), static_cast<int>(fs * 0.3f),
                              Color{g.col2R, g.col2G, g.col2B, 255});
            if (g.atWar()) DrawRectangle(static_cast<int>(bx + 3), static_cast<int>(by + 3 + fs * 0.7f), static_cast<int>(fs * 0.9f), 2, RED);
            ui::drawText(label, bx + fs * 1.2f + 4, by + 3, fs, g.stage == Stage::Nation ? GOLD : RAYWHITE);
        }
    // Nhãn phản ứng hoá học đang diễn ra (lớp "Phản ứng" hoặc khi phóng to)
    if (opt.layer == Layer::Reactions || cam.zoom >= 3.0f) {
        int shown = 0;
        const auto& rec = sim.world.matter.recent();
        for (auto it = rec.rbegin(); it != rec.rend() && shown < 14; ++it) {
            long age = sim.tick - it->tick;
            if (age > 200) break;
            if (it->kind == ReactionKind::Decay) continue;   // phân huỷ xác: quá nhiều, bỏ qua nhãn
            Vector2 sp = GetWorldToScreen2D({nearestCopyX((it->x + 0.5f) * S, cam.target.x, w_ * S), (it->y + 0.5f) * S}, cam);
            if (sp.x < 0 || sp.y < 0 || sp.x > GetScreenWidth() || sp.y > GetScreenHeight()) continue;
            std::string t = std::string(reactionName(it->kind)) + ": " + it->describe(sim.world.materials);
            float f = fs * 0.75f;
            unsigned char a = static_cast<unsigned char>(255 - age);
            float tw = ui::measureText(t, f);
            DrawRectangleRounded({sp.x - 3, sp.y - f - 18 - shown % 3 * 4, tw + 6, f + 4}, 0.3f, 4, Color{20, 10, 30, static_cast<unsigned char>(a * 0.7f)});
            ui::drawText(t, sp.x, sp.y - f - 16 - shown % 3 * 4, f, Color{255, 210, 120, a});
            DrawCircleV(sp, 3, Color{255, 200, 80, a});
            shown++;
        }
    }
    // Bong bóng suy nghĩ
    auto bubble = [&](Entity e, bool full) {
        const Transform& tr = sim.reg.get<Transform>(e);
        const Mind& m = sim.reg.get<Mind>(e);
        Vector2 sp = GetWorldToScreen2D({nearestCopyX(tr.pos.x * S, cam.target.x, w_ * S), tr.pos.y * S - 6}, cam);
        if (sp.x < -100 || sp.y < -30 || sp.x > GetScreenWidth() + 100 || sp.y > GetScreenHeight() + 30) return;
        std::string t = m.thought;
        float f = full ? fs : fs * 0.8f;
        float tw = ui::measureText(t, f);
        DrawRectangleRounded({sp.x - tw / 2 - 3, sp.y - f - 6, tw + 6, f + 4}, 0.4f, 4, Color{255, 255, 255, 200});
        ui::drawText(t, sp.x - tw / 2, sp.y - f - 4, f, Color{30, 30, 40, 255});
        // thanh trí tuệ
        DrawRectangle(static_cast<int>(sp.x - 10), static_cast<int>(sp.y - 2), 20, 2, Color{0, 0, 0, 150});
        DrawRectangle(static_cast<int>(sp.x - 10), static_cast<int>(sp.y - 2), static_cast<int>(20 * m.intellect), 2, SKYBLUE);
    };
    if (opt.debugThoughts && cam.zoom >= 1.5f && opt.layer != Layer::Underground) {
        int shown = 0;
        for (Entity e : sim.agents) {
            if (shown > 60) break;
            const Transform& tr = sim.reg.get<Transform>(e);
            Vector2 sp = GetWorldToScreen2D({nearestCopyX(tr.pos.x * S, cam.target.x, w_ * S), tr.pos.y * S}, cam);
            if (sp.x < 0 || sp.y < 0 || sp.x > GetScreenWidth() || sp.y > GetScreenHeight()) continue;
            if (sim.reg.get<Mind>(e).intellect < 0.12f && cam.zoom < 3) continue; // bỏ qua sinh vật bản năng khi nhìn xa
            bubble(e, false);
            shown++;
        }
    }
    if (sim.reg.valid(opt.selected)) bubble(opt.selected, true);
}

} // namespace wb
