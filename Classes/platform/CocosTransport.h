// cocos2d-x implementations of the backend seams:
//  * CurlHttp: libcurl (bundled with cocos2d-x externals) on worker threads, callbacks on the cocos thread.
//    cocos2d::network::HttpClient cannot send PATCH, which PostgREST updates need, so we use curl directly.
//  * CocosWebSocket: cocos2d::network::WebSocket (libwebsockets) for Supabase Realtime.
//  * UserDefaultStore: cocos2d::UserDefault for settings + the persisted auth session.
#pragma once
#include "backend/Transport.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace nx {

class CurlHttp : public IHttp {
public:
    explicit CurlHttp(int workers = 3);
    ~CurlHttp() override;
    void send(const HttpRequest& req, HttpCallback cb) override;
    /// Path of the CA bundle copied to the writable dir (cacert.pem from Resources).
    static std::string caBundlePath();

private:
    struct Job { HttpRequest req; HttpCallback cb; };
    void loop();
    std::vector<std::thread> threads_;
    std::deque<Job> jobs_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
};

class CocosWebSocket : public IWebSocket {
public:
    CocosWebSocket();
    ~CocosWebSocket() override;
    bool open(const std::string& url, Listener listener) override;
    void sendText(const std::string& text) override;
    void close() override;
    bool isOpen() const override;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

class UserDefaultStore : public IKeyValueStore {
public:
    std::string getString(const std::string& k, const std::string& def = "") override;
    void setString(const std::string& k, const std::string& v) override;
    void remove(const std::string& k) override;
};

}  // namespace nx
