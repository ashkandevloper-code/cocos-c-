#include "games/snakes/SnlRules.h"

namespace nx {
namespace snl {

const std::map<int, int>& ladders() {
    static const std::map<int, int> m = {{1, 38}, {4, 14}, {9, 31}, {21, 42}, {28, 84}, {36, 44}, {51, 67}, {71, 91}, {80, 100}};
    return m;
}
const std::map<int, int>& snakes() {
    static const std::map<int, int> m = {{16, 6}, {47, 26}, {49, 11}, {56, 53}, {62, 19},
                                         {64, 60}, {87, 24}, {93, 73}, {95, 75}, {98, 78}};
    return m;
}

std::vector<int> walk(int from, int roll) {
    std::vector<int> steps;
    int pos = from, dir = 1;
    for (int i = 0; i < roll; ++i) {
        if (pos == kGoal) dir = -1;
        pos += dir;
        steps.push_back(pos);
    }
    return steps;
}

int resolve(int square) {
    auto l = ladders().find(square);
    if (l != ladders().end()) return l->second;
    auto s = snakes().find(square);
    if (s != snakes().end()) return s->second;
    return square;
}

bool isValidLayout() {
    for (auto& kv : ladders()) {
        if (snakes().count(kv.first)) return false;
        if (kv.second <= kv.first) return false;
    }
    for (auto& kv : snakes()) {
        if (kv.second >= kv.first || kv.first == kGoal) return false;
    }
    return true;
}

Engine::Engine(int pc, int64_t seed) : playerCount(pc < 2 ? 2 : (pc > 4 ? 4 : pc)), rng_(seed) {
    positions.assign(playerCount, 0);
}

MoveResult Engine::move(int roll) {
    MoveResult r;
    r.seat = turn;
    r.roll = roll;
    r.from = positions[turn];
    r.steps = walk(r.from, roll);
    r.landed = r.steps.empty() ? r.from : r.steps.back();
    r.finalPos = resolve(r.landed);
    positions[turn] = r.finalPos;
    r.won = r.finalPos == kGoal;
    if (r.won) winner = turn;
    r.extraTurn = !r.won && roll == 6;
    if (!r.won && !r.extraTurn) turn = (turn + 1) % playerCount;
    return r;
}

}  // namespace snl
}  // namespace nx
