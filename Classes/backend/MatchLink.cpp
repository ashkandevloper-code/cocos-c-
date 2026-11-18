#include "backend/MatchLink.h"

#include <algorithm>
#include <cstdio>

namespace nx {

int MatchEnd::forfeitSeat() const {
    if (!isForfeit()) return -1;
    size_t p = reason.rfind(':');
    if (p == std::string::npos) return -1;
    try { return std::stoi(reason.substr(p + 1)); } catch (...) { return -1; }
}

MatchEnd MatchEnd::fromRow(const Json& m) {
    MatchEnd e;
    for (auto& w : m["winner_seats"].items()) e.winners.push_back(static_cast<int>(w.asInt()));
    e.reason = m["reason"].isNull() ? "finished" : m["reason"].str();
    for (auto& s : m["scores"].items()) e.scores.push_back(static_cast<int>(s.asInt()));
    return e;
}

void SupabaseMatchLink::open(SupabaseClient* games, RealtimeClient* rt, const std::string& matchId, const std::string& uid,
                             std::function<void(std::shared_ptr<SupabaseMatchLink>, const std::string&)> cb) {
    games->from("nx_matches").select().eq("id", matchId).single([=](const SbResult& r) {
        if (!r.ok || !r.data.isObject()) {
            cb(nullptr, r.error.empty() ? "match_not_found" : r.error);
            return;
        }
        auto link = std::make_shared<SupabaseMatchLink>(games, rt, matchId, uid, r.data);
        link->connect();
        cb(link, "");
    });
}

SupabaseMatchLink::SupabaseMatchLink(SupabaseClient* c, RealtimeClient* rt, const std::string& matchId, const std::string& uid,
                                     const Json& m)
    : c_(c), rt_(rt), matchId_(matchId), uid_(uid), alive_(std::make_shared<bool>(true)) {
    for (auto& p : m["players"].items()) players_.push_back(p.str());
    for (auto& n : m["names"].items()) names_.push_back(n.str());
    while (names_.size() < players_.size()) names_.push_back("Pilot " + std::to_string(names_.size() + 1));
    seed_ = m["seed"].asInt();
    game_ = m["game"].str();
    roomId_ = m["room_id"].isString() ? m["room_id"].asString() : "";
    settings_ = m["settings"].isObject() ? m["settings"] : Json::object();
    auto it = std::find(players_.begin(), players_.end(), uid_);
    mySeat_ = it == players_.end() ? -1 : static_cast<int>(it - players_.begin());
    status_ = m["status"].str();
    if (status_ != "playing") end_ = MatchEnd::fromRow(m);
}

SupabaseMatchLink::~SupabaseMatchLink() { close(); }

bool SupabaseMatchLink::isAuthority() const {
    std::vector<int> alive;
    for (int s = 0; s < seatCount(); ++s) {
        if (!forfeited_.count(s) && (connected_.count(s) || s == mySeat_)) alive.push_back(s);
    }
    if (alive.empty()) return mySeat_ == 0;
    return alive.front() == mySeat_;
}

void SupabaseMatchLink::notify() {
    if (closed_) return;
    auto cbs = changed_;
    for (auto& f : cbs) f();
}

void SupabaseMatchLink::connect() {
    if (!rt_) return;
    std::weak_ptr<bool> alive = alive_;
    ch_ = rt_->channel("nxm:" + matchId_, std::to_string(mySeat_), false, false);
    PostgresChangeSpec ins{"INSERT", "public", "nx_match_moves", "match_id=eq." + matchId_};
    PostgresChangeSpec upd{"UPDATE", "public", "nx_matches", "id=eq." + matchId_};
    ch_->onPostgresChanges(ins, [this, alive](const Json& rec, const Json&, const std::string&) {
           if (alive.lock()) onRow(rec);
       })
        .onPostgresChanges(upd, [this, alive](const Json& rec, const Json&, const std::string&) {
            if (alive.lock()) onMatch(rec);
        })
        .onBroadcast("live", [this, alive](const Json& p) {
            if (!alive.lock()) return;
            const Json& d = p["payload"].isObject() ? p["payload"] : p;
            NetLiveMessage msg;
            msg.seat = static_cast<int>(d["s"].asInt(-1));
            msg.event = d["e"].str();
            msg.data = d["d"].isObject() ? d["d"] : Json::object();
            auto ls = live_;
            for (auto& l : ls) l(msg);
        })
        .onPresenceSync([this, alive]() { if (alive.lock()) syncPresence(); })
        .subscribe([this, alive](RealtimeChannel::Status st, const std::string&) {
            if (!alive.lock()) return;
            if (st == RealtimeChannel::Status::Subscribed) {
                ch_->track(Json::obj({{"seat", mySeat_}, {"uid", uid_}}));
                catchUp();
            }
        });
}

void SupabaseMatchLink::syncPresence() {
    if (!ch_) return;
    std::set<int> now;
    for (auto& kv : ch_->presenceState()) {
        for (auto& meta : kv.second) {
            if (meta["seat"].isNumber()) now.insert(static_cast<int>(meta["seat"].asInt()));
        }
    }
    now.insert(mySeat_);
    if (now != connected_) {
        connected_ = now;
        for (int s : now) absentSince_.erase(s);
        notify();
    }
}

void SupabaseMatchLink::watchAbsent(int64_t nowMs) {
    if (isOver() || !isAuthority()) return;
    for (int s = 0; s < seatCount(); ++s) {
        if (s == mySeat_ || forfeited_.count(s) || connected_.count(s)) continue;
        auto it = absentSince_.find(s);
        if (it == absentSince_.end()) it = absentSince_.emplace(s, nowMs).first;
        int64_t limit = settings_["grace_sec"].isNumber() ? settings_["grace_sec"].asInt() : (s == turnSeat_ ? 40 : 90);
        if ((nowMs - it->second) / 1000 >= limit) {
            absentSince_.erase(it);
            forfeitSeat(s);
        }
    }
}

void SupabaseMatchLink::forfeitSeat(int seat) {
    std::weak_ptr<bool> alive = alive_;
    c_->rpc("nx_forfeit", Json::obj({{"p_match", matchId_}, {"p_seat", seat}}), [this, alive](const SbResult&) {
        if (alive.lock()) catchUp();
    });
}

void SupabaseMatchLink::onMatch(const Json& m) {
    std::string st = m["status"].str();
    if (st != "playing" && status_ == "playing") {
        status_ = st;
        end_ = MatchEnd::fromRow(m);
        notify();
    }
}

void SupabaseMatchLink::onRow(const Json& r) {
    int seq = static_cast<int>(r["seq"].asInt());
    if (rows_.count(seq) && !ownSeqs_.count(seq)) return;
    NetMove m;
    m.seq = seq;
    m.seat = static_cast<int>(r["seat"].asInt());
    m.kind = r["kind"].str();
    m.data = r["move"].isObject() ? r["move"] : Json::object();
    m.hash = r["hash"].isString() ? r["hash"].asString() : "";
    std::string by = r["by_user"].str();
    if (ownSeqs_.count(seq) && by != uid_) {
        // Our optimistic seq was taken by someone else: the phone must resync.
        rows_[seq] = m;
        rowUser_[seq] = by;
        server_.insert(seq);
        requestResync();
        return;
    }
    rows_[seq] = m;
    rowUser_[seq] = by;
    server_.insert(seq);
    if (m.isForfeit()) {
        int s = m.data["seat"].isNumber() ? static_cast<int>(m.data["seat"].asInt()) : m.seat;
        if (forfeited_.insert(s).second) notify();
    }
    drain();
}

void SupabaseMatchLink::drain() {
    if (replaying_) return;
    while (rows_.count(delivered_ + 1)) {
        ++delivered_;
        if (ownSeqs_.count(delivered_)) continue;
        NetMove m = rows_[delivered_];
        if (listener_) listener_(m);
    }
}

void SupabaseMatchLink::catchUp(std::function<void()> then) {
    if (closed_) { if (then) then(); return; }
    int from = 0;
    while (server_.count(from + 1)) ++from;
    std::weak_ptr<bool> alive = alive_;
    c_->from("nx_match_moves")
        .select("seq, seat, kind, move, hash, by_user")
        .eq("match_id", matchId_)
        .gt("seq", from)
        .order("seq")
        .get([this, alive, then](const SbResult& r) {
            if (!alive.lock()) return;
            if (r.ok) for (auto& row : r.data.items()) onRow(row);
            if (status_ == "playing") {
                c_->from("nx_matches").select("status, winner_seats, reason, scores").eq("id", matchId_).maybeSingle(
                    [this, alive, then](const SbResult& m) {
                        if (!alive.lock()) return;
                        if (m.ok && m.data.isObject()) onMatch(m.data);
                        if (then) then();
                    });
            } else if (then) {
                then();
            }
        });
}

void SupabaseMatchLink::requestResync() {
    ownSeqs_.clear();
    ++resync_;
    auto cbs = resyncCb_;
    for (auto& f : cbs) f();
}

void SupabaseMatchLink::listen(std::function<void(const NetMove&)> onMove, std::function<void()> onReplayDone) {
    listener_ = onMove;
    int gen = ++listenerGen_;
    delivered_ = 0;
    ownSeqs_.clear();
    replaying_ = true;
    std::weak_ptr<bool> alive = alive_;
    catchUp([this, alive, gen, onMove, onReplayDone]() {
        if (!alive.lock() || gen != listenerGen_) return;  // replaced by a newer game instance
        int seq = 1;
        while (rows_.count(seq)) {
            NetMove r = rows_[seq];
            r.replay = true;
            onMove(r);
            if (gen != listenerGen_) return;
            delivered_ = seq;
            ++seq;
        }
        replaying_ = false;
        if (onReplayDone) onReplayDone();
        drain();
    });
}

void SupabaseMatchLink::send(const Json& data, const std::string& kind, const std::string& hash, int asSeat,
                             std::function<void(bool)> done) {
    if (isOver()) { if (done) done(false); return; }
    int seq = delivered_ + 1;
    delivered_ = seq;
    ownSeqs_.insert(seq);
    int seat = asSeat >= 0 ? asSeat : mySeat_;
    NetMove m;
    m.seq = seq;
    m.seat = seat;
    m.kind = kind;
    m.data = data;
    m.hash = hash;
    rows_[seq] = m;
    rowUser_[seq] = uid_;
    sendQueue_.push_back({seq, seat, kind, data, hash, std::move(done), 0});
    pumpSend();
}

void SupabaseMatchLink::pumpSend() {
    if (sending_ || sendQueue_.empty() || closed_) return;
    sending_ = true;
    PendingSend& p = sendQueue_.front();
    Json params = Json::obj({{"p_match", matchId_}, {"p_seq", p.seq}, {"p_kind", p.kind}, {"p_move", p.data},
                             {"p_hash", p.hash.empty() ? Json() : Json(p.hash)}, {"p_seat", p.seat}});
    std::weak_ptr<bool> alive = alive_;
    c_->rpc("nx_push_move", params, [this, alive](const SbResult& r) {
        if (!alive.lock() || sendQueue_.empty()) return;
        PendingSend& p = sendQueue_.front();
        auto finish = [this, alive](bool ok) {
            if (!alive.lock() || sendQueue_.empty()) return;
            PendingSend done = std::move(sendQueue_.front());
            sendQueue_.pop_front();
            sending_ = false;
            if (done.done) done.done(ok);
            pumpSend();
        };
        if (r.ok && r.data["ok"].asBool(false)) { finish(true); return; }
        if (r.ok && r.data["reason"].str() == "seq") {
            // Someone else's move landed first (e.g. a timeout): replay from the log.
            int seq = p.seq;
            catchUp([this, alive, seq, finish]() {
                if (!alive.lock()) return;
                if (rowUser_[seq] != uid_) requestResync();
                finish(false);
            });
            return;
        }
        if (r.ok) {  // refused for another reason: give up
            int seq = p.seq;
            catchUp([this, alive, seq, finish]() {
                if (!alive.lock()) return;
                if (!isOver() && rowUser_[seq] != uid_) requestResync();
                finish(false);
            });
            return;
        }
        // Transport failure: retry up to 3 times with 600 ms * attempt back-off (driven by tick()).
        if (++p.attempt < 3) {
            sending_ = false;
            sendRetryAt_ = c_->nowSec() * 1000 + 600 * p.attempt;
            sending_ = true;  // hold the queue until tick() fires the retry
            return;
        }
        int seq = p.seq;
        catchUp([this, alive, seq, finish]() {
            if (!alive.lock()) return;
            if (!isOver() && rowUser_[seq] != uid_) requestResync();
            finish(false);
        });
    });
}

void SupabaseMatchLink::sendLive(const std::string& event, const Json& data) {
    if (!ch_) return;
    ch_->sendBroadcast("live", Json::obj({{"s", mySeat_}, {"e", event}, {"d", data}}));
}

void SupabaseMatchLink::putSecret(int seat, const std::string& slot, const Json& data) {
    if (seat < 0 || seat >= seatCount()) return;
    c_->rpc("nx_put_secret",
            Json::obj({{"p_match", matchId_}, {"p_user", players_[seat]}, {"p_slot", slot}, {"p_data", data}}), nullptr);
}

void SupabaseMatchLink::readSecret(const std::string& slot, std::function<void(const Json&)> cb) {
    c_->from("nx_match_secrets").select("data").eq("match_id", matchId_).eq("user_id", uid_).eq("slot", slot).maybeSingle(
        [cb](const SbResult& r) { cb(r.ok && r.data["data"].isObject() ? r.data["data"] : Json()); });
}

void SupabaseMatchLink::verify(int seq, const std::string& expected, const std::string& actual) {
    if (expected.empty() || expected == actual) return;
    ++desyncs_;
    std::fprintf(stderr, "NEXORA desync at seq %d: expected %s got %s\n", seq, expected.c_str(), actual.c_str());
}

void SupabaseMatchLink::report(const std::vector<int>& winners, const std::vector<int>& scores, const std::string& hash,
                               std::function<void(bool)> cb) {
    std::weak_ptr<bool> alive = alive_;
    Json params = Json::obj({{"p_match", matchId_}, {"p_winners", Json::arr(winners)},
                             {"p_scores", scores.empty() ? Json() : Json::arr(scores)},
                             {"p_hash", hash.empty() ? Json() : Json(hash)}, {"p_reason", "finished"}});
    c_->rpc("nx_finish_match", params, [this, alive, cb](const SbResult& r) {
        if (!alive.lock()) return;
        if (!r.ok) { if (cb) cb(false); return; }
        c_->from("nx_matches").select("status, winner_seats, reason, scores").eq("id", matchId_).maybeSingle(
            [this, alive, cb](const SbResult& m) {
                if (!alive.lock()) return;
                if (m.ok && m.data.isObject()) {
                    status_ = m.data["status"].str();
                    end_ = MatchEnd::fromRow(m.data);
                }
                if (cb) cb(true);
            });
    });
}

void SupabaseMatchLink::forfeit() {
    if (isOver()) return;
    c_->rpc("nx_forfeit", Json::obj({{"p_match", matchId_}}), nullptr);
}

void SupabaseMatchLink::close() {
    if (closed_) return;
    closed_ = true;
    listener_ = nullptr;
    *alive_ = false;
    alive_ = std::make_shared<bool>(false);
    if (rt_ && ch_) rt_->removeChannel(ch_);
    ch_.reset();
}

void SupabaseMatchLink::tick(int64_t nowMs) {
    if (closed_) return;
    if (nextPoll_ == 0) { nextPoll_ = nowMs + 4000; nextWatch_ = nowMs + 5000; }
    if (nowMs >= nextPoll_) { nextPoll_ = nowMs + 4000; catchUp(); }
    if (nowMs >= nextWatch_) { nextWatch_ = nowMs + 5000; watchAbsent(nowMs); }
    if (sendRetryAt_ > 0 && nowMs >= sendRetryAt_) {
        sendRetryAt_ = 0;
        sending_ = false;
        pumpSend();
    }
}

}  // namespace nx
