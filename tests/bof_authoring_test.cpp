#include "bof_authoring_contract.h"
#include <array>
#include <cstdio>
#include <cstdlib>
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
int main() {
    using namespace bof_authoring;
    std::array<int, 12> p{};
    for (int predicate : {0, 2, 3}) for (int subject = 0; subject < 4; ++subject) {
        p = {}; p[1] = predicate; p[2] = subject; p[3] = 1;
        const auto before = p;
        CHECK(ConditionValid(158, p.data())); CHECK(p == before);
        p[4] = 1; CHECK(!ConditionValid(158, p.data()));
    }
    p = {}; p[1] = 1; CHECK(!ConditionValid(158, p.data()));
    p[1] = 3; p[3] = 0; CHECK(!ConditionValid(158, p.data()));
    for (int rule = 1; rule <= 9; ++rule) {
        p = {}; p[1] = 1; p[3] = 15; p[4] = rule; p[5] = 3; p[6] = 2; p[7] = 1; p[8] = 77;
        CHECK(ConditionValid(159, p.data()));
        p[8] = 0; CHECK(!ConditionValid(159, p.data()));
        p[8] = 77; p[3] = 16; CHECK(!ConditionValid(159, p.data()));
    }
    p = {}; p[1] = 2; p[8] = 1; CHECK(ConditionValid(159, p.data()));
    p[4] = 1; CHECK(!ConditionValid(159, p.data()));
    p = {}; p[1] = 1; p[4] = 9; p[5] = 1; p[6] = 2; p[7] = 255; p[8] = 1;
    CHECK(ConditionValid(159, p.data())); p[7] = 256; CHECK(!ConditionValid(159, p.data()));
    for (int op = 0; op <= 4; ++op) {
        p = {}; p[0] = op; CHECK(EffectValid(160, p.data()));
        for (int i = 1; i < 12; ++i) { p[i] = 1; CHECK(!EffectValid(160, p.data())); p[i] = 0; }
    }
    p = {}; p[0] = 5; CHECK(!EffectValid(160, p.data()));
    for (int facing = 0; facing <= 6; ++facing) {
        p = {}; p[0] = 1; p[1] = 350; p[2] = 2; p[3] = -123; p[4] = -456; p[8] = facing; p[10] = 0x10;
        CHECK(EffectValid(161, p.data())); p[10] |= 1; CHECK(!EffectValid(161, p.data()));
        p = {}; p[0] = 27; p[1] = -123; p[3] = 20; p[5] = 40; p[6] = facing; p[7] = 1;
        const auto before = p;
        CHECK(EffectValid(162, p.data())); CHECK(before == p);
        p[9] = 1; CHECK(!EffectValid(162, p.data()));
    }
    CHECK(!ConditionValid(160, p.data())); CHECK(!EffectValid(163, p.data()));
    std::printf("BOF AUTHORING CONTRACT PASS: %d checks\n", checks);
}
