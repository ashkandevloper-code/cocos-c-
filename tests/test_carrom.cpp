#include "golden_util.h"

#include "games/carrom/Carrom.h"

using namespace nx;
using namespace nx::carrom;

static std::string listStr(const std::vector<int>& v) { return "[" + join(v, ", ") + "]"; }

TEST(carrom_matches_dart_golden) {
    std::string out;
    struct C { BotLevel a, b; int seed; } cfgs[] = {{BotLevel::Hard, BotLevel::Normal, 3}, {BotLevel::Easy, BotLevel::Hard, 8}};
    for (auto& c : cfgs) {
        Match m = Match::newBoard();
        std::vector<Bot> bots = {Bot(0, c.a, c.seed), Bot(1, c.b, c.seed + 1)};
        int shots = 0;
        while (m.phase != Phase::Finished && shots < 40) {
            Bot& b = bots[m.turn];
            Shot s = b.choose(m);
            s = b.withExecutionNoise(s);
            int t = m.turn;
            TurnOutcome o = m.playShot(s);
            out += "cr " + std::to_string(c.seed) + " #" + std::to_string(shots) + " t" + std::to_string(t) + " place" + fx(s.placeX, 6) +
                   " dir" + fx(s.dirX, 6) + "," + fx(s.dirY, 6) + " pw" + fx(s.power, 6) + " st" + std::to_string(o.steps) + " own" +
                   std::to_string(o.ownPocketed) + " opp" + std::to_string(o.oppPocketed) + " q" + B(o.queenPocketed) + " foul" + B(o.foul) +
                   " cov" + B(o.queenCovered) + " ret" + listStr(o.returnedIds) + " cont" + B(o.continueTurn) + " h" + std::to_string(m.hash()) + "\n";
            ++shots;
        }
        out += "cr " + std::to_string(c.seed) + " end winner " + (m.hasWinner ? std::to_string(m.winner) : "null") + " pts [" +
               std::to_string(m.points[0]) + ", " + std::to_string(m.points[1]) + "] rem " + std::to_string(m.remaining(0)) + "/" +
               std::to_string(m.remaining(1)) + " json " + m.toJson()["queenCoveredBy"].str() + "\n";
        // JSON round trip keeps the hash.
        Match back = Match::fromJson(Json::parse(m.toJson().dump()));
        CHECK_EQ(back.hash(), m.hash());
    }
    Match m2 = Match::newBoard();
    m2.passTurn();
    out += "pass " + std::to_string(m2.turn) + " " + std::to_string(m2.turnNumber) + " " + std::to_string(stateHash(initialBodies())) + "\n";
    CHECK_GOLDEN(out, "carrom.txt");
}
