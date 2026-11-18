// Port of lib/games/ludo/core/ludo_rules.dart + ludo_bot.dart.
// Token progress: -1 base, 0..50 main track, 51..55 home column, 56 home.
#pragma once
#include "core/DartRandom.h"

#include <array>
#include <string>
#include <vector>

namespace nx {
namespace ludo {

constexpr int kBase = -1, kLastTrack = 50, kHome = 56, kTrackLen = 52, kTokens = 4;

struct Capture { int seat, token, from; };

struct Move {
    int seat = 0, token = 0, from = 0, to = 0;
    std::vector<Capture> captures;
    bool leavesBase() const { return from == kBase; }
    bool finishes() const { return to == kHome; }
    std::string str() const;
};

struct State {
    int playerCount = 2;
    std::vector<int> corners;
    std::vector<std::array<int, kTokens>> tokens;
    int turn = 0;
    int sixStreak = 0;
    int winner = -1;
    static State initial(int playerCount);
    int globalOf(int seat, int progress) const { return (13 * corners[seat] + progress) % kTrackLen; }
    int homeCount(int seat) const;
    bool hasWon(int seat) const { return homeCount(seat) == kTokens; }
};

std::vector<int> cornersFor(int n);
inline bool isOnTrack(int p) { return p >= 0 && p <= kLastTrack; }
inline bool isSafeGlobal(int g) { return g % 13 == 0 || g % 13 == 8; }
std::vector<Move> legalMoves(const State& s, int seat, int roll);
std::vector<Capture> capturesAt(const State& s, int seat, int to);
State apply(const State& s, const Move& m);
inline bool grantsExtraTurn(const Move& m, int roll) { return roll == 6 || !m.captures.empty() || m.to == kHome; }
bool isThreatened(const State& s, int seat, int progress);

struct RollResult {
    int roll = 0;
    std::vector<Move> moves;
    bool forfeited = false;
    bool rollAgain = false;
    bool passes() const { return moves.empty() && !rollAgain; }
};
struct PlayResult { bool extraTurn = false, won = false; };

class Engine {
public:
    Engine(int playerCount, int64_t seed) : state(State::initial(playerCount)), rng_(seed) {}
    State state;
    int pendingRoll = -1;
    int rollDie() { return rng_.nextInt(6) + 1; }
    RollResult roll() { return applyRoll(rollDie()); }
    RollResult applyRoll(int r);
    PlayResult play(const Move& m);
    void advanceTurn();

private:
    DartRandom rng_;
};

class Bot {
public:
    Bot(int level, int64_t seed) : level(level), rng_(seed) {}
    int level;
    Move choose(const State& s, const std::vector<Move>& moves);
    static double score(const State& s, const Move& m);

private:
    DartRandom rng_;
};

}  // namespace ludo
}  // namespace nx
