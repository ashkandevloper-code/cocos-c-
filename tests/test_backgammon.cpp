#include "golden_util.h"

#include "games/backgammon/Backgammon.h"

using namespace nx;

TEST(backgammon_matches_dart_golden) {
    std::string out;
    char buf[256];
    for (int seed : {5, 77}) {
        bg::Dice dice(seed);
        std::vector<bg::Bot> bots = {bg::Bot(seed == 5 ? 2 : 0, seed), bg::Bot(1, seed + 1)};
        bg::State s = bg::State::initial();
        int p = 0, turns = 0;
        while (s.winner() < 0 && turns < 400) {
            int d0 = dice.roll();
            int d1 = dice.roll();
            std::vector<int> d = {d0, d1};
            auto plays = bg::enumeratePlays(s, p, d);
            auto moves = bots[p].choose(s, p, d);
            for (auto& m : moves) s = bg::apply(s, p, m);
            std::string ms;
            for (size_t i = 0; i < moves.size(); ++i) ms += (i ? " " : "") + moves[i].str();
            std::snprintf(buf, sizeof buf, "bg %d t%d p%d d%d,%d plays%d ", seed, turns, p, d0, d1, (int)plays.size());
            out += buf + ms + " ev" + fx(bg::Bot::evaluate(s, p), 4) + " pip" + std::to_string(s.pip(0)) + "/" + std::to_string(s.pip(1)) + "\n";
            p = 1 - p;
            ++turns;
        }
        int w = s.winner();
        out += "bg " + std::to_string(seed) + " winner " + (w < 0 ? std::string("null") : std::to_string(w)) + " mult " +
               std::to_string(w < 0 ? 0 : bg::resultMultiplier(s, w)) + " key " + s.key() + "\n";
    }
    bg::State st = bg::State::initial();
    bg::TurnTracker tt(st, 0, {6, 5});
    out += "tt total " + std::to_string(tt.total) + " force " + (tt.forceDie ? std::to_string(tt.forceDie) : "null") + " legal " +
           std::to_string(tt.legalMoves().size()) + "\n";
    bg::Move first = tt.legalMoves().front();
    tt.play(first);
    out += "tt after [" + tt.played[0].str() + "] remaining [" + join(tt.remaining(), ", ") + "] legal " + std::to_string(tt.legalMoves().size()) + "\n";
    tt.undo();
    out += "tt undo " + std::to_string(tt.played.size()) + " " + tt.state.key() + "\n";
    CHECK_GOLDEN(out, "backgammon.txt");
}
