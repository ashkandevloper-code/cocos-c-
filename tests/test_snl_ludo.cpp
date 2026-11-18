#include "golden_util.h"

#include "games/ludo/LudoRules.h"
#include "games/snakes/SnlRules.h"

using namespace nx;

TEST(snl_layout_valid) { CHECK(snl::isValidLayout()); }

TEST(snl_and_ludo_match_dart_golden) {
    std::string out;
    char buf[512];
    for (int seed : {1, 7, 2024}) {
        snl::Engine e(3, seed);
        int n = 0;
        while (e.winner < 0 && n < 500) {
            auto r = e.move(e.rollDie());
            std::snprintf(buf, sizeof buf, "snl %d s%d r%d %d>%d>%d %s w%s x%s\n", seed, r.seat, r.roll, r.from, r.landed, r.finalPos,
                          join(r.steps).c_str(), B(r.won), B(r.extraTurn));
            out += buf;
            ++n;
        }
    }
    for (int seed : {3, 11}) {
        for (int pc : {2, 4}) {
            ludo::Engine e(pc, seed);
            std::vector<ludo::Bot> bots;
            for (int i = 0; i < pc; ++i) bots.emplace_back(i % 3, seed * 10 + i);
            int n = 0;
            while (e.state.winner < 0 && n < 3000) {
                int seat = e.state.turn;
                auto rr = e.roll();
                std::snprintf(buf, sizeof buf, "ludo %d/%d s%d r%d m%d f%s a%s", seed, pc, seat, rr.roll, (int)rr.moves.size(),
                              B(rr.forfeited), B(rr.rollAgain));
                std::string line = buf;
                if (!rr.moves.empty()) {
                    auto m = bots[seat].choose(e.state, rr.moves);
                    auto p = e.play(m);
                    line += " " + m.str() + " e" + B(p.extraTurn) + " w" + B(p.won) + " sc" + fx(ludo::Bot::score(e.state, m), 3);
                }
                out += line + "\n";
                ++n;
            }
            std::string toks = "[";
            for (int s = 0; s < pc; ++s) {
                toks += s ? ", [" : "[";
                for (int t = 0; t < ludo::kTokens; ++t) toks += (t ? ", " : "") + std::to_string(e.state.tokens[s][t]);
                toks += "]";
            }
            toks += "]";
            out += "ludo " + std::to_string(seed) + "/" + std::to_string(pc) + " winner " + std::to_string(e.state.winner) + " tokens " + toks + "\n";
        }
    }
    CHECK_GOLDEN(out, "snl_ludo.txt");
}
