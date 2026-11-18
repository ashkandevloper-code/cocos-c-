#include "nxtest.h"
#include "core/DartRandom.h"
#include "core/Json.h"
#include "core/text/PersianShaper.h"
#include <fstream>
#include <cstdio>

static std::string readGolden(const std::string& name) {
    std::ifstream f(std::string(NX_GOLDEN_DIR) + "/" + name);
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

TEST(rng_matches_dart_vm) {
    long long seeds[] = {0, 1, 42, 123456789, -5, 9007199254740991LL};
    std::string out;
    char buf[256];
    for (auto s : seeds) {
        nx::DartRandom r(s);
        std::string line = std::to_string(s) + " ";
        for (int i = 0; i < 5; i++) { line += (i ? "," : "") + std::to_string(r.nextInt(6)); }
        line += " ";
        for (int i = 0; i < 3; i++) { line += (i ? "," : "") + std::to_string(r.nextInt(100)); }
        line += " ";
        for (int i = 0; i < 3; i++) { std::snprintf(buf, sizeof buf, "%s%.17f", i ? "," : "", r.nextDouble()); line += buf; }
        line += " ";
        for (int i = 0; i < 3; i++) { line += std::string(i ? "," : "") + (r.nextBool() ? "true" : "false"); }
        line += " " + std::to_string(r.nextInt(1 << 20)) + "\n";
        out += line;
    }
    CHECK_EQ(out, readGolden("rng.txt"));
}

TEST(json_roundtrip_and_order) {
    nx::Json j = nx::Json::obj({{"p_match", "m1"}, {"p_seq", 3}, {"p_move", nx::Json::obj({{"x", 1.5}, {"ok", true}})}, {"p_hash", nullptr}});
    CHECK_EQ(j.dump(), std::string(R"({"p_match":"m1","p_seq":3,"p_move":{"x":1.5,"ok":true},"p_hash":null})"));
    bool ok = false;
    nx::Json p = nx::Json::parse(R"({"a":[1,2,{"b":"س"}],"n":-7,"d":0.25})", &ok);
    CHECK(ok);
    CHECK_EQ(p["a"].size(), (size_t)3);
    CHECK_EQ(p["a"][2]["b"].asString(), std::string("س"));
    CHECK_EQ(p["n"].asInt(), (int64_t)-7);
    CHECK_NEAR(p["d"].asDouble(), 0.25, 1e-12);
    CHECK(p["missing"].isNull());
    nx::Json bad = nx::Json::parse("{oops", &ok);
    CHECK(!ok);
    nx::Json copy = p; copy.set("n", 1);
    CHECK_EQ(p["n"].asInt(), (int64_t)-7);  // copy-on-write
}

TEST(persian_shaping_forms) {
    using namespace nx::text;
    // "سلام" -> seen initial, lam-alef final ligature, meem final.
    std::u32string s = shape(utf8To32("سلام"));
    CHECK_EQ(s.size(), (size_t)3);
    CHECK(s[0] == 0xFEB3);  // seen initial
    CHECK(s[1] == 0xFEFC);  // lam-alef final
    CHECK(s[2] == 0xFEE1);  // meem isolated (alef does not join forward)
    // ZWNJ breaks the join: "می‌شود"
    std::u32string z = shape(utf8To32("می\u200Cشود"));
    CHECK(z[0] == 0xFEE3);  // meem initial
    CHECK(z[1] == 0xFBFD);  // farsi yeh final
    CHECK(z[2] == 0xFEB7);  // sheen initial (ZWNJ removed, not joined to yeh)
    // Persian letters: گ medial in "بگیر"
    std::u32string g = shape(utf8To32("بگیر"));
    CHECK(g[0] == 0xFE91 && g[1] == 0xFB95 && g[2] == 0xFBFF && g[3] == 0xFEAE);
}

TEST(persian_bidi_visual) {
    using namespace nx::text;
    // Pure RTL is reversed.
    std::u32string v = visual(utf8To32("ابج"), true);
    CHECK(v == utf8To32("جبا"));
    // Embedded Latin keeps its order and sits on the left of preceding Persian text.
    std::string d = utf32To8(visual(utf8To32("سلام NEXORA"), true));
    CHECK_EQ(d, std::string("NEXORA مالس"));
    // Numbers keep LTR order inside RTL text.
    CHECK_EQ(utf32To8(visual(utf8To32("ب 123 ا"), true)), std::string("ا 123 ب"));
    // Plain English untouched.
    CHECK_EQ(toDisplay("Hello (world)"), std::string("Hello (world)"));
    CHECK_EQ(normalizeDigits("۱۲۳ ٤٥6"), std::string("123456"));
    CHECK_EQ(toPersianDigits("x9"), std::string("x۹"));
}
