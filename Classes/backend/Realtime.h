// Supabase Realtime client (Phoenix channels protocol v1.0.0 over JSON).
// Supports what the Dart app uses: postgres_changes, broadcast, presence (track/sync/join/leave),
// heartbeat, access_token updates, auto-rejoin after reconnect. Drive it with tick(nowMs).
#pragma once
#include "backend/Transport.h"
#include "core/Json.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace nx {

struct PhxMessage {
    std::string topic, event, ref, joinRef;
    Json payload;
    std::string encode() const;
    static bool decode(const std::string& text, PhxMessage& out);
};

struct PostgresChangeSpec {
    std::string event;   // INSERT / UPDATE / DELETE / *
    std::string schema = "public";
    std::string table;
    std::string filter;  // e.g. "match_id=eq.<uuid>"
};

struct PresenceEntry {
    std::string key;
    std::vector<Json> metas;  // each meta = tracked payload + phx_ref
};

class RealtimeClient;

class RealtimeChannel {
public:
    enum class State { Closed, Joining, Joined, Errored, Leaving };
    enum class Status { Subscribed, ChannelError, TimedOut, Closed };

    const std::string& topic() const { return topic_; }
    State state() const { return state_; }

    RealtimeChannel& onPostgresChanges(const PostgresChangeSpec& spec, std::function<void(const Json& newRecord, const Json& oldRecord, const std::string& type)> cb);
    RealtimeChannel& onBroadcast(const std::string& event, std::function<void(const Json& payload)> cb);
    RealtimeChannel& onPresenceSync(std::function<void()> cb) { presenceSync_.push_back(std::move(cb)); return *this; }
    RealtimeChannel& onPresenceJoin(std::function<void(const std::string&, const std::vector<Json>&)> cb) { presenceJoin_.push_back(std::move(cb)); return *this; }
    RealtimeChannel& onPresenceLeave(std::function<void(const std::string&, const std::vector<Json>&)> cb) { presenceLeave_.push_back(std::move(cb)); return *this; }
    void subscribe(std::function<void(Status, const std::string& err)> cb = nullptr);

    void track(const Json& payload);
    void untrack();
    void sendBroadcast(const std::string& event, const Json& payload);
    void unsubscribe();

    /// Current presence state: key -> metas.
    const std::map<std::string, std::vector<Json>>& presenceState() const { return presence_; }

    // Internal (used by RealtimeClient).
    RealtimeChannel(RealtimeClient* c, std::string name, std::string presenceKey, bool broadcastSelf, bool broadcastAck);
    void handle(const PhxMessage& m);
    void rejoin();
    void onSocketClosed();
    Json joinPayload() const;
    const std::string& joinRef() const { return joinRef_; }

private:
    void applyPresenceState(const Json& state);
    void applyPresenceDiff(const Json& diff);
    void fireSync();

    RealtimeClient* c_;
    std::string topic_;
    std::string presenceKey_;
    bool bSelf_, bAck_;
    State state_ = State::Closed;
    std::string joinRef_;
    std::function<void(Status, const std::string&)> subCb_;
    bool wantJoined_ = false;
    Json pendingTrack_;
    bool hasPendingTrack_ = false;

    struct PgBinding {
        PostgresChangeSpec spec;
        std::function<void(const Json&, const Json&, const std::string&)> cb;
        int64_t serverId = -1;
    };
    std::vector<PgBinding> pg_;
    std::multimap<std::string, std::function<void(const Json&)>> broadcast_;
    std::vector<std::function<void()>> presenceSync_;
    std::vector<std::function<void(const std::string&, const std::vector<Json>&)>> presenceJoin_, presenceLeave_;
    std::map<std::string, std::vector<Json>> presence_;
};

class RealtimeClient {
public:
    RealtimeClient(std::string supabaseUrl, std::string anonKey, WebSocketFactory factory, Clock clock = nullptr);
    ~RealtimeClient();

    /// Websocket endpoint for the project (wss://<ref>.supabase.co/realtime/v1/websocket?apikey=..&vsn=1.0.0).
    std::string endpoint() const;
    void setAuth(const std::string& accessToken);
    const std::string& accessToken() const { return token_; }

    /// Topic is prefixed with "realtime:" like supabase-dart.
    std::shared_ptr<RealtimeChannel> channel(const std::string& name, const std::string& presenceKey = "",
                                             bool broadcastSelf = false, bool broadcastAck = false);
    void removeChannel(const std::shared_ptr<RealtimeChannel>& ch);

    void connect();
    void disconnect();
    bool connected() const { return ws_ && ws_->isOpen() && open_; }

    /// Heartbeat (25 s), reconnect back-off, join timeouts. Call every frame or at least every second.
    void tick(int64_t nowMs);

    // Internal
    std::string makeRef();
    void push(const PhxMessage& m);
    int64_t now() const { return clock_(); }

    // Test hooks
    void injectOpen();
    void injectMessage(const std::string& text);
    void injectClose();

private:
    void onOpen();
    void onMessage(const std::string& text);
    void onClose();

    std::string url_, anonKey_, token_;
    WebSocketFactory factory_;
    Clock clock_;
    std::unique_ptr<IWebSocket> ws_;
    bool open_ = false;
    bool wantConnected_ = false;
    int64_t ref_ = 0;
    std::vector<std::string> sendBuffer_;
    std::vector<std::shared_ptr<RealtimeChannel>> channels_;
    int64_t lastHeartbeat_ = 0;
    std::string pendingHeartbeatRef_;
    int64_t reconnectAt_ = 0;
    int reconnectTries_ = 0;
};

}  // namespace nx
