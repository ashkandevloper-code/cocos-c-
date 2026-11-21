import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/core/online/match_link.dart';

void main() {
  test('only a finished, non-disputed server result is confirmed', () {
    const MatchEnd finished = MatchEnd(winners: <int>[0], reason: 'finished');
    const MatchEnd disputed = MatchEnd(
      winners: <int>[],
      reason: 'disputed',
      status: 'disputed',
    );
    const MatchEnd abandoned = MatchEnd(
      winners: <int>[],
      reason: 'abandoned',
      status: 'abandoned',
    );

    expect(finished.isConfirmed, isTrue);
    expect(disputed.isDisputed, isTrue);
    expect(disputed.isConfirmed, isFalse);
    expect(abandoned.isConfirmed, isFalse);
  });
}
