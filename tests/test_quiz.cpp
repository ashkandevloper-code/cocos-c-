#include "golden_util.h"

#include "games/quiz/Quiz.h"

using namespace nx;

TEST(quiz_matches_dart_golden) {
    std::ifstream f(std::string(NX_RES_DIR) + "/games/quiz/questions.json");
    std::stringstream ss;
    ss << f.rdbuf();
    bool ok = false;
    auto bank = quiz::parseBank(Json::parse(ss.str(), &ok));
    CHECK(ok);
    CHECK_EQ(bank.size(), (size_t)268);
    std::string out;
    for (int seed : {3, 500}) {
        for (quiz::Category cat : {quiz::Category::None, quiz::Category::Space, quiz::Category::Media}) {
            quiz::Engine e(bank, seed, true, static_cast<quiz::BotLevel>(seed % 3));
            e.start(cat);
            DartRandom me(seed * 7);
            std::string cn = quiz::categoryKey(cat);
            while (e.phase != quiz::Phase::Finished) {
                const quiz::Question q = e.currentQuestion();
                auto plan = e.botPlan(q);
                int myChoice = me.nextInt(4);
                int myMs = me.nextInt(16000);
                e.submit(0, myChoice, myMs);
                e.submit(1, plan.choice, plan.delayMs);
                out += "quiz " + std::to_string(seed) + " " + cn + " r" + std::to_string(e.roundIndex) + " " + q.id + " me" + std::to_string(myChoice) +
                       "/" + std::to_string(myMs) + " bot" + std::to_string(plan.choice) + "/" + std::to_string(plan.delayMs) + "/" + B(plan.correct) +
                       " sc[" + std::to_string(e.scores[0]) + ", " + std::to_string(e.scores[1]) + "]\n";
                e.next();
            }
            out += "quiz " + std::to_string(seed) + " " + cn + " winner " + std::to_string(e.winner()) + " correct [" +
                   std::to_string(e.correctCounts[0]) + ", " + std::to_string(e.correctCounts[1]) + "] cats " +
                   std::to_string(e.availableCategories().size()) + " count " + std::to_string(e.countFor(cat)) + "\n";
        }
    }
    out += "score " + std::to_string(quiz::Engine::scoreFor(true, 7500)) + " " + std::to_string(quiz::Engine::scoreFor(true, 0)) + " " +
           std::to_string(quiz::Engine::scoreFor(true, 14700)) + "\n";
    CHECK_GOLDEN(out, "quiz.txt");
}
