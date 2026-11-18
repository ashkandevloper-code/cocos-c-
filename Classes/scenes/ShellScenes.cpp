#include "scenes/ShellScenes.h"

#include "app/NxApp.h"
#include "core/Worlds.h"
#include "ui/Widgets.h"

#include <regex>

USING_NS_CC;
using namespace nx::ui;

namespace nx {

// Forward declarations of world entry points living in other scene files.
cocos2d::Scene* createArcadeScene();
cocos2d::Scene* createWorldScene(const std::string& id);  // nullptr = not ported yet

// ---------------- Intro ----------------

bool IntroScene::init() {
    if (!Scene::init()) return false;
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    addChild(makeSpaceBackground(Size(vs.width + o.x * 2, vs.height + o.y * 2)));
    auto logo = makeImage("branding/nexora_logo.webp", Size(vs.width * 0.7f, vs.width * 0.7f));
    logo->setPosition(o.x + vs.width / 2, o.y + vs.height * 0.56f);
    logo->setOpacity(0);
    addChild(logo);
    logo->runAction(Sequence::create(FadeIn::create(0.8f), DelayTime::create(0.9f), nullptr));
    auto tag = makeText(tx("جهانی کیهانی و به‌هم‌پیوسته", "A connected cosmic universe"), 28, color::muted, Weight::Medium);
    tag->setPosition(o.x + vs.width / 2, o.y + vs.height * 0.3f);
    tag->setOpacity(0);
    addChild(tag);
    tag->runAction(Sequence::create(DelayTime::create(0.5f), FadeIn::create(0.6f), nullptr));
    runAction(Sequence::create(DelayTime::create(2.2f), CallFunc::create([] { IntroScene::next(); }), nullptr));
    auto tap = EventListenerTouchOneByOne::create();
    tap->onTouchBegan = [](Touch*, Event*) { return true; };
    tap->onTouchEnded = [this](Touch*, Event*) {
        stopAllActions();
        IntroScene::next();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(tap, this);
    return true;
}

void IntroScene::next() {
    static bool going = false;
    if (going) return;
    going = true;
    auto& app = NxApp::get();
    const std::string& mode = app.settings().authMode();
    Scene* s;
    if (mode.empty() && !app.online().isOnline()) s = AuthScene::create();
    else if (!app.settings().onboarded()) s = OnboardingScene::create();
    else s = HubScene::create();
    NxApp::replace(s);
    going = false;
}

// ---------------- Auth ----------------

bool AuthScene::init() {
    if (!Scene::init()) return false;
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    addChild(makeSpaceBackground(Size(vs.width + o.x * 2, vs.height + o.y * 2)));
    build();
    schedule(CC_SCHEDULE_SELECTOR(AuthScene::tickCooldown), 1.0f);
    return true;
}

void AuthScene::build() {
    if (form_) form_->removeFromParent();
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    form_ = Node::create();
    form_->setPosition(o);
    addChild(form_, 1);
    float cx = vs.width / 2;
    float y = vs.height - 140;
    auto logo = makeImage("branding/nexora_mark.webp", Size(170, 170));
    logo->setPosition(cx, y - 60);
    form_->addChild(logo);
    y -= 190;
    auto title = makeText(tx("ورود به NEXORA", "Sign in to NEXORA"), 40, color::text, Weight::Black);
    title->setPosition(cx, y);
    form_->addChild(title);
    y -= 60;
    auto sub = makeText(passwordMode_ ? tx("با ایمیل یا نام کاربری و رمز وارد شو.", "Sign in with e-mail or username and password.")
                                      : tx("ابتدا ایمیل واقعی‌ات را تأیید کن؛ سپس وارد حساب می‌شوی.",
                                           "Verify your real e-mail first; then you enter your account."),
                        24, color::muted, Weight::Regular, vs.width - 120);
    sub->setPosition(cx, y);
    form_->addChild(sub);
    y -= 90;
    const Size field(vs.width - 120, 84);
    std::string keepEmail = email_ ? email_->getText() : "";
    email_ = makeInput(field, passwordMode_ ? tx("ایمیل یا نام کاربری", "E-mail or username") : tx("ایمیل", "E-mail"), false,
                       passwordMode_ ? cocos2d::ui::EditBox::InputMode::SINGLE_LINE : cocos2d::ui::EditBox::InputMode::EMAIL_ADDRESS);
    email_->setText(keepEmail.c_str());
    email_->setPosition(Vec2(cx, y));
    form_->addChild(email_);
    y -= 104;
    code_ = nullptr;
    password_ = nullptr;
    if (passwordMode_) {
        password_ = makeInput(field, tx("رمز عبور", "Password"), true);
        password_->setPosition(Vec2(cx, y));
        form_->addChild(password_);
        y -= 110;
        auto go = NxButton::create(tx("ورود", "Sign in"), Size(field.width, 86), [this] { passwordSignIn(); });
        go->setPosition(cx, y);
        form_->addChild(go);
        y -= 100;
    } else {
        if (sent_) {
            code_ = makeInput(field, tx("کد ۶ رقمی", "6-digit code"), false, cocos2d::ui::EditBox::InputMode::NUMERIC);
            code_->setPosition(Vec2(cx, y));
            form_->addChild(code_);
            y -= 110;
            auto verify = NxButton::create(tx("تأیید و ورود", "Verify and enter"), Size(field.width, 86), [this] { verifyCode(); });
            verify->setPosition(cx, y);
            form_->addChild(verify);
            y -= 100;
        }
        auto send = NxButton::create(sent_ ? tx("ارسال دوباره‌ی کد", "Resend code") : tx("ارسال کد ورود", "Send sign-in code"),
                                     Size(field.width, 86), [this] { sendCode(); },
                                     sent_ ? ButtonStyle::Ghost : ButtonStyle::Primary);
        send->setPosition(cx, y);
        send->setName("send");
        form_->addChild(send);
        y -= 100;
    }
    auto toggle = NxButton::create(passwordMode_ ? tx("ورود با کد ایمیل", "Sign in with e-mail code")
                                                 : tx("ورود با رمز عبور", "Sign in with password"),
                                   Size(field.width, 70), [this] {
                                       passwordMode_ = !passwordMode_;
                                       setError("");
                                       build();
                                   }, ButtonStyle::Ghost);
    toggle->setPosition(cx, y);
    form_->addChild(toggle);
    y -= 90;
    error_ = makeText(" ", 24, color::danger, Weight::Bold, vs.width - 100);
    error_->setPosition(cx, y);
    form_->addChild(error_);
    y -= 50;
    info_ = makeText(" ", 22, color::muted, Weight::Regular, vs.width - 100);
    info_->setPosition(cx, y - 20);
    form_->addChild(info_);
    auto guest = NxButton::create(tx("ورود مهمان؛ فقط آفلاین", "Continue as Guest · offline only"), Size(field.width, 80),
                                  [this] { continueGuest(); }, ButtonStyle::Secondary);
    guest->setPosition(cx, 170);
    form_->addChild(guest);
    auto note = makeText(tx("مهمان به حساب آنلاین، چت، بازی آنلاین، تورنمنت و همگام‌سازی دسترسی ندارد.",
                            "Guests have no online account, chat, online games, tournaments or sync."),
                         20, color::muted, Weight::Regular, vs.width - 120);
    note->setPosition(cx, 90);
    form_->addChild(note);
    auto lang = NxButton::create(uiIsFa() ? "English" : "فارسی", Size(150, 60), [this] {
        auto& s = NxApp::get().settings();
        s.setLanguage(s.isFa() ? "en" : "fa");
        build();
    }, ButtonStyle::Ghost);
    lang->setPosition(vs.width - 100, vs.height - 60);
    form_->addChild(lang);
}

void AuthScene::setError(const std::string& e) { if (error_) ui::setText(error_, e.empty() ? " " : e, visibleSize().width - 100); }
void AuthScene::setInfo(const std::string& i) { if (info_) ui::setText(info_, i.empty() ? " " : i, visibleSize().width - 100); }

std::string AuthScene::friendly(const std::string& v, int retryAfter) {
    if (v == "network") return tx("اتصال اینترنت را بررسی کن.", "Check your internet connection.");
    if (v == "otp_invalid") return tx("کد نادرست است یا منقضی شده. کد تازه بگیر.", "The code is wrong or expired. Request a new one.");
    if (v == "invalid_login") return tx("ایمیل/نام کاربری یا رمز نادرست است.", "Wrong e-mail/username or password.");
    if (v == "rate_limit") {
        std::string s = retryAfter > 0 ? " (" + std::to_string(retryAfter) + " s)" : "";
        return tx("سرور فعلاً اجازه‌ی ارسال ایمیل دیگری نمی‌دهد" + s + ". کمی بعد دوباره امتحان کن یا فعلاً مهمان وارد شو.",
                  "The server is not allowing another email right now" + s + ". Try again a bit later, or continue as a guest for now.");
    }
    if (v == "smtp") return tx("سرور نتوانست ایمیل را بفرستد (تنظیمات ایمیل سرور). فعلاً مهمان وارد شو.",
                               "The server could not send the email (server mail settings). Continue as a guest for now.");
    if (v == "signup_disabled") return tx("ثبت‌نام حساب تازه روی سرور بسته است.", "New sign-ups are disabled on the server.");
    if (v == "not_configured") return tx("اتصال آنلاین تنظیم نشده است.", "Online authentication is not configured.");
    if (v.empty()) return tx("ارسال یا تأیید کد انجام نشد. دوباره تلاش کن.", "Could not send or verify the code. Try again.");
    return tx("خطای سرور: ", "Server error: ") + v;
}

void AuthScene::tickCooldown(float) {
    if (cooldown_ <= 0) return;
    --cooldown_;
    if (auto b = dynamic_cast<NxButton*>(form_->getChildByName("send"))) {
        if (cooldown_ > 0) b->setTitle(tx("ارسال دوباره (", "Resend (") + std::to_string(cooldown_) + "s)");
        else b->setTitle(tx("ارسال دوباره‌ی کد", "Resend code"));
        b->setEnabled(cooldown_ == 0);
    }
}

void AuthScene::sendCode() {
    if (busy_) return;
    std::string email = email_->getText();
    if (!std::regex_match(email, std::regex("^\\s*[^@\\s]+@[^@\\s]+\\.[^@\\s]+\\s*$"))) {
        setError(tx("ایمیل معتبر وارد کن.", "Enter a valid email address."));
        return;
    }
    auto& online = NxApp::get().online();
    if (!online.configured()) { setError(friendly("not_configured", -1)); return; }
    int wait = online.otpCooldownLeft(email);
    if (wait > 0) {
        cooldown_ = wait;
        setError(tx("برای ارسال دوباره " + std::to_string(wait) + " ثانیه صبر کن.", "Wait " + std::to_string(wait) + " s before requesting another code."));
        return;
    }
    busy_ = true;
    setError("");
    setInfo(tx("در حال ارسال…", "Sending…"));
    retain();
    online.sendEmailOtp(email, [this](bool ok) {
        busy_ = false;
        auto& on = NxApp::get().online();
        if (ok) {
            sent_ = true;
            build();
            cooldown_ = 60;
            setInfo(tx("ایمیل ارسال شد. کد را وارد کن. پوشه‌ی اسپم را هم ببین.", "Email sent. Enter the code. Check your spam folder too."));
        } else {
            setInfo("");
            setError(friendly(on.lastError(), on.lastRetryAfter()));
            if (on.lastError() == "rate_limit") cooldown_ = on.lastRetryAfter() > 0 ? on.lastRetryAfter() : 60;
        }
        release();
    });
}

void AuthScene::verifyCode() {
    if (busy_ || !code_) return;
    auto& online = NxApp::get().online();
    std::string code = OnlineService::normalizeEmailOtpDigits(code_->getText());
    if (!OnlineService::isSixDigitEmailOtp(code)) {
        setError(tx("کد ۶ رقمی ایمیل را وارد کن.", "Enter the 6-digit code from the email."));
        return;
    }
    busy_ = true;
    setError("");
    retain();
    online.verifyEmailOtp(email_->getText(), code, [this](bool ok) {
        busy_ = false;
        if (ok) afterSignIn();
        else setError(friendly(NxApp::get().online().lastError(), -1));
        release();
    });
}

void AuthScene::passwordSignIn() {
    if (busy_ || !password_) return;
    std::string id = email_->getText(), pw = password_->getText();
    if (id.empty() || pw.empty()) {
        setError(tx("ایمیل/نام کاربری و رمز را وارد کن.", "Enter e-mail/username and password."));
        return;
    }
    auto& online = NxApp::get().online();
    if (!online.configured()) { setError(friendly("not_configured", -1)); return; }
    busy_ = true;
    setError("");
    retain();
    online.signInEmail(id, pw, [this](bool ok) {
        busy_ = false;
        if (ok) afterSignIn();
        else setError(friendly(NxApp::get().online().lastError(), -1));
        release();
    });
}

void AuthScene::afterSignIn() {
    auto& app = NxApp::get();
    app.settings().setAuthMode("email");
    NxApp::replace(app.settings().onboarded() ? static_cast<Scene*>(HubScene::create()) : OnboardingScene::create());
}

void AuthScene::continueGuest() {
    auto& app = NxApp::get();
    app.settings().setAuthMode("guest");
    NxApp::replace(app.settings().onboarded() ? static_cast<Scene*>(HubScene::create()) : OnboardingScene::create());
}

// ---------------- Onboarding ----------------

bool OnboardingScene::init() {
    if (!Scene::init()) return false;
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    addChild(makeSpaceBackground(Size(vs.width + o.x * 2, vs.height + o.y * 2)));
    auto root = Node::create();
    root->setPosition(o);
    addChild(root);
    float cx = vs.width / 2;
    auto oracle = makeImage("characters/oracle/hero_orbit.webp", Size(360, 360));
    oracle->setPosition(cx, vs.height - 300);
    root->addChild(oracle);
    auto t = makeText(tx("سلام کاوشگر! من اوراکل هستم.", "Hi explorer! I'm ORACLE."), 36, color::text, Weight::Black, vs.width - 100);
    t->setPosition(cx, vs.height - 540);
    root->addChild(t);
    auto s = makeText(tx("اسمت در NEXORA چیست؟", "What's your name in NEXORA?"), 26, color::muted, Weight::Medium, vs.width - 100);
    s->setPosition(cx, vs.height - 610);
    root->addChild(s);
    auto name = makeInput(Size(vs.width - 120, 86), tx("نام نمایشی", "Display name"));
    name->setText(NxApp::get().playerName().c_str());
    name->setPosition(Vec2(cx, vs.height - 720));
    root->addChild(name);
    auto go = NxButton::create(tx("ورود به هاب", "Enter the Hub"), Size(vs.width - 120, 90), [name] {
        std::string n = name->getText();
        if (n.size() < 2) {
            toast(tx("یک نام حداقل دو حرفی وارد کن.", "Enter a name with at least 2 letters."), true);
            return;
        }
        auto& app = NxApp::get();
        app.setPlayerName(n);
        app.settings().setOnboarded(true);
        app.settings().setJourneySeen(true);
        NxApp::replace(HubScene::create());
    });
    go->setPosition(cx, vs.height - 850);
    root->addChild(go);
    return true;
}

// ---------------- Hub ----------------

bool HubScene::init() {
    if (!Scene::init()) return false;
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    addChild(makeSpaceBackground(Size(vs.width + o.x * 2, vs.height + o.y * 2)));
    auto root = Node::create();
    root->setPosition(o);
    addChild(root);

    // Header: avatar name + online state + settings.
    auto header = makePanel(Size(vs.width - 24, 120), color::f4(color::bgMid, 0.85f), color::f4(color::accent2, 0.7f), 30);
    header->setPosition(12, vs.height - 136);
    root->addChild(header);
    name_ = makeText(NxApp::get().playerName(), 32, color::text, Weight::Bold);
    name_->setPosition(vs.width / 2, vs.height - 58);
    root->addChild(name_);
    status_ = makeText(" ", 22, color::muted, Weight::Medium);
    status_->setPosition(vs.width / 2, vs.height - 102);
    root->addChild(status_);
    auto settings = NxButton::create(tx("تنظیمات", "Settings"), Size(150, 64), [] { NxApp::push(SettingsScene::create()); },
                                     ButtonStyle::Ghost);
    settings->setPosition(uiIsFa() ? 100 : vs.width - 100, vs.height - 76);
    root->addChild(settings);

    // World portals (2 columns, scrollable).
    auto list = VList::create(Size(vs.width, vs.height - 170), 18);
    list->setPosition(Vec2(0, 0));
    root->addChild(list);
    const auto& ws = worlds();
    float cardW = (vs.width - 60) / 2, cardH = 330;
    for (size_t i = 0; i < ws.size(); i += 2) {
        auto row = Node::create();
        row->setContentSize(Size(vs.width, cardH));
        row->setAnchorPoint(Vec2(0.5f, 0.5f));
        for (size_t k = i; k < std::min(i + 2, ws.size()); ++k) {
            const NexoraWorld& w = ws[k];
            int col = static_cast<int>(k - i);
            if (uiIsFa()) col = 1 - col;  // RTL grid
            float x = 20 + col * (cardW + 20);
            auto card = makePanel(Size(cardW, cardH), Color4F(1, 1, 1, 0.05f), color::f4(color::hex(w.color), 0.8f), 28, 2);
            card->setPosition(x, 0);
            row->addChild(card);
            auto img = makeImage(w.portal(), Size(cardW - 30, 200));
            img->setPosition(cardW / 2, cardH - 120);
            card->addChild(img);
            auto nm = makeText(uiIsFa() ? w.nameFa : w.name, 26, color::hex(w.color), Weight::Black, cardW - 20);
            nm->setPosition(cardW / 2, 92);
            card->addChild(nm);
            auto tg = makeText(uiIsFa() ? w.taglineFa : w.taglineEn, 18, color::muted, Weight::Regular, cardW - 24);
            tg->setPosition(cardW / 2, 42);
            card->addChild(tg);
            std::string id = w.id;
            auto tap = EventListenerTouchOneByOne::create();
            tap->onTouchBegan = [card](Touch* t, Event*) {
                Vec2 p = card->convertToNodeSpace(t->getLocation());
                return Rect(0, 0, card->getContentSize().width, card->getContentSize().height).containsPoint(p);
            };
            tap->onTouchEnded = [id](Touch* t, Event*) {
                if ((t->getLocation() - t->getStartLocation()).length() < 20) HubScene::openWorld(id);
            };
            card->getEventDispatcher()->addEventListenerWithSceneGraphPriority(tap, card);
        }
        list->addRow(row);
    }
    return true;
}

void HubScene::onEnter() {
    Scene::onEnter();
    listenerId_ = NxApp::get().online().addListener([this]() { refreshStatus(); });
    refreshStatus();
    if (name_) ui::setText(name_, NxApp::get().playerName());
}

void HubScene::onExit() {
    NxApp::get().online().removeListener(listenerId_);
    listenerId_ = -1;
    Scene::onExit();
}

void HubScene::refreshStatus() {
    auto& on = NxApp::get().online();
    std::string s;
    if (!on.configured()) s = tx("نسخه‌ی آفلاین", "Offline build");
    else if (on.isOnline()) s = tx("آنلاین · ", "Online · ") + on.email();
    else s = tx("مهمان · آفلاین", "Guest · offline");
    ui::setText(status_, s);
}

void HubScene::openWorld(const std::string& id) {
    if (id == "nexora_arcade") {
        NxApp::push(createArcadeScene());
        return;
    }
    if (Scene* s = createWorldScene(id)) {
        NxApp::push(s);
        return;
    }
    const NexoraWorld* w = worldById(id);
    NxApp::push(NotPortedScene::create(w ? w->nameFa : id, w ? w->name : id,
                                       tx("این بخش هنوز به نسخه‌ی cocos2d-x منتقل نشده است.",
                                          "This world has not been ported to the cocos2d-x build yet.")));
}

// ---------------- Settings ----------------

bool SettingsScene::init() {
    if (!Scene::init()) return false;
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    addChild(makeSpaceBackground(Size(vs.width + o.x * 2, vs.height + o.y * 2)));
    rebuild();
    return true;
}

void SettingsScene::rebuild() {
    if (content_) content_->removeFromParent();
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    content_ = Node::create();
    addChild(content_, 1);
    content_->addChild(makeTopBar(tx("تنظیمات", "Settings")));
    auto list = VList::create(Size(vs.width, vs.height - 130), 14);
    list->setPosition(o);
    content_->addChild(list);
    auto& st = NxApp::get().settings();
    auto row = [&](const std::string& label, const std::string& value, std::function<void()> onTap) {
        auto n = Node::create();
        n->setContentSize(Size(vs.width - 40, 96));
        n->setAnchorPoint(Vec2(0.5f, 0.5f));
        auto p = makePanel(n->getContentSize(), Color4F(1, 1, 1, 0.05f), color::f4(color::accent2, 0.5f), 22);
        n->addChild(p);
        auto l = makeText(label, 26, color::text, Weight::Medium);
        l->setAnchorPoint(Vec2(uiIsFa() ? 1.f : 0.f, 0.5f));
        l->setPosition(uiIsFa() ? vs.width - 70 : 30, 48);
        n->addChild(l);
        auto b = NxButton::create(value, Size(220, 64), [onTap, this] {
            onTap();
            auto self = this;
            self->runAction(CallFunc::create([self] { self->rebuild(); }));
        }, ButtonStyle::Ghost);
        b->setPosition(uiIsFa() ? 140 : vs.width - 170, 48);
        n->addChild(b);
        list->addRow(n);
    };
    auto onOff = [](bool v) { return v ? tx("روشن", "On") : tx("خاموش", "Off"); };
    row(tx("زبان", "Language"), st.isFa() ? "فارسی" : "English", [&st] { st.setLanguage(st.isFa() ? "en" : "fa"); });
    row(tx("موسیقی", "Music"), onOff(st.music()), [&st] { st.setMusic(!st.music()); });
    row(tx("صدای بازی", "Game sound"), onOff(st.sound()), [&st] { st.setSound(!st.sound()); });
    row(tx("لرزش", "Haptics"), onOff(st.haptics()), [&st] { st.setHaptics(!st.haptics()); });
    row(tx("حرکت کمتر", "Reduced motion"), onOff(st.reducedMotion()), [&st] { st.setReducedMotion(!st.reducedMotion()); });
    static const char* qFa[] = {"کم", "متوسط", "بالا", "فوق‌العاده"};
    static const char* qEn[] = {"Low", "Medium", "High", "Ultra"};
    int q = static_cast<int>(st.quality());
    row(tx("کیفیت گرافیک", "Graphics quality"), tx(qFa[q], qEn[q]),
        [&st, q] { st.setQuality(static_cast<GraphicsQuality>((q + 1) % 4)); });
    row(tx("اوراکل همراه", "ORACLE buddy"), onOff(st.oracleBuddy()), [&st] { st.setOracleBuddy(!st.oracleBuddy()); });
    row(tx("حرف‌زدن اوراکل", "ORACLE talk"), onOff(st.oracleTalk()), [&st] { st.setOracleTalk(!st.oracleTalk()); });

    auto& on = NxApp::get().online();
    if (on.isOnline()) {
        row(tx("حساب: ", "Account: ") + on.email(), tx("خروج", "Sign out"), [] {
            NxApp::get().online().signOut([] {
                NxApp::get().settings().setAuthMode("");
                NxApp::replace(AuthScene::create());
            });
        });
    } else {
        row(tx("حساب آنلاین", "Online account"), tx("ورود", "Sign in"), [] {
            NxApp::get().settings().setAuthMode("");
            NxApp::replace(AuthScene::create());
        });
    }
    auto ver = makeText("NEXORA cocos2d-x · v1.0.0", 20, color::muted);
    auto vn = Node::create();
    vn->setContentSize(Size(vs.width, 60));
    vn->setAnchorPoint(Vec2(0.5f, 0.5f));
    ver->setPosition(vs.width / 2, 30);
    vn->addChild(ver);
    list->addRow(vn);
}

// ---------------- NotPorted ----------------

NotPortedScene* NotPortedScene::create(const std::string& fa, const std::string& en, const std::string& note) {
    auto s = new (std::nothrow) NotPortedScene();
    if (s && s->initWith(fa, en, note)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool NotPortedScene::initWith(const std::string& fa, const std::string& en, const std::string& note) {
    if (!Scene::init()) return false;
    Size vs = visibleSize();
    Vec2 o = visibleOrigin();
    addChild(makeSpaceBackground(Size(vs.width + o.x * 2, vs.height + o.y * 2)));
    addChild(makeTopBar(tx(fa, en)));
    auto l = makeText(note, 28, color::muted, Weight::Medium, vs.width - 120);
    l->setPosition(o.x + vs.width / 2, o.y + vs.height / 2);
    addChild(l);
    return true;
}

}  // namespace nx
