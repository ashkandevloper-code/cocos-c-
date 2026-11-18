// Port of lib/core/online/match_link.dart (SupabaseMatchLink): ordered move log in nx_match_moves
// (insert via RPC nx_push_move, seq = last+1), delivery by Realtime postgres_changes + 4 s catch-up poll,
// presence + ephemeral 'live' broadcast on channel nxm:<matchId>, absent-seat forfeits by the authority.
#pragma once
#include "backend/Realtime.h"
#include "backend/SupabaseClient.h"
#include "games/GameContract.h"

#include <deque>
#include <map>
#include <memory>
#include <set>

namespace nx {

struct MatchEnd {
    std::vector<int> winners;
    std::string reason = "finished";
    std::vector<int> scores;
    bool isForfeit() const { return reason.rfind("forfeit", 0) == 0; }
    int forfeitSeat() const;
    static MatchEnd fromRow(const Json& m);
};

class SupabaseMatchLink : public NetLink {
public:
    /// Loads nx_matches row [matchId] then connects. [cb] receives nullptr on failure.
    static void open(SupabaseClient* games, RealtimeClient* rt, const std::string& matchId, const std::string& uid,
                     std::function<void(std::shared_ptr<SupabaseMatchLink>, const std::string& err)> cb);

    SupabaseMatchLink(SupabaseClient* c, RealtimeClient* rt, const std::string& matchId, const std::string& uid, const Json& row);
    ~SupabaseMatchLink() override;

    // NetLink
    const std::string& matchId() const override { return matchId_; }
    int mySeat() const override { return mySeat_; }
    int seatCount() const override { return static_cast<int>(players_.size()); }
    int64_t seed() const override { return seed_; }
    const Json& settings() const override { return settings_; }
    const std::vector<std::string>& names() const override { return names_; }
    std::set<int> connectedSeats() const override { return connected_; }
    std::set<int> forfeitedSeats() const override { return forfeited_; }
    bool isAuthority() const override;
    bool replaying() const override { return replaying_; }
    void listen(std::function<void(const NetMove&)> onMove, std::function<void()> onReplayDone = nullptr) override;
    void send(const Json& data, const std::string& kind = "move", const std::string& hash = "", int asSeat = -1,
              std::function<void(bool)> done = nullptr) override;
    void sendLive(const std::string& event, const Json& data) override;
    void onLive(std::function<void(const NetLiveMessage&)> cb) override { live_.push_back(std::move(cb)); }
    void setTurnSeat(int seat) override { turnSeat_ = seat; }
    void putSecret(int seat, const std::string& slot, const Json& data) override;
    void readSecret(const std::string& slot, std::function<void(const Json&)> cb) override;
    void verify(int seq, const std::string& expected, const std::string& actual) override;
    int desyncs() const override { return desyncs_; }
    void onChanged(std::function<void()> cb) override { changed_.push_back(std::move(cb)); }

    const std::string& game() const { return game_; }
    const std::string& roomId() const { return roomId_; }
    bool isOver() const { return status_ != "playing"; }
    const MatchEnd& end() const { return end_; }
    int resyncRequests() const { return resync_; }
    void onResync(std::function<void()> cb) { resyncCb_.push_back(std::move(cb)); }

    void report(const std::vector<int>& winners, const std::vector<int>& scores, const std::string& hash,
                std::function<void(bool)> cb);
    void forfeit();
    void close();

    /// Poll (4 s) + absent watch (5 s). Call every frame.
    void tick(int64_t nowMs);

    // Exposed for tests.
    void onRow(const Json& r);
    void onMatch(const Json& m);
    void catchUp(std::function<void()> then = nullptr);

private:
    void connect();
    void syncPresence();
    void watchAbsent(int64_t nowMs);
    void forfeitSeat(int seat);
    void drain();
    void requestResync();
    void notify();
    void pumpSend();

    SupabaseClient* c_;
    RealtimeClient* rt_;
    std::string matchId_, uid_;
    std::vector<std::string> players_, names_;
    int64_t seed_ = 0;
    std::string game_, roomId_;
    Json settings_;
    int mySeat_ = -1;
    std::string status_ = "playing";
    MatchEnd end_;

    std::shared_ptr<RealtimeChannel> ch_;
    std::shared_ptr<bool> alive_;
    int64_t nextPoll_ = 0, nextWatch_ = 0;
    bool closed_ = false;

    std::map<int, NetMove> rows_;
    std::map<int, std::string> rowUser_;
    std::set<int> server_;
    int delivered_ = 0;
    std::set<int> ownSeqs_;
    std::function<void(const NetMove&)> listener_;
    int listenerGen_ = 0;
    bool replaying_ = false;

    struct PendingSend {
        int seq; int seat; std::string kind; Json data; std::string hash; std::function<void(bool)> done; int attempt = 0;
    };
    std::deque<PendingSend> sendQueue_;
    bool sending_ = false;
    int64_t sendRetryAt_ = 0;

    std::set<int> connected_, forfeited_;
    std::map<int, int64_t> absentSince_;
    int turnSeat_ = -1;
    int desyncs_ = 0;
    int resync_ = 0;
    std::vector<std::function<void(const NetLiveMessage&)>> live_;
    std::vector<std::function<void()>> changed_, resyncCb_;
};

}  // namespace nx
