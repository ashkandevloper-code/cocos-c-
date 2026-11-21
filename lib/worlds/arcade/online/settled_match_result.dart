import 'dart:math' as math;

import '../../../core/online/match_link.dart';
import '../../../games/core/game_contract.dart';

/// Converts the stored match row, not a phone's optimistic result, into the local
/// outcome. This is result *consensus*, not proof that either client played fairly.
/// Disputed/abandoned/administratively closed matches must never reach this path.
GameResult settledMatchResult(MatchEnd end, int seat) {
  if (!end.rewardsGranted) throw StateError('match has no settled reward');
  final GameOutcome outcome = end.winners.isEmpty
      ? GameOutcome.draw
      : (end.winners.contains(seat) ? GameOutcome.win : GameOutcome.loss);
  final List<int>? scores = end.scores;
  final int own = scores != null && seat >= 0 && seat < scores.length ? scores[seat] : 0;
  int others = 0;
  if (scores != null) {
    for (int i = 0; i < scores.length; i++) {
      if (i != seat) others = math.max(others, scores[i]);
    }
  }
  return GameResult(
    outcome: outcome,
    score: own,
    opponentScore: others,
    winnerSeats: end.winners,
    scores: scores,
  );
}
