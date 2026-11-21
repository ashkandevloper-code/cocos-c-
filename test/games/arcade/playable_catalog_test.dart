import 'package:flutter_test/flutter_test.dart';
import 'package:nexora/core/profile/achievements.dart';
import 'package:nexora/games/arcade_kit/arcade_meta.dart';
import 'package:nexora/games/arcade_registry.dart';
import 'package:nexora/games/game_registry.dart';

void main() {
  test('achievement catalog counts ONLY playable arcade games', () {
    final playable = kArcadeGames.map((g) => g.id).toSet();
    expect(playable.length, 7);
    expect(kPlayableArcadeIds.toSet(), playable);
    expect(kGames.map((g) => g.id).toSet().length, kGames.length);
    expect(kGames.length, 16);
    expect(kArcadeIds.length, 20, reason: 'the remaining 13 are designs, not game entries');
    for (final id in kArcadeIds.toSet().difference(playable)) {
      expect(gameById(id), isNull, reason: '$id must not be advertised as playable');
    }
  });

  test('planned-only achievements do not appear as unlockable rewards', () {
    final available = kAvailableAchievements.map((a) => a.id).toSet();
    expect(available, contains('arcade_all'));
    expect(available, isNot(contains('arcade_gold')));
    expect(available, isNot(contains('grandmaster')));
    expect(kAchievements.map((a) => a.id), contains('grandmaster'),
        reason: 'preserve the future award id / existing saved profile data');
  });
}
