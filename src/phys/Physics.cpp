// =============================================================================
// Physics.cpp — Tích phân chuyển động, va chạm, nổ.
// =============================================================================
#include "Physics.h"
#include "../sim/Simulation.h"
#include "../ai/Brain.h"
#include <cmath>

namespace wb {

bool ballisticAngle(float v, float d, float& angle) {
    // Tầm xa trên mặt phẳng: R = v² sin(2θ) / g  →  θ = ½·asin(g·d / v²)  (chọn góc thấp)
    float s = kGravity * d / (v * v);
    if (s > 1.0f) return false;
    angle = 0.5f * std::asin(s);
    return true;
}

int PhysicsWorld::spawn(const PhysBody& b) {
    if (bodies.size() > 600) return -1; // giới hạn hiệu năng (điện thoại)
    bodies.push_back(b);
    return static_cast<int>(bodies.size()) - 1;
}

void PhysicsWorld::step(Simulation& sim) {
    // Giữ sự kiện ~30 tick gần nhất để sinh vật kịp quan sát (chúng không nghĩ mỗi tick)
    events.erase(std::remove_if(events.begin(), events.end(), [&](const PhysEvent& e) { return e.tick < sim.tick - 30; }), events.end());
    World& w = sim.world;
    const float dt = kPhysDt;
    for (auto& b : bodies) {
        if (!b.alive) continue;
        if (--b.life <= 0) { b.alive = false; continue; }
        const MaterialDef& md = w.materials[b.mat];
        // ---- Lực: trọng lực, cản không khí, gió ----
        if (!b.floating) b.vz -= kGravity * dt;
        float speed = std::sqrt(b.vel.len2() + b.vz * b.vz);
        float drag = 0.02f * b.radius * b.radius * speed / std::max(0.05f, b.mass); // cản ∝ v²·diện tích / khối lượng
        b.vel = b.vel * (1.0f - std::min(0.5f, drag * dt * 10));
        b.vz *= 1.0f - std::min(0.5f, drag * dt * 10);
        b.vel += w.matter.wind * (dt * 2.0f / std::max(0.2f, b.mass));
        Vec2 prev = b.pos;
        b.pos += b.vel * dt;
        b.z += b.vz * dt;
        int tx = static_cast<int>(b.pos.x), ty = static_cast<int>(b.pos.y);
        if (!w.inBounds(tx, ty)) { b.alive = false; continue; }
        Tile& tile = w.at(tx, ty);

        // ---- Va chạm sinh vật (khi bay thấp) ----
        if (b.kind == BodyKind::Projectile || b.kind == BodyKind::Boulder || b.kind == BodyKind::Debris) {
            if (b.z < 2.0f && speed > 4.0f) {
                Entity hit = NullEntity;
                sim.grid.query(b.pos, 0.7f + b.radius, [&](Entity e, Vec2) {
                    if (hit != NullEntity || e == b.owner) return;
                    const Body* eb = sim.reg.pool<Body>().tryGet(e);
                    if (eb && !eb->dead) hit = e;
                });
                if (hit != NullEntity && b.blast > 0) {
                    w.matter.explode(tx, ty, b.blast, sim.tick);   // bom trúng người → nổ tung
                    b.alive = false;
                    continue;
                }
                if (hit != NullEntity) {
                    // Động năng ½mv² → sát thương; vật sắc (mũi tên) xuyên tốt hơn vật tù
                    float ke = 0.5f * b.mass * speed * speed;
                    float dmg = ke * 0.08f * b.sharpness;
                    Body& hb = sim.reg.get<Body>(hit);
                    hb.hp -= dmg;
                    hb.lastAttacker = b.owner;
                    if (Transform* ht = sim.reg.tryGet<Transform>(hit)) ht->push += b.vel.normalized() * std::min(0.6f, ke * 0.002f);
                    if (Mind* hm = sim.reg.tryGet<Mind>(hit)) { hm->needReplan = true; hm->nextThink = sim.tick; }
                    if (hb.hp <= 0) {
                        const Social* os = sim.reg.tryGet<Social>(b.owner);
                        const Social* vs = sim.reg.tryGet<Social>(hit);
                        if (os && vs && os->group >= 0 && vs->group >= 0 && os->group != vs->group) sim.killEvents.push_back({os->group, vs->group});
                        if (Body* ob = sim.reg.tryGet<Body>(b.owner)) ob->kills++;
                        sim.kill(hit, 2);
                    }
                    events.push_back({sim.tick, PhysEventKind::Impact, b.pos, b.mat, ke});
                    b.alive = false;
                    if (b.kind == BodyKind::Projectile) w.dropItem(tx, ty, ItemKind::OldWeapon, 1, b.design);
                    continue;
                }
            }
        }

        // ---- Va chạm công trình: phá huỷ theo động năng so với độ cứng vật liệu ----
        if (tile.building != Building::None && b.z < 3.0f) {
            float ke = 0.5f * b.mass * speed * speed;
            float wallHard = buildingInfo(tile.building).flammable ? 2.5f : 6.0f;
            int dmg = static_cast<int>(ke / (wallHard * 40.0f));
            if (dmg > 0) {
                tile.buildingHp = static_cast<uint8_t>(std::max(0, tile.buildingHp - dmg));
                if (tile.buildingHp == 0) w.setBuilding(tx, ty, Building::None);
                events.push_back({sim.tick, PhysEventKind::Impact, b.pos, b.mat, ke});
            }
            if (buildingInfo(tile.building).blocksMove || dmg > 0) {
                b.pos = prev; b.vel = b.vel * -0.2f; // dội ngược
            }
        }

        // ---- Chạm mặt đất / mặt nước ----
        if (b.z <= 0 && b.blast > 0 && !w.isWater(tx, ty)) {
            w.matter.explode(tx, ty, b.blast, sim.tick);
            b.alive = false;
            continue;
        }
        if (b.z <= 0) {
            b.z = 0;
            if (w.isWater(tx, ty)) {
                // Định luật Archimedes: nổi nếu khối lượng riêng < nước
                if (md.density < 1000) {
                    if (!b.floating) events.push_back({sim.tick, PhysEventKind::Float, b.pos, b.mat, 0});
                    b.floating = true; b.vz = 0;
                    b.vel = b.vel * 0.9f + w.matter.wind * 0.3f; // trôi theo gió/sóng
                } else {
                    events.push_back({sim.tick, PhysEventKind::Sink, b.pos, b.mat, 0});
                    b.alive = false;
                }
                continue;
            }
            b.floating = false;
            if (b.temp > 300) w.matter.addHeat(tx, ty, (b.temp - 300) * 0.5f); // mảnh nóng đốt nơi rơi
            float vImpact = -b.vz;
            float restitution = 0.15f + md.elasticity * 0.5f;
            if (vImpact > 2.0f) {
                b.vz = vImpact * restitution;     // nảy lên
                b.vel = b.vel * 0.7f;             // ma sát khi chạm đất
                events.push_back({sim.tick, PhysEventKind::Bounce, b.pos, b.mat, 0.5f * b.mass * vImpact * vImpact});
            } else {
                b.vz = 0;
                // Lăn trên mặt đất: ma sát lăn nhỏ hơn ma sát trượt; xuống dốc thì tăng tốc
                float slopeX = (w.inBounds(tx + 1, ty) ? w.at(tx + 1, ty).height : tile.height) - tile.height;
                float slopeY = (w.inBounds(tx, ty + 1) ? w.at(tx, ty + 1).height : tile.height) - tile.height;
                b.vel.x -= slopeX * kGravity * 6 * dt;
                b.vel.y -= slopeY * kGravity * 6 * dt;
                b.vel = b.vel * 0.93f;
                if (b.vel.len2() > 1.0f && b.kind == BodyKind::Boulder && sim.tick % 10 == 0)
                    events.push_back({sim.tick, PhysEventKind::Roll, b.pos, b.mat, 0});
                if (b.vel.len2() < 0.05f) {
                    // Dừng hẳn → trở thành vật chất / vật phẩm trên mặt đất
                    b.alive = false;
                    if (b.kind == BodyKind::Projectile) w.dropItem(tx, ty, ItemKind::OldWeapon, 1, b.design);
                    else if (b.mat != kNoMat) w.matter.addMatter(tx, ty, b.mat, std::max(1, static_cast<int>(b.mass)), b.temp);
                }
            }
            if (!w.walkable(tx, ty) && !w.isWater(tx, ty)) { b.pos = prev; b.vel = b.vel * -0.3f; }
        }
    }
    // ---- Va chạm vật – vật (bảo toàn động lượng), chỉ giữa các vật lớn để rẻ ----
    for (size_t i = 0; i < bodies.size(); ++i) {
        PhysBody& a = bodies[i];
        if (!a.alive || a.kind == BodyKind::Projectile) continue;
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            PhysBody& c = bodies[j];
            if (!c.alive || c.kind == BodyKind::Projectile) continue;
            Vec2 d = c.pos - a.pos;
            float r = a.radius + c.radius;
            if (d.len2() >= r * r || std::fabs(a.z - c.z) > r) continue;
            Vec2 n = d.normalized();
            float va = a.vel.x * n.x + a.vel.y * n.y, vc = c.vel.x * n.x + c.vel.y * n.y;
            if (va - vc <= 0) continue;
            float e = 0.5f;
            float jimp = (1 + e) * (va - vc) / (1 / a.mass + 1 / c.mass);
            a.vel += n * (-jimp / a.mass);
            c.vel += n * (jimp / c.mass);
        }
    }
    bodies.erase(std::remove_if(bodies.begin(), bodies.end(), [](const PhysBody& b) { return !b.alive; }), bodies.end());
}

