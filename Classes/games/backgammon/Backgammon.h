// Port of lib/games/backgammon/core/backgammon_rules.dart + backgammon_bot.dart.
// Points 0..23; positive = player 0 (moves 23 -> 0, home 0..5), negative = player 1 (0 -> 23, home 18..23).
#pragma once
#include "core/DartRandom.h"

#include <array>
#include <string>
#include <vector>

namespace nx {
namespace bg {

constexpr int kBar = -1, kOff = -2, kCheckers = 15;

struct Move {
    int from = 0, to = 0, die = 0;
    bool hit = false;
    bool sameAs(const Move& o) const { return o.from == from && o.to == to && o.die == die; }
    std::string str() const;
};

struct State {
    std::array<int, 24> points{};
    std::array<int, 2> bar{{0, 0}};
    std::array<int, 2> off{{0, 0}};
    int turn = 0;
    static State initial();
    int own(int p, int idx) const {
        int v = points[idx];
        if (p == 0) return v > 0 ? v : 0;
        return v < 0 ? -v : 0;
    }
    int opp(int p, int idx) const { return own(1 - p, idx); }
    static int sign(int p) { return p == 0 ? 1 : -1; }
    std::string key() const;
    int winner() const;  // -1 none
    int pip(int p) const;
    int checkersOnBoard(int p) const;
};

struct Play {
    std::vector<Move> moves;
    State state;
};

int relPos(int p, int idx);
int idxOf(int p, int pos);
int entryIndex(int p, int die);
bool isHome(int p, int idx);
std::vector<int> expandDice(const std::vector<int>& dice);
bool allHome(const State& s, int p);
std::vector<Move> singleMoves(const State& s, int p, int die);
State apply(const State& s, int p, const Move& m);
void applyInPlace(State& n, int p, const Move& m);
int maxDepth(const State& s, int p, const std::vector<int>& pool);
int forcedDie(const State& s, int p, const std::vector<int>& pool, int total);  // 0 = none
std::vector<Move> legalNextMoves(const State& s, int p, const std::vector<int>& remaining, int needed, int forceDie = 0);
std::vector<Play> enumeratePlays(const State& s, int p, const std::vector<int>& dice);
int resultMultiplier(const State& s, int winner);
bool hasContact(const State& s);

/// Human turn helper: move-by-move legality + undo.
class TurnTracker {
public:
    TurnTracker(const State& start, int player, const std::vector<int>& dice);
    int player;
    std::vector<int> rolled, pool;
    std::vector<bool> used;
    int total = 0;
    int forceDie = 0;
    State state;
    std::vector<Move> played;
    std::vector<int> remaining() const;
    int needed() const { return total - static_cast<int>(played.size()); }
    bool complete() const { return needed() <= 0; }
    std::vector<Move> legalMoves() const;
    std::vector<Move> movesFrom(int from) const;
    bool play(const Move& m, Move* out = nullptr);
    bool canUndo() const { return !played.empty(); }
    void undo();

private:
    State start_;
};

class Dice {
public:
    explicit Dice(int64_t seed) : rng_(seed) {}
    int roll() { return rng_.nextInt(6) + 1; }

private:
    DartRandom rng_;
};

class Bot {
public:
    Bot(int level, int64_t seed) : level(level), rng_(seed) {}
    int level;
    std::vector<Move> choose(const State& s, int p, const std::vector<int>& dice);
    static int shotsOn(const State& s, int p, int idx);
    static double evaluate(const State& s, int p);

private:
    DartRandom rng_;
};

}  // namespace bg
}  // namespace nx
