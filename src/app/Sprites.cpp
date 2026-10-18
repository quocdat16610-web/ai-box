// =============================================================================
// Sprites.cpp — Mô hình pixel art của chủng tộc, động vật và dụng cụ.
// =============================================================================
#include "Sprites.h"
#include "Renderer.h"
#include "../sim/Simulation.h"
#include <cmath>
#include <cstring>

namespace wb {

namespace {

Color rgb(int r, int g, int b) {
    auto c = [](int v) { return static_cast<unsigned char>(std::clamp(v, 0, 255)); };
    return Color{c(r), c(g), c(b), 255};
}
Color shade(Color c, int d) { return rgb(c.r + d, c.g + d, c.b + d); }

// ---- Sprite: các hàng ký tự cùng độ rộng, hướng mặt sang PHẢI ----
// Bảng màu: '.' trong suốt · S da · s da tối · H tóc · C áo (màu nhóm) · c áo tối · E mắt · N mũi
// B râu · T nanh/răng · L giày/da · M mũ kim loại · F lông · f lông tối · W lông bụng · A gạc · K đen
struct Sprite { const char* rows[11]; int h; };

const Sprite kHuman  = {{"..HHH..", ".HHSSS.", ".HSSES.", "..SSS..", ".CCCCC.", "SCCCCCS", ".CCCCC.", ".cc.cc.", ".LL.LL."}, 9};
const Sprite kElf    = {{"..HHH..", ".HHSSSS", "SHSSES.", "..SSS..", "..CCC..", ".SCCCS.", "..CCC..", "..CcC..", "..c.c..", "..L.L.."}, 10};
const Sprite kOrc    = {{"..HH...", ".SSSSS.", ".SSSES.", ".STSTS.", "CCCCCCC", "SCCCCCS", ".CCCCC.", ".cc.cc.", ".LL.LL."}, 9};
const Sprite kDwarf  = {{"..MMM..", ".MMMMMM", ".SSSES.", ".BBBBB.", "SBBBBBS", ".CBBBC.", ".cc.cc.", ".LL.LL."}, 8};
const Sprite kGoblin = {{"S.SSS.S", ".SSSES.", "..STS..", "..CCC..", ".SCCCS.", "..c.c..", "..L.L.."}, 7};
const Sprite kTroll  = {{"..SSSS..", ".SSSSSE.", ".SSTSTS.", "SSCCCCSS", "SSCCCCSS", "S.CCCC.S", "..cccc..", "..SS.SS.", "..ss.ss."}, 9};
const Sprite kBeast  = {{"..F.F..", ".FFFFF.", ".FFFEN.", "..FFF..", ".CCCCC.", "FCCCCCF", ".CCCCC.", ".ff.ff.", ".ff.ff."}, 9};
const Sprite kWolf   = {{".......FF.", "......FFFF", "f.FFFFFFEN", ".fFFFFFFF.", "..WWWWW...", "..f.f.f.f."}, 6};
const Sprite kDeer   = {{"......A.A.", ".......AA.", ".......FF.", "......FFEN", ".FFFFFFFF.", "WFFFFFFF..", ".FWWWWF...", ".f.f.f.f.."}, 8};
const Sprite kRabbit = {{"...FF.", "...FF.", ".FFFEF", "WFFFFN", ".f..f."}, 5};
const Sprite kBear   = {{".......ff.", "......FFFF", ".FFFFFFFEN", "FFFFFFFFFF", "FFFFFFFF..", ".FF..FF...", ".ff..ff..."}, 7};

struct Palette {
    Color skin, skinDark, hair, cloth, clothDark, eye{25, 20, 25, 255}, beard, tusk{240, 235, 210, 255}, leather{80, 55, 35, 255},
        metal{170, 175, 185, 255}, fur, furDark, belly, antler{150, 115, 70, 255};
};

Color pick(const Palette& p, char ch, bool& ok) {
    ok = true;
    switch (ch) {
    case 'S': return p.skin;   case 's': return p.skinDark; case 'H': return p.hair;
    case 'C': return p.cloth;  case 'c': return p.clothDark; case 'E': return p.eye;
    case 'N': return Color{30, 25, 25, 255};
    case 'B': return p.beard;  case 'T': return p.tusk;  case 'L': return p.leather; case 'M': return p.metal;
    case 'F': return p.fur;    case 'f': return p.furDark; case 'W': return p.belly; case 'A': return p.antler;
    case 'K': return Color{20, 20, 20, 255};
    default: ok = false; return BLANK;
    }
}

// Vẽ sprite: (ax, ay) = điểm giữa-chân; u = cạnh một pixel (đơn vị thế giới).
void blit(const Sprite& s, const Palette& p, float ax, float ay, float u, bool flip) {
    int w = static_cast<int>(std::strlen(s.rows[0]));
    float x0 = ax - w * u * 0.5f, y0 = ay - s.h * u;
    for (int r = 0; r < s.h; ++r)
        for (int col = 0; col < w; ++col) {
            bool ok;
            Color c = pick(p, s.rows[r][col], ok);
            if (!ok) continue;
            int cx = flip ? w - 1 - col : col;
            DrawRectangleV({x0 + cx * u, y0 + r * u}, {u + 0.02f, u + 0.02f}, c);
        }
}

// ---- Dụng cụ: lưới nhỏ, 'm' = vật liệu chính, 'h' = cán, 's' = dây, 'o' = lửa ----
struct ToolSprite { const char* rows[9]; int h; };
const ToolSprite kAxe     = {{".mm", "mmh", ".mh", "..h", "..h", "..h"}, 6};
const ToolSprite kPick    = {{"mmmm", "m.h.", "..h.", "..h.", "..h."}, 5};
const ToolSprite kSpear   = {{"m", "m", "h", "h", "h", "h", "h", "h"}, 8};
const ToolSprite kClub    = {{"mmm", "mmm", ".h.", ".h.", ".h."}, 5};
const ToolSprite kBow     = {{".hs", "h.s", "h.s", "h.s", "h.s", ".hs"}, 6};
const ToolSprite kTorch   = {{"o", "o", "m", "h", "h"}, 5};

Color matColor(const Simulation& sim, MatId m) {
    if (m == kNoMat) return rgb(150, 105, 60);
    const MaterialDef& d = sim.world.materials[m];
    return rgb(d.r, d.g, d.b);
}

void blitTool(const ToolSprite& t, Color main, Color handle, float ax, float ay, float u, bool flip, float time) {
    int w = static_cast<int>(std::strlen(t.rows[0]));
    float x0 = flip ? ax - w * u : ax, y0 = ay - t.h * u;
    for (int r = 0; r < t.h; ++r)
        for (int col = 0; col < w; ++col) {
            char ch = t.rows[r][col];
            Color c;
            if (ch == 'm') c = main;
            else if (ch == 'h') c = handle;
            else if (ch == 's') c = Color{230, 225, 200, 255};
            else if (ch == 'o') c = Color{255, static_cast<unsigned char>(140 + 80 * std::sin(time * 15 + r)), 30, 255};
            else continue;
            int cx = flip ? w - 1 - col : col;
            DrawRectangleV({x0 + cx * u, y0 + r * u}, {u + 0.02f, u + 0.02f}, c);
        }
}

// Dụng cụ nào đang "cầm trên tay": theo việc đang làm, nếu không thì món tiêu biểu nhất.
int toolSlotFor(Simulation& sim, Entity e, const Mind& m, const Inventory& inv) {
    auto has = [&](Purpose p) { return inv.item[static_cast<int>(p)] >= 0; };
    auto slot = [](Purpose p) { return static_cast<int>(p); };
    ActionId a = m.plan.done() ? ActionId::Count : m.plan.current();
    switch (a) {
    case ActionId::GatherWood: if (has(Purpose::Cut)) return slot(Purpose::Cut); break;
    case ActionId::GatherStone: case ActionId::GatherOre: case ActionId::Farm: if (has(Purpose::Dig)) return slot(Purpose::Dig); break;
    case ActionId::Hunt: case ActionId::Fight: case ActionId::SeekEnemy:
        if (has(Purpose::Ranged)) return slot(Purpose::Ranged);
        if (has(Purpose::Pierce)) return slot(Purpose::Pierce);
        if (has(Purpose::Smash)) return slot(Purpose::Smash);
        break;
    case ActionId::Build: case ActionId::GatherMaterials: if (has(Purpose::Smash)) return slot(Purpose::Smash); if (has(Purpose::Cut)) return slot(Purpose::Cut); break;
    case ActionId::Experiment: case ActionId::Produce: if (has(Purpose::FireStart)) return slot(Purpose::FireStart); break;
    default: break;
    }
    (void)sim; (void)e;
    for (Purpose p : {Purpose::Pierce, Purpose::Ranged, Purpose::Smash, Purpose::Cut, Purpose::Dig, Purpose::FireStart})
        if (has(p)) return slot(p);
    return -1;
}

Palette paletteFor(Simulation& sim, int species, int root, Entity e, int group) {
    const SpeciesDef& sd = sim.species[species];
    Palette p;
    Color base = rgb(sd.r, sd.g, sd.b);
    uint32_t h = hash32(entityIndex(e) * 2654435761u);
    p.skin = base; p.skinDark = shade(base, -35);
    p.fur = base; p.furDark = shade(base, -40); p.belly = shade(base, 45);
    static const Color hairs[] = {{60, 40, 25, 255}, {30, 25, 20, 255}, {120, 80, 40, 255}, {170, 120, 60, 255}, {200, 180, 120, 255}};
    p.hair = hairs[h % 5];
    switch (root) {
    case sp::Human: p.skin = shade(base, static_cast<int>(h % 50) - 30); p.skinDark = shade(p.skin, -30); break;
    case sp::Elf: p.hair = (h & 1) ? rgb(235, 220, 150) : rgb(220, 230, 240); break;
    case sp::Orc: p.hair = rgb(25, 25, 25); break;
    case sp::Dwarf: p.beard = (h & 1) ? rgb(170, 80, 40) : rgb(120, 110, 100); p.skin = rgb(225, 175, 140); break;
    case sp::Troll: p.hair = shade(base, -20); break;
    default: break;
    }
    if (root == sp::Dwarf) p.beard = (h & 1) ? rgb(170, 80, 40) : rgb(120, 110, 100);
    Color cloth = group >= 0 ? groupColor(sim, group) : rgb(140, 110, 80);   // chưa có nhóm: áo da thú
    p.cloth = cloth; p.clothDark = shade(cloth, -45);
    return p;
}

} // namespace

void drawCreatureSprite(const Simulation& csim, Entity e, float px, float py, float time, bool detailed) {
    Simulation& sim = const_cast<Simulation&>(csim);   // registry chỉ cung cấp get() không-const
    const Transform& tr = sim.reg.get<Transform>(e);
    const Body& b = sim.reg.get<Body>(e);
    const Social& so = sim.reg.get<Social>(e);
    const SpeciesDef& sd = sim.species[b.species];
    int root = sim.species.root(b.species);
    float sz = std::clamp(sd.size, 0.4f, 2.0f);
    bool child = b.ageTicks < sd.adultYears * kTicksPerYear;
    if (child) sz *= 0.65f;
    float bob = tr.moving ? std::sin(time * 14 + entityIndex(e)) * 0.35f : 0;
    bool flip = tr.moving ? tr.target.x < tr.pos.x : (entityIndex(e) & 1);
    if (b.sleeping) bob = 0;

    if (!detailed) {
        // Nhìn xa: khối 2 màu (rẻ)
        Color c = rgb(sd.r, sd.g, sd.b);
        Color cloth = so.group >= 0 ? groupColor(sim, so.group) : shade(c, -50);
        if (sd.dexterous) {
            DrawRectangleV({px - 1.5f * sz, py - 3.0f * sz}, {3 * sz, 3 * sz}, cloth);
            DrawRectangleV({px - 1.0f * sz, py - 5.0f * sz}, {2 * sz, 2 * sz}, c);
        } else {
            DrawRectangleV({px - 2.2f * sz, py - 2.4f * sz}, {4.4f * sz, 2.4f * sz}, shade(c, -15));
        }
        return;
    }

    const Sprite* spr = &kHuman;
    bool humanoid = true;
    switch (root) {
    case sp::Human: spr = &kHuman; break;
    case sp::Elf: spr = &kElf; break;
    case sp::Orc: spr = &kOrc; break;
    case sp::Dwarf: spr = &kDwarf; break;
    case sp::Goblin: spr = &kGoblin; break;
    case sp::Troll: spr = &kTroll; break;
    case sp::Wolf: spr = &kWolf; humanoid = false; break;
    case sp::Deer: spr = &kDeer; humanoid = false; break;
    case sp::Rabbit: spr = &kRabbit; humanoid = false; break;
    case sp::Bear: spr = &kBear; humanoid = false; break;
    default: spr = sd.dexterous ? &kHuman : &kWolf; humanoid = sd.dexterous; break;
    }
    // Thú tiến hoá ra đôi tay khéo → đứng thẳng: dáng "thú nhân" mang lông của loài gốc
    if (!humanoid && sd.dexterous) { spr = &kBeast; humanoid = true; }
    Palette pal = paletteFor(sim, b.species, root, e, so.group);
    float u = (humanoid ? 0.55f : 0.5f) * sz;
    // Bóng dưới chân
    DrawEllipse(static_cast<int>(px), static_cast<int>(py), 2.2f * sz, 0.8f * sz, Color{0, 0, 0, 70});
    if (b.sleeping && !humanoid) {
        blit(*spr, pal, px, py + bob + u * 1.0f, u, flip);   // nằm thấp xuống
    } else {
        blit(*spr, pal, px, py + bob, u, flip);
    }

    // ---- Đồ đạc: áo choàng giữ ấm, giỏ đeo lưng, dụng cụ trên tay ----
    if (humanoid && sim.reg.has<Inventory>(e) && sim.reg.has<Mind>(e)) {
        const Inventory& inv = sim.reg.get<Inventory>(e);
        const Mind& m = sim.reg.get<Mind>(e);
        int w = static_cast<int>(std::strlen(spr->rows[0]));
        float bodyTop = py + bob - spr->h * u;
        float backX = flip ? px + w * u * 0.5f - u * 0.3f : px - w * u * 0.5f - u * 0.7f;
        if (inv.item[static_cast<int>(Purpose::Warmth)] >= 0) {
            Color cape = matColor(sim, sim.designs[inv.item[static_cast<int>(Purpose::Warmth)]].mainMat);
            DrawRectangleV({backX, bodyTop + u * 3.5f}, {u, u * 4.0f}, shade(cape, -10));
        }
        if (inv.item[static_cast<int>(Purpose::Carry)] >= 0) {
            Color bag = matColor(sim, sim.designs[inv.item[static_cast<int>(Purpose::Carry)]].mainMat);
            DrawRectangleV({backX - (flip ? -u * 0.5f : u * 0.5f), bodyTop + u * 4.0f}, {u * 1.6f, u * 2.2f}, shade(bag, -15));
        }
        int slot = toolSlotFor(sim, e, m, inv);
        if (slot >= 0) {
            const Design& d = sim.designs[inv.item[slot]];
            Color main = matColor(sim, d.mainMat);
            MatId shaftMat = kNoMat;
            for (const Part& pt : d.parts) if (pt.shape == Shape::Shaft || pt.shape == Shape::Limb) { shaftMat = pt.mat; break; }
            Color handle = shaftMat != kNoMat ? matColor(sim, shaftMat) : rgb(130, 90, 50);
            const ToolSprite* ts = &kClub;
            switch (static_cast<Purpose>(slot)) {
            case Purpose::Cut: ts = &kAxe; break;
            case Purpose::Dig: ts = &kPick; break;
            case Purpose::Pierce: ts = &kSpear; break;
            case Purpose::Smash: case Purpose::Siege: ts = &kClub; break;
            case Purpose::Ranged: ts = &kBow; break;
            case Purpose::FireStart: ts = &kTorch; break;
            default: ts = nullptr; break;
            }
            if (ts) {
                // Vung dụng cụ khi đang làm việc
                ActionId a = m.plan.done() ? ActionId::Count : m.plan.current();
                bool working = a == ActionId::GatherWood || a == ActionId::GatherStone || a == ActionId::GatherOre || a == ActionId::Build ||
                               a == ActionId::Farm || a == ActionId::Fight || a == ActionId::Hunt;
                float swing = working && !tr.moving ? std::fabs(std::sin(time * 8 + entityIndex(e))) * u * 1.2f : 0;
                float handX = flip ? px - w * u * 0.5f + u * 0.2f : px + w * u * 0.5f - u * 0.2f;
                float handY = bodyTop + u * 6.0f + swing;
                float tu = u * 0.85f;
                blitTool(*ts, main, handle, handX, handY, tu, flip, time);
            }
        }
    }
    if (b.sleeping) {
        float z = std::fmod(time * 1.5f + entityIndex(e) * 0.3f, 1.0f);
        DrawRectangleV({px + 2, py - 6 - z * 3}, {1.2f, 1.2f}, Color{200, 200, 255, static_cast<unsigned char>(220 - z * 200)});
    }
    if (so.role == Role::Leader) {
        // Vương miện / mũ lông thủ lĩnh
        float top = py + bob - spr->h * u - u;
        DrawRectangleV({px - u * 1.5f, top}, {u * 3, u}, GOLD);
        DrawRectangleV({px - u * 1.5f, top - u}, {u, u}, GOLD);
        DrawRectangleV({px + u * 0.5f, top - u}, {u, u}, GOLD);
    }
}

void drawGroundItems(const Simulation& csim, float x0, float y0, float x1, float y1) {
    const float S = kTileWorld;
    for (const auto& it : csim.world.items()) {
        if (it.amount <= 0 || it.x < x0 || it.y < y0 || it.x > x1 || it.y > y1) continue;
        float cx = it.x * S + 2 + (hash32(it.x * 31 + it.y * 17) % 4), cy = it.y * S + 3 + (hash32(it.x * 7 + it.y * 13) % 3);
        switch (it.kind) {
        case ItemKind::Corpse:
            DrawRectangleV({cx, cy}, {3.2f, 1.6f}, Color{120, 60, 55, 230});
            DrawRectangleV({cx + 0.4f, cy + 0.4f}, {1.0f, 0.6f}, Color{170, 40, 40, 230});
            break;
        case ItemKind::Bones:
            DrawRectangleV({cx, cy + 0.6f}, {3.0f, 0.5f}, Color{235, 230, 210, 230});
            DrawRectangleV({cx + 0.6f, cy}, {0.5f, 1.6f}, Color{235, 230, 210, 230});
            break;
        case ItemKind::Meat: DrawRectangleV({cx, cy}, {1.6f, 1.2f}, Color{200, 80, 80, 240}); break;
        case ItemKind::Rock: DrawRectangleV({cx, cy}, {1.6f, 1.4f}, Color{140, 140, 135, 240}); break;
        case ItemKind::OldTool: case ItemKind::OldWeapon:
            DrawLineEx({cx, cy + 2}, {cx + 2.4f, cy}, 0.5f, Color{140, 100, 60, 240});
            DrawRectangleV({cx + 1.8f, cy - 0.3f}, {1.0f, 1.0f}, Color{170, 170, 180, 240});
            break;
        default: break;
        }
    }
}

} // namespace wb
