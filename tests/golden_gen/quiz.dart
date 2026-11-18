import 'dart:math';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/quiz/core/question_bank.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/quiz/core/question_model.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/quiz/core/quiz_engine.dart';
void main() {
  for (final seed in [3, 500]) {
    for (final cat in <QuizCategory?>[null, QuizCategory.space, QuizCategory.media]) {
      final e = QuizEngine(bank: kQuestionBank, seed: seed, botLevel: QuizBotLevel.values[seed % 3]);
      e.start(cat);
      final me = Random(seed * 7);
      while (e.phase != QuizPhase.finished) {
        final q = e.currentQuestion;
        final plan = e.botPlan(q);
        final myChoice = me.nextInt(4);
        final myMs = me.nextInt(16000);
        e.submit(0, myChoice, myMs);
        e.submit(1, plan.choice, plan.delayMs);
        print('quiz $seed ${cat?.name} r${e.roundIndex} ${q.id} me$myChoice/$myMs bot${plan.choice}/${plan.delayMs}/${plan.correct} sc${e.scores}');
        e.next();
      }
      print('quiz $seed ${cat?.name} winner ${e.winner()} correct ${e.correctCounts} cats ${e.availableCategories().length} count ${e.countFor(cat)}');
    }
  }
  print('score ${QuizEngine.scoreFor(correct: true, elapsedMs: 7500)} ${QuizEngine.scoreFor(correct: true, elapsedMs: 0)} ${QuizEngine.scoreFor(correct: true, elapsedMs: 14700)}');
}
