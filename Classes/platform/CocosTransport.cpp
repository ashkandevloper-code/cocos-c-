#include "platform/CocosTransport.h"

#include "cocos2d.h"
#include "network/WebSocket.h"

#include <curl/curl.h>

USING_NS_CC;

namespace nx {

// ---------------- CurlHttp ----------------

namespace {
size_t writeCb(char* ptr, size_t size, size_t nmemb, void* user) {
    static_cast<std::string*>(user)->append(ptr, size * nmemb);
    return size * nmemb;
}
}  // namespace

std::string CurlHttp::caBundlePath() {
    static std::string path;
    static std::once_flag once;
    std::call_once(once, [] {
        auto fu = FileUtils::getInstance();
        std::string dst = fu->getWritablePath() + "cacert.pem";
        if (!fu->isFileExist(dst)) {
            Data d = fu->getDataFromFile("cacert.pem");  // from APK assets
            if (!d.isNull()) fu->writeDataToFile(d, dst);
        }
        path = fu->isFileExist(dst) ? dst : "";
    });
    return path;
}

CurlHttp::CurlHttp(int workers) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    caBundlePath();  // resolve on the cocos thread (FileUtils is not thread-safe on Android)
    for (int i = 0; i < workers; ++i) threads_.emplace_back([this] { loop(); });
}

CurlHttp::~CurlHttp() {
    stop_ = true;
    cv_.notify_all();
    for (auto& t : threads_) if (t.joinable()) t.join();
}

void CurlHttp::send(const HttpRequest& req, HttpCallback cb) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        jobs_.push_back({req, std::move(cb)});
    }
    cv_.notify_one();
}

void CurlHttp::loop() {
    const std::string ca = caBundlePath();
    while (!stop_) {
        Job job;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [this] { return stop_ || !jobs_.empty(); });
            if (stop_) return;
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        HttpResponse resp;
        CURL* curl = curl_easy_init();
        if (!curl) {
            resp.error = "curl_init";
        } else {
            struct curl_slist* hdrs = nullptr;
            for (auto& h : job.req.headers) hdrs = curl_slist_append(hdrs, (h.first + ": " + h.second).c_str());
            hdrs = curl_slist_append(hdrs, "Expect:");
            curl_easy_setopt(curl, CURLOPT_URL, job.req.url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, job.req.method.c_str());
            if (job.req.method != "GET" && job.req.method != "DELETE") {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, job.req.body.data());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(job.req.body.size()));
            }
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
            if (!ca.empty()) {
                curl_easy_setopt(curl, CURLOPT_CAINFO, ca.c_str());
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            }
            CURLcode rc = curl_easy_perform(curl);
            if (rc == CURLE_OK) {
                long code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
                resp.status = code;
            } else {
                resp.status = 0;
                resp.error = curl_easy_strerror(rc);
                CCLOG("NEXORA http %s failed: %s", job.req.tag.c_str(), resp.error.c_str());
            }
            curl_slist_free_all(hdrs);
            curl_easy_cleanup(curl);
        }
        auto cb = std::move(job.cb);
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([cb, resp]() {
            if (cb) cb(resp);
        });
    }
}

// ---------------- CocosWebSocket ----------------

struct CocosWebSocket::Impl : public network::WebSocket::Delegate {
    network::WebSocket* ws = nullptr;
    Listener l;
    bool open = false;
    bool detached = false;  // owner gone: swallow callbacks, free socket on close
    std::shared_ptr<Impl> self;  // keeps the delegate alive until onClose

    void onOpen(network::WebSocket*) override {
        open = true;
        if (!detached && l.onOpen) l.onOpen();
    }
    void onMessage(network::WebSocket*, const network::WebSocket::Data& d) override {
        if (detached || !l.onMessage) return;
        l.onMessage(std::string(d.bytes, d.len));
    }
    void onClose(network::WebSocket* s) override {
        open = false;
        bool notify = !detached;
        auto keep = self;  // may drop the last reference below
        self.reset();
        ws = nullptr;
        if (notify && l.onClose) l.onClose();
        // cocos2d-x contract: the delegate frees the socket in onClose (see cocos tests/WebSocketTest).
        delete s;
    }
    void onError(network::WebSocket*, const network::WebSocket::ErrorCode& e) override {
        if (!detached && l.onError) l.onError("ws error " + std::to_string(static_cast<int>(e)));
    }
};

CocosWebSocket::CocosWebSocket() : impl_(std::make_shared<Impl>()) {}

CocosWebSocket::~CocosWebSocket() {
    impl_->detached = true;
    if (impl_->ws) impl_->ws->closeAsync();
}

bool CocosWebSocket::open(const std::string& url, Listener listener) {
    impl_->l = std::move(listener);
    impl_->ws = new network::WebSocket();
    impl_->self = impl_;
    std::string ca = CurlHttp::caBundlePath();
    if (!impl_->ws->init(*impl_, url, nullptr, ca)) {
        impl_->self.reset();
        delete impl_->ws;
        impl_->ws = nullptr;
        return false;
    }
    return true;
}

void CocosWebSocket::sendText(const std::string& text) {
    if (impl_->ws && impl_->open) impl_->ws->send(text);
}

void CocosWebSocket::close() {
    impl_->detached = true;
    if (impl_->ws) impl_->ws->closeAsync();
}

bool CocosWebSocket::isOpen() const { return impl_->open; }

// ---------------- UserDefaultStore ----------------

std::string UserDefaultStore::getString(const std::string& k, const std::string& def) {
    return UserDefault::getInstance()->getStringForKey(k.c_str(), def);
}
void UserDefaultStore::setString(const std::string& k, const std::string& v) {
    UserDefault::getInstance()->setStringForKey(k.c_str(), v);
    UserDefault::getInstance()->flush();
}
void UserDefaultStore::remove(const std::string& k) { UserDefault::getInstance()->deleteValueForKey(k.c_str()); }

}  // namespace nx
