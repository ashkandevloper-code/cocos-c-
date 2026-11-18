#include "games/uno/Uno.h"

#include "core/DartCollections.h"

#include <algorithm>

namespace nx {
namespace uno {

static const Color kColors[4] = {Color::Turquoise, Color::Violet, Color::Gold, Color::Pink};

const char* colorName(Color c) {
    switch (c) {
        case Color::Turquoise: return "turquoise";
        case Color::Violet: return "violet";
        case Color::Gold: return "gold";
        case Color::Pink: return "pink";
        default: return "null";
    }
}
const char* kindName(Kind k) {
    switch (k) {
        case Kind::Number: return "number";
        case Kind::Skip: return "skip";
        case Kind::Reverse: return "reverse";
        case Kind::DrawTwo: return "drawTwo";
        case Kind::Wild: return "wild";
        default: return "wildDrawFour";
    }
}
const char* eventName(EventType t) {
    static const char* n[] = {"deal", "play", "draw", "skip", "reverse", "colorChosen", "nxCall", "nxCaught", "reshuffle", "win", "pass"};
    return n[static_cast<int>(t)];
}

int Card::points() const {
    switch (kind) {
        case Kind::Number: return number;
        case Kind::Skip:
        case Kind::Reverse:
        case Kind::DrawTwo: return 20;
        default: return 50;
    }
}

bool Card::sameFace(const Card& o) const {
    if (kind != o.kind) return false;
    if (kind == Kind::Number) return number == o.number;
    return true;
}

std::string Card::str() const {
    std::string c = color == Color::None ? "wild" : colorName(color);
    if (kind == Kind::Number) return c + "-" + std::to_string(number);
    return c + "-" + kindName(kind);
}

std::string Event::str() const {
    return std::string("UnoEvent(") + eventName(type) + ", p" + std::to_string(player) + ", " + (hasCard ? card.str() : "null") +
           ", x" + std::to_string(count) + ", " + colorName(color) + ")";
}

std::vector<Card> buildDeck() {
    std::vector<Card> deck;
    int id = 0;
    for (Color c : kColors) {
        deck.push_back({id++, Kind::Number, c, 0});
        for (int n = 1; n <= 9; ++n) {
            deck.push_back({id++, Kind::Number, c, n});
            deck.push_back({id++, Kind::Number, c, n});
        }
        for (int k = 0; k < 2; ++k) {
            deck.push_back({id++, Kind::Skip, c, -1});
            deck.push_back({id++, Kind::Reverse, c, -1});
            deck.push_back({id++, Kind::DrawTwo, c, -1});
        }
    }
    for (int k = 0; k < 4; ++k) deck.push_back({id++, Kind::Wild, Color::None, -1});
    for (int k = 0; k < 4; ++k) deck.push_back({id++, Kind::WildDrawFour, Color::None, -1});
    return deck;
}

static Event ev(EventType t, int player = -1, const Card* card = nullptr, int count = 0, Color color = Color::None) {
    Event e;
    e.type = t;
    e.player = player;
    if (card) { e.hasCard = true; e.card = *card; }
    e.count = count;
    e.color = color;
    return e;
}

Game::Game(int pc, int64_t seed, bool strict, bool) : playerCount(pc), strictWildDrawFour(strict), rng_(seed) {}

Game::Game(int pc, int64_t seed, bool strict, int handSize) : playerCount(pc), strictWildDrawFour(strict), rng_(seed) {
    hands.assign(playerCount, {});
    nxCalled.assign(playerCount, false);
    drawPile = buildDeck();
    dartShuffle(drawPile, rng_);
    for (int r = 0; r < handSize; ++r) {
        for (int p = 0; p < playerCount; ++p) {
            hands[p].push_back(drawPile.back());
            drawPile.pop_back();
        }
    }
    Card first = drawPile.back();
    drawPile.pop_back();
    int guard = 0;
    while (first.kind != Kind::Number && guard < 200) {
        drawPile.insert(drawPile.begin(), first);
        first = drawPile.back();
        drawPile.pop_back();
        ++guard;
    }
    discard = {first};
    activeColor = first.color == Color::None ? Color::Turquoise : first.color;
    events.push_back(ev(EventType::Deal));
}

Game Game::fromState(const std::vector<std::vector<Card>>& hands, const std::vector<Card>& drawPile,
                     const std::vector<Card>& discard, Color activeColor, int current, int direction, int64_t seed, bool strict) {
    Game g(static_cast<int>(hands.size()), seed, strict, true);
    g.hands = hands;
    g.drawPile = drawPile;
    g.discard = discard;
    g.activeColor = activeColor;
    g.current = current;
    g.direction = direction;
    g.nxCalled.assign(g.playerCount, false);
    return g;
}

int Game::nextSeat(int from, int steps) const {
    int s = (from + direction * steps) % playerCount;
    if (s < 0) s += playerCount;
    return s;
}

bool Game::isLegal(int player, const Card& card) const {
    if (card.isWild()) {
        if (card.kind == Kind::WildDrawFour && strictWildDrawFour) {
            for (auto& c : hands[player]) {
                if (c.id != card.id && !c.isWild() && c.color == activeColor) return false;
            }
        }
        return true;
    }
    if (card.color == activeColor) return true;
    const Card& top = topCard();
    if (top.isWild()) return false;
    return card.sameFace(top);
}

std::vector<Card> Game::legalCards(int player) const {
    std::vector<Card> out;
    if (isOver() || player != current) return out;
    for (auto& c : hands[player]) {
        if (drawnCardId >= 0 && c.id != drawnCardId) continue;
        if (isLegal(player, c)) out.push_back(c);
    }
    return out;
}

const Card* Game::cardInHand(int player, int cardId) const {
    for (auto& c : hands[player]) if (c.id == cardId) return &c;
    return nullptr;
}

bool Game::play(int player, int cardId, Color chosen) {
    if (isOver() || player != current) return false;
    const Card* cp = cardInHand(player, cardId);
    if (!cp) return false;
    Card card = *cp;
    if (drawnCardId >= 0 && drawnCardId != cardId) return false;
    if (!isLegal(player, card)) return false;
    if (card.isWild() && chosen == Color::None) return false;
    onPlayerActs(player);
    auto& h = hands[player];
    h.erase(std::remove_if(h.begin(), h.end(), [cardId](const Card& c) { return c.id == cardId; }), h.end());
    discard.push_back(card);
    drawnCardId = -1;
    events.push_back(ev(EventType::Play, player, &card));
    if (card.isWild()) {
        activeColor = chosen;
        events.push_back(ev(EventType::ColorChosen, player, nullptr, 0, chosen));
    } else {
        activeColor = card.color;
    }
    int left = static_cast<int>(h.size());
    if (left == 1) {
        if (!nxCalled[player]) vulnerable = player;
    } else if (left > 1) {
        nxCalled[player] = false;
    }
    bool won = left == 0;
    switch (card.kind) {
        case Kind::Number:
        case Kind::Wild:
            if (!won) current = nextSeat(player, 1);
            break;
        case Kind::Skip:
            events.push_back(ev(EventType::Skip, nextSeat(player, 1)));
            if (!won) current = nextSeat(player, 2);
            break;
        case Kind::Reverse:
            direction = -direction;
            events.push_back(ev(EventType::Reverse, player));
            if (!won) {
                if (playerCount == 2) {
                    events.push_back(ev(EventType::Skip, nextSeat(player, 1)));
                    current = player;
                } else {
                    current = nextSeat(player, 1);
                }
            }
            break;
        case Kind::DrawTwo: {
            int v2 = nextSeat(player, 1);
            give(v2, 2);
            events.push_back(ev(EventType::Skip, v2));
            if (!won) current = nextSeat(player, 2);
            break;
        }
        case Kind::WildDrawFour: {
            int v4 = nextSeat(player, 1);
            give(v4, 4);
            events.push_back(ev(EventType::Skip, v4));
            if (!won) current = nextSeat(player, 2);
            break;
        }
    }
    if (won) {
        winner = player;
        vulnerable = -1;
        events.push_back(ev(EventType::Win, player, nullptr, scoreFor(player)));
    }
    return true;
}

bool Game::draw(int player, Card* drawn) {
    if (!canDraw(player)) return false;
    onPlayerActs(player);
    auto got = give(player, 1);
    if (got.empty()) {
        events.push_back(ev(EventType::Pass, player));
        current = nextSeat(player, 1);
        return false;
    }
    Card c = got.front();
    if (isLegal(player, c)) {
        drawnCardId = c.id;
    } else {
        events.push_back(ev(EventType::Pass, player));
        current = nextSeat(player, 1);
    }
    if (drawn) *drawn = c;
    return true;
}

bool Game::pass(int player) {
    if (isOver() || player != current || drawnCardId < 0) return false;
    drawnCardId = -1;
    events.push_back(ev(EventType::Pass, player));
    current = nextSeat(player, 1);
    return true;
}

bool Game::callNx(int player) {
    if (isOver()) return false;
    int n = static_cast<int>(hands[player].size());
    if (n == 1 && vulnerable == player) {
        vulnerable = -1;
        nxCalled[player] = true;
        events.push_back(ev(EventType::NxCall, player));
        return true;
    }
    if (n == 2 && current == player && !nxCalled[player]) {
        nxCalled[player] = true;
        events.push_back(ev(EventType::NxCall, player));
        return true;
    }
    return false;
}

bool Game::canCallNx(int player) const {
    if (isOver()) return false;
    int n = static_cast<int>(hands[player].size());
    if (n == 1 && vulnerable == player) return true;
    return n == 2 && current == player && !nxCalled[player];
}

bool Game::catchNx(int catcher) {
    int v = vulnerable;
    if (isOver() || v < 0 || v == catcher) return false;
    vulnerable = -1;
    give(v, 2);
    events.push_back(ev(EventType::NxCaught, v, nullptr, 2));
    return true;
}

int Game::handPoints(int player) const {
    int s = 0;
    for (auto& c : hands[player]) s += c.points();
    return s;
}

int Game::scoreFor(int player) const {
    int s = 0;
    for (int p = 0; p < playerCount; ++p) if (p != player) s += handPoints(p);
    return s;
}

void Game::onPlayerActs(int player) {
    if (vulnerable >= 0 && vulnerable != player) vulnerable = -1;
}

std::vector<Card> Game::give(int player, int n) {
    std::vector<Card> got;
    for (int i = 0; i < n; ++i) {
        if (drawPile.empty()) reshuffle();
        if (drawPile.empty()) break;
        Card c = drawPile.back();
        drawPile.pop_back();
        hands[player].push_back(c);
        got.push_back(c);
    }
    if (!got.empty()) {
        events.push_back(ev(EventType::Draw, player, nullptr, static_cast<int>(got.size())));
        if (hands[player].size() > 1) nxCalled[player] = false;
        if (vulnerable == player && hands[player].size() > 1) vulnerable = -1;
    }
    return got;
}

void Game::reshuffle() {
    if (discard.size() <= 1) return;
    Card top = discard.back();
    discard.pop_back();
    std::vector<Card> rest = discard;
    discard.clear();
    discard.push_back(top);
    dartShuffle(rest, rng_);
    drawPile.insert(drawPile.begin(), rest.begin(), rest.end());
    events.push_back(ev(EventType::Reshuffle, -1, nullptr, static_cast<int>(rest.size())));
}

std::map<Color, int> Game::colorCounts(int player) const {
    std::map<Color, int> m;
    for (Color c : kColors) m[c] = 0;
    for (auto& card : hands[player]) if (card.color != Color::None) m[card.color] += 1;
    return m;
}

// ---------------- Bot ----------------

double Bot::nxCallChance(BotLevel l) { return l == BotLevel::Easy ? 0.7 : (l == BotLevel::Normal ? 0.9 : 1.0); }
double Bot::catchChance(BotLevel l) { return l == BotLevel::Easy ? 0.3 : (l == BotLevel::Normal ? 0.6 : 0.9); }

Color Bot::bestColor(const Game& g, int seat, DartRandom& rng, int excludingCardId) {
    std::map<Color, int> counts;
    for (Color c : kColors) counts[c] = 0;
    for (auto& card : g.hands[seat]) {
        if (card.id == excludingCardId) continue;
        if (card.color != Color::None) counts[card.color] += 1;
    }
    Color best = kColors[rng.nextInt(4)];
    int bestN = -1;
    for (Color c : kColors) {
        int n = counts[c];
        if (n > bestN) {
            best = c;
            bestN = n;
        }
    }
    return best;
}

BotMove Bot::decide(const Game& g, int seat, BotLevel level, DartRandom& rng) {
    auto legal = g.legalCards(seat);
    if (legal.empty()) return BotMove{};
    Card pick = legal.front();
    switch (level) {
        case BotLevel::Easy: pick = legal[rng.nextInt(static_cast<int>(legal.size()))]; break;
        case BotLevel::Normal: pick = normalPick(g, seat, legal); break;
        case BotLevel::Hard: pick = hardPick(g, seat, legal); break;
    }
    BotMove m;
    m.draw = false;
    m.cardId = pick.id;
    if (pick.isWild()) {
        m.color = level == BotLevel::Easy ? kColors[rng.nextInt(4)] : bestColor(g, seat, rng, pick.id);
    }
    return m;
}

bool Bot::playDrawn(const Game& g, int seat, BotLevel level, DartRandom& rng) {
    int id = g.drawnCardId;
    if (id < 0) return false;
    if (level == BotLevel::Easy) return rng.nextDouble() < 0.9;
    const Card* c = g.cardInHand(seat, id);
    if (!c) return false;
    if (level == BotLevel::Normal && c->kind == Kind::Wild && g.hands[seat].size() > 3) return false;
    return true;
}

Card Bot::normalPick(const Game& g, int seat, const std::vector<Card>& legal) {
    std::vector<Card> colorMatch;
    for (auto& c : legal) if (!c.isWild() && c.color == g.activeColor) colorMatch.push_back(c);
    if (!colorMatch.empty()) {
        dartSort(colorMatch, [](const Card& a, const Card& b) { return dartCompare(b.points(), a.points()); });
        return colorMatch.front();
    }
    std::vector<Card> faceMatch;
    for (auto& c : legal) if (!c.isWild()) faceMatch.push_back(c);
    if (!faceMatch.empty()) {
        auto counts = g.colorCounts(seat);
        dartSort(faceMatch, [&counts](const Card& a, const Card& b) { return dartCompare(counts[b.color], counts[a.color]); });
        return faceMatch.front();
    }
    for (auto& c : legal) if (c.kind == Kind::Wild) return c;
    return legal.front();
}

Card Bot::hardPick(const Game& g, int seat, const std::vector<Card>& legal) {
    int next = g.nextSeat(seat, 1);
    int nextCount = static_cast<int>(g.hands[next].size());
    int leader = -1, leaderCount = 999;
    for (int p = 0; p < g.playerCount; ++p) {
        if (p == seat) continue;
        if (static_cast<int>(g.hands[p].size()) < leaderCount) {
            leaderCount = static_cast<int>(g.hands[p].size());
            leader = p;
        }
    }
    bool threat = nextCount <= 2 || (next == leader && leaderCount <= 4);
    auto score = [&](const Card& c) {
        int s = 0;
        switch (c.kind) {
            case Kind::WildDrawFour: s = threat ? 120 : -40; break;
            case Kind::DrawTwo: s = threat ? 100 : 10; break;
            case Kind::Skip: s = threat ? 90 : 8; break;
            case Kind::Reverse: s = threat ? (g.playerCount > 2 ? 70 : 90) : 6; break;
            case Kind::Wild: s = -30; break;
            case Kind::Number: s = c.number; break;
        }
        if (!c.isWild() && c.color == g.activeColor) s += 4;
        if (c.color != Color::None) s += g.colorCounts(seat)[c.color] * 3;
        if (g.hands[seat].size() <= 2 && c.isWild()) s += 60;
        return s;
    };
    std::vector<Card> sorted = legal;
    dartSort(sorted, [&score](const Card& a, const Card& b) { return dartCompare(score(b), score(a)); });
    return sorted.front();
}

}  // namespace uno
}  // namespace nx