void PhysicsWorld::blast(Simulation& sim, Vec2 c, float energy) {
    float radius = std::sqrt(energy) / 4.0f + 1.5f;
    events.push_back({sim.tick, PhysEventKind::Blast, c, kNoMat, energy});
    // Vật thể bị hất văng: xung lực ∝ E / (khoảng cách² + 1)
    for (auto& b : bodies) {
        Vec2 d = b.pos - c;
        float r2 = d.len2();
        if (r2 > radius * radius) continue;
        float imp = energy / (r2 + 1.0f) * 0.05f;
        b.vel += d.normalized() * (imp / b.mass);
        b.vz += imp / b.mass * 0.6f;
        b.floating = false;
    }
    // Sinh vật: sát thương + bị đẩy lùi
    sim.grid.query(c, radius, [&](Entity e, Vec2 p) {
        Body* bd = sim.reg.pool<Body>().tryGet(e);
        if (!bd || bd->dead) return;
        float r2 = dist2(p, c);
        float force = energy / (r2 + 1.0f);
        bd->hp -= force * 0.04f;
        if (Transform* t = sim.reg.tryGet<Transform>(e)) t->push += (p - c).normalized() * std::min(1.5f, force * 0.003f);
        if (Mind* m = sim.reg.tryGet<Mind>(e)) { m->needReplan = true; m->pendingStim[static_cast<int>(Stimulus::SeeDeath)]++; }
        if (bd->hp <= 0) sim.kill(e, 3);
    });
    // Công trình gần tâm nổ sụp đổ; mảnh vỡ văng ra (vật thể vật lý thật)
    int R = static_cast<int>(radius * 0.6f);
    for (int dy = -R; dy <= R; ++dy)
        for (int dx = -R; dx <= R; ++dx) {
            int x = static_cast<int>(c.x) + dx, y = static_cast<int>(c.y) + dy;
            if (!sim.world.inBounds(x, y) || dx * dx + dy * dy > R * R) continue;
            Tile& t = sim.world.at(x, y);
            if (t.building != Building::None) sim.world.setBuilding(x, y, Building::None);
            if (sim.rng.chance(0.15f)) {
                PhysBody deb;
                deb.kind = BodyKind::Debris;
                deb.pos = {x + 0.5f, y + 0.5f};
                deb.mat = t.stone > 0 ? t.stoneMat : groundMaterial(t.terrain);
                deb.mass = 3;
                deb.radius = 0.3f;
                Vec2 dir = (deb.pos - c).normalized();
                float v = std::min(25.0f, std::sqrt(energy) * 0.4f);
                deb.vel = dir * v * sim.rng.range(0.5f, 1.0f);
                deb.vz = v * sim.rng.range(0.4f, 1.0f);
                deb.life = 200;
                deb.sharpness = 0.5f;
                spawn(deb);
            }
        }
}

} // namespace wb
