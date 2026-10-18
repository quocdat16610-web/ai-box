// =============================================================================
// SocietySystem.h — Hệ thống xã hội vĩ mô: các nhóm tự hình thành & tiến hoá.
//
//   formGroups()      — cá thể sống bầy đàn chưa có nhóm tụ lại → lập Bầy/Nhóm
//   updateGroup()     — tri thức tập thể, văn hoá, lãnh đạo, vai trò, lãnh thổ
//   advanceStage()    — Bầy → Bộ lạc → Làng → Thị trấn → Thành phố → Quốc gia
//   enactLaws()       — ban hành luật dựa trên tri thức + giá trị văn hoá
//   diplomacy()       — thiện cảm, tuyên chiến, nghị hoà, sáp nhập
//   splitGroup()      — nhóm quá đông / bất đồng → tách nhóm, ngôn ngữ trôi dạt
//   speciation()      — nhóm cô lập lâu, gen lệch xa → loài mới
// =============================================================================
#pragma once
#include "../core/Ecs.h"

namespace wb {

class Simulation;
struct Group;

class SocietySystem {
public:
    explicit SocietySystem(Simulation& sim) : sim_(sim) {}
    void update();

private:
    void formGroups();
    void updateGroup(Group& g);
    void recountBuildings(Group& g);
    void aggregateKnowhow(Group& g);
    void claimTerritory(Group& g);
    void advanceStage(Group& g);
    void enactLaws(Group& g);
    void assignRoles(Group& g);
    void chooseLeader(Group& g);
    void diplomacy();
    void splitGroup(Group& g);
    void speciation(Group& g);
    void dissolve(Group& g);
    void foundNation(Group& g);
    void announce(Group& g, const char* what);
    Simulation& sim_;
};

} // namespace wb
