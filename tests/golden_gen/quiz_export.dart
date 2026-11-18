// Exports the Dart question bank to Resources/games/quiz/questions.json (same order => same seeded picks).
import 'dart:convert';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/quiz/core/question_bank.dart';
void main() {
  final list = [for (final q in kQuestionBank) {'id': q.id, 'c': q.category.index, 'd': q.difficulty, 'fa': q.fa, 'en': q.en, 'ofa': q.optionsFa, 'oen': q.optionsEn, 'k': q.correct}];
  print(jsonEncode(list));
}
