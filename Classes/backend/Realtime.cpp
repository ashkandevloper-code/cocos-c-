#include "backend/Realtime.h"

#include <algorithm>

namespace nx {

// ---------------- PhxMessage ----------------

std::string PhxMessage::encode() const {
    Json j = Json::obj({{"topic", topic}, {"event", event}, {"payload", payload.isNull() ? Json::object() : payload},
                        {"ref", ref.empty() ? Json() : Json(ref)}});
    if (!joinRef.empty()) j.set("join_ref", joinRef);
    return j.dump();
}

bool PhxMessage::decode(const std::string& text, PhxMessage& out) {
    bool ok = false;
    Json j = Json::parse(text, &ok);
    if (!ok || !j.isObject()) return false;
    out.topic = j["topic"].asString();
    out.event = j["event"].asString();
    out.ref = j["ref"].isNull() ? "" : j["ref"].str();
    out.joinRef = j["join_ref"].isNull() ? "" : j["join_ref"].str();
    out.payload = j["payload"];
    return true;
}

// ---------------- RealtimeChannel ----------------

RealtimeChannel::RealtimeChannel(RealtimeClient* c, std::string name, std::string presenceKey, bool broadcastSelf,
                                 bool broadcastAck)
    : c_(c), topic_("realtime:" + name), presenceKey_(std::move(presenceKey)), bSelf_(broadcastSelf), bAck_(broadcastAck) {}

RealtimeChannel& RealtimeChannel::onPostgresChanges(
    const PostgresChangeSpec& spec, std::function<void(const Json&, const Json&, const std::string&)> cb) {
    pg_.push_back({spec, std::move(cb), -1});
    return *this;
}

RealtimeChannel& RealtimeChannel::onBroadcast(const std::string& event, std::function<void(const Json&)> cb) {
    broadcast_.emplace(event, std::move(cb));
    return *this;
}

Json RealtimeChannel::joinPayload() const {
    Json pgs = Json::array();
    for (auto& b : pg_) {
        Json e = Json::obj({{"event", b.spec.event}, {"schema", b.spec.schema}, {"table", b.spec.table}});
        if (!b.spec.filter.empty()) e.set("filter", b.spec.filter);
        pgs.push(e);
    }
    Json cfg = Json::obj({{"broadcast", Json::obj({{"ack", bAck_}, {"self", bSelf_}})},
                          {"presence", Json::obj({{"key", presenceKey_}})},
                          {"postgres_changes", pgs},
                          {"private", false}});
    Json p = Json::obj({{"config", cfg}});
    if (!c_->accessToken().empty()) p.set("access_token", c_->accessToken());
    return p;
}

void RealtimeChannel::subscribe(std::function<void(Status, const std::string&)> cb) {
    subCb_ = std::move(cb);
    wantJoined_ = true;
    c_->connect();
    rejoin();
}

void RealtimeChannel::rejoin() {
    if (!wantJoined_) return;
    state_ = State::Joining;
    joinRef_ = c_->makeRef();
    PhxMessage m;
    m.topic = topic_;
    m.event = "phx_join";
    m.payload = joinPayload();
    m.ref = joinRef_;
    m.joinRef = joinRef_;
    c_->push(m);
}

void RealtimeChannel::onSocketClosed() {
    if (state_ == State::Joined || state_ == State::Joining) state_ = State::Errored;
}

void RealtimeChannel::track(const Json& payload) {
    if (state_ != State::Joined) {
        pendingTrack_ = payload;
        hasPendingTrack_ = true;
        return;
    }
    PhxMessage m;
    m.topic = topic_;
    m.event = "presence";
    m.payload = Json::obj({{"type", "presence"}, {"event", "track"}, {"payload", payload}});
    m.ref = c_->makeRef();
    m.joinRef = joinRef_;
    c_->push(m);
    pendingTrack_ = payload;  // re-tracked after a rejoin
    hasPendingTrack_ = true;
}

void RealtimeChannel::untrack() {
    hasPendingTrack_ = false;
    if (state_ != State::Joined) return;
    PhxMessage m;
    m.topic = topic_;
    m.event = "presence";
    m.payload = Json::obj({{"type", "presence"}, {"event", "untrack"}});
    m.ref = c_->makeRef();
    m.joinRef = joinRef_;
    c_->push(m);
}

void RealtimeChannel::sendBroadcast(const std::string& event, const Json& payload) {
    if (state_ != State::Joined) return;  // supabase-dart falls back to REST; live messages are best effort
    PhxMessage m;
    m.topic = topic_;
    m.event = "broadcast";
    m.payload = Json::obj({{"type", "broadcast"}, {"event", event}, {"payload", payload}});
    m.ref = c_->makeRef();
    m.joinRef = joinRef_;
    c_->push(m);
}

void RealtimeChannel::unsubscribe() {
    wantJoined_ = false;
    if (state_ == State::Joined || state_ == State::Joining) {
        PhxMessage m;
        m.topic = topic_;
        m.event = "phx_leave";
        m.ref = c_->makeRef();
        m.joinRef = joinRef_;
        c_->push(m);
    }
    state_ = State::Closed;
}

void RealtimeChannel::fireSync() {
    auto cbs = presenceSync_;
    for (auto& f : cbs) f();
}

static std::vector<Json> metasOf(const Json& v) {
    std::vector<Json> out;
    for (auto& m : v["metas"].items()) out.push_back(m);
    return out;
}

void RealtimeChannel::applyPresenceState(const Json& state) {
    // Full state: joins = new keys / metas, leaves = keys no longer present.
    std::map<std::string, std::vector<Json>> next;
    for (auto& kv : state.entries()) next[kv.first] = metasOf(kv.second);
    for (auto& kv : next) {
        if (!presence_.count(kv.first)) for (auto& f : presenceJoin_) f(kv.first, kv.second);
    }
    for (auto& kv : presence_) {
        if (!next.count(kv.first)) for (auto& f : presenceLeave_) f(kv.first, kv.second);
    }
    presence_ = std::move(next);
    fireSync();
}

void RealtimeChannel::applyPresenceDiff(const Json& diff) {
    for (auto& kv : diff["joins"].entries()) {
        auto metas = metasOf(kv.second);
        auto& cur = presence_[kv.first];
        for (auto& m : metas) cur.push_back(m);
        for (auto& f : presenceJoin_) f(kv.first, metas);
    }
    for (auto& kv : diff["leaves"].entries()) {
        auto metas = metasOf(kv.second);
        auto it = presence_.find(kv.first);
        if (it != presence_.end()) {
            auto& cur = it->second;
            for (auto& m : metas) {
                std::string ref = m["phx_ref"].str();
                cur.erase(std::remove_if(cur.begin(), cur.end(), [&](const Json& x) { return x["phx_ref"].str() == ref; }),
                          cur.end());
            }
            if (cur.empty()) presence_.erase(it);
        }
        for (auto& f : presenceLeave_) f(kv.first, metas);
    }
    fireSync();
}

void RealtimeChannel::handle(const PhxMessage& m) {
    if (m.event == "phx_reply") {
        if (m.ref != joinRef_ || state_ != State::Joining) return;
        std::string status = m.payload["status"].asString();
        if (status == "ok") {
            state_ = State::Joined;
            // Server assigns ids to postgres_changes bindings in the same order we sent them.
            const Json& srv = m.payload["response"]["postgres_changes"];
            for (size_t i = 0; i < pg_.size() && i < srv.size(); ++i) pg_[i].serverId = srv[i]["id"].asInt(-1);
            if (hasPendingTrack_) {
                Json p = pendingTrack_;
                track(p);
            }
            if (subCb_) subCb_(Status::Subscribed, "");
        } else {
            state_ = State::Errored;
            if (subCb_) subCb_(Status::ChannelError, m.payload["response"].dump());
        }
        return;
    }
    if (m.event == "phx_error") {
        state_ = State::Errored;
        if (subCb_) subCb_(Status::ChannelError, "phx_error");
        return;
    }
    if (m.event == "phx_close") {
        state_ = State::Closed;
        if (subCb_) subCb_(Status::Closed, "");
        return;
    }
    if (m.event == "system") {
        if (m.payload["status"].asString() == "error" && subCb_) subCb_(Status::ChannelError, m.payload["message"].asString());
        return;
    }
    if (m.event == "postgres_changes") {
        const Json& data = m.payload["data"];
        std::string type = data["type"].asString();
        std::set<int64_t> ids;
        for (auto& id : m.payload["ids"].items()) ids.insert(id.asInt());
        for (auto& b : pg_) {
            bool idMatch = b.serverId >= 0 ? ids.count(b.serverId) > 0 : (b.spec.table == data["table"].asString());
            bool evMatch = b.spec.event == "*" || b.spec.event == type;
            if (idMatch && evMatch) b.cb(data["record"], data["old_record"], type);
        }
        return;
    }
    if (m.event == "broadcast") {
        std::string ev = m.payload["event"].asString();
        auto range = broadcast_.equal_range(ev);
        for (auto it = range.first; it != range.second; ++it) it->second(m.payload);
        return;
    }
    if (m.event == "presence_state") { applyPresenceState(m.payload); return; }
    if (m.event == "presence_diff") { applyPresenceDiff(m.payload); return; }
}

// ---------------- RealtimeClient ----------------

RealtimeClient::RealtimeClient(std::string supabaseUrl, std::string anonKey, WebSocketFactory factory, Clock clock)
    : url_(std::move(supabaseUrl)), anonKey_(std::move(anonKey)), factory_(std::move(factory)),
      clock_(clock ? std::move(clock) : Clock(systemNowMs)) {}

RealtimeClient::~RealtimeClient() {
    if (ws_) ws_->close();
}

std::string RealtimeClient::endpoint() const {
    std::string u = url_;
    if (u.rfind("https://", 0) == 0) u = "wss://" + u.substr(8);
    else if (u.rfind("http://", 0) == 0) u = "ws://" + u.substr(7);
    while (!u.empty() && u.back() == '/') u.pop_back();
    return u + "/realtime/v1/websocket?apikey=" + urlEncode(anonKey_) + "&vsn=1.0.0";
}

void RealtimeClient::setAuth(const std::string& accessToken) {
    if (token_ == accessToken) return;
    token_ = accessToken;
    for (auto& ch : channels_) {
        if (ch->state() != RealtimeChannel::State::Joined) continue;
        PhxMessage m;
        m.topic = ch->topic();
        m.event = "access_token";
        m.payload = Json::obj({{"access_token", token_}});
        m.ref = makeRef();
        m.joinRef = ch->joinRef();
        push(m);
    }
}

std::shared_ptr<RealtimeChannel> RealtimeClient::channel(const std::string& name, const std::string& presenceKey,
                                                         bool broadcastSelf, bool broadcastAck) {
    auto ch = std::make_shared<RealtimeChannel>(this, name, presenceKey, broadcastSelf, broadcastAck);
    channels_.push_back(ch);
    return ch;
}

void RealtimeClient::removeChannel(const std::shared_ptr<RealtimeChannel>& ch) {
    if (!ch) return;
    ch->unsubscribe();
    channels_.erase(std::remove(channels_.begin(), channels_.end(), ch), channels_.end());
    if (channels_.empty()) disconnect();
}

std::string RealtimeClient::makeRef() { return std::to_string(++ref_); }

void RealtimeClient::push(const PhxMessage& m) {
    std::string text = m.encode();
    if (connected()) ws_->sendText(text);
    else sendBuffer_.push_back(text);
}

void RealtimeClient::connect() {
    wantConnected_ = true;
    if (ws_ && (open_ || ws_->isOpen())) return;
    if (ws_) return;  // connecting
    if (!factory_) return;
    ws_ = factory_();
    if (!ws_) return;
    IWebSocket::Listener l;
    l.onOpen = [this]() { onOpen(); };
    l.onMessage = [this](const std::string& t) { onMessage(t); };
    l.onError = [this](const std::string&) { onClose(); };
    l.onClose = [this]() { onClose(); };
    if (!ws_->open(endpoint(), l)) {
        ws_.reset();
        reconnectAt_ = now() + 2000;
    }
}

void RealtimeClient::disconnect() {
    wantConnected_ = false;
    open_ = false;
    if (ws_) {
        auto w = std::move(ws_);
        w->close();
    }
    sendBuffer_.clear();
}

void RealtimeClient::onOpen() {
    open_ = true;
    reconnectTries_ = 0;
    lastHeartbeat_ = now();
    pendingHeartbeatRef_.clear();
    // Channels that were joined before a drop rejoin with a fresh join ref; the buffer then holds
    // only messages that were never tied to an old join.
    std::vector<std::string> buf;
    buf.swap(sendBuffer_);
    for (auto& ch : channels_) {
        if (ch->state() == RealtimeChannel::State::Errored) ch->rejoin();
    }
    for (auto& t : buf) ws_->sendText(t);
}

void RealtimeClient::onMessage(const std::string& text) {
    PhxMessage m;
    if (!PhxMessage::decode(text, m)) return;
    if (m.topic == "phoenix") {
        if (m.event == "phx_reply" && m.ref == pendingHeartbeatRef_) pendingHeartbeatRef_.clear();
        return;
    }
    auto chans = channels_;
    for (auto& ch : chans) {
        if (ch->topic() != m.topic) continue;
        // Ignore stale replies for an old join.
        if (!m.joinRef.empty() && m.event != "phx_reply" && m.joinRef != ch->joinRef()) continue;
        ch->handle(m);
    }
}

void RealtimeClient::onClose() {
    bool was = open_ || ws_ != nullptr;
    open_ = false;
    if (ws_) {
        auto w = std::move(ws_);
        (void)w;  // dropped; the platform socket deletes itself
    }
    for (auto& ch : channels_) ch->onSocketClosed();
    if (was && wantConnected_) {
        int backoff[] = {1000, 2000, 5000, 10000};
        reconnectAt_ = now() + backoff[std::min(reconnectTries_, 3)];
        ++reconnectTries_;
    }
}

void RealtimeClient::tick(int64_t nowMs) {
    if (!wantConnected_) return;
    if (!ws_ && reconnectAt_ > 0 && nowMs >= reconnectAt_) {
        reconnectAt_ = 0;
        connect();
        return;
    }
    if (connected() && nowMs - lastHeartbeat_ >= 25000) {
        if (!pendingHeartbeatRef_.empty()) {
            // Previous heartbeat never answered: the connection is dead.
            if (ws_) ws_->close();
            onClose();
            return;
        }
        lastHeartbeat_ = nowMs;
        PhxMessage hb;
        hb.topic = "phoenix";
        hb.event = "heartbeat";
        hb.ref = makeRef();
        pendingHeartbeatRef_ = hb.ref;
        ws_->sendText(hb.encode());
    }
}

void RealtimeClient::injectOpen() { onOpen(); }
void RealtimeClient::injectMessage(const std::string& text) { onMessage(text); }
void RealtimeClient::injectClose() { onClose(); }

}  // namespace nx
