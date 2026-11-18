#include "golden_util.h"

#include "core/DartCollections.h"
#include "games/uno/Uno.h"

using namespace nx;

TEST(uno_and_dart_sort_match_dart_golden) {
    std::string out;
    DartRandom r0(9);
    std::vector<int> big;
    for (int i = 0; i < 200; ++i) big.push_back(r0.nextInt(17) * 1000 + i);
    dartSort(big, [](int a, int b) { return dartCompare(a / 1000, b / 1000); });
    out += "sort " + join(big) + "\n";
    for (int seed : {1, 42, 99}) {
        for (int pc : {2, 4}) {
            uno::Game g(pc, seed);
            DartRandom rng(seed + 1000);
            std::vector<uno::BotLevel> levels;
            for (int i = 0; i < pc; ++i) levels.push_back(static_cast<uno::BotLevel>((i + seed) % 3));
            int steps = 0;
            auto pre = [&](int seat) { return "uno " + std::to_string(seed) + "/" + std::to_string(pc) + " st" + std::to_string(steps) + " s" + std::to_string(seat); };
            while (!g.isOver() && steps < 2000) {
                int seat = g.current;
                auto lvl = levels[seat];
                if (g.canCallNx(seat) && rng.nextDouble() < uno::Bot::nxCallChance(lvl)) g.callNx(seat);
                auto mv = uno::Bot::decide(g, seat, lvl, rng);
                if (mv.draw) {
                    uno::Card c;
                    bool got = g.draw(seat, &c);
                    if (g.drawnCardId >= 0) {
                        if (uno::Bot::playDrawn(g, seat, lvl, rng)) {
                            uno::Card card = *g.cardInHand(seat, g.drawnCardId);
                            g.play(seat, card.id, card.isWild() ? uno::Bot::bestColor(g, seat, rng, card.id) : uno::Color::None);
                        } else {
                            g.pass(seat);
                        }
                    }
                    out += pre(seat) + " draw " + (got ? c.str() : "null") + "\n";
                } else {
                    bool ok = g.play(seat, mv.cardId, mv.color);
                    out += pre(seat) + " play " + std::to_string(mv.cardId) + " " + uno::colorName(mv.color) + " ok" + B(ok) + " top " +
                           g.topCard().str() + " ac" + uno::colorName(g.activeColor) + "\n";
                }
                for (int p = 0; p < pc; ++p) {
                    if (p != seat && g.vulnerable >= 0 && rng.nextDouble() < uno::Bot::catchChance(levels[p])) {
                        bool c = g.catchNx(p);
                        out += "uno catch " + std::to_string(p) + " -> " + B(c) + "\n";
                    }
                }
                ++steps;
            }
            out += "uno " + std::to_string(seed) + "/" + std::to_string(pc) + " winner " + (g.winner < 0 ? "null" : std::to_string(g.winner)) +
                   " score " + std::to_string(g.winner < 0 ? 0 : g.scoreFor(g.winner)) + " events " + std::to_string(g.events.size()) +
                   " last " + g.events.back().str() + "\n";
        }
    }
    CHECK_GOLDEN(out, "uno.txt");
}
