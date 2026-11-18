// Port of lib/games/snakes_ladders/core/snl_rules.dart (deterministic; same RNG as Dart).
#pragma once
#include "core/DartRandom.h"

#include <map>
#include <vector>

namespace nx {
namespace snl {

constexpr int kGoal = 100;
const std::map<int, int>& ladders();  // bottom -> top
const std::map<int, int>& snakes();   // head -> tail

struct MoveResult {
    int seat = 0, roll = 0, from = 0;
    std::vector<int> steps;  // squares visited while walking (bounce included)
    int landed = 0, finalPos = 0;
    bool won = false, extraTurn = false;
    bool climbed() const { return finalPos > landed; }
    bool bitten() const { return finalPos < landed; }
};

std::vector<int> walk(int from, int roll);
int resolve(int square);
bool isValidLayout();

class Engine {
public:
    Engine(int playerCount, int64_t seed);
    int playerCount;
    std::vector<int> positions;
    int turn = 0;
    int winner = -1;
    int rollDie() { return rng_.nextInt(6) + 1; }
    MoveResult move(int roll);

private:
    DartRandom rng_;
};

}  // namespace snl
}  // namespace nx
