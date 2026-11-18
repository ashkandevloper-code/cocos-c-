#include "games/backgammon/Backgammon.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace nx {
namespace bg {

std::string Move::str() const {
    return "BgMove(" + std::to_string(from) + "->" + std::to_string(to) + " d" + std::to_string(die) + (hit ? " hit" : "") + ")";
}

State State::initial() {
    State s;
    s.points.fill(0);
    s.points[23] = 2;
    s.points[12] = 5;
    s.points[7] = 3;
    s.points[5] = 5;
    s.points[0] = -2;
    s.points[11] = -5;
    s.points[16] = -3;
    s.points[18] = -5;
    return s;
}

std::string State::key() const {
    std::string k;
    for (int i = 0; i < 24; ++i) k += (i ? "," : "") + std::to_string(points[i]);
    k += "|" + std::to_string(bar[0]) + "," + std::to_string(bar[1]) + "|" + std::to_string(off[0]) + "," + std::to_string(off[1]);
    return k;
}

int State::winner() const {
    if (off[0] >= kCheckers) return 0;
    if (off[1] >= kCheckers) return 1;
    return -1;
}

int State::pip(int p) const {
    int total = bar[p] * 25;
    for (int i = 0; i < 24; ++i) {
        int c = own(p, i);
        if (c > 0) total += c * relPos(p, i);
    }
    return total;
}

int State::checkersOnBoard(int p) const {
    int c = bar[p];
    for (int i = 0; i < 24; ++i) c += own(p, i);
    return c;
}

int relPos(int p, int idx) { return p == 0 ? idx + 1 : 24 - idx; }
int idxOf(int p, int pos) { return p == 0 ? pos - 1 : 24 - pos; }
int entryIndex(int p, int die) { return p == 0 ? 24 - die : die - 1; }
bool isHome(int p, int idx) { return relPos(p, idx) <= 6; }

std::vector<int> expandDice(const std::vector<int>& dice) {
    if (dice.size() == 2 && dice[0] == dice[1]) return {dice[0], dice[0], dice[0], dice[0]};
    return dice;
}

bool allHome(const State& s, int p) {
    if (s.bar[p] > 0) return false;
    int c = s.off[p];
    for (int i = 0; i < 24; ++i) if (isHome(p, i)) c += s.own(p, i);
    return c == kCheckers;
}

static bool hasHigher(const State& s, int p, int pos) {
    for (int q = pos + 1; q <= 6; ++q) if (s.own(p, idxOf(p, q)) > 0) return true;
    return false;
}

std::vector<Move> singleMoves(const State& s, int p, int die) {
    std::vector<Move> out;
    if (s.bar[p] > 0) {
        int to = entryIndex(p, die);
        int o = s.opp(p, to);
        if (o < 2) out.push_back({kBar, to, die, o == 1});
        return out;
    }
    bool canBear = allHome(s, p);
    for (int i = 0; i < 24; ++i) {
        if (s.own(p, i) == 0) continue;
        int pos = relPos(p, i);
        int targetPos = pos - die;
        if (targetPos >= 1) {
            int to = p == 0 ? i - die : i + die;
            int o = s.opp(p, to);
            if (o < 2) out.push_back({i, to, die, o == 1});
        } else if (canBear) {
            if (targetPos == 0 || !hasHigher(s, p, pos)) out.push_back({i, kOff, die, false});
        }
    }
    return out;
}

State apply(const State& s, int p, const Move& m) {
    State n = s;
    applyInPlace(n, p, m);
    return n;
}

void applyInPlace(State& n, int p, const Move& m) {
    int sg = State::sign(p);
    if (m.from == kBar) n.bar[p] -= 1;
    else n.points[m.from] -= sg;
    if (m.to == kOff) {
        n.off[p] += 1;
        return;
    }
    if (n.opp(p, m.to) == 1) {
        n.points[m.to] = 0;
        n.bar[1 - p] += 1;
    }
    n.points[m.to] += sg;
}

int maxDepth(const State& s, int p, const std::vector<int>& pool) {
    if (pool.empty()) return 0;
    int best = 0;
    std::set<int> tried;
    for (size_t k = 0; k < pool.size(); ++k) {
        int d = pool[k];
        if (!tried.insert(d).second) continue;
        auto moves = singleMoves(s, p, d);
        if (moves.empty()) continue;
        std::vector<int> rest = pool;
        rest.erase(rest.begin() + k);
        for (auto& m : moves) {
            int v = 1 + maxDepth(apply(s, p, m), p, rest);
            if (v > best) best = v;
            if (best == static_cast<int>(pool.size())) return best;
        }
    }
    return best;
}

int forcedDie(const State& s, int p, const std::vector<int>& pool, int total) {
    if (total != 1 || pool.size() != 2 || pool[0] == pool[1]) return 0;
    int hi = std::max(pool[0], pool[1]);
    if (!singleMoves(s, p, hi).empty()) return hi;
    return std::min(pool[0], pool[1]);
}

std::vector<Move> legalNextMoves(const State& s, int p, const std::vector<int>& remaining, int needed, int forceDie) {
    std::vector<Move> out;
    if (needed <= 0) return out;
    std::set<int> tried;
    for (size_t k = 0; k < remaining.size(); ++k) {
        int d = remaining[k];
        if (!tried.insert(d).second) continue;
        if (forceDie != 0 && d != forceDie) continue;
        std::vector<int> rest = remaining;
        rest.erase(rest.begin() + k);
        for (auto& m : singleMoves(s, p, d)) {
            int depth = 1 + maxDepth(apply(s, p, m), p, rest);
            if (depth >= needed) out.push_back(m);
        }
    }
    return out;
}

std::vector<Play> enumeratePlays(const State& s, int p, const std::vector<int>& dice) {
    std::vector<int> pool = expandDice(dice);
    int total = maxDepth(s, p, pool);
    if (total == 0) return {Play{{}, s}};
    int force = forcedDie(s, p, pool, total);
    // Dart: Map<String, BgPlay> keeps insertion order; putIfAbsent keeps the first play reaching a position.
    std::vector<Play> results;
    std::map<std::string, size_t> index;
    std::set<std::string> visited;
    std::function<void(const State&, const std::vector<int>&, std::vector<Move>&)> rec =
        [&](const State& st, const std::vector<int>& rest, std::vector<Move>& prefix) {
            if (static_cast<int>(prefix.size()) == total) {
                std::string k = st.key();
                if (!index.count(k)) {
                    index[k] = results.size();
                    results.push_back(Play{prefix, st});
                }
                return;
            }
            std::vector<int> sorted = rest;
            std::sort(sorted.begin(), sorted.end());
            std::string vk = st.key() + "#";
            for (int d : sorted) vk += std::to_string(d);
            if (!visited.insert(vk).second) return;
            std::set<int> tried;
            for (size_t k = 0; k < rest.size(); ++k) {
                int d = rest[k];
                if (!tried.insert(d).second) continue;
                if (prefix.empty() && force != 0 && d != force) continue;
                std::vector<int> nextRest = rest;
                nextRest.erase(nextRest.begin() + k);
                for (auto& m : singleMoves(st, p, d)) {
                    prefix.push_back(m);
                    rec(apply(st, p, m), nextRest, prefix);
                    prefix.pop_back();
                }
            }
        };
    std::vector<Move> prefix;
    rec(s, pool, prefix);
    return results;
}

int resultMultiplier(const State& s, int winner) {
    int loser = 1 - winner;
    if (s.off[loser] > 0) return 1;
    if (s.bar[loser] > 0) return 3;
    for (int i = 0; i < 24; ++i) if (isHome(winner, i) && s.own(loser, i) > 0) return 3;
    return 2;
}

bool hasContact(const State& s) {
    int back0 = -100, back1 = 100;
    if (s.bar[0] > 0) back0 = 24;
    if (s.bar[1] > 0) back1 = -1;
    for (int i = 0; i < 24; ++i) {
        if (s.own(0, i) > 0 && i > back0) back0 = i;
        if (s.own(1, i) > 0 && i < back1) back1 = i;
    }
    if (back0 == -100 || back1 == 100) return false;
    return back0 > back1;
}

// ---------------- TurnTracker ----------------

TurnTracker::TurnTracker(const State& start, int pl, const std::vector<int>& dice)
    : player(pl), rolled(dice), pool(expandDice(dice)), state(start), start_(start) {
    used.assign(pool.size(), false);
    total = maxDepth(start_, player, pool);
    forceDie = forcedDie(start_, player, pool, total);
}

std::vector<int> TurnTracker::remaining() const {
    std::vector<int> r;
    for (size_t i = 0; i < pool.size(); ++i) if (!used[i]) r.push_back(pool[i]);
    return r;
}

std::vector<Move> TurnTracker::legalMoves() const {
    return legalNextMoves(state, player, remaining(), needed(), played.empty() ? forceDie : 0);
}

std::vector<Move> TurnTracker::movesFrom(int from) const {
    std::vector<Move> out;
    for (auto& m : legalMoves()) if (m.from == from) out.push_back(m);
    return out;
}

bool TurnTracker::play(const Move& m, Move* out) {
    auto legal = legalMoves();
    const Move* found = nullptr;
    for (auto& l : legal) {
        if (l.sameAs(m)) { found = &l; break; }
    }
    if (!found) return false;
    Move f = *found;
    applyInPlace(state, player, f);
    for (size_t i = 0; i < pool.size(); ++i) {
        if (!used[i] && pool[i] == f.die) { used[i] = true; break; }
    }
    played.push_back(f);
    if (out) *out = f;
    return true;
}

void TurnTracker::undo() {
    if (played.empty()) return;
    std::vector<Move> keep(played.begin(), played.end() - 1);
    state = start_;
    used.assign(pool.size(), false);
    played.clear();
    for (auto& m : keep) play(m);
}

// ---------------- Bot ----------------

static const int kShotTable[25] = {0, 11, 12, 14, 15, 15, 17, 6, 6, 5, 3, 2, 3, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1};

std::vector<Move> Bot::choose(const State& s, int p, const std::vector<int>& dice) {
    auto plays = enumeratePlays(s, p, dice);
    if (plays.empty()) return {};
    if (plays.size() == 1) return plays.front().moves;
    if (level == 0 && rng_.nextDouble() < 0.3) return plays[rng_.nextInt(static_cast<int>(plays.size()))].moves;
    double bestScore = -1e18;
    const Play* best = &plays.front();
    for (auto& pl : plays) {
        double v = evaluate(pl.state, p);
        if (level == 0) v += rng_.nextDouble() * 30.0;
        else if (level == 1) v += rng_.nextDouble() * 5.0;
        if (v > bestScore) {
            bestScore = v;
            best = &pl;
        }
    }
    return best->moves;
}

int Bot::shotsOn(const State& s, int p, int idx) {
    int o = 1 - p;
    std::set<int> distances;  // Dart Set: iteration order irrelevant for a sum
    if (o == 1) {
        if (s.bar[1] > 0) distances.insert(idx + 1);
        for (int j = 0; j < idx; ++j) if (s.own(1, j) > 0) distances.insert(idx - j);
    } else {
        if (s.bar[0] > 0) distances.insert(24 - idx);
        for (int j = idx + 1; j < 24; ++j) if (s.own(0, j) > 0) distances.insert(j - idx);
    }
    int shots = 0;
    for (int d : distances) if (d > 0 && d < 25) shots += kShotTable[d];
    return shots > 36 ? 36 : shots;
}

double Bot::evaluate(const State& s, int p) {
    int o = 1 - p;
    if (s.off[p] >= kCheckers) return 100000.0;
    int myPip = s.pip(p), opPip = s.pip(o);
    double v = static_cast<double>(opPip - myPip);
    if (!hasContact(s)) {
        v = v * 2.0 + s.off[p] * 3.0;
        for (int pos = 1; pos <= 6; ++pos) {
            int c = s.own(p, idxOf(p, pos));
            if (c > 3) v -= (c - 3) * 0.6;
        }
        return v;
    }
    v += s.off[p] * 2.0;
    v += s.bar[o] * 5.0;
    v -= s.bar[p] * 4.0;
    int run = 0, bestRun = 0, homeMade = 0;
    for (int pos = 1; pos <= 24; ++pos) {
        int idx = idxOf(p, pos);
        int c = s.own(p, idx);
        if (c >= 2) {
            ++run;
            if (run > bestRun) bestRun = run;
            double pv;
            if (pos <= 6) {
                pv = 4.0;
                ++homeMade;
                if (s.bar[o] > 0) pv += 3.0;
            } else if (pos == 7) pv = 3.5;
            else if (pos <= 11) pv = 2.0;
            else if (pos >= 19) pv = 2.5;
            else pv = 1.0;
            v += pv;
            if (c > 4) v -= (c - 4) * 1.5;
        } else {
            run = 0;
            if (c == 1) {
                int shots = shotsOn(s, p, idx);
                double loss = static_cast<double>(25 - pos);
                v -= 1.0 + shots / 36.0 * loss * 0.9;
            }
        }
    }
    if (bestRun >= 3) v += bestRun * bestRun * 1.2;
    if (s.bar[o] > 0) v += homeMade * homeMade * 0.6;
    return v;
}

}  // namespace bg
}  // namespace nx
