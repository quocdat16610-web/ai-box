// =============================================================================
// Renderer.cpp — Pixel art thủ tục (không cần file ảnh → APK nhỏ gọn).
// =============================================================================
#include "Renderer.h"
#include "Text.h"
#include "../sim/Simulation.h"
#include <cmath>

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
        if (x > 0) paintTile(sim, x - 1, y);
        if (y > 0) paintTile(sim, x, y - 1);
        if (x + 1 < w_) paintTile(sim, x + 1, y);
        if (y + 1 < h_) paintTile(sim, x, y + 1);
    }
    sim.world.clearDirty();
    UpdateTexture(tex_, image_.data);
}

void Renderer::drawWorld(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float time) {
    const float S = kTileWorld;
    DrawTexturePro(tex_, {0, 0, static_cast<float>(tex_.width), static_cast<float>(tex_.height)},
                   {0, 0, w_ * S, h_ * S}, {0, 0}, 0, WHITE);
    // Khung nhìn (để culling)
    Vector2 tl = GetScreenToWorld2D({0, 0}, cam);
    Vector2 br = GetScreenToWorld2D({static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, cam);
    int x0 = std::max(0, static_cast<int>(tl.x / S) - 1), y0 = std::max(0, static_cast<int>(tl.y / S) - 1);
    int x1 = std::min(w_ - 1, static_cast<int>(br.x / S) + 1), y1 = std::min(h_ - 1, static_cast<int>(br.y / S) + 1);

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
        DrawCircleV({w.rainX * S, w.rainY * S}, w.rainRadius * S, Color{40, 60, 110, 50});
        for (int i = 0; i < 120; ++i) {
            float a = hash32(i * 31) % 6283 / 1000.0f, r = (hash32(i * 17) % 1000) / 1000.0f * w.rainRadius * S;
            float yy = std::fmod(time * 90 + i * 13, 16.0f);
            DrawLineV({w.rainX * S + std::cos(a) * r, w.rainY * S + std::sin(a) * r + yy},
                      {w.rainX * S + std::cos(a) * r - 1, w.rainY * S + std::sin(a) * r + yy + 3}, Color{180, 200, 255, 160});
        }
    }
    if (w.droughtTicks > 0) DrawRectangle(0, 0, static_cast<int>(w_ * S), static_cast<int>(h_ * S), Color{255, 180, 60, 25});

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
        float px = b.pos.x * S, py = b.pos.y * S;
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

    // Sinh vật
    for (Entity e : sim.agents) {
        const Transform& tr = sim.reg.get<Transform>(e);
        float px = tr.pos.x * S, py = tr.pos.y * S;
        if (px < tl.x - 8 || py < tl.y - 8 || px > br.x + 8 || py > br.y + 8) continue;
        const Body& b = sim.reg.get<Body>(e);
        const Social& so = sim.reg.get<Social>(e);
        const SpeciesDef& sd = sim.species[b.species];
        Color c = speciesColor(sim, b.species);
        float sz = std::clamp(sd.size, 0.4f, 2.0f);
        bool child = b.ageTicks < sd.adultYears * kTicksPerYear;
        if (child) sz *= 0.7f;
        // Nhịp bước chân
        float bob = tr.moving ? std::sin(time * 14 + entityIndex(e)) * 0.5f : 0;
        if (sd.dexterous) {
            // Dạng người: thân mặc "áo" màu nhóm, đầu màu da
            Color cloth = so.group >= 0 ? groupColor(sim, so.group) : shade(c, -50);
            DrawRectangleV({px - 1.5f * sz, py - 1.0f * sz + bob}, {3 * sz, 3 * sz}, cloth);
            DrawRectangleV({px - 1.0f * sz, py - 3.0f * sz + bob}, {2 * sz, 2 * sz}, c);
            if (heldValue(sim, e, Purpose::Pierce) > 0 || heldValue(sim, e, Purpose::Ranged) > 0) DrawRectangleV({px + 1.5f * sz, py - 3 * sz + bob}, {0.7f, 4 * sz}, Color{200, 200, 210, 255});
        } else {
            // Dạng thú: thân ngang + đầu
            DrawRectangleV({px - 2.2f * sz, py - 1.0f * sz + bob}, {4.4f * sz, 2.4f * sz}, shade(c, -15));
            DrawRectangleV({px + 1.4f * sz, py - 2.0f * sz + bob}, {1.8f * sz, 1.8f * sz}, c);
            if (so.group >= 0) DrawRectangleV({px - 0.5f, py - 2.8f * sz}, {1, 1}, groupColor(sim, so.group));
        }
        if (b.sleeping) DrawRectangleV({px + 2, py - 5}, {1.2f, 1.2f}, Color{200, 200, 255, 200});
        if (so.role == Role::Leader) DrawRectangleV({px - 1, py - 5 * sz}, {2, 1}, GOLD);
    }
    // Cá thể được chọn: viền + đường đi tới mục tiêu
    if (sim.reg.valid(opt.selected)) {
        const Transform& tr = sim.reg.get<Transform>(opt.selected);
        DrawCircleLinesV({tr.pos.x * S, tr.pos.y * S}, 6, WHITE);
        if (tr.moving) DrawLineV({tr.pos.x * S, tr.pos.y * S}, {tr.target.x * S, tr.target.y * S}, Color{255, 255, 255, 120});
        const Mind& m = sim.reg.get<Mind>(opt.selected);
        float r = (5.0f + m.intellect * 6.0f + m.pers.openness * 3.0f) * S;
        DrawCircleLinesV({tr.pos.x * S, tr.pos.y * S}, r, Color{255, 255, 255, 50}); // tầm nhìn
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
            Vector2 sp = GetWorldToScreen2D({(g.homeX + 0.5f) * S, (g.homeY - 1.5f) * S}, cam);
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
            Vector2 sp = GetWorldToScreen2D({(it->x + 0.5f) * S, (it->y + 0.5f) * S}, cam);
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
        Vector2 sp = GetWorldToScreen2D({tr.pos.x * S, tr.pos.y * S - 6}, cam);
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
    if (opt.debugThoughts && cam.zoom >= 1.5f) {
        int shown = 0;
        for (Entity e : sim.agents) {
            if (shown > 60) break;
            const Transform& tr = sim.reg.get<Transform>(e);
            Vector2 sp = GetWorldToScreen2D({tr.pos.x * S, tr.pos.y * S}, cam);
            if (sp.x < 0 || sp.y < 0 || sp.x > GetScreenWidth() || sp.y > GetScreenHeight()) continue;
            if (sim.reg.get<Mind>(e).intellect < 0.12f && cam.zoom < 3) continue; // bỏ qua sinh vật bản năng khi nhìn xa
            bubble(e, false);
            shown++;
        }
    }
    if (sim.reg.valid(opt.selected)) bubble(opt.selected, true);
}

} // namespace wb
