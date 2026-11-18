#include "games/quiz/Quiz.h"

#include "core/DartCollections.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace nx {
namespace quiz {

const char* categoryKey(Category c) {
    static const char* k[] = {"space", "science", "geography", "history", "sports", "technology", "iran", "general", "media"};
    int i = static_cast<int>(c);
    return i >= 0 && i < kCategoryCount ? k[i] : "null";
}
std::string categoryNameFa(Category c) {
    static const char* k[] = {"فضا و نجوم", "علوم", "جغرافیا", "تاریخ", "ورزش", "فناوری", "ایران و فرهنگ پارسی", "دانش عمومی", "فیلم و بازی"};
    int i = static_cast<int>(c);
    return i >= 0 && i < kCategoryCount ? k[i] : "ترکیبی";
}
std::string categoryNameEn(Category c) {
    static const char* k[] = {"Space & Astronomy", "Science", "Geography", "History", "Sports", "Technology", "Iran & Persian culture", "General knowledge", "Movies & Games"};
    int i = static_cast<int>(c);
    return i >= 0 && i < kCategoryCount ? k[i] : "Mixed";
}

std::vector<Question> parseBank(const Json& j) {
    std::vector<Question> out;
    for (auto& e : j.items()) {
        Question q;
        q.id = e["id"].asString();
        q.category = static_cast<Category>(e["c"].asInt());
        q.difficulty = static_cast<int>(e["d"].asInt(1));
        q.fa = e["fa"].asString();
        q.en = e["en"].asString();
        for (int i = 0; i < 4; ++i) {
            q.optionsFa[i] = e["ofa"][i].asString();
            q.optionsEn[i] = e["oen"][i].asString();
        }
        q.correct = static_cast<int>(e["k"].asInt());
        out.push_back(q);
    }
    return out;
}

Engine::Engine(const std::vector<Question>& bank, int64_t seed, bool vb, BotLevel level)
    : vsBot(vb), botLevel(level), bank_(bank), rng_(seed) {}

std::vector<Category> Engine::availableCategories() const {
    std::set<int> s;
    for (auto& q : bank_) s.insert(static_cast<int>(q.category));
    std::vector<Category> out;
    for (int i = 0; i < kCategoryCount; ++i) if (s.count(i)) out.push_back(static_cast<Category>(i));
    return out;
}

int Engine::countFor(Category c) const {
    if (c == Category::None) return static_cast<int>(bank_.size());
    int n = 0;
    for (auto& q : bank_) if (q.category == c) ++n;
    return n;
}

std::vector<Question> Engine::selectQuestions(Category cat) {
    std::vector<Question> unique;
    std::set<std::string> seen;
    for (auto& q : bank_) if (seen.insert(q.id).second) unique.push_back(q);
    std::vector<Question> picked;
    std::set<std::string> used;
    auto addFrom = [&](const std::vector<Question>& pool) {
        for (auto& q : pool) {
            if (static_cast<int>(picked.size()) >= rounds) return;
            if (used.insert(q.id).second) picked.push_back(q);
        }
    };
    if (cat != Category::None) {
        std::vector<Question> pool;
        for (auto& q : unique) if (q.category == cat) pool.push_back(q);
        dartShuffle(pool, rng_);
        addFrom(pool);
    }
    if (static_cast<int>(picked.size()) < rounds) {
        std::vector<int> keysInOrder;  // Dart map insertion order
        std::map<int, std::vector<Question>> byCat;
        for (auto& q : unique) {
            if (used.count(q.id)) continue;
            int c = static_cast<int>(q.category);
            if (!byCat.count(c)) keysInOrder.push_back(c);
            byCat[c].push_back(q);
        }
        std::vector<int> order = keysInOrder;
        dartSort(order, [](int a, int b) { return a - b; });
        dartShuffle(order, rng_);
        for (int c : order) dartShuffle(byCat[c], rng_);
        size_t depth = 0;
        bool any = true;
        while (static_cast<int>(picked.size()) < rounds && any) {
            any = false;
            for (int c : order) {
                auto& pool = byCat[c];
                if (depth < pool.size()) {
                    any = true;
                    if (static_cast<int>(picked.size()) < rounds && used.insert(pool[depth].id).second) picked.push_back(pool[depth]);
                }
            }
            ++depth;
        }
    }
    return picked;
}

void Engine::start(Category cat) {
    category = cat;
    questions = selectQuestions(cat);
    roundIndex = 0;
    scores = {{0, 0}};
    correctCounts = {{0, 0}};
    history.clear();
    has_ = {{false, false}};
    phase = questions.empty() ? Phase::Finished : Phase::Question;
}

int Engine::scoreFor(bool correct, int elapsedMs) {
    if (!correct) return 0;
    int e = std::min(std::max(elapsedMs, 0), timeLimitMs);
    double frac = static_cast<double>(timeLimitMs - e) / timeLimitMs;
    return basePoints + static_cast<int>(std::round(maxSpeedBonus * frac));
}

double Engine::remainingFraction(int elapsedMs) {
    double v = 1.0 - static_cast<double>(elapsedMs) / timeLimitMs;
    return std::min(1.0, std::max(0.0, v));
}

double Engine::accuracyFor(BotLevel l) { return l == BotLevel::Easy ? 0.45 : (l == BotLevel::Normal ? 0.65 : 0.85); }

std::array<int, 2> Engine::answerWindowFor(BotLevel l) {
    if (l == BotLevel::Easy) return {{5500, 12500}};
    if (l == BotLevel::Normal) return {{3500, 9500}};
    return {{2000, 6500}};
}

BotPlan Engine::planBot(const Question& q, BotLevel level, DartRandom& rng) {
    BotPlan p;
    p.correct = rng.nextDouble() < accuracyFor(level);
    p.choice = q.correct;
    if (!p.correct) {
        std::vector<int> wrong = {0, 1, 2, 3};
        wrong.erase(std::find(wrong.begin(), wrong.end(), q.correct));
        p.choice = wrong[rng.nextInt(static_cast<int>(wrong.size()))];
    }
    auto w = answerWindowFor(level);
    int delay = w[0] + rng.nextInt(w[1] - w[0] + 1);
    delay += (q.difficulty - 1) * 700;
    if (!p.correct) delay += 400;
    p.delayMs = std::min(std::max(delay, 800), timeLimitMs - 600);
    return p;
}

bool Engine::submit(int player, int choice, int elapsedMs) {
    if (phase != Phase::Question) return false;
    if (player < 0 || player > 1 || has_[player]) return false;
    int e = std::min(std::max(elapsedMs, 0), timeLimitMs);
    bool ok = choice == currentQuestion().correct && e < timeLimitMs;
    cur_[player] = Answer{choice, e, ok, scoreFor(ok, e)};
    has_[player] = true;
    if (bothAnswered()) closeRound();
    return true;
}

bool Engine::submitTimeout(int player) {
    if (phase != Phase::Question || player < 0 || player > 1 || has_[player]) return false;
    cur_[player] = Answer{-1, timeLimitMs, false, 0};
    has_[player] = true;
    if (bothAnswered()) closeRound();
    return true;
}

void Engine::timeUp() {
    if (phase != Phase::Question) return;
    for (int p = 0; p < 2; ++p) {
        if (!has_[p]) {
            cur_[p] = Answer{-1, timeLimitMs, false, 0};
            has_[p] = true;
        }
    }
    closeRound();
}

void Engine::closeRound() {
    for (int p = 0; p < 2; ++p) {
        scores[p] += cur_[p].points;
        if (cur_[p].correct) correctCounts[p]++;
    }
    history.push_back(cur_);
    phase = Phase::Reveal;
}

void Engine::next() {
    if (phase != Phase::Reveal) return;
    if (isLastRound()) {
        phase = Phase::Finished;
        return;
    }
    ++roundIndex;
    has_ = {{false, false}};
    phase = Phase::Question;
}

int Engine::winner() const {
    if (scores[0] != scores[1]) return scores[0] > scores[1] ? 0 : 1;
    if (correctCounts[0] != correctCounts[1]) return correctCounts[0] > correctCounts[1] ? 0 : 1;
    return -1;
}

}  // namespace quiz
}  // namespace nx
