// Port of lib/games/quiz/core/{question_model,quiz_engine}.dart. The 268-question bank ships as
// Resources/games/quiz/questions.json (exported from question_bank_a/b.dart in the same order).
#pragma once
#include "core/DartRandom.h"
#include "core/Json.h"

#include <array>
#include <string>
#include <vector>

namespace nx {
namespace quiz {

enum class Category { Space = 0, Science, Geography, History, Sports, Technology, Iran, General, Media, None = -1 };
constexpr int kCategoryCount = 9;
const char* categoryKey(Category c);
std::string categoryNameFa(Category c);
std::string categoryNameEn(Category c);

struct Question {
    std::string id;
    Category category = Category::General;
    int difficulty = 1;
    std::string fa, en;
    std::array<std::string, 4> optionsFa, optionsEn;
    int correct = 0;
};

std::vector<Question> parseBank(const Json& j);

enum class BotLevel { Easy = 0, Normal, Hard };
enum class Phase { Picking, Question, Reveal, Finished };

struct Answer {
    int choice = -1;  // -1 = timeout
    int elapsedMs = 0;
    bool correct = false;
    int points = 0;
};

struct BotPlan { int choice = 0; int delayMs = 0; bool correct = false; };

class Engine {
public:
    static constexpr int rounds = 10, timeLimitMs = 15000, basePoints = 100, maxSpeedBonus = 50;
    Engine(const std::vector<Question>& bank, int64_t seed = 20251112, bool vsBot = true, BotLevel level = BotLevel::Normal);

    Phase phase = Phase::Picking;
    Category category = Category::None;
    std::vector<Question> questions;
    int roundIndex = 0;
    std::array<int, 2> scores{{0, 0}};
    std::array<int, 2> correctCounts{{0, 0}};
    std::vector<std::array<Answer, 2>> history;
    bool vsBot;
    BotLevel botLevel;

    const Question& currentQuestion() const { return questions[roundIndex]; }
    int totalRounds() const { return static_cast<int>(questions.size()); }
    bool isLastRound() const { return roundIndex >= totalRounds() - 1; }
    std::vector<Category> availableCategories() const;
    int countFor(Category c) const;
    std::vector<Question> selectQuestions(Category cat);
    void start(Category cat);
    static int scoreFor(bool correct, int elapsedMs);
    static double remainingFraction(int elapsedMs);
    static bool isTimeUp(int elapsedMs) { return elapsedMs >= timeLimitMs; }
    static double accuracyFor(BotLevel l);
    static std::array<int, 2> answerWindowFor(BotLevel l);
    BotPlan botPlan(const Question& q) { return planBot(q, botLevel, rng_); }
    static BotPlan planBot(const Question& q, BotLevel level, DartRandom& rng);
    bool hasAnswered(int p) const { return has_[p]; }
    bool bothAnswered() const { return has_[0] && has_[1]; }
    const std::array<Answer, 2>& currentAnswers() const { return cur_; }
    bool submit(int player, int choice, int elapsedMs);
    bool submitTimeout(int player);
    void timeUp();
    void next();
    int winner() const;

private:
    void closeRound();
    std::vector<Question> bank_;
    DartRandom rng_;
    std::array<Answer, 2> cur_;
    std::array<bool, 2> has_{{false, false}};
};

}  // namespace quiz
}  // namespace nx
