import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/core/online/match_link.dart';
import 'package:nexora/games/core/game_contract.dart';
import 'package:nexora/worlds/arcade/online/settled_match_result.dart';

void main() {
  test('server winners and scores, not a local claim, determine each seat outcome', () {
    const MatchEnd end = MatchEnd(
      status: 'finished', reason: 'finished', winners: <int>[1, 3], scores: <int>[1, 4, 0, 4],
    );
    final GameResult loser = settledMatchResult(end, 0);
    expect(loser.outcome, GameOutcome.loss);
    expect(loser.score, 1);
    expect(loser.opponentScore, 4);
    final GameResult winner = settledMatchResult(end, 3);
    expect(winner.outcome, GameOutcome.win);
    expect(winner.score, 4);
    expect(winner.winnerSeats, <int>[1, 3]);
  });

  test('empty winners are a genuine draw only when server finalized the match', () {
    const MatchEnd draw = MatchEnd(status: 'finished', reason: 'finished', winners: <int>[]);
    expect(settledMatchResult(draw, 0).outcome, GameOutcome.draw);
    expect(settledMatchResult(draw, 0).score, 0);
  });

  test('dispute, abandonment and administrative closure are NOT rewarded draws', () {
    for (final MatchEnd end in <MatchEnd>[
      const MatchEnd(status: 'disputed', reason: 'disputed', winners: <int>[]),
      const MatchEnd(status: 'abandoned', reason: 'admin', winners: <int>[]),
      const MatchEnd(status: 'finished', reason: 'admin_ended', winners: <int>[]),
      const MatchEnd(status: 'finished', reason: 'admin_closed', winners: <int>[]),
    ]) {
      expect(end.rewardsGranted, isFalse);
      expect(() => settledMatchResult(end, 0), throwsStateError);
    }
  });

  test('forfeit gets the outcome stored by server, never the optimistic result', () {
    const MatchEnd end = MatchEnd(status: 'finished', reason: 'forfeit:0', winners: <int>[1]);
    expect(end.isForfeit, isTrue);
    expect(end.forfeitSeat, 0);
    expect(settledMatchResult(end, 0).outcome, GameOutcome.loss);
    expect(settledMatchResult(end, 1).outcome, GameOutcome.win);
  });
}
