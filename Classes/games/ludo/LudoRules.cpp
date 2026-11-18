#include "games/ludo/LudoRules.h"

namespace nx {
namespace ludo {

std::string Move::str() const {
    return "LudoMove(s" + std::to_string(seat) + " t" + std::to_string(token) + " " + std::to_string(from) + "->" +
           std::to_string(to) + " x" + std::to_string(captures.size()) + ")";
}

std::vector<int> cornersFor(int n) {
    if (n <= 2) return {3, 1};
    if (n == 3) return {3, 0, 1};
    return {3, 0, 1, 2};
}

State State::initial(int pc) {
    State s;
    s.playerCount = pc < 2 ? 2 : (pc > 4 ? 4 : pc);
    s.corners = cornersFor(s.playerCount);
    std::array<int, kTokens> base;
    base.fill(kBase);
    s.tokens.assign(s.playerCount, base);
    return s;
}

int State::homeCount(int seat) const {
    int c = 0;
    for (int p : tokens[seat]) if (p == kHome) ++c;
    return c;
}

std::vector<Capture> capturesAt(const State& s, int seat, int to) {
    std::vector<Capture> caps;
    if (!isOnTrack(to)) return caps;
    int g = s.globalOf(seat, to);
    if (isSafeGlobal(g)) return caps;
    for (int o = 0; o < s.playerCount; ++o) {
        if (o == seat) continue;
        for (int u = 0; u < kTokens; ++u) {
            int q = s.tokens[o][u];
            if (isOnTrack(q) && s.globalOf(o, q) == g) caps.push_back({o, u, q});
        }
    }
    return caps;
}

std::vector<Move> legalMoves(const State& s, int seat, int roll) {
    std::vector<Move> out;
    for (int t = 0; t < kTokens; ++t) {
        int p = s.tokens[seat][t];
        int to;
        if (p == kHome) continue;
        if (p == kBase) {
            if (roll != 6) continue;
            to = 0;
        } else {
            to = p + roll;
            if (to > kHome) continue;
        }
        Move m;
        m.seat = seat;
        m.token = t;
        m.from = p;
        m.to = to;
        m.captures = capturesAt(s, seat, to);
        out.push_back(m);
    }
    return out;
}

State apply(const State& s, const Move& m) {
    State n = s;
    n.tokens[m.seat][m.token] = m.to;
    for (auto& c : m.captures) n.tokens[c.seat][c.token] = kBase;
    if (n.hasWon(m.seat)) n.winner = m.seat;
    return n;
}

bool isThreatened(const State& s, int seat, int progress) {
    if (!isOnTrack(progress)) return false;
    int g = s.globalOf(seat, progress);
    if (isSafeGlobal(g)) return false;
    for (int o = 0; o < s.playerCount; ++o) {
        if (o == seat) continue;
        for (int q : s.tokens[o]) {
            if (!isOnTrack(q)) continue;
            int d = (g - s.globalOf(o, q) + kTrackLen) % kTrackLen;
            if (d >= 1 && d <= 6 && q + d <= kLastTrack) return true;
        }
    }
    return false;
}

RollResult Engine::applyRoll(int r) {
    RollResult res;
    res.roll = r;
    if (state.winner >= 0) return res;
    if (r == 6 && state.sixStreak >= 2) {
        advanceTurn();
        res.forfeited = true;
        return res;
    }
    res.moves = legalMoves(state, state.turn, r);
    if (res.moves.empty()) {
        if (r == 6) {
            state.sixStreak += 1;
            res.rollAgain = true;
            return res;
        }
        advanceTurn();
        return res;
    }
    pendingRoll = r;
    return res;
}

PlayResult Engine::play(const Move& m) {
    int r = pendingRoll >= 0 ? pendingRoll : 0;
    pendingRoll = -1;
    state = apply(state, m);
    if (state.winner >= 0) return {false, true};
    if (grantsExtraTurn(m, r)) {
        state.sixStreak = r == 6 ? state.sixStreak + 1 : 0;
        return {true, false};
    }
    advanceTurn();
    return {false, false};
}

void Engine::advanceTurn() {
    state.turn = (state.turn + 1) % state.playerCount;
    state.sixStreak = 0;
}

Move Bot::choose(const State& s, const std::vector<Move>& moves) {
    if (moves.size() == 1) return moves.front();
    if (level == 0 && rng_.nextDouble() < 0.4) return moves[rng_.nextInt(static_cast<int>(moves.size()))];
    Move best = moves.front();
    double bestScore = -1e9;
    for (auto& m : moves) {
        double v = score(s, m);
        if (level == 0) v += rng_.nextDouble() * 25;
        else if (level == 1) v += rng_.nextDouble() * 8;
        if (v > bestScore) {
            bestScore = v;
            best = m;
        }
    }
    return best;
}

double Bot::score(const State& s, const Move& m) {
    double v = 0;
    if (!m.captures.empty()) {
        v += 100;
        for (auto& c : m.captures) v += c.from * 0.8;
    }
    if (m.finishes()) v += 70;
    State after = apply(s, m);
    bool dangerBefore = isOnTrack(m.from) && isThreatened(s, m.seat, m.from);
    bool dangerAfter = isOnTrack(m.to) && isThreatened(after, m.seat, m.to);
    if (dangerBefore && !dangerAfter) v += 45 + m.from * 0.4;
    if (dangerAfter) v -= 30 + m.to * 0.4;
    if (m.leavesBase()) {
        v += 18;
    } else {
        int steps = m.to - m.from;
        v += 10 + steps * 1.5;
        if (m.to > kLastTrack && m.from <= kLastTrack) v += 22;
        if (m.to >= 40) v += 6;
    }
    if (isOnTrack(m.to) && isSafeGlobal(s.globalOf(m.seat, m.to))) v += 12;
    int exposed = 0;
    for (int t = 0; t < kTokens; ++t) {
        if (t == m.token) continue;
        int p = after.tokens[m.seat][t];
        if (isThreatened(after, m.seat, p)) ++exposed;
    }
    v -= exposed * 6;
    return v;
}

}  // namespace ludo
}  // namespace nx
