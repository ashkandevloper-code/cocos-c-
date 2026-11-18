#pragma once
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "nxtest.h"

inline std::string readGoldenText(const std::string& name) {
    std::ifstream f(std::string(NX_GOLDEN_DIR) + "/" + name);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
inline const char* B(bool b) { return b ? "true" : "false"; }
inline std::string fx(double v, int digits) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", digits, v);
    std::string s(buf);
    if (s == "-0.000" || s == "-0.00" || s == "-0.0" || s == "-0") s = s.substr(1);  // Dart prints -0.0 as "-0.000"? keep parity below
    return s;
}
template <class T>
std::string join(const std::vector<T>& v, const char* sep = ",") {
    std::string o;
    for (size_t i = 0; i < v.size(); ++i) { if (i) o += sep; o += std::to_string(v[i]); }
    return o;
}
/// Compares line by line and reports the first mismatch (much easier to debug than one giant string).
inline void checkGolden(const std::string& got, const std::string& goldenName, const char* file, int line) {
    std::string want = readGoldenText(goldenName);
    ++nxtest::checks();
    if (got == want) return;
    std::istringstream a(got), b(want);
    std::string la, lb;
    int n = 1;
    while (true) {
        bool ea = !std::getline(a, la), eb = !std::getline(b, lb);
        if (ea && eb) break;
        if (ea || eb || la != lb) {
            nxtest::fail(file, line, goldenName + " differs at line " + std::to_string(n) + "\n    got:  " + (ea ? "<eof>" : la) +
                                         "\n    want: " + (eb ? "<eof>" : lb));
            return;
        }
        ++n;
    }
}
#define CHECK_GOLDEN(got, name) checkGolden(got, name, __FILE__, __LINE__)
