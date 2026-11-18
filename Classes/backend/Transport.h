// Engine-independent seams for the backend layer. The cocos2d-x app plugs in HttpClient / WebSocket /
// UserDefault implementations (Classes/platform/CocosTransport.*); unit tests plug in fakes.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nx {

struct HttpRequest {
    std::string method = "GET";  // GET POST PATCH PUT DELETE
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    std::string tag;  // for logs
    std::string header(const std::string& k) const {
        for (auto& h : headers) if (h.first == k) return h.second;
        return "";
    }
};

struct HttpResponse {
    long status = 0;   // 0 = transport failure
    std::string body;
    std::string error;  // transport error text
    bool ok() const { return status >= 200 && status < 300; }
};

using HttpCallback = std::function<void(const HttpResponse&)>;

class IHttp {
public:
    virtual ~IHttp() = default;
    /// Must invoke [cb] exactly once, on the thread that drives the app (cocos main thread).
    virtual void send(const HttpRequest& req, HttpCallback cb) = 0;
};

class IWebSocket {
public:
    struct Listener {
        std::function<void()> onOpen;
        std::function<void(const std::string&)> onMessage;
        std::function<void(const std::string&)> onError;
        std::function<void()> onClose;
    };
    virtual ~IWebSocket() = default;
    virtual bool open(const std::string& url, Listener listener) = 0;
    virtual void sendText(const std::string& text) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
};
using WebSocketFactory = std::function<std::unique_ptr<IWebSocket>()>;

class IKeyValueStore {
public:
    virtual ~IKeyValueStore() = default;
    virtual std::string getString(const std::string& k, const std::string& def = "") = 0;
    virtual void setString(const std::string& k, const std::string& v) = 0;
    virtual void remove(const std::string& k) = 0;
    bool getBool(const std::string& k, bool def) {
        std::string v = getString(k, "");
        if (v.empty()) return def;
        return v == "1" || v == "true";
    }
    void setBool(const std::string& k, bool v) { setString(k, v ? "1" : "0"); }
    int getInt(const std::string& k, int def) {
        std::string v = getString(k, "");
        if (v.empty()) return def;
        try { return std::stoi(v); } catch (...) { return def; }
    }
    void setInt(const std::string& k, int v) { setString(k, std::to_string(v)); }
};

class MemoryStore : public IKeyValueStore {
public:
    std::map<std::string, std::string> data;
    std::string getString(const std::string& k, const std::string& def = "") override {
        auto it = data.find(k);
        return it == data.end() ? def : it->second;
    }
    void setString(const std::string& k, const std::string& v) override { data[k] = v; }
    void remove(const std::string& k) override { data.erase(k); }
};

/// Wall clock in ms (injectable for tests).
using Clock = std::function<int64_t()>;
int64_t systemNowMs();

std::string urlEncode(const std::string& s);

}  // namespace nx
