// Minimal self-contained test runner (no external deps, so the host CI job needs only g++/cmake).
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace nxtest {

struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }

struct Reg { Reg(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };

inline void fail(const char* file, int line, const std::string& msg) {
    ++failures();
    std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, msg.c_str());
}

template <class A, class B>
std::string show(const A& a, const B& b) {
    std::ostringstream o;
    o << "got [" << a << "] expected [" << b << "]";
    return o.str();
}

inline int runAll(const char* filter) {
    int ran = 0, failedCases = 0;
    for (auto& c : registry()) {
        if (filter && std::string(c.name).find(filter) == std::string::npos) continue;
        int before = failures();
        try {
            c.fn();
        } catch (const std::exception& e) {
            fail(c.name, 0, std::string("exception: ") + e.what());
        }
        ++ran;
        bool ok = failures() == before;
        if (!ok) ++failedCases;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name);
    }
    std::printf("\n%d test cases, %d checks, %d failed cases, %d failed checks\n", ran, checks(), failedCases, failures());
    return failures() == 0 ? 0 : 1;
}

}  // namespace nxtest

#define NX_CAT2(a, b) a##b
#define NX_CAT(a, b) NX_CAT2(a, b)
#define TEST(name)                                                                 \
    static void NX_CAT(test_fn_, name)();                                          \
    static nxtest::Reg NX_CAT(test_reg_, name)(#name, &NX_CAT(test_fn_, name));    \
    static void NX_CAT(test_fn_, name)()

#define CHECK(cond)                                                                \
    do {                                                                           \
        ++nxtest::checks();                                                        \
        if (!(cond)) nxtest::fail(__FILE__, __LINE__, "CHECK(" #cond ")");         \
    } while (0)

#define CHECK_EQ(a, b)                                                             \
    do {                                                                           \
        ++nxtest::checks();                                                        \
        auto _va = (a);                                                            \
        auto _vb = (b);                                                            \
        if (!(_va == _vb)) nxtest::fail(__FILE__, __LINE__, #a " == " #b ": " + nxtest::show(_va, _vb)); \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                      \
    do {                                                                           \
        ++nxtest::checks();                                                        \
        double _va = (a), _vb = (b);                                               \
        if (std::fabs(_va - _vb) > (eps)) nxtest::fail(__FILE__, __LINE__, #a " ~= " #b ": " + nxtest::show(_va, _vb)); \
    } while (0)
