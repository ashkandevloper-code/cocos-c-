// Port of lib/games/uno/core/uno_models.dart, uno_engine.dart, uno_bot.dart (NX Cards, UNO-style).
#pragma once
#include "core/DartRandom.h"

#include <map>
#include <string>
#include <vector>

namespace nx {
namespace uno {

enum class Color { Turquoise = 0, Violet, Gold, Pink, None = -1 };
enum class Kind { Number = 0, Skip, Reverse, DrawTwo, Wild, WildDrawFour };
enum class BotLevel { Easy = 0, Normal, Hard };

const char* colorName(Color c);
const char* kindName(Kind k);

struct Card {
    int id = -1;
    Kind kind = Kind::Number;
    Color color = Color::None;
    int number = -1;
    bool isWild() const { return kind == Kind::Wild || kind == Kind::WildDrawFour; }
    bool isAction() const { return kind == Kind::Skip || kind == Kind::Reverse || kind == Kind::DrawTwo; }
    int points() const;
    bool sameFace(const Card& o) const;
    std::string str() const;
    bool operator==(const Card& o) const { return id == o.id; }
};

enum class EventType { Deal, Play, Draw, Skip, Reverse, ColorChosen, NxCall, NxCaught, Reshuffle, Win, Pass };
const char* eventName(EventType t);

struct Event {
    EventType type;
    int player = -1;
    bool hasCard = false;
    Card card;
    int count = 0;
    Color color = Color::None;
    std::string str() const;
};

std::vector<Card> buildDeck();

class Game {
public:
    Game(int playerCount, int64_t seed = 20251111, bool strictWildDrawFour = true, int handSize = 7);
    static Game fromState(const std::vector<std::vector<Card>>& hands, const std::vector<Card>& drawPile,
                          const std::vector<Card>& discard, Color activeColor, int current = 0, int direction = 1,
                          int64_t seed = 7, bool strictWildDrawFour = true);

    int playerCount;
    bool strictWildDrawFour;
    std::vector<std::vector<Card>> hands;
    std::vector<Card> drawPile, discard;
    Color activeColor = Color::Turquoise;
    std::vector<bool> nxCalled;
    int current = 0;
    int direction = 1;
    int drawnCardId = -1;
    int vulnerable = -1;
    int winner = -1;
    std::vector<Event> events;

    bool isOver() const { return winner >= 0; }
    const Card& topCard() const { return discard.back(); }
    int nextSeat(int from, int steps = 1) const;
    bool isLegal(int player, const Card& card) const;
    std::vector<Card> legalCards(int player) const;
    bool hasLegalPlay(int player) const { return !legalCards(player).empty(); }
    bool canDraw(int player) const { return !isOver() && player == current && drawnCardId < 0; }
    const Card* cardInHand(int player, int cardId) const;

    bool play(int player, int cardId, Color chosen = Color::None);
    bool draw(int player, Card* drawn = nullptr);  // returns false when nothing was drawn
    bool pass(int player);
    bool callNx(int player);
    bool canCallNx(int player) const;
    bool catchNx(int catcher);
    int handPoints(int player) const;
    int scoreFor(int player) const;
    std::map<Color, int> colorCounts(int player) const;

private:
    Game(int playerCount, int64_t seed, bool strict, bool raw);
    void onPlayerActs(int player);
    std::vector<Card> give(int player, int n);
    void reshuffle();
    DartRandom rng_;
};

struct BotMove {
    bool draw = true;
    int cardId = -1;
    Color color = Color::None;
};

struct Bot {
    static double nxCallChance(BotLevel l);
    static double catchChance(BotLevel l);
    static Color bestColor(const Game& g, int seat, DartRandom& rng, int excludingCardId = -1);
    static BotMove decide(const Game& g, int seat, BotLevel level, DartRandom& rng);
    static bool playDrawn(const Game& g, int seat, BotLevel level, DartRandom& rng);
    static Card normalPick(const Game& g, int seat, const std::vector<Card>& legal);
    static Card hardPick(const Game& g, int seat, const std::vector<Card>& legal);
};

}  // namespace uno
}  // namespace nx
