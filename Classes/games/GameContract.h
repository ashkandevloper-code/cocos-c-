// Port of lib/games/core/game_contract.dart + net_link.dart: what every game receives and returns.
#pragma once
#include "core/Json.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace nx {

struct NetMove {
    int seq = 0;
    int seat = 0;
    std::string kind = "move";  // move | snap | timeout | forfeit | ...
    Json data;
    std::string hash;  // empty = none
    bool replay = false;
    bool isForfeit() const { return kind == "forfeit"; }
    bool isSnap() const { return kind == "snap"; }
    bool isTimeout() const { return kind == "timeout"; }
};

struct NetLiveMessage {
    int seat = -1;
    std::string event;
    Json data;
};

/// Ordered, replayable move log shared by every seat of an online match (Dart: NetLink).
class NetLink {
public:
    virtual ~NetLink() = default;
    virtual const std::string& matchId() const = 0;
    virtual int mySeat() const = 0;
    virtual int seatCount() const = 0;
    virtual int64_t seed() const = 0;
    virtual const Json& settings() const = 0;
    virtual const std::vector<std::string>& names() const = 0;
    virtual std::set<int> connectedSeats() const = 0;
    virtual std::set<int> forfeitedSeats() const = 0;
    virtual bool isAuthority() const = 0;
    virtual bool replaying() const = 0;
    virtual void listen(std::function<void(const NetMove&)> onMove, std::function<void()> onReplayDone = nullptr) = 0;
    virtual void send(const Json& data, const std::string& kind = "move", const std::string& hash = "", int asSeat = -1,
                      std::function<void(bool)> done = nullptr) = 0;
    virtual void sendLive(const std::string& event, const Json& data) = 0;
    virtual void onLive(std::function<void(const NetLiveMessage&)> cb) = 0;
    virtual void setTurnSeat(int seat) = 0;  // -1 = none
    virtual void putSecret(int seat, const std::string& slot, const Json& data) = 0;
    virtual void readSecret(const std::string& slot, std::function<void(const Json&)> cb) = 0;
    virtual void verify(int seq, const std::string& expected, const std::string& actual) = 0;
    virtual int desyncs() const = 0;
    /// Called when anything observable changed (presence, forfeits, end of match).
    virtual void onChanged(std::function<void()> cb) = 0;
};

/// FNV-1a over UTF-16 code units, low byte then high byte (identical to Dart hashOf()).
std::string hashOf(const std::string& utf8);

enum class GameMode { VsBot, Local, Online };
enum class BotDifficulty { Easy = 0, Normal = 1, Hard = 2 };
enum class GameOutcome { Win, Loss, Draw };

struct GameResult {
    GameOutcome outcome = GameOutcome::Draw;
    int score = 0;
    int opponentScore = 0;
    std::string summaryFa, summaryEn;
    std::vector<int> winnerSeats;
    std::vector<int> scores;
    std::string tournamentMatchId;
    int64_t durationMs = 0;
};

struct GameLaunch {
    std::string gameId;
    GameMode mode = GameMode::VsBot;
    BotDifficulty difficulty = BotDifficulty::Normal;
    std::string playerName = "Pilot";
    bool isFa = true;
    bool fancyEffects = true;
    bool reducedMotion = false;
    bool soundOn = true;
    int playerCount = 2;
    std::map<std::string, std::string> skins;
    bool hasSeed = false;
    int64_t seed = 0;
    NetLink* net = nullptr;  // not owned
    std::string tournamentMatchId;
    bool daily = false;
    std::function<void(const GameResult&)> onFinish;

    bool vsBot() const { return mode == GameMode::VsBot; }
    bool online() const { return net != nullptr; }
    int mySeat() const { return net ? net->mySeat() : 0; }
    bool controlsSeat(int seat) const { return net == nullptr || seat == net->mySeat(); }
    bool isRemoteSeat(int seat) const { return net != nullptr && seat != net->mySeat(); }
    std::string tx(const std::string& fa, const std::string& en) const { return isFa ? fa : en; }
};

}  // namespace nx
