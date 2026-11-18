import 'dart:math';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/uno/core/uno_engine.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/uno/core/uno_models.dart';
import '/workspace/work/nexora/Nexora-Online-redesign-2-fix/lib/games/uno/core/uno_bot.dart';

void main() {
  // Dart sort port check on a big list (dual-pivot path) with many ties.
  final r0 = Random(9);
  final big = [for (int i = 0; i < 200; i++) r0.nextInt(17) * 1000 + i];
  big.sort((a, b) => (a ~/ 1000).compareTo(b ~/ 1000));
  print('sort ${big.join(",")}');
  for (final seed in [1, 42, 99]) {
    for (final pc in [2, 4]) {
      final g = UnoGame(playerCount: pc, seed: seed);
      final rng = Random(seed + 1000);
      final levels = [for (int i = 0; i < pc; i++) UnoBotLevel.values[(i + seed) % 3]];
      int steps = 0;
      while (!g.isOver && steps < 2000) {
        final seat = g.current;
        final lvl = levels[seat];
        if (g.canCallNx(seat) && rng.nextDouble() < UnoBot.nxCallChance(lvl)) g.callNx(seat);
        final mv = UnoBot.decide(g, seat, lvl, rng);
        if (mv.draw) {
          final c = g.draw(seat);
          if (g.drawnCardId != null) {
            if (UnoBot.playDrawn(g, seat, lvl, rng)) {
              final card = g.cardInHand(seat, g.drawnCardId!)!;
              g.play(seat, card.id, chosenColor: card.isWild ? UnoBot.bestColor(g, seat, rng, excludingCardId: card.id) : null);
            } else {
              g.pass(seat);
            }
          }
          print('uno $seed/$pc st$steps s$seat draw $c');
        } else {
          final ok = g.play(seat, mv.cardId!, chosenColor: mv.color);
          print('uno $seed/$pc st$steps s$seat play ${mv.cardId} ${mv.color?.name} ok$ok top ${g.topCard} ac${g.activeColor.name}');
        }
        for (int p = 0; p < pc; p++) {
          if (p != seat && g.vulnerable != null && rng.nextDouble() < UnoBot.catchChance(levels[p])) {
            print('uno catch $p -> ${g.catchNx(p)}');
          }
        }
        steps++;
      }
      print('uno $seed/$pc winner ${g.winner} score ${g.winner == null ? 0 : g.scoreFor(g.winner!)} events ${g.events.length} last ${g.events.last}');
    }
  }
}
