// Port of lib/games/carrom/core/{carrom_config,carrom_physics,carrom_match,carrom_bot}.dart.
// Determinism contract (same as Dart): fixed dt = 1/120, semi-implicit Euler, fixed order, only + - * / sqrt,
// snap to 1/1000 after each shot, FNV-1a state hash. Build with -ffp-contract=off (no FMA) to stay bit-exact.
#pragma once
#include "core/DartRandom.h"
#include "core/Json.h"

#include <array>
#include <string>
#include <vector>

namespace nx {
namespace carrom {

namespace cfg {
constexpr double boardSize = 1024, left = 75, top = 67, right = 949, bottom = 916, centerX = 512, centerY = 491.5;
constexpr double pockets[4][2] = {{91.5, 88}, {932.5, 88}, {91.5, 895}, {932.5, 895}};
constexpr double pocketR = 54, captureR = 44, coinR = 21, strikerR = 27, coinMass = 1.0, strikerMass = 2.0;
constexpr double dt = 1.0 / 120, drag = 1.3, stopSpeed = 8.0, ePiece = 0.90, eWall = 0.72, maxShotSpeed = 2600;
constexpr int maxSteps = 1200;
constexpr double baseline0 = 797, baseline1 = 186, baseMinX = 248, baseMaxX = 776;
constexpr int coinsPerSide = 9, queenBonus = 3, maxTurns = 160;
constexpr double turnSeconds = 10, maxPull = 210;
constexpr int strikerId = 0, queenId = 19, bodyCount = 20;
}  // namespace cfg

enum class Kind { Striker = 0, White, Black, Queen };

struct Body {
    int id = 0;
    Kind kind = Kind::White;
    double r = 0, m = 1, x = 0, y = 0, vx = 0, vy = 0;
    bool onBoard = true;
    bool isStriker() const { return kind == Kind::Striker; }
    bool isQueen() const { return kind == Kind::Queen; }
};

struct Impact { double x, y, strength; bool wall; };

std::vector<Body> initialBodies();
int pocketAt(double x, double y);
void step(std::vector<Body>& bs, std::vector<int>& pocketed, std::vector<Impact>* sink = nullptr);
bool isSettled(const std::vector<Body>& bs);
void snap(std::vector<Body>& bs);
uint32_t stateHash(const std::vector<Body>& bs);
bool isFree(const std::vector<Body>& bs, double x, double y, double r, int ignoreId);
void respot(std::vector<Body>& bs, int id);

class Simulator {
public:
    explicit Simulator(std::vector<Body>& b, std::vector<Impact>* sink = nullptr) : bodies(b), sink_(sink) {}
    std::vector<Body>& bodies;
    std::vector<int> pocketed;
    int steps = 0;
    bool done = false;
    bool tick();
    void runToEnd() { while (!tick()) {} }

private:
    std::vector<Impact>* sink_;
};

enum class Phase { Aiming = 0, Simulating, Finished };

struct Shot {
    double placeX = 0, dirX = 0, dirY = -1, power = 0.5;
    Json toJson() const;
    static Shot fromJson(const Json& j);
};

struct TurnOutcome {
    int shooter = 0, steps = 0, ownPocketed = 0, oppPocketed = 0;
    bool queenPocketed = false, foul = false, queenCovered = false, queenPending = false, queenReturned = false;
    std::vector<int> returnedIds;
    bool continueTurn = false, matchOver = false;
};

class Match {
public:
    static Match newBoard(int firstTurn = 0);
    std::vector<Body> bodies;
    int turn = 0, turnNumber = 0;
    Phase phase = Phase::Aiming;
    bool hasWinner = false;
    int winner = 0;  // valid when hasWinner; -1 = draw
    int queenPendingFor = -1, queenCoveredBy = -1;
    std::array<int, 2> points{{0, 0}};

    Body& striker() { return bodies[cfg::strikerId]; }
    static Kind kindFor(int seat) { return seat == 0 ? Kind::White : Kind::Black; }
    static double baselineY(int seat) { return seat == 0 ? cfg::baseline0 : cfg::baseline1; }
    int remaining(int seat) const;
    int pocketedCount(int seat) const { return cfg::coinsPerSide - remaining(seat); }
    int tally(int seat) const { return pocketedCount(seat) + (queenCoveredBy == seat ? cfg::queenBonus : 0); }
    bool placementOk(double x) const;
    bool nearestPlacement(double x, double* out) const;
    std::string validate(const Shot& s) const;  // "" = legal
    void beginShot(const Shot& s);
    TurnOutcome finishShot(const std::vector<int>& pocketed, int steps);
    TurnOutcome playShot(const Shot& s);  // throws std::runtime_error on illegal shots
    void passTurn();
    uint32_t hash() const { return stateHash(bodies); }
    Json toJson() const;
    static Match fromJson(const Json& j);
    void applyJson(const Json& j) { *this = fromJson(j); }

private:
    void decideByTally();
};

enum class BotLevel { Easy = 0, Normal, Hard };

class Bot {
public:
    Bot(int seat, BotLevel level = BotLevel::Normal, int64_t seed = 9) : seat(seat), level(level), rng_(seed) {}
    int seat;
    BotLevel level;
    int candidateCount() const { return level == BotLevel::Easy ? 10 : (level == BotLevel::Normal ? 24 : 64); }
    std::vector<Shot> candidates(const Match& m);
    bool evaluate(const Match& m, const Shot& shot, double* score);
    double score(const Match& before, const Match& after, const TurnOutcome& o) const;
    Shot withExecutionNoise(const Shot& s);
    Shot fallback(const Match& m) const;
    Shot choose(const Match& m);

private:
    double aimJitter() const { return level == BotLevel::Easy ? 0.06 : (level == BotLevel::Normal ? 0.02 : 0.005); }
    double execNoise() const { return level == BotLevel::Easy ? 0.06 : (level == BotLevel::Normal ? 0.02 : 0.003); }
    Shot rotated(double x, double dx, double dy, double power, double ang) const;
    DartRandom rng_;
};

/// Incremental bot search (spread over frames by the scene).
class BotSearch {
public:
    BotSearch(Bot& bot, const Match& m) : bot_(bot), base_(m), cands_(bot.candidates(m)) {}
    bool done() const { return i_ >= cands_.size(); }
    double progress() const { return cands_.empty() ? 1.0 : static_cast<double>(i_) / cands_.size(); }
    bool step(int budget);
    Shot best() const { return hasBest_ ? best_ : bot_.fallback(base_); }

private:
    Bot& bot_;
    Match base_;
    std::vector<Shot> cands_;
    size_t i_ = 0;
    bool hasBest_ = false;
    Shot best_;
    double bestScore_ = -1e18;
};

}  // namespace carrom
}  // namespace nx
